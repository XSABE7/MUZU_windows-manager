# Liquid Glass, Squircle Dock & 20s Inactivity Ghost Windows Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement modern Liquid/Acrylic Frosted Glass visual styling, cached pre-rendered Titlebars, a Squircle (rounded-rectangle plate) Dock with zero-overhead auto-hide, a 20-second inactivity Ghost Glass window transparency system with Hover-to-Wake, and lightweight Picom compositor integration keeping total RAM strictly under 200 MB.

**Architecture:** 
- Titlebars pre-render static glass backgrounds into cached `cairo_surface_t` instances per client, invalidating only on resize or theme changes.
- Unfocused windows track `last_unfocus_time` and `is_hovered`. A 1-second event loop check applies `_NET_WM_WINDOW_OPACITY` (65% Ghost Glass) after 20 seconds. `EnterNotify` and `LeaveNotify` provide instantaneous Hover-to-Wake.
- The Dock uses a sleek Squircle (rounded rectangle plate with 14px continuous curvature) geometry and freezes redrawing when auto-hidden.
- MUZUwm writes and manages a tuned dual-kawase blur `~/.config/muzuwm/picom.conf` running daemonized with total RAM ~50 MB.

**Tech Stack:** C11, Xlib, Cairo Graphics, FreeDesktop EWMH (`_NET_WM_WINDOW_OPACITY`), Picom.

**Spec:** [docs/superpowers/specs/2026-10-20-liquid-glass-dock-inactivity.md](file:///home/peach/git/MUZUwm/docs/superpowers/specs/2026-10-20-liquid-glass-dock-inactivity.md)

## Global Constraints

- **Language & Compiler:** Pure C11, compiled with `-Wall -Wextra -pedantic -std=c11`, zero compiler warnings.
- **Memory Footprint:** Total combined RSS memory (MUZUwm + Picom) must remain strictly under 200 MB (measured ~40–60 MB target).
- **Inactivity Timeout:** Exactly 20 seconds after losing focus.
- **Ghost Opacity:** Set to `0xA6AAAAAA` (approx. 65% alpha).
- **Dock Corner Radius:** Exactly 14px rounded rectangle (`draw_rounded_rectangle`).
- **Auto-Hide Idle CPU:** 0% CPU utilization while dock is hidden.

## Review Focus

1. **Titlebar Cache Lifecycle on Client Destruction:** Ensure `c->titlebar_cache` is destroyed cleanly in `client_destroy` / `unmanage` to avoid memory leaks.
2. **Ghost Window Hover Cycling / Rapid Crossing:** Rapid mouse movements across multiple tiled ghost windows must update `is_hovered` and restore/dim opacity smoothly without X11 event pileup.
3. **Ghost Window Direct Click Focus:** Clicking on a ghost client window must immediately focus the window, clear ghost status, and set opacity to 100% without losing the click event.
4. **Dock Auto-Hide Render Bypass Integrity:** When `dock.autohide_enabled == 1` and `dock.is_hidden == 1`, `dock_render` must exit immediately without acquiring Cairo contexts or blitting surfaces.
5. **Picom Daemon Cleanup on WM Exit:** When MUZUwm exits or receives SIGINT/SIGTERM, any launched picom background process must be terminated cleanly without leaving orphaned daemons.

---

### Task 1: Pre-rendered Liquid / Acrylic Glass Titlebars

**Files:**
- Modify: `include/wm.h:45-75` (Add `titlebar_cache`, `titlebar_cache_w`, `titlebar_cache_focused` to `Client`)
- Modify: `include/titlebar.h:1-25` (Add `titlebar_pre_render` prototype)
- Modify: `src/titlebar.c:1-120` (Implement Liquid Glass gradient & `titlebar_pre_render`)
- Modify: `src/wm.c:180-250` (Call `titlebar_pre_render` on frame resize; free cache on unmanage)
- Test: `tests/test_layout.c`

**Interfaces:**
- Produces: `cairo_surface_t *titlebar_pre_render(Display *dpy, int width, int is_focused, const GtkTheme *theme);`
- Consumes: `Client` from `include/wm.h`, `GtkTheme` from `include/gtk_theme.h`

- [ ] **Step 1: Write test verifying pre-rendered titlebar cache creation and dimensions**

Add unit test in `tests/test_layout.c`:
```c
// Test pre-rendered titlebar surface
cairo_surface_t *cache = titlebar_pre_render(dpy, 640, 1, NULL);
assert(cache != NULL);
assert(cairo_surface_status(cache) == CAIRO_STATUS_SUCCESS);
assert(cairo_image_surface_get_width(cache) == 640);
assert(cairo_image_surface_get_height(cache) == TITLEBAR_HEIGHT);
cairo_surface_destroy(cache);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_layout`
Expected: FAIL with `undefined reference to titlebar_pre_render`

- [ ] **Step 3: Implement `titlebar_pre_render` and Liquid Glass rendering in `src/titlebar.c` and `include/wm.h`**

- In `include/wm.h`: Add `cairo_surface_t *titlebar_cache; int titlebar_cache_w; int titlebar_cache_focused;` to `struct Client`.
- In `src/titlebar.c`:
  - Implement `titlebar_pre_render(Display *dpy, int width, int is_focused, const GtkTheme *theme)` generating an ARGB32 image surface with:
    - Base fill: `rgba(22, 24, 30, 0.72)` (focused) or `rgba(18, 20, 25, 0.55)` (unfocused).
    - Top highlight rim: `rgba(255, 255, 255, 0.22)`.
    - Bottom divider: `rgba(0, 0, 0, 0.45)`.
    - Outer border: `rgba(255, 255, 255, 0.14)`.
  - In `titlebar_render`: If `c->titlebar_cache` matches width and focus state, blit via `cairo_set_source_surface(cr, c->titlebar_cache, 0, 0); cairo_paint(cr);`, then render title text and traffic light buttons.
- In `src/wm.c`: Free `c->titlebar_cache` in `client_destroy` / `unmanage` and regenerate on `XMoveResizeWindow`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_layout && ./test_layout`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/wm.h include/titlebar.h src/titlebar.c src/wm.c tests/test_layout.c
git commit -m "feat(titlebar): implement pre-rendered liquid glass titlebars and surface cache"
```

---

### Task 2: Squircle Dock Geometry & Frosted Acrylic Styling

**Files:**
- Modify: `include/dock.h:20-45`
- Modify: `src/dock.c:420-560` (Update `dock_render` with Squircle plate 14px curvature and frosted acrylic styling)
- Test: `tests/test_layout.c`

**Interfaces:**
- Produces: Updated `dock_render(Display *dpy, Dock *dock, int screen_w, int screen_h)` with Squircle geometry.
- Consumes: `Dock` from `include/dock.h`.

- [ ] **Step 1: Write test verifying Squircle dock dimensions and corner radius attributes**

Add unit test in `tests/test_layout.c`:
```c
Dock test_dock;
dock_init(dpy, &test_dock, DOCK_BOTTOM, 1920, 1080, NULL);
assert(test_dock.width > 0 && test_dock.height > 0);
dock_cleanup(&test_dock);
```

- [ ] **Step 2: Run test to verify it compiles and runs**

Run: `make test_layout && ./test_layout`
Expected: PASS

- [ ] **Step 3: Update `src/dock.c` to render Squircle Plate with Liquid Frosted Glass aesthetic**

- Render background using rounded rectangle with 14px radius.
- Fill with dark smoked frosted glass `rgba(18, 20, 26, 0.75)`.
- Stroke inner specular reflection border `rgba(255, 255, 255, 0.16)`.
- Render subtle outer shadow border `rgba(0, 0, 0, 0.50)`.
- Maintain active window indicator dots, magnification wave scaling, and divider separators.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_layout && ./test_layout`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/dock.h src/dock.c tests/test_layout.c
git commit -m "feat(dock): implement squircle rounded rectangle plate geometry and liquid glass theme"
```

---

### Task 3: Zero-Overhead Auto-hide Rendering Freeze

**Files:**
- Modify: `src/dock.c:430-460` (Early exit in `dock_render` when `is_hidden && autohide_enabled`)
- Modify: `src/main.c:260-310` (Debounce leave events and handle edge reveal)
- Test: `tests/test_layout.c`

**Interfaces:**
- Consumes: `dock.autohide_enabled`, `dock.is_hidden`
- Produces: 0-overhead render bypass for hidden dock.

- [ ] **Step 1: Write test verifying render bypass when dock is hidden**

Add unit test in `tests/test_layout.c`:
```c
test_dock.autohide_enabled = 1;
test_dock.is_hidden = 1;
// Calling dock_render should perform no ops when hidden
dock_render(dpy, &test_dock, 1920, 1080);
assert(test_dock.is_hidden == 1);
```

- [ ] **Step 2: Implement render bypass and edge reveal in `src/dock.c` and `src/main.c`**

- In `src/dock.c:dock_render()`:
  ```c
  if (dock->autohide_enabled && dock->is_hidden) {
      return; // 0% CPU consumption while hidden
  }
  ```
- In `src/main.c`: When `EnterNotify` is received on `dock.win`:
  - If `dock.is_hidden`, set `dock.is_hidden = 0`, recalculate geometry to revealed position, `XMoveResizeWindow`, and call `dock_render`.
- When `LeaveNotify` is received on `dock.win`:
  - If `dock.autohide_enabled && !dock.is_dragging`, set `dock.is_hidden = 1`, slide to 4px edge sliver, and bypass rendering.

- [ ] **Step 3: Run test to verify it passes**

Run: `make test_layout && ./test_layout`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add src/dock.c src/main.c tests/test_layout.c
git commit -m "perf(dock): freeze dock rendering during auto-hide for zero CPU overhead"
```

---

### Task 4: 20-Second Inactivity Timer & Ghost Glass with Hover-to-Wake

**Files:**
- Modify: `include/wm.h:45-75` (Add `time_t last_unfocus_time; int is_ghost; int is_hovered;` to `Client`)
- Modify: `include/client_props.h:1-30` (Add `client_set_opacity(Display *dpy, Window win, unsigned int opacity)`)
- Modify: `src/client_props.c:1-100` (Implement `client_set_opacity` using `_NET_WM_WINDOW_OPACITY`)
- Modify: `src/wm.c:200-350` (Initialize timestamps; reset opacity on focus)
- Modify: `src/main.c:120-220` (Implement periodic 1s inactivity check; handle `EnterNotify`/`LeaveNotify` for Hover-to-Wake)
- Test: `tests/test_client_props.c`

**Interfaces:**
- Produces: `void client_set_opacity(Display *dpy, Window win, unsigned int opacity);`
- Consumes: EWMH `_NET_WM_WINDOW_OPACITY` atom

- [ ] **Step 1: Write test for `client_set_opacity`**

Add unit test in `tests/test_client_props.c`:
```c
// Verify setting opacity on mock window
Window test_win = XCreateSimpleWindow(dpy, root, 0, 0, 100, 100, 0, 0, 0);
client_set_opacity(dpy, test_win, 0xA6AAAAAA);
// Verify property exists
Atom actual_type;
int actual_format;
unsigned long nitems, bytes_after;
unsigned char *prop = NULL;
Atom net_opacity = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
XGetWindowProperty(dpy, test_win, net_opacity, 0, 1, False, XA_CARDINAL,
                   &actual_type, &actual_format, &nitems, &bytes_after, &prop);
assert(nitems == 1);
assert(*(unsigned long *)prop == 0xA6AAAAAA);
XFree(prop);
XDestroyWindow(dpy, test_win);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_client_props`
Expected: FAIL with `undefined reference to client_set_opacity`

- [ ] **Step 3: Implement `client_set_opacity` and 20s Inactivity + Hover-to-Wake engine**

- In `src/client_props.c`:
  ```c
  void client_set_opacity(Display *dpy, Window win, unsigned int opacity) {
      if (!dpy || win == None) return;
      Atom net_opacity = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
      if (opacity >= 0xFFFFFFFF) {
          XDeleteProperty(dpy, win, net_opacity);
      } else {
          unsigned long val = opacity;
          XChangeProperty(dpy, win, net_opacity, XA_CARDINAL, 32, PropModeReplace,
                          (unsigned char *)&val, 1);
      }
  }
  ```
- In `src/wm.c`:
  - When focus changes (`client_set_focus`):
    - Current focused window: if `c->is_ghost`, `client_set_opacity(dpy, c->win, 0xFFFFFFFF); c->is_ghost = 0;`.
    - Old window losing focus: `old->last_unfocus_time = time(NULL);`.
- In `src/main.c`:
  - In main event loop: Use a non-blocking timeout on `select()` or 1-second check:
    - If `now - c->last_unfocus_time >= 20 && c != focused && !c->is_hovered && !c->is_minimized`:
      `client_set_opacity(dpy, c->win, 0xA6AAAAAA); c->is_ghost = 1;`.
  - In `EnterNotify`: If pointer enters an unfocused client window/frame, set `c->is_hovered = 1;` and if `c->is_ghost`, `client_set_opacity(dpy, c->win, 0xFFFFFFFF);`.
  - In `LeaveNotify`: Set `c->is_hovered = 0;`. If `now - c->last_unfocus_time >= 20 && c != focused`, restore `client_set_opacity(dpy, c->win, 0xA6AAAAAA);`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_client_props && ./test_client_props`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/client_props.h src/client_props.c include/wm.h src/wm.c src/main.c tests/test_client_props.c
git commit -m "feat(inactivity): implement 20s unfocused ghost glass transparency with hover-to-wake"
```

---

### Task 5: Picom Compositor Integration & Config Generation

**Files:**
- Create: `include/compositor.h`
- Create: `src/compositor.c`
- Modify: `src/main.c:80-120` (Call `compositor_init()` on startup, `compositor_cleanup()` on exit)
- Modify: `Makefile:6-14` (Add `src/compositor.c` to SRCS)
- Test: `tests/test_runner.sh` & `tests/test_leak_and_race.sh`

**Interfaces:**
- Produces: `void compositor_init(void);`, `void compositor_cleanup(void);`
- Consumes: `~/.config/muzuwm/picom.conf`

- [ ] **Step 1: Create `include/compositor.h` and `src/compositor.c`**

- `include/compositor.h`:
  ```c
  #ifndef MUZU_COMPOSITOR_H
  #define MUZU_COMPOSITOR_H
  void compositor_init(void);
  void compositor_cleanup(void);
  #endif
  ```
- `src/compositor.c`:
  - Generates `~/.config/muzuwm/picom.conf` if not present:
    - Backend: `glx` (fallback `xrender`).
    - Blur method: `dual_kawase`, strength: 5.
    - Blur background for `_NET_WM_WINDOW_TYPE_DOCK` and normal client windows with alpha.
    - Shadows: radius 8, opacity 0.5.
    - VSync: true.
  - Spawns `picom -b --config ~/.config/muzuwm/picom.conf` if installed and not already running.
  - Tracks child process PID and sends `SIGTERM` in `compositor_cleanup()`.

- [ ] **Step 2: Update `Makefile` and wire into `src/main.c`**

- Add `src/compositor.c` to `SRCS` in `Makefile`.
- Call `compositor_init()` in `main()` before event loop.
- Call `compositor_cleanup()` on shutdown.

- [ ] **Step 3: Run full automated test suite and RAM verification**

Run: `make clean && make tests muzuwm && bash tests/test_runner.sh && bash tests/test_leak_and_race.sh`
Expected:
- All unit tests pass.
- Xephyr stress test passes with zero crashes or leaks.
- RAM footprint remains well under 200 MB budget ceiling.

- [ ] **Step 4: Commit**

```bash
git add include/compositor.h src/compositor.c src/main.c Makefile
git commit -m "feat(compositor): integrate picom with tuned dual-kawase liquid glass configuration"
```
