# Dock Magnification, Drag-and-Drop Pinning, and Context Menus Spec

## 1. Overview & Goals
Enhance MUZUwm with modern, sleek desktop interaction:
1. **Clean Minimal Dock:**
   - Default dock contains only 1 item: the three dots ("Apps" button).
   - All legacy pre-filled launchers (Web, Files, Terminal) removed from default state.
2. **Drag & Drop Pinning from Rofi to Dock:**
   - In Rofi, dragging an application item with the mouse and releasing it over the Dock pins it to the Dock.
   - Pinned apps persist in `~/.config/muzuwm/dock_apps.ini` across restarts.
3. **macOS-Style Dock Magnification:**
   - When cursor hovers over dock icons, the active icon scales up smoothly (e.g. 1.4x scale) and adjacent neighbor icons scale proportionally with smooth Gaussian/distance falloff.
   - Can be toggled on/off in the Dock Context Menu.
4. **Dock Context Menu (Right-Click on Dock):**
   - Clicking `Button3` on the Dock opens a sleek popup menu:
     - "Magnification: [On / Off]"
     - "Position: [Bottom / Left / Top / Right]"
     - "Size: [Small / Medium / Large]"
     - "Auto-Hide: [On / Off]"
     - "Move Dock" (activates drag mode)
     - If right-clicked on a pinned app: "Remove from Dock"
5. **Desktop Context Menu (Right-Click on Empty Space / Root Window):**
   - Clicking `Button3` on the desktop root window opens a desktop context menu:
     - "Change Wallpaper..." (runs Waypaper GUI or picks from `~/Pictures`)
     - "Open Terminal"
     - "Applications" (opens Rofi)

---

## 2. Technical Architecture & Data Structures

### Dock Magnification Formula
When mouse is at coordinate $x_{\text{mouse}}$ (or $y_{\text{mouse}}$ in vertical mode):
For each icon $i$ centered at $c_i$:
- Distance $d = |x_{\text{mouse}} - c_i|$
- Influence radius $R \approx 72\text{px}$
- Scale factor:
  $$s_i = 1.0 + (s_{\max} - 1.0) \cdot \cos^2\left(\frac{\pi d}{2 R}\right) \quad \text{for } d < R, \text{ else } 1.0$$
  where $s_{\max} = 1.45$.

### Context Menu Data Structure
A reusable, lightweight popup menu component (`include/menu.h`, `src/menu.c`):
```c
typedef struct MenuItem {
    char label[64];
    int action_id;
    int is_separator;
} MenuItem;

typedef struct Menu {
    Window win;
    cairo_surface_t *surface;
    int is_visible;
    int x, y, width, height;
    MenuItem items[16];
    int item_count;
    int hover_idx;
    void (*on_select)(int action_id, void *userdata);
    void *userdata;
} Menu;
```

---

## 3. Interaction Details
- **Rofi Drag-to-Dock:**
  - In Rofi, `Button1` press on an item with cursor movement > 8px initiates drag state (`rofi.dragged_app`).
  - Pointer grab tracks motion.
  - When dropped on `dock.win`: adds app to `dock.items`, saves to disk, renders dock.
- **Dock Right-Click Menu:**
  - Closes any open menus.
  - Opens `Menu` at cursor coordinates with dock-specific actions.
- **Root Right-Click Menu:**
  - On root `ButtonPress` with `button == Button3`: checks if not on window or dock.
  - Opens desktop `Menu` with wallpaper & quick action items.

---

## 4. Performance & Constraints
- Pure C11, Xlib, Cairo.
- Zero extra dependencies.
- RSS memory remains under 15 MB (< 30 MB budget).
