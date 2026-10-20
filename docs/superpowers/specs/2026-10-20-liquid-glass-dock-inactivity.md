# Design Spec: MUZUwm Liquid Glass, Squircle Dock & 20s Inactivity Ghost Windows

## 1. Intent and Architecture Overview

The goal of this project is to implement a modern **Liquid / Acrylic Glass** aesthetic in MUZUwm, combined with high-performance pre-rendering, an ergonomic **20-second inactivity window transparency system (Ghost Glass) with Hover-to-Wake**, and a **Squircle (rounded-rectangle plate) Dock**, all while maintaining a total RAM footprint strictly **under 200 MB** (target ~40–60 MB combined).

```
+---------------------------------------------------------------------------------+
|                                    MUZUwm                                       |
|                                                                                 |
|  +---------------------------+  +--------------------------+  +--------------+  |
|  |   Pre-rendered Titlebar   |  |   Squircle Glass Dock    |  | Inactivity   |  |
|  |   (Cached Cairo Surface)  |  | (Zero-Overhead AutoHide) |  | 20s Timer    |  |
|  +---------------------------+  +--------------------------+  +--------------+  |
|                                                                                 |
|                                  | _NET_WM_WINDOW_OPACITY (Ghost Glass)         |
|                                  v                                              |
|  +---------------------------------------------------------------------------+  |
|  |     X11 Compositor Daemon (Picom with Dual-Kawase Blur, RAM ~35 MB)        |  |
|  +---------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------+
```

---

## 2. Core Features & Specifications

### 2.1 Liquid / Acrylic Glass Aesthetic
- **Visual Styling**:
  - Deep dark frosted smoked glass look (`rgba(22, 24, 30, 0.72)`).
  - Subtle top edge refraction highlight (`rgba(255, 255, 255, 0.22)`).
  - Clean bottom shadow divider (`rgba(0, 0, 0, 0.45)`).
  - Crisp outer border (`rgba(255, 255, 255, 0.14)`).
  - Traffic lights window controls (Red, Yellow, Green) retained with polished specular glass shine.

### 2.2 Pre-rendered Titlebar Surfaces (Zero CPU Overhead)
- **Problem**: Redrawing complex vector gradients, reflections, and borders on every mouse move or expose event burns CPU cycles.
- **Solution**:
  - Each `Client` maintains a pre-rendered background cache surface `cairo_surface_t *titlebar_cache`.
  - When a window is created or resized, `titlebar_pre_render(c, width, height)` generates the static liquid glass background into the cache surface.
  - On standard redraws, `cairo_set_source_surface(cr, c->titlebar_cache, 0, 0); cairo_paint(cr);` blits the background instantly, followed only by dynamic elements (title text extents and traffic light hover states).
  - Destroys and cleans up `titlebar_cache` when the window is destroyed.

### 2.3 20-Second Inactivity Window Transparency (Ghost Glass with Hover-to-Wake)
- **Behavior & Timing**:
  - Each `Client` tracks `time_t last_unfocus_time`, `int is_ghost`, and `int is_hovered`.
  - While a window is currently focused, it remains 100% opaque (`_NET_WM_WINDOW_OPACITY = 0xFFFFFFFF`).
  - When a window **loses focus**, `c->last_unfocus_time = time(NULL)`.
  - In the event loop, an inactivity check runs periodically (1-second interval):
    - Any mapped window that is **not currently focused**, has `c->is_hovered == 0`, and has `now - c->last_unfocus_time >= 20` transitions into Ghost Glass.
    - MUZUwm sets `_NET_WM_WINDOW_OPACITY` to ~65% (`0xA6AAAAAA`).
    - `c->is_ghost = 1`.
- **Hover-to-Wake & Click Semantics**:
  - When pointer enters an unfocused ghost window (`EnterNotify`), `c->is_hovered = 1`. MUZUwm immediately wakes the window up to 100% opacity (`_NET_WM_WINDOW_OPACITY = 0xFFFFFFFF`).
  - When pointer leaves the window (`LeaveNotify`), `c->is_hovered = 0`. If the window was not clicked/focused and `now - c->last_unfocus_time >= 20`, it smoothly returns to ghost opacity (~65%).
  - When clicked, the window receives X11 input focus, resets `c->last_unfocus_time`, and stays permanently 100% opaque.

### 2.4 Squircle Dock ("กลมๆ คล้ายสี่เหลี่ยม")
- **Geometry & Rendering**:
  - Instead of an oblong pill or sharp 45° chamfer, the Dock adopts a **Squircle / Rounded Rectangle Plate** geometry.
  - Corner radius: **14px** with continuous smooth curvature (`cairo_arc` rounded rectangle).
  - Horizontal & Vertical orientation support (dimensions automatically adapt based on screen edge: Bottom/Top vs Left/Right).
  - Subtle frosted glass fill with inner refraction ring.
  - Retains all existing features: macOS magnification wave effect, drag-and-drop pinning, app launcher (`...`), running app dots, and right-click decoration menu.

### 2.5 Zero-Overhead Auto-hide Dock
- **Behavior**:
  - When `dock.autohide_enabled == 1` and `dock.is_hidden == 1`:
    - Dock sits as a 4px sliver at the screen edge to receive `EnterNotify`.
    - `dock_render` **skips redrawing entirely** (returns immediately), freezing CPU usage at **0.0%**.
  - When the pointer enters the dock edge (`EnterNotify`):
    - Dock expands / reveals immediately.
    - Redraws once to show current state.
  - When pointer leaves the dock (`LeaveNotify`):
    - After a small 300ms debounce delay, dock slides back to hidden state and freezes rendering.

### 2.6 Compositor & Picom Integration (< 200 MB RAM Budget)
- **Integration**:
  - Generate a tailored `~/.config/muzuwm/picom.conf` configured for:
    - Backend: `glx` or `egl` (fallback to `xrender` if OpenGL unavailable).
    - Blur method: `dual_kawase` (strength 5) for true frosted glass blur on semi-transparent windows and dock.
    - VSync enabled to prevent screen tearing.
    - Shadow radius 8, shadow opacity 0.5.
  - MUZUwm checks if `picom` is installed and running:
    - If not running, launches `picom --config ~/.config/muzuwm/picom.conf -b`.
    - Gracefully cleans up picom child process on WM exit.
  - Combined memory benchmark: MUZUwm (~15 MB) + Picom (~35 MB) = **~50 MB total RAM**, safely beneath the 200 MB budget ceiling.

---

## 3. Goals, Non-Goals, and Anti-Goals

### Goals
1. Render modern Liquid / Acrylic Frosted Glass on Titlebars and Dock.
2. Pre-render titlebar glass textures into reusable Cairo surfaces.
3. Automatically dim unfocused windows to 65% Ghost Glass opacity after 20 seconds of inactivity.
4. Hovering over a ghost window temporarily wakes it to 100% opacity ("Hover-to-Wake").
5. Render the Dock as a sleek Squircle (rounded rectangular plate).
6. Freeze dock rendering when auto-hidden to maintain 0% idle CPU.
7. Auto-start picom with dedicated dual-kawase blur config, keeping combined RAM < 200 MB.

### Non-Goals
1. Implementing an internal GPU compositor directly inside the WM process (using picom as external daemon achieves better modularity, zero crash propagation, and hardware acceleration at ~35 MB RAM).
2. Changing existing tiling layouts or core mouse gestures.

### Anti-Goals
1. Total memory footprint exceeding 200 MB under stress.
2. Lag or hesitation when waking an unfocused window from Ghost Glass.
3. Window flicker or redraw loop when dock is auto-hidden.

---

## 4. Architectural Calls Made
- **Ghost Opacity Value**: Set to `0xA6AAAAAA` (~65% alpha).
- **Inactivity Timer Reference**: Counted strictly from `c->last_unfocus_time`.
- **Periodic Timer**: 1-second timeout check in the main event loop (`select()` / `XPending()`).
- **Dock Auto-Hide Edge Detection**: Kept via 4px edge geometry + render bypass for 0% CPU.
