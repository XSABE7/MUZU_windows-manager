# Spec: macOS Window Controls (Close, Minimize, Maximize) and Moveable Glass Dock

## 1. Objective
Enhance MUZUwm with macOS-style window controls (Red=Close, Yellow=Minimize to Dock, Green=Maximize/Restore), implement window minimization/restoration into the Dock, upgrade the Dock to an elegant macOS frosted-glass aesthetic with separated minimized windows and active running indicators, and support repositioning the Dock across screen edges (Bottom, Left, Right, Top).

---

## 2. Subsystems & Requirements

### 2.1 macOS 3-Button Window Controls (Traffic Lights)
1. **Button Visuals & Layout:**
   - Titlebar renders three vector circular buttons (12px diameter each, 8px spacing) on the left side of the titlebar (standard macOS layout):
     - **Red (`#FF5F56`):** Close window (`client_close`).
     - **Yellow (`#FFBD2E`):** Minimize window to dock.
     - **Green (`#27C93F`):** Toggle Maximize / Fullscreen (`client_toggle_maximize`).
   - Title text shifts to center or to the right of the three control buttons (`x >= 70`).
2. **Maximize / Restore Logic:**
   - When a window is maximized:
     - Save current geometry (`saved_x, saved_y, saved_w, saved_h, saved_mode`).
     - Switch client mode to `CLIENT_FLOATING`.
     - Resize to full screen dimensions (respecting unhidden dock margin if applicable).
     - Set `c->is_maximized = 1`.
   - When toggled again:
     - Restore saved geometry and mode (`saved_x, saved_y, saved_w, saved_h, saved_mode`).
     - If mode was `CLIENT_TILED`, trigger `reflow_all()`.
     - Set `c->is_maximized = 0`.
3. **Minimize to Dock Logic:**
   - When yellow button is clicked:
     - Set `c->is_minimized = 1`.
     - Unmap client window and frame (`XUnmapWindow`).
     - Inform the Dock (`dock_add_minimized(dock, c)`).
     - If the client was tiled, reflow remaining tiled windows!

---

### 2.2 macOS Frosted-Glass Aesthetic Dock
1. **Visual Styling:**
   - Rounded pill geometry (corner radius: 16px).
   - Multi-layer frosted glass shading:
     - Translucent dark surface (`rgba(0.12, 0.14, 0.18, 0.88)` or matching GTK surface).
     - Top specular gloss line (`rgba(1.0, 1.0, 1.0, 0.18)`).
     - Thin crisp outline (`rgba(1.0, 1.0, 1.0, 0.12)`).
2. **Divided Sections:**
   - **Section 1: Pinned Applications** (Web, Files, Terminal, Apps).
   - **Section 2: Separator Divider** (semi-transparent vertical or horizontal line).
   - **Section 3: Minimized Windows** (each minimized window gets an icon slot with its title tooltip or letter badge).
3. **Active Running Indicators:**
   - If an application is currently running (matching client window exists), draw a small glowing white dot (3px) below the icon.
4. **Magnification & Hover Feedback:**
   - Hovered icon expands smoothly (from 32px to 38px) with a glowing backdrop badge.

---

### 2.3 Dock Repositioning (Moveable Dock)
1. **Edge Positioning:**
   - Supports 4 edges: `DOCK_BOTTOM`, `DOCK_LEFT`, `DOCK_RIGHT`, `DOCK_TOP`.
   - Dynamic orientation:
     - Bottom / Top: Horizontal dock bar.
     - Left / Right: Vertical dock bar.
2. **Reposition Triggers:**
   - **Right-Click Menu / Cycle:** Right-clicking (`Button3`) on the dock separator or background cycles position:
     `DOCK_BOTTOM -> DOCK_LEFT -> DOCK_TOP -> DOCK_RIGHT -> DOCK_BOTTOM`.
   - **Keyboard Shortcut / Hotkey:** Optional Super+D or dock API `dock_set_position()` dynamically calculates new geometry and re-renders.
