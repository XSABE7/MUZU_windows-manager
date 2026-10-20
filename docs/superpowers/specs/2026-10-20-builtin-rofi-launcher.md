# Built-in Rofi-Style Application Launcher Spec

## 1. Overview & Objective
Replace the heavy full-screen Launchpad (calendar, digital clock, full-screen blur grid, and easter egg) with a native, ultra-responsive, low-RAM application launcher built directly into MUZUwm ("Rofi จริงๆ ที่มาพร้อมกับ WM ไม่ต้องลงเพิ่ม").

### Core Requirements
1. **Built-in & Zero Extra Dependencies:**
   - Implemented in pure C11, Xlib, and Cairo.
   - No external packages (`rofi`, `glib`, `wayland`, etc.) required.
   - Memory footprint overhead < 3 MB (total MUZUwm RSS remains ~22 MB, well below 30 MB / 200 MB limits).
2. **Instant Open ("กดปุ๊บใช้ได้ปั๊บ"):**
   - Single press of `Super` (`XK_Super_L` or `XK_Super_R`) toggles the launcher instantly (< 1ms).
   - Clicking the three dots "Apps" icon in the Dock toggles the launcher instantly.
   - `Alt+F1` retained as keyboard fallback shortcut.
3. **Classic Rofi drun Look & Feel:**
   - Centered floating modal window (640x440px) with subtle 1px border and rounded corners ($r=12\text{px}$).
   - **Smooth Open/Close Micro-Animation:**
     - High-speed buttery-smooth slide & ease-out transition (80-100ms) on open, giving a modern premium feel without sacrificing instant responsiveness ("กดปุ๊บใช้ได้ปั๊บ").
     - Dynamic ease-out cubic interpolation ($y_{\text{start}} = y_{\text{final}} + 24\text{px} \to y_{\text{final}}$) rendered at 60fps.
   - Top search box with prompt indicator ("  " / "> ") and live text cursor.
   - Vertical list of matching applications:
     - 32x32 high-resolution icon (resolved via FreeDesktop icon theme chain: Adwaita, gnome, hicolor).
     - Application name (bold/accented).
     - Generic description / commentary in subdued color.
   - Active item highlight pill with smooth rounded background.
4. **Keyboard & Mouse Navigation:**
   - Instant typing filters applications by name and exec substring.
   - `Up` / `Down` arrows, `Ctrl+p` / `Ctrl+n`, `PageUp` / `PageDown` navigate selection.
   - `Enter` launches selected app via `fork() + setsid() + exec` and dismisses launcher.
   - `Escape` dismisses launcher.
   - Mouse hover highlights item, mouse click launches item.
   - Clicking anywhere outside the launcher window dismisses it.
5. **Clean Removal of Launchpad:**
   - Remove `src/launchpad.c`, `include/launchpad.h`, and `src/easter_egg.c`.
   - Update `Makefile`, `main.c`, `dock.c`, and test suites.

6. **Zero-Overhead Built-in Wallpaper Engine:**
   - Native X11 Root Window Pixmap setter (`XSetWindowBackgroundPixmap` + `_XROOTPMAP_ID` + `ESETROOT_PMAP_ID`).
   - Zero daemon processes, zero background CPU cycles, and zero memory overhead after setting root pixmap.
   - Reads configured wallpaper from `~/.config/waypaper/config.ini` (compatible with Waypaper GUI) or `~/.config/muzuwm/wallpaper.png`.
   - Renders image centered/scaled via Cairo image surfaces with smooth high-quality bilinear filtering.

---

## 2. Architecture & Data Structures

### `RofiApp` Structure
```c
typedef struct {
    char name[128];
    char exec[256];
    char comment[256];
    char icon_name[128];
    cairo_surface_t *icon_surface;
} RofiApp;
```

### `RofiLauncher` State Structure
```c
#define ROFI_MAX_APPS 256
#define ROFI_VISIBLE_ROWS 6
#define ROFI_WIDTH 640
#define ROFI_ROW_HEIGHT 52
#define ROFI_HEADER_HEIGHT 60

typedef struct {
    Window win;
    cairo_surface_t *surface;
    int is_visible;
    int x, y, width, height;
    
    // Application registry
    RofiApp apps[ROFI_MAX_APPS];
    int app_count;
    
    // Filtered matches
    int matches[ROFI_MAX_APPS];
    int match_count;
    int selected_idx;
    int scroll_offset;
    
    // Search input buffer
    char query[128];
    int query_len;
    
    const GtkTheme *theme;
} RofiLauncher;
```

---

## 3. Keyboard & Event Interaction
- **Activation:**
  - `Super` key press (`ev.xkey.keycode == super_kc`) -> toggles `rofi_launcher_toggle(dpy, &rofi)`.
  - Dock Item 3 ("Apps") click -> calls `rofi_launcher_toggle(dpy, &rofi)`.
- **Keyboard Grab:**
  - When visible, launcher grabs keyboard focus (`XSetInputFocus(dpy, rofi.win, RevertToPointerRoot, CurrentTime)`).
  - All keystrokes route directly to `rofi_launcher_handle_key`:
    - `Escape`: dismisses.
    - `Return`: executes `matches[selected_idx]`, dismisses.
    - `Up` / `Down`: changes `selected_idx`, adjusts `scroll_offset`.
    - `BackSpace`: removes last character in `query`, updates matches.
    - Printable ASCII: appends to `query`, updates matches.
- **Mouse Interaction:**
  - `MotionNotify` updates `selected_idx` to hovered row.
  - `ButtonPress` on row executes and dismisses.
  - Clicking outside closes.

---

## 4. Performance & Memory Budget
- Memory footprint: Pre-scans desktop entries on startup. Icon surfaces loaded lazily or on startup (< 2 MB total pixmap memory).
- Zero background polling or timers.
- RSS memory < 30 MB budget.
