# Functional Dock, Safe Launchpad Exit, and GTK CSD Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform the Dock into a fully functional, interactive app launcher with themed icons, constrain Launchpad dismissal strictly to app activation or explicit exit, and support GTK Client-Side Decoration (CSD) via `_MOTIF_WM_HINTS` and `_NET_WM_MOVERESIZE` to eliminate double titlebars on GNOME Files (Nautilus).

**Architecture:** Extend `client_props.c` to parse `_MOTIF_WM_HINTS` and detect CSD windows; bypass titlebar frame creation for CSD clients while handling `_NET_WM_MOVERESIZE` client messages; update `launchpad.c` to reject background clicks while providing an explicit close ('✕') button; upgrade `dock.c` to manage launcher items, resolve themed icon surfaces, track hover states, and launch apps via `fork()`/`exec()`.

**Tech Stack:** C11, Xlib (X11), Cairo, Freedesktop Icon Theme Resolver.

**Spec:** `docs/superpowers/specs/2026-10-20-dock-launchpad-csd.md`

## Global Constraints

- Pure C11 (`-Wall -Wextra -pedantic -std=c11 -O2`). Zero compiler warnings.
- Zero runtime dependencies on `libgtk` or `libglib` in the WM binary.
- RAM budget: Window manager RSS must remain strictly under 30 MB.
- All existing and new automated unit tests must pass cleanly.

## Review Focus

1. **CSD Window Resizing/Tiling Dimensions:** CSD windows must not have their height subtracted by `TITLEBAR_HEIGHT` (24px) during tiling reflow.
2. **`_NET_WM_MOVERESIZE` Message Sanitization:** Malformed `_NET_WM_MOVERESIZE` events (invalid coordinates or window IDs) must not crash the event loop.
3. **Empty Background Click in Launchpad:** Clicking anywhere in the background, calendar, or clock must not close the Launchpad.
4. **Dock Item Bounds & Multiple Clicks:** Clicking between dock icons or rapid clicking on dock items must not create zombie processes.
5. **Icon Surface Teardown in Dock:** Surface handles in `DockItem` must be finished and destroyed cleanly in `dock_cleanup()`.

---

### Task 1: GTK CSD Detection (`_MOTIF_WM_HINTS`) and `_NET_WM_MOVERESIZE`

**Files:**
- Modify: `include/client_props.h`
- Modify: `src/client_props.c`
- Modify: `include/wm.h`
- Modify: `src/main.c`
- Test: `tests/test_client_props.c`

**Interfaces:**
- Produces: `int client_is_csd(Display *dpy, Window win);`
- Adds to `Client`: `int is_csd;`

- [ ] **Step 1: Write the failing test for CSD detection**

In `tests/test_client_props.c`, add tests for `client_is_csd()`:
- Window without `_MOTIF_WM_HINTS` returns 0.
- Window with `_MOTIF_WM_HINTS` having `decorations = 0` returns 1.
- Window with `_MOTIF_WM_HINTS` having `decorations = 1` returns 0.
- Window with non-zero `_GTK_FRAME_EXTENTS` returns 1.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_client_props && ./test_client_props`
Expected: FAIL (symbol `client_is_csd` undefined)

- [ ] **Step 3: Implement `client_is_csd` in `src/client_props.c` and integrate into `src/main.c`**

- In `include/client_props.h` and `src/client_props.c`:
  - Implement `client_is_csd()`: read `_MOTIF_WM_HINTS` (5 `CARDINAL32` elements) and `_GTK_FRAME_EXTENTS`.
- In `include/wm.h`:
  - Add `int is_csd;` to `Client`.
- In `src/main.c`:
  - Advertise `_NET_WM_MOVERESIZE` in `_NET_SUPPORTED`.
  - In `MapRequest`: If `client_is_csd(dpy, w)` is true:
    - Set `c->is_csd = 1;` and `c->frame = None;`
    - Do NOT call `titlebar_create()`.
  - In `reflow_all`:
    - If `curr->is_csd || !curr->frame`, resize client window directly with full allocated width and height.
  - In `ClientMessage`:
    - Handle `_NET_WM_MOVERESIZE`: if `ev.xclient.message_type == net_wm_moveresize`, trigger floating mode transition and pointer grab drag for `_NET_WM_MOVERESIZE_MOVE` (8).

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_client_props && ./test_client_props`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/client_props.h src/client_props.c include/wm.h src/main.c tests/test_client_props.c
git commit -m "feat: add GTK CSD detection and _NET_WM_MOVERESIZE support"
```

---

### Task 2: Launchpad Explicit Close Button & Safe Dismissal

**Files:**
- Modify: `include/launchpad.h`
- Modify: `src/launchpad.c`
- Test: `tests/test_calendar.c`

**Interfaces:**
- Consumes: `launchpad_toggle(Display *dpy, Launchpad *lp);`
- Modifies: `launchpad_handle_click()`, `launchpad_render()`

- [ ] **Step 1: Write the failing test for safe dismissal**

In `tests/test_calendar.c`:
- Verify clicking background coordinates (e.g. `x = 20, y = 20`, or calendar cell `x = 600, y = 300`) leaves `lp.is_visible == 1`.
- Verify clicking the close button coordinates (`screen_w - 50, 48`) calls `launchpad_toggle()` and closes launchpad.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_calendar && ./test_calendar`
Expected: FAIL (currently background click closes launchpad)

- [ ] **Step 3: Implement explicit close button and safe dismissal in `src/launchpad.c`**

- In `launchpad_render()`:
  - Render an elegant circular '✕' button at `(screen_w - 60, 32)` with diameter 32px using `theme->bg_surface` and `theme->border_color`.
- In `launchpad_handle_click()`:
  - Check if click is inside the close button `(x >= screen_w - 64 && x <= screen_w - 28 && y >= 28 && y <= 64)`. If so, toggle launchpad.
  - When click does NOT match an app item or close button, do NOTHING (remove lines 481-482).

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_calendar && ./test_calendar`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/launchpad.h src/launchpad.c tests/test_calendar.c
git commit -m "feat: add Launchpad explicit close button and safe click dismissal"
```

---

### Task 3: Interactive Dock with App Launching & Themed Icons

**Files:**
- Modify: `include/dock.h`
- Modify: `src/dock.c`
- Modify: `src/main.c`
- Test: `tests/test_layout.c`

**Interfaces:**
- Consumes: `icon_load_surface()` from `icon_fetcher.h`
- Produces:
  - `void dock_handle_click(Display *dpy, Dock *dock, struct Launchpad *lp, int x, int y);`
  - `void dock_handle_hover(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h);`

- [ ] **Step 1: Write the failing test for dock click and item geometry**

In `tests/test_layout.c`:
- Test dock item bounding boxes for 4 launcher items: Web, Files, Terminal, Apps.
- Test `dock_handle_click()` item detection for given coordinates.

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_layout && ./test_layout`
Expected: FAIL

- [ ] **Step 3: Implement interactive Dock in `src/dock.c` and wire into `src/main.c`**

- In `include/dock.h`:
  - Define `DockItem` array in `Dock` struct with names, commands, icon surfaces, and `hover_item`.
- In `src/dock.c`:
  - Initialize launcher items: Web (`x-www-browser`), Files (`nautilus ~`), Terminal (`x-terminal-emulator`), Apps (`__LAUNCHPAD__`).
  - Load themed icon surfaces using `icon_load_surface()`.
  - Draw icons centered in dock slots; render hover background highlight when `hover_item == i`.
  - Implement `dock_handle_click()`: if `__LAUNCHPAD__`, call `launchpad_toggle()`, else `fork()` + `setsid()` + `exec`.
  - In `dock_cleanup()`: destroy loaded icon surfaces.
- In `src/main.c`:
  - Handle `ButtonPress` for `dock.win`: call `dock_handle_click(dpy, &dock, &launchpad, ev.xbutton.x, ev.xbutton.y)`.
  - Handle `MotionNotify` on `dock.win`: call `dock_handle_hover()`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_layout && ./test_layout`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/dock.h src/dock.c src/main.c tests/test_layout.c
git commit -m "feat: implement interactive dock with themed icons and app launcher"
```

---

### Task 4: Full Verification & Xephyr GNOME Files (Nautilus) Test

**Files:**
- Modify: `tests/test_runner.sh`

- [ ] **Step 1: Run all unit tests**

Run: `make clean && make -j4 && make tests && bash tests/test_runner.sh`
Expected: All 7 unit tests PASS, RAM < 30 MB.

- [ ] **Step 2: Run Xephyr GNOME Files (Nautilus) CSD test**

Spawn Xephyr with MUZUwm and run `nautilus --new-window`. Verify:
- Only 1 window frame exists (no double titlebar on Nautilus).
- Nautilus is responsive and renders cleanly.
- Memory stress test passes without leaks.

- [ ] **Step 3: Commit and clean up**

```bash
git add tests/test_runner.sh
git commit -m "test: verify interactive dock, launchpad exit, and Nautilus CSD"
```
