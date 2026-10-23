# MUZUwm (Mouse UserZ Window Manager)

### Modern, Lightweight, Mouse-First Hybrid Floating & Tiling X11 Window Manager

> Intuitive, fluid one-handed desktop ergonomics for the user — engineered with pure low-level C11, Xlib, Xcursor, and Cairo graphics under the hood.

---

## Table of Contents

- [Overview & Philosophy](#overview--philosophy)
- [Implemented Systems & New Features](#implemented-systems--new-features)
  - [1. Floating-by-Default Window Management, Allowlists & Custom Shortcuts](#1-floating-by-default-window-management-allowlists--custom-shortcuts)
  - [2. Liquid Glass Tint2-Compatible Top Panel & 4-Workspace Switcher](#2-liquid-glass-tint2-compatible-top-panel--4-workspace-switcher)
  - [3. Native Animated Rofi-Style Application Launcher](#3-native-animated-rofi-style-application-launcher)
  - [4. Moveable & Magnified Frosted-Glass Dock](#4-moveable--magnified-frosted-glass-dock)
  - [5. macOS-Style Traffic Light Controls & Smart Edge Snapping](#5-macos-style-traffic-light-controls--smart-edge-snapping)
  - [6. Picom Compositor & 32-Bit ARGB Frosted Glass](#6-picom-compositor--32-bit-argb-frosted-glass)
  - [7. Automatic GTK Theme, Icon Theme & Cursor Synchronization](#7-automatic-gtk-theme-icon-theme--cursor-synchronization)
  - [8. Zero-Overhead Native Wallpaper Engine & Context Menus](#8-zero-overhead-native-wallpaper-engine--context-menus)
  - [9. Application-Grouped Alt-Tab HUD Switcher with Window Dots](#9-application-grouped-alt-tab-hud-switcher-with-window-dots)
  - [10. Mission Viewer / Mission Control Fullscreen Overview Overlay](#10-mission-viewer--mission-control-fullscreen-overview-overlay)
  - [11. Dock Context Menu Separation (App Actions vs Dock Configuration)](#11-dock-context-menu-separation-app-actions-vs-dock-configuration)
  - [12. Unified GUI & CLI Control Centre (muzuwm-settings)](#12-unified-gui--cli-control-centre-muzuwm-settings)
- [Desktop Controls & Shortcuts Cheat Sheet](#desktop-controls--shortcuts-cheat-sheet)
- [Configuration & Allowlist Files](#configuration--allowlist-files)
- [Recommended Applications & Integrations](#recommended-applications--integrations)
- [Known Bugs & Limitations](#known-bugs--limitations)
- [System Architecture & Codebase Map](#system-architecture--codebase-map)
- [Prerequisites & Build Instructions](#prerequisites--build-instructions)
- [Running & Testing](#running--testing)
- [Test Suite & Quality Assurance](#test-suite--quality-assurance)
- [Roadmap](#roadmap)
- [License](#license)
- [Suppoet and community](#Suppoet-and-community)

---

## Overview & Philosophy

Traditional tiling window managers (`i3`, `bspwm`, `dwm`) deliver speed and keyboard efficiency but demand memorizing dozens of multi-key combinations and writing complex dotfiles. Conversely, full-featured desktop environments (`GNOME`, `KDE`) offer mouse navigation at the cost of heavy RAM consumption and background services.

**MUZUwm** closes this gap with an **out-of-the-box, mouse-first hybrid floating and tiling window manager**:

1. **Hovering Floating by Default, Instant Tiling on Demand:** Applications open hovering at their natural size on the desktop (or at their multi-window docked positions for apps like Audacious). Snap any resizable window to 50/50 split or fullscreen using `Super + Arrow` keys or by dragging to a screen edge.
2. **True Out-of-the-Box (OOTB) Usability:** Integrated with a Liquid Glass top panel, 4 workspaces, vector macOS-style titlebars, a magnified frosted-glass dock with Trash and App Grid buttons, a 60fps native application launcher, a root wallpaper engine, context menus, an Alt-Tab HUD switcher with window dots, Mission Control overview, and an all-in-one graphical Control Centre.
3. **Ultra Lightweight on X11:** Built with pure **C11**, **Xlib**, **Xcursor**, and **Cairo Graphics**, maintaining an RSS footprint around **~25 MB** (strictly verified under a 200 MB ceiling).

---

## Implemented Systems & New Features

```
+---------------------------------------------------------------------------------+
|                                    MUZUwm                                       |
|                                                                                 |
|  +---------------------------+  +--------------------------+  +--------------+  |
|  |  Liquid Glass Top Panel   |  | Frosted-Glass Glass Dock |  | Context Menu |  |
|  | (4 Workspaces, Clock, Pwr)|  | (Bottom/Left/Right, DnD) |  | (Dock/Power) |  |
|  +---------------------------+  +--------------------------+  +--------------+  |
|                                                                                 |
|  +---------------------------+  +--------------------------+  +--------------+  |
|  | Native Animated Launcher  |  | Alt-Tab Switcher & HUD   |  |   Mission    |  |
|  | (Rofi drun, Cairo 60fps)  |  | (App Grouping, Dots)     |  | Control/View |  |
|  +---------------------------+  +--------------------------+  +--------------+  |
|                                                                                 |
|  +---------------------------+  +--------------------------+  +--------------+  |
|  | Window Rules & Shortcuts  |  | Settings Control Centre  |  |  GTK, Icons  |  |
|  | (Allowlists, Super+Arrow) |  | (GTK3 GUI & CLI Menu)    |  | & Xcursor    |  |
|  +---------------------------+  +--------------------------+  +--------------+  |
|                                                                                 |
|  +---------------------------------------------------------------------------+  |
|  |        Core Window Management (X11, EWMH, ICCCM, 32-Bit ARGB, Picom)      |  |
|  +---------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------+
```

### 1. Floating-by-Default Window Management, Allowlists & Custom Shortcuts

*Source:* `src/config_rules.c`, `src/client_props.c` | *Headers:* `include/config_rules.h`, `include/client_props.h`

- **Floating by Default:** New windows open floating centered on the desktop workarea at their natural dimensions instead of stretching to fullscreen. Multi-window applications that specify explicit coordinates before mapping (such as **Audacious** in Winamp skin mode with docked Main, Equalizer, and Playlist windows) have their exact docked positions preserved.
- **`fullscreen_allowlist` (`~/.config/muzuwm/fullscreen_allowlist`):** Only applications explicitly listed by `WM_CLASS` (`res_name` or `res_class`) or window title automatically maximize when opened.
- **`auto_allowlist` (`~/.config/muzuwm/auto_allowlist`):** Launches user-defined startup commands asynchronously on WM startup (no hardcoded `xterm` autostart).
- **Fullscreen Capability Detection (`can_fullscreen`):** Inspects ICCCM `WM_NORMAL_HINTS` (`PMinSize`, `PMaxSize`) and dialog/transient state. Fixed-size windows (`min_width == max_width && min_height == max_height`), small-max windows (`< 800x600`), and dialogs set `can_fullscreen = 0` and safely ignore `Super + Arrow` tiling, double-click titlebar tiling, green maximize button clicks, and mouse edge-snapping.
- **Custom Shortcuts (`~/.config/muzuwm/shortcuts.conf`):** Configurable keybindings with lock-key (`NumLock` / `CapsLock`) immunity and support for `none` to unbind any default shortcut.

### 2. Liquid Glass Tint2-Compatible Top Panel & 4-Workspace Switcher

*Source:* `src/tint2_panel.c` | *Header:* `include/tint2_panel.h`

- **4 Interactive Workspaces (`1`–`4`):** Left-click a workspace pill or scroll the mouse wheel on the panel to switch workspaces; right-click a workspace pill (or press `Alt/Super + Shift + 1..4`) to send the focused window to that workspace.
- **Focused Window Title & Real-Time Clock:** Displays the active window title in the center and a live clock with seconds (`HH:MM:SS` + date) synced every second. No desktop branding is shown when nothing is focused.
- **Keyboard Layout Menu:** The abbreviated current layout (for example `EN` or `TH`) appears at the top-right. Click it to choose an installed XKB layout; `Super + Space` cycles through the available layouts.
- **System Power Menu:** Click the power icon on the top-right to open a sleek menu for `Suspend`, `Restart / Reboot`, `Power Off`, `Lock Screen`, and `Log Out`. Suspend, reboot, power off, and log out require explicit confirmation dialog (`src/confirm_dialog.c`); `Esc` or **Cancel** safely dismisses the prompt.
- **Dual-Path `tint2rc` Compatibility:** Reads custom panel height, position, and clock format from `~/.config/muzuwm/tint2rc` or `~/.config/i3/tint2/tint2rc`.

### 3. Native Animated Rofi-Style Application Launcher

*Source:* `src/rofi_launcher.c` | *Header:* `include/rofi_launcher.h`

- **Tap-to-Open `Super` Key:** Tapping and releasing `Super` alone toggles the launcher without interfering with `Super + Arrow` or `Super + Return` combinations. Also accessible via `Alt + F1` or the Dock's 3-dots Apps icon.
- **60fps Micro-Animation & Mouse Wheel Scrolling:** Smoothly animates into view, supports live fuzzy search, keyboard navigation, mouse hover/click, mouse wheel scrolling, and drag-to-pin onto the Dock.
- **Dual-Path `config.rasi` Parsing:** Reads font and prompt customizations from `~/.config/muzuwm/rofi/config.rasi` or `~/.config/i3/rofi/config.rasi`.

### 4. Moveable & Magnified Frosted-Glass Dock

*Source:* `src/dock.c` | *Header:* `include/dock.h`

- **macOS-Style Magnification Wave:** Smooth cosine-falloff icon magnification on hover with floating app-name tooltips.
- **Bottom, Left & Right Edge Positioning:** Position the dock at `Bottom`, `Left`, or `Right` via the Dock Context Menu (top edge is disabled so the dock never overlaps the top panel or window titlebars).
- **Drag-to-Reorder & Drag-to-Pin:** Reorder pinned icons with left-click drag, or drag apps from the launcher onto the dock (persisted in `~/.config/muzuwm/dock_apps.ini` and `~/.config/muzuwm/dock_settings.ini`).
- **Built-in Trash Bin & Apps Grid Icon:** Includes a vector Trash Bin icon (with `Empty Trash` action connected to `~/.local/share/Trash/files`) and a 3-dots Apps Launcher button configurable to either corner.

### 5. macOS-Style Traffic Light Controls & Smart Edge Snapping

*Source:* `src/titlebar.c`, `src/wm.c` | *Headers:* `include/titlebar.h`, `include/wm.h`

- **32-Bit ARGB Frosted Titlebar (88% Opacity):**
  - 🔴 **Red (`#FF5F56`):** Closes the window via `WM_DELETE_WINDOW` (or `XKillClient` fallback).
  - 🟡 **Yellow (`#FFBD2E`):** Minimizes the window into the Dock.
  - 🟢 **Green (`#27C93F`):** Toggles fullscreen/maximize (guarded by `can_fullscreen`).
- **Keyboard & Mouse Edge Snapping:** Tile windows left/right (`50/50`), maximize (`fullscreen`), or restore to centered floating (`float`) using `Super + Arrow` or mouse drag-to-edge.
- **CSD Auto-Detection:** Detects `_MOTIF_WM_HINTS` and `_GTK_FRAME_EXTENTS` so CSD windows (e.g. Audacious skins, GTK4 apps) are not double-decorated.
- **Unfocused Window Ghost Opacity:** Unfocused windows automatically fade to 85% opacity (`_NET_WM_WINDOW_OPACITY`) after 30 seconds of inactivity and restore to 100% immediately on focus.

### 6. Picom Compositor & 32-Bit ARGB Frosted Glass

*Source:* `src/compositor.c` | *Header:* `include/compositor.h`

- Automatically launches `picom` (searching both `$PATH` and `~/.local/bin/picom`) using `~/.config/muzuwm/picom.conf` or `~/.config/i3/picom.conf`.
- Automatically detects nested `Xephyr` displays and switches to `--backend xrender --no-vsync` for glitch-free rendering in test environments.

### 7. Automatic GTK Theme, Icon Theme & Cursor Synchronization

*Source:* `src/gtk_theme.c`, `src/icon_fetcher.c` | *Headers:* `include/gtk_theme.h`, `include/icon_fetcher.h`

- Parses `~/.gtkrc-2.0`, `~/.config/gtk-3.0/settings.ini`, and `~/.config/gtk-4.0/settings.ini` for `gtk-theme-name`, `gtk-icon-theme-name`, `gtk-cursor-theme-name`, `gtk-cursor-theme-size`, and `gtk-font-name`.
- Exports `GTK_THEME`, `XCURSOR_THEME`, and `XCURSOR_SIZE` to child applications.
- Uses `libXcursor` (`gtk_theme_apply_cursor`) to apply the GTK `left_ptr` arrow cursor directly onto the X11 root window, replacing the legacy X11 `X` cross cursor.

### 8. Zero-Overhead Native Wallpaper Engine & Context Menus

*Source:* `src/wallpaper.c`, `src/menu.c` | *Headers:* `include/wallpaper.h`, `include/menu.h`

- Renders PNG, JPG, and BMP wallpapers directly to the X11 root pixmap (`_XROOTPMAP_ID` / `ESETROOT_PMAP_ID`) and frees client buffers immediately for zero ongoing RAM overhead.
- Supports `--wallpaper <path>`, `~/.config/muzuwm/wallpaper.png`, `~/.config/i3/wallpaper.png`, and Waypaper (`~/.config/waypaper/config.ini`), plus live reload via `_MUZU_RELOAD_WALLPAPER`.

### 9. Application-Grouped Alt-Tab HUD Switcher with Window Dots

*Source:* `src/alttab.c` | *Header:* `include/alttab.h`

- **Intelligent Application Grouping:** Groups all open windows by their application class (`WM_CLASS` / `res_class`) instead of cluttering the switcher with repetitive windows.
- **Frosted-Glass Centered HUD:** Displays a macOS/iPadOS-inspired rounded HUD with frosted glass backdrop, crisp application icons, and active application titles.
- **Window Count Indicator Dots:** Beneath each application icon, circular dot indicators dynamically represent the number of open windows for that app.
- **Intuitive Keyboard Flow:** Press `Alt + Tab` to cycle forward, `Alt + Shift + Tab` (or arrow keys) to cycle backward. Releasing `Alt` immediately raises, unminimizes, and focuses the selected application's active window; `Escape` safely cancels the switch.

### 10. Mission Viewer / Mission Control Fullscreen Overview Overlay

*Source:* `src/mission_viewer.c` | *Header:* `include/mission_viewer.h`

- **Instant Exposé Overview:** Triggered by `Super + Tab` or `Super + w` to display an enlarged, visual overview of all managed windows on the desktop.
- **Dynamic Card Grid:** Intelligently calculates an optimal thumbnail grid displaying application icons, window titles, and preview canvas cards with active hover/focus highlights over a dimmed backdrop.
- **Interactive Navigation:** Select windows via mouse hover and click, or navigate seamlessly with arrow keys, `Tab`, `Shift + Tab`, and `Enter`.
- **Quick Dismiss:** Click anywhere on the empty background or press `Escape` to close the overview and return to the current active workspace.

### 11. Dock Context Menu Separation (App Actions vs Dock Configuration)

*Source:* `src/menu.c`, `src/dock.c`

- **Context-Aware Right-Click Actions:**
  - **Right-clicking on an App Icon:** Strictly displays application-specific actions: *Launch New Instance*, *Pin / Unpin to Dock*, *Close Window*, and *Kill Process*.
  - **Right-clicking on Empty Dock Space / Separator:** Strictly displays Dock preferences: *Position: Bottom / Left / Right*, *Icon Size*, *Auto-Hide*, *Magnification Wave*, and *Apps Button Corner (Left/Right)*.
  - **Right-clicking on Desktop Background:** Opens the master desktop menu for quick access to *Settings…*, *Terminal*, *Launcher*, *Reload Wallpaper*, and *Power Session*.

### 12. Unified GUI & CLI Control Centre (muzuwm-settings)

*Source:* `tools/muzuwm-settings`

- **All-in-One Settings Hub:** An out-of-the-box Control Centre available via right-click desktop menu (*Settings…*), app launcher, or terminal command `muzuwm-settings`. Operates in a modern GTK 3 sidebar window or interactive terminal menu (`muzuwm-settings --cli`).
- **Comprehensive Configuration Tabs:**
  1. **Keyboard & Language:** Switch between popular XKB layouts (`us,th`, `us,th(pat)`, `us`, `us,jp`, or custom), configure hotkeys (`Super+Space`, `Alt+Shift`, `Caps Lock`, `Tilde ~`), and test typing live in-window.
  2. **Dock Settings & Pinned Apps:** Configure edge position (Bottom/Left/Right), icon size (36–96px), auto-hide, magnification wave, apps button corner, add/remove/reorder pinned applications, and empty the Trash bin.
  3. **Titlebar & Window Controls:** Adjust titlebar height (24, 28, 32px), typography, title alignment, double-click behavior, and inactivity dimming timeout & opacity.
  4. **Fullscreen Allowlist:** Easily manage applications opening maximized automatically with 1-click presets for Chrome, Firefox, VLC, MPV, VS Code, LibreOffice, GIMP, and Steam.
  5. **Autostart Commands:** Manage startup daemons (`picom`, `dunst`, `nm-applet`, `volumeicon`, `redshift`) with toggle switches.
  6. **Top Panel & Shortcuts:** Customize panel geometry, clock format (`%H:%M:%S`), and configure keybindings for terminal, browser, and file manager.
  7. **Appearance & Wallpaper:** Choose and apply desktop wallpapers immediately (`_MUZU_RELOAD_WALLPAPER`) and synchronize GTK/icon themes.
  8. **System, Backup & Reset:** Create timestamped configuration archives, restore backups, factory reset defaults, and trigger live reloads (`_MUZU_RELOAD_SETTINGS`).

---

## Desktop Controls & Shortcuts Cheat Sheet

### Default Keyboard Shortcuts (`~/.config/muzuwm/shortcuts.conf`)

| Shortcut | Default Action | Description |
| :--- | :--- | :--- |
| `Super` (Tap & Release) | Toggle Launcher | Opens/closes the built-in animated Rofi application launcher. |
| `Alt + Tab` | Next Application | Cycles forward through open applications in the Alt-Tab HUD switcher. |
| `Alt + Shift + Tab` | Previous Application | Cycles backward through open applications in the Alt-Tab HUD switcher. |
| `Super + Tab` / `Super + w` | Mission Viewer | Opens Mission Control fullscreen window overview. |
| `Super + Left` | `tile_left` | Snaps the focused window to the left half of the screen (ignored if fixed-size). |
| `Super + Right` | `tile_right` | Snaps the focused window to the right half of the screen (ignored if fixed-size). |
| `Super + Up` | `fullscreen` | Toggles maximize/fullscreen on the focused window (ignored if fixed-size). |
| `Super + Down` | `float` | Restores a tiled or maximized window back to centered floating mode. |
| `Super + Return` | `exec wezterm` | Launches terminal emulator (customizable in `shortcuts.conf`). |
| `Super + b` | `exec google-chrome` | Launches web browser (customizable or set `= none` to unbind). |
| `Super + e` | `exec thunar` | Launches file manager. |
| `Super + q` | `close_window` | Gracefully closes the focused window. |
| `Super + Space` | Cycle keyboard layout | Switches to the next configured XKB layout (e.g. `EN` $\leftrightarrow$ `TH`). |
| `Super + 1..4` / `Alt + 1..4` | Switch Workspace | Switches active workspace (`1` to `4`). |
| `Super + Shift + 1..4` | Move to Workspace | Moves the focused window to workspace `1` to `4`. |
| `Alt + F1` | Toggle Launcher | Fallback hotkey for the application launcher. |

### Mouse Interactions

| Action | Target | Description |
| :--- | :--- | :--- |
| **Click & Drag** | Window Titlebar | Moves floating window (detaches tiled/maximized windows automatically). |
| **Drag to Left / Right Edge** | Screen Edges | Snaps resizable window to 50/50 split. |
| **Drag to Top Edge** | Top Screen Edge | Maximizes resizable window. |
| **Double-Click** | Window Titlebar | Toggles tiled/floating mode on resizable windows. |
| **Red / Yellow / Green Buttons** | Titlebar Top-Left | Close / Minimize to Dock / Toggle Maximize. |
| **Left-Click & Drag** | Dock Icons | Reorders pinned applications on the Dock. |
| **Right-Click on App Icon** | Dock App Icon | Opens app actions (`Launch New Instance`, `Pin/Unpin`, `Close`, `Kill`). |
| **Right-Click on Dock Space** | Dock Blank Space | Opens Dock settings (`Position: Bottom/Left/Right`, `Magnification`, `Auto-Hide`, `Corner`). |
| **Right-Click → Settings…** | Empty Desktop | Opens the comprehensive MUZUwm Control Centre. |
| **Mouse Wheel Up / Down** | Top Panel / Launcher | Cycles workspaces on Top Panel, or scrolls app list in Launcher. |

---

## Configuration & Allowlist Files

All configuration files live in `~/.config/muzuwm/` and are automatically created with documented defaults on first launch:

| Path | Purpose |
| :--- | :--- |
| `~/.config/muzuwm/settings.conf` | Core window manager & desktop preferences (keyboard layouts, master count, titlebars). |
| `~/.config/muzuwm/shortcuts.conf` | Custom keyboard shortcuts (`Combo = tile_left \| tile_right \| fullscreen \| float \| close_window \| launcher \| exec <cmd> \| none`). |
| `~/.config/muzuwm/fullscreen_allowlist` | One `WM_CLASS` or window title per line. Only matching resizable windows auto-maximize on launch. |
| `~/.config/muzuwm/auto_allowlist` | One shell command per line to auto-start when MUZUwm launches. |
| `~/.config/muzuwm/dock_apps.ini` | Pinned Dock applications (`Name\|Exec\|Icon`). |
| `~/.config/muzuwm/dock_settings.ini` | Dock preferences (`autohide`, `magnification`, `position`, `size`, `apps_corner`). |
| `~/.config/muzuwm/wallpaper.png` | Default wallpaper image path (also compatible with `~/.config/waypaper/config.ini`). |
| `~/.config/muzuwm/picom.conf` | Optional custom Picom compositor config. |
| `~/.config/muzuwm/tint2rc` | Optional custom top panel config. |

---

## Recommended Applications & Integrations

MUZUwm focuses strictly on window management, desktop ergonomics, and lightweight performance (~25 MB RSS). To build a complete desktop workflow that complements MUZUwm's macOS-style Server-Side Decorations (SSD) and GTK theme synchronization, the following curated applications are recommended:

| Category | Recommended Tool | Command / Package | Description & Integration |
| :--- | :--- | :--- | :--- |
| **Terminal** | **WezTerm** | `wezterm` | GPU-accelerated terminal; default mapped to `Super + Return`. |
| **File Manager** | **Thunar** | `thunar gvfs-backends` | Full SSD compatibility, syncs GTK themes, and links directly with the Dock's Trash bin. |
| **Screenshots** | **Flameshot** | `flameshot` | Rich interactive GUI annotation tool. Bind `Print = exec flameshot gui`. |
| | **maim + slop** | `maim slop xclip` | Minimalist CLI utility for instantaneous area clipping directly to clipboard. |
| **Audio Applet** | **volumeicon** | `volumeicon-alsa` | Scroll-wheel volume adjustment in the top panel; place in `auto_allowlist`. |
| **Audio GUI** | **pavucontrol** | `pavucontrol` | Per-application audio stream routing and PulseAudio/PipeWire mixer GUI. |
| **Notifications** | **Dunst** | `dunst` | Low-latency notification daemon matching frosted-glass aesthetics. |
| **Networking** | **nm-applet** | `network-manager-gnome` | Standard tray applet for Wi-Fi and Ethernet management. |
| **Bluetooth** | **Blueman** | `blueman` | GTK applet for peripheral pairing and audio profiles (`blueman-applet &`). |
| **Media Player** | **mpv** | `mpv` | High-performance, clean video player with hardware acceleration. |
| **Audio Player** | **Audacious** | `audacious` | Classic Winamp-skin mode; docked geometry preserved natively by MUZUwm. |
| **Image Viewer** | **Viewnior** | `viewnior` | Fast GTK image viewer respecting floating-by-default window sizes. |
| **Document Reader**| **Zathura** | `zathura zathura-pdf-mupdf` | Minimalist document viewer with synctex and distraction-free viewing. |
| **Clipboard** | **greenclip** | `greenclip` | Clipboard daemon integrating seamlessly with launcher fuzzy search. |
| **Night Light** | **Redshift** | `redshift redshift-gtk` | Automatic sunrise/sunset color temperature adjustment (`redshift-gtk &`). |
| **Screen Lock** | **betterlockscreen**| `betterlockscreen` | Frosted-glass lockscreen matching the desktop wallpaper (`Super + l`). |

---

## Known Bugs & Limitations

1. **GTK4 / Libadwaita Client-Side Headerbars (e.g., GNOME Files / `nautilus`):**
   - Applications built with GTK4/libadwaita enforce their own client-side headerbars and dark/light styling independently of standard X11 SSD frames and `GTK_THEME`. Using GTK3 SSD file managers like **Thunar**, **PCManFM**, or **Nemo** is recommended.
2. **Nested `Xephyr` Server GLX VSync Error Message:**
   - When testing inside `Xephyr`, the Xephyr server lacks hardware GLX VSync support (`glx_init ERROR: Failed to enable vsync`). MUZUwm automatically detects `Xephyr` and passes `--backend xrender --no-vsync` to `picom`.
3. **Titlebar Green Maximize Button Color on Fixed-Size Windows:**
   - Fixed-size windows and dialogs (`can_fullscreen == 0`) still render the green traffic-light circle in full color, though clicking it, double-clicking the titlebar, or pressing `Super + Arrow` is safely ignored.
4. **Dynamic `WM_NORMAL_HINTS` Updates After Mapping:**
   - Fullscreen/resize support (`can_fullscreen`) is evaluated when a window maps (`MapRequest`). Applications that dynamically lock their min/max size hints after already mapping retain their initial resize capability until remapped.
5. **Single-Monitor Layout Geometry:**
   - Workspace tiling and panel/dock placement currently target the primary X11 screen (`DefaultScreen`); multi-monitor RandR per-output layouts are planned for a future release.

---

## System Architecture & Codebase Map

```
MUZUwm/
├── include/
│   ├── alttab.h          # Application-grouped Alt-Tab switcher HUD with window count dots
│   ├── client_props.h    # EWMH/ICCCM property parsers, CSD & fullscreen support detection
│   ├── compositor.h      # Picom compositor lifecycle & Xephyr backend detection
│   ├── config_rules.h    # fullscreen_allowlist, auto_allowlist & shortcuts.conf engine
│   ├── confirm_dialog.h  # Frosted-glass modal confirmation dialog for power actions
│   ├── dock.h            # Frosted-glass dock, magnification, drag reorder & Trash/Apps items
│   ├── gtk_theme.h       # GTK theme, icon theme & Xcursor configuration parser
│   ├── icon_fetcher.h    # FreeDesktop icon lookup & _NET_WM_ICON ARGB parser
│   ├── kbd_layout.h      # Multi-language keyboard layout switching (XKB) engine
│   ├── menu.h            # Reusable Cairo context popup menu component
│   ├── mission_viewer.h  # Mission Control / Exposé fullscreen overview overlay
│   ├── muzu.h            # Global macro definitions and constants
│   ├── rofi_launcher.h   # Built-in animated application launcher engine
│   ├── tint2_panel.h     # Liquid Glass top panel, 4 workspaces, clock & power menu
│   ├── titlebar.h        # 32-bit ARGB Cairo titlebar & macOS traffic light controls
│   ├── trigger.h         # Double-tap timer & hotkey event triggers
│   ├── wallpaper.h       # Zero-overhead X11 root window pixmap wallpaper engine
│   └── wm.h              # Client state, tiling reflow & snap layout engine
├── src/
│   ├── alttab.c          # Alt-Tab HUD switcher rendering, window grouping & keyboard cycling
│   ├── client_props.c    # Client properties, initial geometry resolution & size hints
│   ├── compositor.c      # Compositor launcher & config resolution
│   ├── config_rules.c    # Allowlists & custom keyboard shortcut parser/matcher
│   ├── confirm_dialog.c  # Power menu confirmation modal dialog
│   ├── dock.c            # Dock rendering, magnification falloff & item persistence
│   ├── gtk_theme.c       # GTK settings parser, env export & libXcursor root cursor setter
│   ├── icon_fetcher.c    # Icon theme directory traverser and GdkPixbuf loader
│   ├── kbd_layout.c      # XKB layout parsing, switching and cycling
│   ├── main.c            # Primary X11 event dispatch loop, EWMH & signal management
│   ├── menu.c            # Cairo context menu drawing and selection events
│   ├── mission_viewer.c  # Fullscreen Mission Control window cards and thumbnail previews
│   ├── rofi_launcher.c   # Desktop app scanner, fuzzy search & 60fps animation
│   ├── tint2_panel.c     # Top panel rendering, tint2rc parser & workspace hit-testing
│   ├── titlebar.c        # Titlebar Cairo rendering, maximize guard & drag handling
│   ├── trigger.c         # Keyboard timing logic
│   ├── wallpaper.c       # Root pixmap setter supporting PNG, JPG, and BMP
│   └── wm.c              # Core window management and master-stack/half-split reflow
├── tests/                # Automated unit tests, X11 stress tests & RAM budget runner
├── tools/
│   └── muzuwm-settings   # Comprehensive GTK 3 & CLI desktop control centre
├── run-xephyr.sh         # Nested X11 test server launch script
└── Makefile              # C11 build configuration with automatic header dependency tracking
```

---

## Prerequisites & Build Instructions

### Dependencies

**Debian / Ubuntu / Linux Mint:**

```bash
sudo apt update
sudo apt install build-essential libcairo2-dev libx11-dev libxext-dev libxcursor-dev \
                 libfontconfig1-dev libgdk-pixbuf-2.0-dev pkg-config picom \
                 x11-server-utils xserver-xephyr python3-gi
```

**Arch Linux / Manjaro:**

```bash
sudo pacman -S base-devel cairo libx11 libxext libxcursor fontconfig gdk-pixbuf2 \
               pkgconf picom xorg-server-xephyr python-gobject
```

**Fedora:**

```bash
sudo dnf install @development-tools cairo-devel libX11-devel libXext-devel libXcursor-devel \
                 fontconfig-devel gdk-pixbuf2-devel pkgconf-pkg-config picom xorg-x11-server-Xephyr python3-gobject
```

### Compilation

```bash
make clean && make
```

Build with AddressSanitizer enabled for memory safety verification:

```bash
make asan
```

---

## Running & Testing

### 1. Isolated Testing via Xephyr

```bash
./run-xephyr.sh 1280x700
```

### 2. Setting a Custom Wallpaper

```bash
./muzuwm --wallpaper ~/Pictures/wallpaper.png
```

### 3. Installing as a Display Manager Session (GNOME / GDM / LightDM)

```bash
sudo ./install.sh
```
*(or `sudo make install`)*

This installs the binary to `/usr/local/bin/muzuwm`, the settings centre to `/usr/local/bin/muzuwm-settings`, and registers `/usr/share/xsessions/muzuwm.desktop` so you can select **MUZUwm** directly from your login screen alongside GNOME.

### 4. Uninstallation

To remove MUZUwm from your system:

```bash
sudo ./uninstall.sh
```

The uninstaller will ask interactively whether you wish to delete your user configuration (`~/.config/muzuwm`) or keep it.

You can also use flags for non-interactive execution:
- `sudo ./uninstall.sh --keep-config` (removes binary & session, preserves `~/.config/muzuwm`)
- `sudo ./uninstall.sh --purge` (removes binary & session and deletes `~/.config/muzuwm`)
- Or run `sudo make uninstall`

---

## Test Suite & Quality Assurance

Run the full automated unit test suite and 200 MB RAM ceiling benchmark:

```bash
./tests/test_runner.sh
```

Individual test binaries:
- `./test_layout` — Tiling, edge snapping, maximize/unmaximize, and `can_fullscreen` guards
- `./test_config_rules` — `fullscreen_allowlist`, `auto_allowlist`, and `shortcuts.conf` parser & modifier matching
- `./test_client_props` — ICCCM/EWMH properties, CSD detection, size hints, and multi-window docked geometry
- `./test_gtk_theme` — GTK settings parsing, `XCURSOR_THEME`/`XCURSOR_SIZE` export, and cursor safety
- `./test_tint2_panel` — Top panel clock, workspace indicators, and `tint2rc` parser
- `./test_compositor` — Picom command generation and Xephyr fallback detection
- `./test_alttab` — Alt-Tab HUD switcher, application grouping, and window count dots
- `./test_mission_viewer` — Mission Control fullscreen overview grid & thumbnail card hit testing
- `./test_dock_menu` — Dock context menu separation (app items vs dock preferences)
- `./test_kbd_layout` — Keyboard layout parsing and switching logic
- `./test_confirm_dialog` — Power menu confirmation modal dialog logic
- `./test_rofi_launcher`, `./test_icon_fetcher`, `./test_wallpaper`, `./test_menu`, `./test_trigger`

---

## Roadmap

- [x] Floating-by-default desktop with `fullscreen_allowlist`, `auto_allowlist`, and `shortcuts.conf`.
- [x] Fullscreen/resize capability detection (`WM_NORMAL_HINTS`) protecting fixed-size apps and dialogs.
- [x] Multi-window docked coordinate preservation (Audacious Winamp skin compatibility).
- [x] Liquid Glass Tint2-compatible top panel with 4 workspaces, seconds clock, and power menu.
- [x] 32-bit ARGB frosted-glass titlebars (88% opacity) and Picom dual-kawase blur integration.
- [x] Moveable frosted-glass dock (Bottom/Left/Right) with magnification wave, drag-to-reorder, Trash bin, and Apps button.
- [x] Built-in animated Rofi-style application launcher with 60fps animation and mouse wheel scrolling.
- [x] Automatic GTK theme, icon theme, and `libXcursor` (`left_ptr`) cursor synchronization.
- [x] Application-grouped Alt-Tab switcher HUD with dynamic window count dots.
- [x] Mission Viewer (Mission Control) fullscreen overview overlay with interactive window cards.
- [x] Dock context menu separation: app actions vs dock configuration.
- [x] Unified GUI & CLI Control Centre (`muzuwm-settings`) for language, dock, titlebar, allowlists, and themes.
- [ ] Multi-monitor support via X11 RandR / Xinerama.
- [ ] Wayland compositor exploration (`wlroots`).

---

> Please don’t put your life in the hands  
> Of a Rock ‘n’ Roll band  
> Who’ll throw it all away


# Suppoet and community

https://discord.gg/jc3QngErz8

---

## License

**Acknowledgements & Credits**

This project stands on the shoulders of giants. While architected to serve a custom workflow, parts of the codebase, internal logic, and core mechanics incorporate or derive from several outstanding open-source projects:

* **tinywm** (Nick Welch) — Foundational blueprint for minimalist window management under X11.
* **Openbox** (Dana Jansens, Mikael Magnusson, Derek Foreman, and contributors) — Window manipulation concepts and event-handling mechanics.
* **Rofi** (Dave Davenport, Rasmus Steinke, and contributors) — Application launcher and dynamic menu design patterns.
* **waypaper** (Anufriev Roman) — Wallpaper configuration compatibility.

### Licensing & Derivative Works

In compliance with the licenses of the upstream projects—specifically the GNU General Public License (GPL) governing code derived from Openbox—this entire project is proudly licensed under the **GNU General Public License v3.0 (GPL-3.0)** (or later).
