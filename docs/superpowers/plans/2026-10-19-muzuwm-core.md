# MUZUwm (むず) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build MUZUwm, an X11 mouse-first hybrid window manager based on the `tinywm` foundation, featuring auto-tiling to floating windowing, a full-screen Launchpad with calendar, a flexible auto-hiding Dock, and the "Will She Know" Easter egg, with total RAM footprint < 200 MB.

**Architecture:** Extend the minimal event loop of `tinywm` into a modular C11 architecture. Use Xlib for X11 connection, window management, and input grabbing; use Cairo (`cairo-xlib`) for rendering vector UI (titlebars, Launchpad dashboard, Dock); use an isolated event state machine for double-tap Super detection, hot corners, and Easter egg key sequence buffering.

**Tech Stack:** C (C11), Xlib (`libX11`, `libXext`), Cairo (`cairo-xlib`), Make, picom (external compositor).

**Spec:** [docs/superpowers/specs/2026-10-19-muzuwm-design.md](file:///home/peach/git/MUZUwm/docs/superpowers/specs/2026-10-19-muzuwm-design.md)

---

## Global Constraints

- **Platform:** Linux X11 (Single-monitor target for v1).
- **Language & Standard:** C11 with `-Wall -Wextra -pedantic`.
- **Memory Ceiling:** Combined runtime RAM of `muzuwm` + `picom` < 200 MB (Target: `muzuwm` ~30–50 MB).
- **Core Base:** Built upon the minimal event loop principles of `tinywm/tinywm.c`.
- **Easter Egg URL:** `https://youtu.be/-3haoCPz6MM?si=-nqANYwpOqBDr81x` launched via `xdg-open`.
- **Easter Egg Sequence:** Exactly `w`, `w`, `s`, `s`, `a`, `d`, `a`, `d`, `b`, `a` in Launchpad.

---

## Review Focus

1. **Window Close Reflow with Empty Lists:** When all tiled windows close or only one window remains, ensure no division-by-zero or segfault occurs in stack geometry calculation.
2. **Rapid Key/Mouse Event Interleaving:** Double-tap Super timer expiring exactly while a normal window keypress occurs must not leave the key grab in a hung state.
3. **Malformed `.desktop` Entries:** App scanner encountering missing `Exec`, unescaped quotes, or non-UTF8 strings in `/usr/share/applications` must not crash or leak memory.
4. **Dock Auto-Hide Edge Re-entry Race:** Cursor rapidly moving across the dock trigger threshold must debounce smoothly without rapid flicker or X11 event queue buildup.
5. **Easter Egg Buffer Overflow:** Continuous random keystrokes on Launchpad must safely slide the buffer without buffer overflow or unintended trigger.

---

## File Structure & Module Decomposition

```
/home/peach/git/MUZUwm/
├── Makefile
├── include/
│   ├── muzu.h              # Common definitions, types, config constants
│   ├── wm.h                # Window management (tiling, floating, client list)
│   ├── titlebar.h          # Cairo titlebar decoration and drag state
│   ├── trigger.h           # Double-tap Super & Hot Corner detector
│   ├── launchpad.h         # Full-screen Launchpad, app grid, search & calendar
│   ├── easter_egg.h        # Konami sequence detector & Sek Loso trigger
│   └── dock.h              # Quick-access auto-hiding dock
├── src/
│   ├── main.c              # Main X11 event loop (evolved from tinywm)
│   ├── wm.c                # Window layout engine (auto-tiling, snapping, reflow)
│   ├── titlebar.c          # Titlebar window reparenting and Cairo rendering
│   ├── trigger.c           # Timer and pointer coordinate tracking
│   ├── launchpad.c         # Launchpad UI & desktop app scanner
│   ├── easter_egg.c        # Keystroke buffer state machine
│   └── dock.c              # Dock overlay window & edge auto-hide
└── tests/
    ├── test_layout.c       # Unit test for tiling layout geometry & reflow
    ├── test_easter_egg.c   # Unit test for sequence buffer matching
    ├── test_calendar.c     # Unit test for month/date calculation
    └── test_runner.sh      # Xephyr / memory check integration script
```

---

## Tasks

### Task 1: Scaffolding, Build System & Layout Engine Unit Tests

**Files:**
- Create: `include/muzu.h`
- Create: `include/wm.h`
- Create: `src/wm.c`
- Create: `tests/test_layout.c`
- Create: `Makefile`

**Interfaces:**
- Produces:
  ```c
  typedef enum { CLIENT_TILED, CLIENT_FLOATING } ClientMode;
  typedef struct Client {
      Window win;
      Window frame;
      int x, y, width, height;
      ClientMode mode;
      struct Client *next;
  } Client;

  typedef struct {
      int screen_width;
      int screen_height;
      int master_count;
  } LayoutConfig;

  void layout_calculate(const LayoutConfig *cfg, Client *head, int count);
  void client_add(Client **head, Window win, ClientMode mode);
  void client_remove(Client **head, Window win);
  Client *client_find(Client *head, Window win);
  ```

- [ ] **Step 1: Write the failing test for layout calculations**

In `tests/test_layout.c`:
```c
#include <assert.h>
#include <stdio.h>
#include "wm.h"

int main(void) {
    LayoutConfig cfg = { .screen_width = 1920, .screen_height = 1080, .master_count = 1 };
    Client *head = NULL;

    // Test 1: Single window auto-maximize
    client_add(&head, 101, CLIENT_TILED);
    layout_calculate(&cfg, head, 1);
    assert(head->width == 1920);
    assert(head->height == 1080);
    assert(head->x == 0 && head->y == 0);

    // Test 2: Two windows 50:50 split
    client_add(&head, 102, CLIENT_TILED);
    layout_calculate(&cfg, head, 2);
    Client *first = client_find(head, 101);
    Client *second = client_find(head, 102);
    assert(first->width == 960);
    assert(second->width == 960);
    assert(first->x == 0 && second->x == 960);

    // Test 3: Three windows (Master 50% left, Stack 2x 50% right split)
    client_add(&head, 103, CLIENT_TILED);
    layout_calculate(&cfg, head, 3);
    Client *third = client_find(head, 103);
    assert(first->width == 960 && first->height == 1080);
    assert(second->width == 960 && second->height == 540);
    assert(third->width == 960 && third->height == 540);
    assert(third->y == 540);

    printf("test_layout passed!\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_layout && ./test_layout`  
Expected: FAIL (missing header / functions)

- [ ] **Step 3: Implement `include/muzu.h`, `include/wm.h`, `src/wm.c`, and `Makefile`**

Implement `client_add`, `client_remove`, `client_find`, and `layout_calculate` in `src/wm.c` handling 1 window (100%), 2 windows (50:50), and 3+ windows (left 50% master, right stacked vertically).

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_layout && ./test_layout`  
Expected: PASS (`test_layout passed!`)

- [ ] **Step 5: Commit**

```bash
git add include/muzu.h include/wm.h src/wm.c tests/test_layout.c Makefile
git commit -m "feat(wm): implement core tiling layout engine and unit tests"
```

---

### Task 2: Window Titlebar Reparenting & Mouse Drag-to-Float

**Files:**
- Create: `include/titlebar.h`
- Create: `src/titlebar.c`
- Modify: `src/wm.c`
- Test: `tests/test_layout.c`

**Interfaces:**
- Consumes: `Client`, `LayoutConfig` from `include/wm.h`
- Produces:
  ```c
  #define TITLEBAR_HEIGHT 24

  Window titlebar_create(Display *dpy, Window client_win, int width);
  void titlebar_draw(Display *dpy, Window frame, const char *title, int width);
  void handle_titlebar_drag(Display *dpy, Client *c, int root_x, int root_y, int screen_w, int screen_h);
  void handle_titlebar_double_click(Display *dpy, Client *c, LayoutConfig *cfg);
  ```

- [ ] **Step 1: Write unit test for Drag-to-Float state transition in `tests/test_layout.c`**

Add test:
```c
// Verify that dragging titlebar transitions CLIENT_TILED -> CLIENT_FLOATING
Client *c = client_find(head, 101);
assert(c->mode == CLIENT_TILED);
c->mode = CLIENT_FLOATING;
layout_calculate(&cfg, head, 2); // Only 1 tiled window left, so remaining tiled window auto-maximizes
assert(client_find(head, 102)->width == 1920);
```

- [ ] **Step 2: Run test to verify it passes**

Run: `make test_layout && ./test_layout`  
Expected: PASS

- [ ] **Step 3: Implement `src/titlebar.c` with Cairo drawing**

Implement Cairo vector titlebar with window title and close button. When left button drags, detach `c->mode = CLIENT_FLOATING` and follow mouse delta. When double clicked, toggle between `CLIENT_TILED` and `CLIENT_FLOATING`.

- [ ] **Step 4: Build to ensure clean compilation**

Run: `make src/titlebar.o`  
Expected: Clean compilation with no warnings.

- [ ] **Step 5: Commit**

```bash
git add include/titlebar.h src/titlebar.c tests/test_layout.c
git commit -m "feat(titlebar): add cairo titlebars with drag-to-float support"
```

---

### Task 3: Trigger Detector (Double-tap Super & Top-Left Hot Corner)

**Files:**
- Create: `include/trigger.h`
- Create: `src/trigger.c`
- Test: `tests/test_trigger.c`

**Interfaces:**
- Produces:
  ```c
  typedef struct {
      unsigned long last_super_press_ms;
      int hot_corner_active;
  } TriggerState;

  int trigger_check_super(TriggerState *state, unsigned long event_time_ms);
  int trigger_check_hot_corner(int x, int y, int threshold_px);
  ```

- [ ] **Step 1: Write failing test in `tests/test_trigger.c`**

```c
#include <assert.h>
#include <stdio.h>
#include "trigger.h"

int main(void) {
    TriggerState state = {0};
    // 1st press
    assert(trigger_check_super(&state, 1000) == 0);
    // 2nd press within 220ms -> triggered!
    assert(trigger_check_super(&state, 1220) == 1);
    // Subsequent press after 400ms -> not triggered
    assert(trigger_check_super(&state, 1700) == 0);

    // Hot corner at (0,0) with 8px threshold
    assert(trigger_check_hot_corner(0, 0, 8) == 1);
    assert(trigger_check_hot_corner(5, 5, 8) == 1);
    assert(trigger_check_hot_corner(20, 20, 8) == 0);

    printf("test_trigger passed!\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_trigger && ./test_trigger`  
Expected: FAIL

- [ ] **Step 3: Implement `src/trigger.c`**

Implement timestamp delta (< 250ms) comparison and coordinate threshold check.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_trigger && ./test_trigger`  
Expected: PASS (`test_trigger passed!`)

- [ ] **Step 5: Commit**

```bash
git add include/trigger.h src/trigger.c tests/test_trigger.c
git commit -m "feat(trigger): implement double-tap Super and hot corner detection"
```

---

### Task 4: Easter Egg Engine ("Will She Know" Konami Code)

**Files:**
- Create: `include/easter_egg.h`
- Create: `src/easter_egg.c`
- Test: `tests/test_easter_egg.c`

**Interfaces:**
- Produces:
  ```c
  #define SEK_LOSO_URL "https://youtu.be/-3haoCPz6MM?si=-nqANYwpOqBDr81x"

  typedef struct {
      char buffer[16];
      int length;
      int triggered;
  } EasterEggState;

  void easter_egg_init(EasterEggState *st);
  int easter_egg_feed_key(EasterEggState *st, char key);
  void easter_egg_launch_music(void);
  ```

- [ ] **Step 1: Write failing test in `tests/test_easter_egg.c`**

```c
#include <assert.h>
#include <stdio.h>
#include "easter_egg.h"

int main(void) {
    EasterEggState st;
    easter_egg_init(&st);

    const char *seq = "wwssadadba";
    for (int i = 0; seq[i] != '\0'; i++) {
        int res = easter_egg_feed_key(&st, seq[i]);
        if (i == 9) assert(res == 1);
        else assert(res == 0);
    }
    assert(st.triggered == 1);

    // Random typing resets sequence
    easter_egg_init(&st);
    assert(easter_egg_feed_key(&st, 'w') == 0);
    assert(easter_egg_feed_key(&st, 'x') == 0);
    assert(easter_egg_feed_key(&st, 'w') == 0);
    assert(st.triggered == 0);

    printf("test_easter_egg passed!\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_easter_egg && ./test_easter_egg`  
Expected: FAIL

- [ ] **Step 3: Implement `src/easter_egg.c`**

Implement rolling buffer comparison against target `"wwssadadba"`. Implement `easter_egg_launch_music()` using `fork()` + `execlp("xdg-open", "xdg-open", SEK_LOSO_URL, NULL)`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_easter_egg && ./test_easter_egg`  
Expected: PASS (`test_easter_egg passed!`)

- [ ] **Step 5: Commit**

```bash
git add include/easter_egg.h src/easter_egg.c tests/test_easter_egg.c
git commit -m "feat(easter-egg): implement wwssadadba sequence buffer and music launcher"
```

---

### Task 5: Launchpad & Calendar Dashboard (Cairo Overlay)

**Files:**
- Create: `include/launchpad.h`
- Create: `src/launchpad.c`
- Create: `tests/test_calendar.c`
- Test: `tests/test_calendar.c`

**Interfaces:**
- Consumes: `EasterEggState` from `include/easter_egg.h`
- Produces:
  ```c
  typedef struct {
      char name[64];
      char exec[128];
  } AppItem;

  typedef struct {
      Window win;
      int is_visible;
      AppItem apps[128];
      int app_count;
      char search_query[64];
      int current_month_offset;
      EasterEggState egg;
  } Launchpad;

  void launchpad_init(Display *dpy, Launchpad *lp, int screen_w, int screen_h);
  void launchpad_scan_apps(Launchpad *lp);
  void launchpad_render(Display *dpy, Launchpad *lp, int screen_w, int screen_h);
  void launchpad_toggle(Display *dpy, Launchpad *lp);
  void launchpad_handle_key(Display *dpy, Launchpad *lp, XKeyEvent *ev);
  void launchpad_handle_click(Display *dpy, Launchpad *lp, int x, int y);
  ```

- [ ] **Step 1: Write failing test in `tests/test_calendar.c`**

Test calendar date calculations (determining starting weekday and total days in month):
```c
#include <assert.h>
#include <stdio.h>
#include <time.h>

int days_in_month(int year, int month); // 1-12

int main(void) {
    assert(days_in_month(2026, 2) == 28);
    assert(days_in_month(2024, 2) == 29); // Leap year
    assert(days_in_month(2026, 10) == 31);
    printf("test_calendar passed!\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_calendar && ./test_calendar`  
Expected: FAIL

- [ ] **Step 3: Implement calendar logic and `src/launchpad.c`**

- Implement `days_in_month()` in `src/launchpad.c`.
- Implement `launchpad_scan_apps()` scanning `/usr/share/applications/*.desktop`.
- Implement Cairo split rendering: Left side search input + app grid; Right side digital clock + calendar grid + "Will She Know" button if `lp->egg.triggered`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_calendar && ./test_calendar`  
Expected: PASS (`test_calendar passed!`)

- [ ] **Step 5: Commit**

```bash
git add include/launchpad.h src/launchpad.c tests/test_calendar.c
git commit -m "feat(launchpad): implement split dashboard with app grid and calendar"
```

---

### Task 6: Quick-Access Dock with Auto-Hide

**Files:**
- Create: `include/dock.h`
- Create: `src/dock.c`
- Test: `tests/test_layout.c`

**Interfaces:**
- Produces:
  ```c
  typedef enum { DOCK_BOTTOM, DOCK_TOP, DOCK_LEFT, DOCK_RIGHT } DockPosition;

  typedef struct {
      Window win;
      DockPosition pos;
      int size;           // Thickness (e.g. 48px)
      int is_hidden;      // 1 if auto-hidden
      int hover_state;
  } Dock;

  void dock_init(Display *dpy, Dock *dock, DockPosition pos, int screen_w, int screen_h);
  void dock_render(Display *dpy, Dock *dock, int screen_w, int screen_h);
  void dock_handle_motion(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h);
  void dock_set_position(Display *dpy, Dock *dock, DockPosition new_pos, int screen_w, int screen_h);
  ```

- [ ] **Step 1: Write integration check for Dock positioning**

Add test in `tests/test_layout.c` verifying geometry calculation for all 4 edges (Bottom, Top, Left, Right).

- [ ] **Step 2: Implement `src/dock.c`**

Create `override_redirect` window with Cairo icons. Implement auto-hide slide when cursor leaves, and slide-in when cursor approaches screen edge.

- [ ] **Step 3: Compile and run test**

Run: `make test_layout && ./test_layout`  
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add include/dock.h src/dock.c
git commit -m "feat(dock): implement configurable 4-edge dock with auto-hide"
```

---

### Task 7: Main Event Loop Integration & tinywm Evolution

**Files:**
- Create: `src/main.c` (incorporating and evolving `tinywm/tinywm.c`)
- Modify: `Makefile`
- Test: Integration build

**Interfaces:**
- Connects: `wm.h`, `titlebar.h`, `trigger.h`, `launchpad.h`, `dock.h`, `easter_egg.h`

- [ ] **Step 1: Implement `src/main.c`**

Initialize Xlib connection:
1. Grab `Super` key and mouse buttons on root window.
2. Setup subwindows for Dock and Launchpad.
3. Handle X11 events:
   - `MapRequest`: Register new client, auto-maximize or auto-tile.
   - `UnmapNotify` / `DestroyNotify`: Unregister client and reflow remaining windows.
   - `KeyPress`: Handle Super double-tap, hot corner, and Launchpad inputs.
   - `ButtonPress` / `MotionNotify`: Handle titlebar drag-to-float, edge snapping to maximize/tile.
   - `EnterNotify`: Trigger Dock auto-reveal.

- [ ] **Step 2: Build binary `muzuwm`**

Run: `make muzuwm`  
Expected: Clean binary produced with no warnings or errors.

- [ ] **Step 3: Commit**

```bash
git add src/main.c Makefile
git commit -m "feat(core): integrate main X11 event loop and window lifecycle"
```

---

### Task 8: Verification & RAM Benchmark (< 200 MB Hard Gate)

**Files:**
- Create: `tests/test_runner.sh`

- [ ] **Step 1: Write verification script `tests/test_runner.sh`**

Script starts `Xvfb` (or `Xephyr`), runs `muzuwm`, spawns test windows (`xterm`), runs `picom`, measures RSS memory via `ps -o rss= -p <PID>` and validates that `total_ram_mb < 200`.

- [ ] **Step 2: Execute benchmark script**

Run: `bash tests/test_runner.sh`  
Expected: All unit tests pass and combined RAM is < 150 MB (comfortably below 200 MB).

- [ ] **Step 3: Commit**

```bash
git add tests/test_runner.sh
git commit -m "test(perf): add automated integration test and ram budget verification"
```
