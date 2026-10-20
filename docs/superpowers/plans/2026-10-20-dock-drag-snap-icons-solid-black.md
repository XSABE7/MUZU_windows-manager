# Dock Drag-and-Snap, Accurate App Icons, and Solid Black Styling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow users to drag the Dock with right-click hold and snap to the nearest screen edge, resolve high-fidelity application icons (via `_NET_WM_ICON`, `WM_CLASS`, and expanded FreeDesktop theme lookup), and apply a solid, sleek deep black theme to the Dock.

**Architecture:**
- Extend `Dock` with drag tracking fields and edge snap calculation logic (`dock_calculate_nearest_edge`, `dock_handle_drag`, `dock_end_drag`).
- Enhance `icon_fetcher` and `client_props` to extract ARGB32 pixmaps from `_NET_WM_ICON` properties and expand fallback search paths (`22x22`, `24x24`, `gnome`, `Adwaita`).
- Update `dock_render()` to render clean, solid deep black (`#0a0a0a`) with sleek rounded pill geometry and subtle borders.

**Tech Stack:** C11, Xlib (X11), Cairo.

**Spec:** `docs/superpowers/specs/2026-10-20-dock-drag-snap-icons-solid-black.md`

## Global Constraints

- Pure C11 (`-Wall -Wextra -pedantic -std=c11 -O2`). Zero compiler warnings.
- RAM budget: Window manager RSS must remain strictly under 30 MB.
- All existing and new automated unit tests must pass cleanly.

---

### Task 1: Dock Right-Click Drag and Snap to Nearest Edge

**Files:**
- Modify: `include/dock.h`
- Modify: `src/dock.c`
- Modify: `src/main.c`
- Test: `tests/test_layout.c`

**Interfaces:**
- Produces in `dock.h`:
  - `DockPosition dock_calculate_nearest_edge(int center_x, int center_y, int screen_w, int screen_h);`
  - `void dock_handle_drag(Display *dpy, Dock *dock, int root_x, int root_y, int screen_w, int screen_h);`
  - `void dock_end_drag(Display *dpy, Dock *dock, int screen_w, int screen_h);`
- Modifies `src/main.c`:
  - `ButtonPress` on `dock.win` with `Button3`: start dock dragging, grab pointer.
  - `MotionNotify`: if `dock.is_dragging`, move dock window with cursor.
  - `ButtonRelease` on `Button3`: call `dock_end_drag(...)`, snap to nearest edge, ungrab pointer.

- [ ] **Step 1: Write the failing test for `dock_calculate_nearest_edge`**

In `tests/test_layout.c`:
- Test `dock_calculate_nearest_edge()`:
  - Center close to bottom: `(960, 1000)` in `1920x1080` -> returns `DOCK_BOTTOM`.
  - Center close to top: `(960, 50)` in `1920x1080` -> returns `DOCK_TOP`.
  - Center close to left: `(50, 540)` in `1920x1080` -> returns `DOCK_LEFT`.
  - Center close to right: `(1880, 540)` in `1920x1080` -> returns `DOCK_RIGHT`.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_layout && ./test_layout`
Expected: FAIL (`dock_calculate_nearest_edge` undefined)

- [ ] **Step 3: Implement drag and snap logic in `src/dock.c` and `src/main.c`**

- In `include/dock.h`:
  - Add `int is_dragging;`, `int drag_offset_x, drag_offset_y;` to `Dock`.
  - Declare `dock_calculate_nearest_edge`, `dock_handle_drag`, `dock_end_drag`.
- In `src/dock.c`:
  - Implement `dock_calculate_nearest_edge`: calculates distances to all 4 edges, returns closest edge.
  - Implement `dock_handle_drag`: updates `dock->x`, `dock->y` clamped to screen bounds and calls `XMoveWindow()`.
  - Implement `dock_end_drag`: calculates center, snaps to nearest edge with `dock_set_position()`, resets `is_dragging = 0`.
- In `src/main.c`:
  - In `ButtonPress` on `dock.win`: if `Button3`, start drag (`dock.is_dragging = 1`, save offsets, `XGrabPointer`).
  - In `MotionNotify`: if `dock.is_dragging`, call `dock_handle_drag()`.
  - In `ButtonRelease`: if `dock.is_dragging` and `Button3`, call `dock_end_drag()`, ungrab pointer.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_layout && ./test_layout && make muzuwm`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/dock.h src/dock.c src/main.c tests/test_layout.c
git commit -m "feat: implement dock right-click drag and snap to nearest edge"
```

---

### Task 2: Accurate Application Icons (`_NET_WM_ICON` & Expanded Theme Chain)

**Files:**
- Modify: `include/icon_fetcher.h`
- Modify: `src/icon_fetcher.c`
- Modify: `include/client_props.h`
- Modify: `src/client_props.c`
- Modify: `include/wm.h`
- Modify: `src/wm.c`
- Modify: `src/dock.c`
- Modify: `src/main.c`
- Test: `tests/test_icon_fetcher.c`

**Interfaces:**
- Produces in `icon_fetcher.h`:
  - `cairo_surface_t *icon_create_from_net_wm_icon(const unsigned long *data, unsigned long nitems, int target_size);`
- Produces in `client_props.h`:
  - `cairo_surface_t *client_get_icon(Display *dpy, Window win, const char *title, int target_size);`
- Adds to `Client` in `wm.h`:
  - `cairo_surface_t *icon_surface;`

- [ ] **Step 1: Write the failing test for `icon_create_from_net_wm_icon`**

In `tests/test_icon_fetcher.c`:
- Create synthetic 16x16 and 32x32 ARGB icon data buffer.
- Call `icon_create_from_net_wm_icon(data, nitems, 32)`:
  - Verify surface is not NULL.
  - Verify dimensions are 32x32.
  - Verify Cairo surface status is `CAIRO_STATUS_SUCCESS`.
- Call `cairo_surface_destroy(surf)`.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_icon_fetcher && ./test_icon_fetcher`
Expected: FAIL (`icon_create_from_net_wm_icon` undefined)

- [ ] **Step 3: Implement `icon_create_from_net_wm_icon`, expanded paths, and client icon retrieval**

- In `src/icon_fetcher.c`:
  - Implement `icon_create_from_net_wm_icon()`.
  - Add `gnome`, `Adwaita` to `icon_theme_set()` fallback chain.
  - Add `22x22/apps`, `24x24/apps`, `16x16/apps` to `app_subdirs[]`.
- In `src/client_props.c`:
  - Implement `client_get_icon()`: queries `_NET_WM_ICON`, then `WM_CLASS`, then title keywords.
- In `include/wm.h` & `src/wm.c`:
  - Add `cairo_surface_t *icon_surface;` to `Client`.
  - In `client_remove()` / teardown: destroy `icon_surface` if present.
- In `src/main.c`:
  - When window is mapped in `MapRequest`: populate `c->icon_surface = client_get_icon(dpy, c->win, c->title, 32);`.
- In `src/dock.c`:
  - In `dock_add_minimized()`: use `c->icon_surface` directly for `minimized[idx].icon_surface`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_icon_fetcher && ./test_icon_fetcher && make muzuwm`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/icon_fetcher.h src/icon_fetcher.c include/client_props.h src/client_props.c include/wm.h src/wm.c src/dock.c src/main.c tests/test_icon_fetcher.c
git commit -m "feat: implement _NET_WM_ICON parser and high-accuracy app icon resolution"
```

---

### Task 3: Solid Sleek Black Dock Styling

**Files:**
- Modify: `src/dock.c`
- Test: `tests/test_layout.c`

**Interfaces:**
- Modifies `dock_render()`:
  - Solid sleek deep black (`#0a0a0a` / `rgba(0.04, 0.04, 0.04, 0.98)`).
  - Subtle crisp border (`rgba(0.25, 0.25, 0.25, 0.70)`).
  - Minimalist divider and active glowing running indicator dots.
  - Eliminates multi-color gradient fills and blur artifacts.

- [ ] **Step 1: Write the test verifying solid black color palette in dock**

In `tests/test_layout.c`:
- Test dock render functions compile and handle both horizontal and vertical geometries cleanly with the updated solid aesthetic.

- [ ] **Step 2: Update `dock_render()` in `src/dock.c`**

- Set background fill strictly to solid black `cairo_set_source_rgba(cr, 0.04, 0.04, 0.04, 0.98);`.
- Set crisp border stroke `cairo_set_source_rgba(cr, 0.25, 0.25, 0.25, 0.70);`.
- Remove gradient overlay pattern and specular white lines.
- Refine running indicator dots and divider lines.

- [ ] **Step 3: Run test to verify it passes**

Run: `make test_layout && ./test_layout && make muzuwm`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add src/dock.c tests/test_layout.c
git commit -m "style: apply solid sleek deep black theme to dock"
```

---

### Task 4: Full System Verification, Xephyr Stress Test & Review

**Files:**
- Modify: `tests/test_runner.sh` (if needed)

- [ ] **Step 1: Run all 7 unit tests**

Run: `make clean && make -j4 && make tests && bash tests/test_runner.sh`
Expected: All tests PASS, RAM remains < 30 MB.

- [ ] **Step 2: Run Xephyr stress & leak test**

Run: `bash tests/test_leak_and_race.sh`
Expected: Stress test PASS with zero crashes and zero leaks.

- [ ] **Step 3: Senior Code Review & Final Commit**

Dispatch review subagent, address any findings, and finalize.
