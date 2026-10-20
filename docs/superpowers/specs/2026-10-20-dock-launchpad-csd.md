# Spec: Functional Dock, Safe Launchpad Exit, and GTK CSD Support (GNOME Files Fix)

## 1. Objective
Make the MUZUwm Dock fully interactive and capable of launching applications, refine Launchpad exit behavior so it only closes upon launching an app or explicit exit request, and eliminate double-titlebar bugs and enable headerbar dragging for GTK Client-Side Decorated (CSD) apps like GNOME Files (Nautilus).

---

## 2. Subsystems & Requirements

### 2.1 Functional Interactive Dock
1. **Pinned Applications:**
   - Web Browser (`x-www-browser` / `firefox` / `google-chrome`)
   - File Manager (`nautilus ~` / `xdg-open ~`)
   - Terminal (`x-terminal-emulator` / `gnome-terminal` / `xterm`)
   - App Launcher (`__LAUNCHPAD__` - triggers `launchpad_toggle`)
2. **Icon Presentation:**
   - Resolve themed PNG icons using `icon_load_surface` / `icon_find_themed_png` (`web-browser`, `system-file-manager`, `utilities-terminal`, `view-app-grid`).
   - Graceful fallback to procedural themed icon badge if PNG not found on disk.
3. **Hover & Visual Polish:**
   - Track hovered dock item.
   - Draw subtle rounded highlight rectangle and theme accent indicator on hover.
4. **Click Handling:**
   - Handle `ButtonPress` on `dock.win`.
   - Calculate clicked icon boundary.
   - For `__LAUNCHPAD__`: toggle Launchpad.
   - For applications: spawn process asynchronously via `fork()` + `setsid()` + `exec` (`/bin/sh -c ...`).

---

### 2.2 Launchpad Exit Behavior
1. **Accidental Dismissal Prevention:**
   - Clicks on empty background space, margins, digital clock, or calendar MUST NOT close Launchpad.
2. **Exit Triggers:**
   - Launchpad closes ONLY when:
     a) An application card is clicked (executes application and hides Launchpad).
     b) Close button ('✕') is clicked.
     c) Escape key or Super key / Alt+F1 is pressed.
3. **Close Button UI:**
   - Render a dedicated '✕' button at the top-right corner of the Launchpad screen (`screen_w - 60, 32, 32, 32`).
   - Styled with GTK theme border and surface colors.

---

### 2.3 GTK CSD & `_MOTIF_WM_HINTS` Support (GNOME Files Fix)
1. **CSD / Borderless Detection:**
   - Check `_MOTIF_WM_HINTS`: if `flags & MWM_HINTS_DECORATIONS` and `decorations == 0`, window is CSD.
   - Check `_GTK_FRAME_EXTENTS`: if present and non-zero, window is CSD.
2. **Window Management for CSD Windows:**
   - Do NOT wrap CSD windows in a titlebar frame (no double titlebar).
   - Set `c->frame = None` and `c->is_csd = 1`.
   - In tiling layout: assign full allocated tile dimensions directly to `c->win` without the 24px `TITLEBAR_HEIGHT` deduction.
3. **`_NET_WM_MOVERESIZE` Protocol:**
   - Advertise `_NET_WM_MOVERESIZE` in `_NET_SUPPORTED` on root window.
   - Handle `ClientMessage` with message type `_NET_WM_MOVERESIZE`:
     - When GTK header bar initiates a move (`direction == 8` / `_NET_WM_MOVERESIZE_MOVE`), detach window from tiling to floating and start interactive pointer drag.
