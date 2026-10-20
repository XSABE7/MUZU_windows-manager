# macOS Window Controls, Window Minimization, and Moveable Glass Dock Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement macOS traffic light titlebar controls (Red=Close, Yellow=Minimize, Green=Maximize/Restore), manage minimized windows inside the Dock, upgrade the Dock to an elegant macOS frosted-glass aesthetic with running indicators and divider, and enable moving the Dock between screen edges.

**Architecture:** Extend `Client` with `is_minimized`, `is_maximized`, and saved geometry; update `titlebar.c` to draw 3 macOS traffic light circles and route click events to close, minimize, or maximize; expand `dock.c` to support a dynamic list of minimized windows, render macOS frosted pill glass styling with dividers and running dots, and handle right-click or drag to cycle/reposition across screen edges (`DOCK_BOTTOM`, `DOCK_LEFT`, `DOCK_TOP`, `DOCK_RIGHT`).

**Tech Stack:** C11, Xlib (X11), Cairo, GTK ColorRGBA Palette.

**Spec:** `docs/superpowers/specs/2026-10-20-macos-dock-and-window-controls.md`

## Global Constraints

- Pure C11 (`-Wall -Wextra -pedantic -std=c11 -O2`). Zero compiler warnings.
- Zero runtime dependencies on `libgtk` or `libglib` in the WM binary.
- RAM budget: Window manager RSS must remain strictly under 30 MB.
- All existing and new automated unit tests must pass cleanly.

## Review Focus

1. **Maximize Toggle State Restoration:** Restoring a maximized window must return it to its exact previous tiled or floating geometry.
2. **Minimize Reflow in Tiled Mode:** When a tiled window is minimized, remaining tiled windows must reflow cleanly without leaving gaps.
3. **Restoring Minimized Windows from Dock:** Clicking a minimized window icon on the Dock must map the window, focus it, raise it, and remove it from the Dock's minimized list.
4. **Dock Reorientation Geometry (Horizontal vs Vertical):** When dock is moved to `DOCK_LEFT` or `DOCK_RIGHT`, items, dividers, and mouse hitboxes must rotate to vertical alignment cleanly.
5. **Memory Teardown of Minimized Icons:** Surface handles and client references in the dock must be freed without memory leaks when windows close while minimized.

---

### Task 1: macOS Window Controls & Maximize/Restore Logic

**Files:**
- Modify: `include/wm.h`
- Modify: `src/wm.c`
- Modify: `include/titlebar.h`
- Modify: `src/titlebar.c`
- Modify: `src/main.c`
- Test: `tests/test_layout.c`

**Interfaces:**
- Adds to `Client`:
  - `int is_minimized;`
  - `int is_maximized;`
  - `int saved_x, saved_y, saved_width, saved_height;`
  - `ClientMode saved_mode;`
- Produces in `titlebar.h`:
  - `void client_toggle_maximize(Display *dpy, Client *c, LayoutConfig *cfg);`
  - `typedef enum { TITLEBAR_CLICK_NONE, TITLEBAR_CLICK_CLOSE, TITLEBAR_CLICK_MINIMIZE, TITLEBAR_CLICK_MAXIMIZE, TITLEBAR_CLICK_DRAG } TitlebarAction;`
  - `TitlebarAction titlebar_get_action(int click_x, int width);`

- [ ] **Step 1: Write the failing test for titlebar actions and maximize/restore**

In `tests/test_layout.c`:
- Test `titlebar_get_action()`:
  - `x < 24`: `TITLEBAR_CLICK_CLOSE` (Red dot)
  - `x >= 24 && x < 40`: `TITLEBAR_CLICK_MINIMIZE` (Yellow dot)
  - `x >= 40 && x < 58`: `TITLEBAR_CLICK_MAXIMIZE` (Green dot)
  - `x >= 58`: `TITLEBAR_CLICK_DRAG`
- Test `client_toggle_maximize()`:
  - Call on floating window: geometry expands to full screen, `c->is_maximized == 1`.
  - Call again: geometry restores to pre-maximized dimensions, `c->is_maximized == 0`.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_layout && ./test_layout`
Expected: FAIL (`titlebar_get_action` not declared)

- [ ] **Step 3: Implement macOS traffic lights and maximize in `src/titlebar.c` and `src/wm.c`**

- In `include/wm.h`: add `is_minimized`, `is_maximized`, `saved_x`, `saved_y`, `saved_width`, `saved_height`, `saved_mode` to `Client`.
- In `src/wm.c`: initialize them in `client_add()`.
- In `src/titlebar.c`:
  - Implement `titlebar_get_action()`.
  - Implement `client_toggle_maximize()`: save pre-max geometry, set width/height to screen, or restore.
  - In `titlebar_draw()`: render 3 circular buttons on top-left:
    - Red dot at `x = 14, y = 12, r = 5.0`
    - Yellow dot at `x = 30, y = 12, r = 5.0`
    - Green dot at `x = 46, y = 12, r = 5.0`
    - Shift window title text to `x = 64`.
- In `src/main.c`:
  - In `ButtonPress` on titlebar frame: use `titlebar_get_action(ev.xbutton.x, curr->width)` to dispatch Close, Minimize, or Maximize!

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_layout && ./test_layout`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/wm.h src/wm.c include/titlebar.h src/titlebar.c src/main.c tests/test_layout.c
git commit -m "feat: implement macOS traffic light controls and window maximize/restore"
```

---

### Task 2: Window Minimization & Dock Integration

**Files:**
- Modify: `include/dock.h`
- Modify: `src/dock.c`
- Modify: `src/main.c`
- Test: `tests/test_layout.c`

**Interfaces:**
- Produces in `dock.h`:
  - `void dock_add_minimized(Dock *dock, Client *c);`
  - `void dock_remove_minimized(Dock *dock, Client *c);`
  - `Client *dock_get_minimized_at(Dock *dock, int x, int y);`
- Modifies `src/main.c`:
  - When yellow button clicked: minimize window, unmap frame/win, add to dock, reflow remaining windows.
  - When minimized dock item clicked: unminimize window, map frame/win, remove from dock, focus & raise.
  - In `client_teardown()`: remove from dock if destroyed while minimized.

- [ ] **Step 1: Write the failing test for minimized window list in dock**

In `tests/test_layout.c`:
- Test `dock_add_minimized()`: adds client to dock minimized list.
- Test `dock_get_minimized_at()`: returns client pointer for coordinates over minimized slot.
- Test `dock_remove_minimized()`: removes client from list.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_layout && ./test_layout`
Expected: FAIL

- [ ] **Step 3: Implement minimized window list in `src/dock.c` and connect in `src/main.c`**

- In `include/dock.h`:
  - Add `minimized_clients[16]`, `minimized_count` to `Dock`.
- In `src/dock.c`:
  - Implement `dock_add_minimized()`, `dock_remove_minimized()`, and lookup in `dock_get_item_at()`.
  - Draw minimized window icons after a vertical separator line.
- In `src/main.c`:
  - When minimize triggered: set `c->is_minimized = 1`, unmap window, call `dock_add_minimized(&dock, c)`, reflow tiled windows.
  - In `dock_handle_click`: if clicked on a minimized window, restore it (`c->is_minimized = 0`, map, focus, raise, `dock_remove_minimized`).
  - In `client_teardown`: call `dock_remove_minimized(&dock, c)`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_layout && ./test_layout`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/dock.h src/dock.c src/main.c tests/test_layout.c
git commit -m "feat: implement window minimize to dock and restore functionality"
```

---

### Task 3: macOS Frosted Glass Dock UI, Divider, & Edge Repositioning

**Files:**
- Modify: `include/dock.h`
- Modify: `src/dock.c`
- Modify: `src/main.c`
- Test: `tests/test_layout.c`

**Interfaces:**
- Produces in `dock.h`:
  - `void dock_cycle_position(Display *dpy, Dock *dock, int screen_w, int screen_h);`
- Modifies `dock_render()`:
  - macOS frosted glass pill shape with rounded corners (r = 16px).
  - Gloss specular highlight on top border.
  - Active running app indicator dots below launcher icons.
  - Vertical/horizontal separator divider before minimized windows section.
  - Dynamic horizontal vs vertical layout for `DOCK_BOTTOM`/`DOCK_TOP` vs `DOCK_LEFT`/`DOCK_RIGHT`.

- [ ] **Step 1: Write the failing test for dock position cycling**

In `tests/test_layout.c`:
- Verify `dock_cycle_position()` cycles `DOCK_BOTTOM -> DOCK_LEFT -> DOCK_TOP -> DOCK_RIGHT -> DOCK_BOTTOM`.
- Verify width and height dynamically adapt for horizontal vs vertical orientation.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_layout && ./test_layout`
Expected: FAIL

- [ ] **Step 3: Implement macOS styling, dividers, and position cycling in `src/dock.c`**

- In `src/dock.c`:
  - Enhance `dock_render()`:
    - Draw rounded pill rectangle with radius 16px.
    - Render subtle frosted glass gradient background with thin highlight outline.
    - Draw separator bar between pinned apps and minimized windows.
    - Draw running indicator dot (3px white circle) below apps that have open windows.
    - Handle vertical drawing when `pos == DOCK_LEFT || pos == DOCK_RIGHT`.
  - Implement `dock_cycle_position()`.
- In `src/main.c`:
  - In `ButtonPress` on `dock.win`: if `ev.xbutton.button == Button3` (Right-click), call `dock_cycle_position(dpy, &dock, screen_w, screen_h)`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_layout && ./test_layout`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/dock.h src/dock.c src/main.c tests/test_layout.c
git commit -m "feat: add macOS frosted glass dock styling, running indicators, and repositioning"
```

---

### Task 4: Full System Verification, Xephyr Visual Test & Memory Budget Check

**Files:**
- Modify: `tests/test_runner.sh`

- [ ] **Step 1: Run all unit tests**

Run: `make clean && make -j4 && make tests && bash tests/test_runner.sh`
Expected: All 7 unit tests PASS, RAM remains < 30 MB.

- [ ] **Step 2: Run Xephyr visual and functional verification**

Verify:
- Red/Yellow/Green macOS titlebar buttons work as expected.
- Maximizing expands window and restoring returns to exact dimensions.
- Minimizing window puts it into the Dock; clicking it restores the window.
- Right-clicking Dock cycles positions (Bottom -> Left -> Top -> Right -> Bottom).
- Stress test runs with zero memory leaks.

- [ ] **Step 3: Commit and clean up**

```bash
git add tests/test_runner.sh
git commit -m "test: verify macOS window controls, dock minimization, and repositioning"
```
