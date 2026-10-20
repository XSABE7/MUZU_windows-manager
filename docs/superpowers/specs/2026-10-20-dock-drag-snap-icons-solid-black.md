# Spec: Dock Right-Click Drag-and-Snap, Accurate App Icons, and Solid Black Styling

## 1. Objective
Enhance MUZUwm Dock with:
1. **Interactive Right-Click Drag & Snap:** Holding right-click (`Button3`) allows freely dragging the dock across the screen. Releasing snaps the dock to the nearest screen edge (`DOCK_BOTTOM`, `DOCK_LEFT`, `DOCK_TOP`, `DOCK_RIGHT`), automatically adapting horizontal vs. vertical orientation.
2. **Accurate App Icons:** Extract high-fidelity application icons for both launcher items and running/minimized windows using `_NET_WM_ICON` (EWMH ARGB32 pixmap parsing), expanded FreeDesktop icon theme lookup (`gnome`, `Adwaita`, `22x22`, `24x24`, `16x16`), and `WM_CLASS` fallback.
3. **Solid Deep Black Styling:** Replace frosted glass gradients with a minimalist, clean, solid deep black aesthetic (`#0a0a0a`), crisp subtle border, and refined running indicator dots.

---

## 2. Requirements & Architecture

### 2.1 Dock Drag & Snap to Nearest Edge
1. **State & Events:**
   - In `Dock`: add `int is_dragging;`, `int drag_start_x, drag_start_y;`.
   - On `ButtonPress` on `dock.win` with `Button3`:
     - Record offset: `dock->drag_start_x = ev.xbutton.x; dock->drag_start_y = ev.xbutton.y;`.
     - Set `dock->is_dragging = 1;`.
     - Grab pointer on root: `XGrabPointer(dpy, root, False, ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);`.
   - On `MotionNotify`:
     - If `dock->is_dragging`:
       - Calculate new window position: `x = ev.xmotion.x_root - dock->drag_start_x`, `y = ev.xmotion.y_root - dock->drag_start_y`.
       - Clamp within screen bounds.
       - Move dock window with `XMoveWindow(dpy, dock->win, x, y);`.
   - On `ButtonRelease` with `Button3`:
     - If `dock->is_dragging`:
       - Calculate dock center point: `cx = dock->x + dock->width / 2; cy = dock->y + dock->height / 2;`.
       - Compute Euclidean / orthogonal distances to all 4 edges:
         - Bottom: `screen_h - cy`
         - Top: `cy`
         - Left: `cx`
         - Right: `screen_w - cx`
       - Choose minimum distance as `nearest_edge`.
       - Call `dock_set_position(dpy, dock, nearest_edge, screen_w, screen_h);`.
       - Reset `dock->is_dragging = 0;`.
       - Ungrab pointer with `XUngrabPointer(dpy, CurrentTime);`.

### 2.2 Accurate App Icons
1. **`_NET_WM_ICON` Parsing:**
   - Modern X11 client windows (Firefox, Chrome, VS Code, Gedit, etc.) provide `_NET_WM_ICON`, which is a list of icons in 32-bit format: `[width, height, ARGB data...]`.
   - Implement `icon_create_from_net_wm_icon(const unsigned long *data, unsigned long nitems, int target_size)` in `icon_fetcher.c`:
     - Scan through icons in the array, find the icon closest to `target_size` (e.g. 32px or 48px).
     - Convert ARGB unsigned long pixels into a Cairo `CAIRO_FORMAT_ARGB32` image surface.
2. **`WM_CLASS` & Enhanced Theme Chain:**
   - In `icon_fetcher.c`:
     - Add `gnome`, `Adwaita` to the fallback theme list in `icon_theme_set()`.
     - Add `22x22/apps`, `24x24/apps`, `16x16/apps`, `22/apps`, `24/apps`, `16/apps` to `app_subdirs[]`.
   - In `client_props.c`:
     - Implement `client_get_icon(Display *dpy, Window win, const char *title, int target_size)`:
       - 1) Check `_NET_WM_ICON`.
       - 2) Check `WM_CLASS` (`ch.res_class`, `ch.res_name` converted to lowercase).
       - 3) Common app title keyword mapping.
       - 4) Return Cairo surface or NULL.
3. **Minimized Window Integration:**
   - When a client is created or mapped, retrieve and store its icon surface in `Client *c->icon_surface`.
   - Clean up `c->icon_surface` in `client_teardown()` / `titlebar_cleanup()`.
   - When added to minimized list (`dock_add_minimized`), use client's icon surface or load it directly.

### 2.3 Solid Deep Black Styling
1. **Visual Styling:**
   - Background: Solid deep black `rgba(0.04, 0.04, 0.04, 0.98)` (`#0a0a0a`).
   - Pill geometry with radius `r = 14px`.
   - Clean border: `rgba(0.25, 0.25, 0.25, 0.70)` with line width 1.0.
   - Separator divider: Crisp line `rgba(1.0, 1.0, 1.0, 0.20)`.
   - Running indicator dot: Minimalist glowing dot `rgba(1.0, 1.0, 1.0, 0.90)` (r = 2px).
   - Zero gradients or blurry noise for a sleek, clean, modern aesthetic.

---

## 3. Constraints
- Pure C11 (`-Wall -Wextra -pedantic -std=c11 -O2`). Zero warnings.
- Memory: Window manager RSS < 30 MB.
- All unit tests and stress tests must pass cleanly.
