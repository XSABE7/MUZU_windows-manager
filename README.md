# MUZUwm (Mouse UserZ Window Manager)

### Modern, Lightweight, Mouse-First Hybrid Floating &amp; Tiling X11 Window Manager

> Intuitive, fluid one-handed desktop ergonomics for the user — engineered with pure low-level C11, Xlib, Xcursor, and Cairo graphics under the hood.

![Language](https://img.shields.io/badge/Language-C11-blue.svg)
![Graphics](https://img.shields.io/badge/Graphics-Cairo%20%2F%20Xlib-orange.svg)
![Platform](https://img.shields.io/badge/Platform-X11%20%2F%20Linux-green.svg)
![Memory Footprint](https://img.shields.io/badge/Memory%20RSS-~25%20MB-success.svg)
![Standards](https://img.shields.io/badge/Standards-EWMH%20%2F%20ICCCM-informational.svg)
![License](https://img.shields.io/badge/License-GPL--3.0-lightgrey.svg)

---

## Table of Contents

- [Overview &amp; Philosophy](#overview--philosophy)
- [Implemented Systems &amp; New Features](#implemented-systems--new-features)
  - [1. Floating-by-Default Window Management, Allowlists &amp; Custom Shortcuts](#1-floating-by-default-window-management-allowlists--custom-shortcuts)
  - [2. Liquid Glass Tint2-Compatible Top Panel &amp; 4-Workspace Switcher](#2-liquid-glass-tint2-compatible-top-panel--4-workspace-switcher)
  - [3. Native Animated Rofi-Style Application Launcher](#3-native-animated-rofi-style-application-launcher)
  - [4. Moveable &amp; Magnified Frosted-Glass Dock](#4-moveable--magnified-frosted-glass-dock)
  - [5. macOS-Style Traffic Light Controls &amp; Smart Edge Snapping](#5-macos-style-traffic-light-controls--smart-edge-snapping)
  - [6. Picom Compositor &amp; 32-Bit ARGB Frosted Glass](#6-picom-compositor--32-bit-argb-frosted-glass)
  - [7. Automatic GTK Theme, Icon Theme &amp; Cursor Synchronization](#7-automatic-gtk-theme-icon-theme--cursor-synchronization)
  - [8. Zero-Overhead Native Wallpaper Engine &amp; Context Menus](#8-zero-overhead-native-wallpaper-engine--context-menus)
- [Desktop Controls &amp; Shortcuts Cheat Sheet](#desktop-controls--shortcuts-cheat-sheet)
- [Configuration &amp; Allowlist Files](#configuration--allowlist-files)
- [Recommended Applications](#recommended-applications)
- [Known Bugs &amp; Limitations](#known-bugs--limitations)
- [System Architecture &amp; Codebase Map](#system-architecture--codebase-map)
- [Prerequisites &amp; Build Instructions](#prerequisites--build-instructions)
- [Running &amp; Testing](#running--testing)
- [Test Suite &amp; Quality Assurance](#test-suite--quality-assurance)
- [Roadmap](#roadmap)
- [License](#license)

---

## Overview &amp; Philosophy

Traditional tiling window managers (`i3`, `bspwm`, `dwm`) deliver speed and keyboard efficiency but demand memorizing dozens of multi-key combinations and writing complex dotfiles. Conversely, full-featured desktop environments (`GNOME`, `KDE`) offer mouse navigation at the cost of heavy RAM consumption and background services.

**MUZUwm** closes this gap with an **out-of-the-box, mouse-first hybrid floating and tiling window manager**:

1. **Hovering Floating by Default, Instant Tiling on Demand:** Applications open hovering at their natural size on the desktop (or at their multi-window docked positions for apps like Audacious). Snap any resizable window to 50/50 split or fullscreen using `Super + Arrow` keys or by dragging to a screen edge.
2. **True Out-of-the-Box (OOTB) Usability:** Integrated with a Liquid Glass top panel, 4 workspaces, vector macOS-style titlebars, a magnified frosted-glass dock with Trash and App Grid buttons, a 60fps native application launcher, a root wallpaper engine, and context menus.
3. **Ultra Lightweight on X11:** Built with pure **C11**, **Xlib**, **Xcursor**, and **Cairo Graphics**, maintaining an RSS footprint around **~25 MB** (strictly verified under a 200 MB ceiling).

---

## Implemented Systems &amp; New Features

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
|  | Native Animated Launcher  |  | Window Rules & Shortcuts |  |  GTK, Icons  |  |
|  | (Rofi drun, Cairo 60fps)  |  | (Allowlists, Super+Arrow)|  | & Xcursor    |  |
|  +---------------------------+  +--------------------------+  +--------------+  |
|                                                                                 |
|  +---------------------------------------------------------------------------+  |
|  |        Core Window Management (X11, EWMH, ICCCM, 32-Bit ARGB, Picom)      |  |
|  +---------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------+
```

### 1. Floating-by-Default Window Management, Allowlists &amp; Custom Shortcuts

*Source:* `src/config_rules.c`, `src/client_props.c` | *Headers:* `include/config_rules.h`, `include/client_props.h`

- **Floating by Default:** New windows open floating centered on the desktop workarea at their natural dimensions instead of stretching to fullscreen. Multi-window applications that specify explicit coordinates before mapping (such as **Audacious** in Winamp skin mode with docked Main, Equalizer, and Playlist windows) have their exact docked positions preserved.
- **`fullscreen_allowlist` (`~/.config/muzuwm/fullscreen_allowlist`):** Only applications explicitly listed by `WM_CLASS` (`res_name` or `res_class`) or window title automatically maximize when opened.
- **`auto_allowlist` (`~/.config/muzuwm/auto_allowlist`):** Launches user-defined startup commands asynchronously on WM startup (no hardcoded `xterm` autostart).
- **Fullscreen Capability Detection (`can_fullscreen`):** Inspects ICCCM `WM_NORMAL_HINTS` (`PMinSize`, `PMaxSize`) and dialog/transient state. Fixed-size windows (`min_width == max_width && min_height == max_height`), small-max windows (`< 800x600`), and dialogs set `can_fullscreen = 0` and safely ignore `Super + Arrow` tiling, double-click titlebar tiling, green maximize button clicks, and mouse edge-snapping.
- **Custom Shortcuts (`~/.config/muzuwm/shortcuts.conf`):** Configurable keybindings with lock-key (`NumLock` / `CapsLock`) immunity and support for `none` to unbind any default shortcut.

### 2. Liquid Glass Tint2-Compatible Top Panel &amp; 4-Workspace Switcher

*Source:* `src/tint2_panel.c` | *Header:* `include/tint2_panel.h`

- **4 Interactive Workspaces (`1`–`4`):** Left-click a workspace pill or scroll the mouse wheel on the panel to switch workspaces; right-click a workspace pill (or press `Alt/Super + Shift + 1..4`) to send the focused window to that workspace.
- **Focused Window Title &amp; Real-Time Clock:** Displays the active window title in the center and a live clock with seconds (`HH:MM:SS` + date) synced every second.
- **System Power Menu:** Click the power icon on the top-right to open a sleek menu for `Suspend`, `Restart / Reboot`, `Power Off`, `Lock Screen`, and `Log Out`.
- **Dual-Path `tint2rc` Compatibility:** Reads custom panel height, position, and clock format from `~/.config/muzuwm/tint2rc` or `~/.config/i3/tint2/tint2rc`.

### 3. Native Animated Rofi-Style Application Launcher

*Source:* `src/rofi_launcher.c` | *Header:* `include/rofi_launcher.h`

- **Tap-to-Open `Super` Key:** Tapping and releasing `Super` alone toggles the launcher without interfering with `Super + Arrow` or `Super + Return` combinations. Also accessible via `Alt + F1` or the Dock's 3-dots Apps icon.
- **60fps Micro-Animation &amp; Mouse Wheel Scrolling:** Smoothly animates into view, supports live fuzzy search, keyboard navigation, mouse hover/click, mouse wheel scrolling, and drag-to-pin onto the Dock.
- **Dual-Path `config.rasi` Parsing:** Reads font and prompt customizations from `~/.config/muzuwm/rofi/config.rasi` or `~/.config/i3/rofi/config.rasi`.

### 4. Moveable &amp; Magnified Frosted-Glass Dock

*Source:* `src/dock.c` | *Header:* `include/dock.h`

- **macOS-Style Magnification Wave:** Smooth cosine-falloff icon magnification on hover with floating app-name tooltips.
- **Bottom, Left &amp; Right Edge Positioning:** Position the dock at `Bottom`, `Left`, or `Right` via the Dock Context Menu (top edge is disabled so the dock never overlaps the top panel or window titlebars).
- **Drag-to-Reorder &amp; Drag-to-Pin:** Reorder pinned icons with left-click drag, or drag apps from the launcher onto the dock (persisted in `~/.config/muzuwm/dock_apps.ini` and `~/.config/muzuwm/dock_settings.ini`).
- **Built-in Trash Bin &amp; Apps Grid Icon:** Includes a vector Trash Bin icon (with `Empty Trash` action connected to `~/.local/share/Trash/files`) and a 3-dots Apps Launcher button configurable to either corner.

### 5. macOS-Style Traffic Light Controls &amp; Smart Edge Snapping

*Source:* `src/titlebar.c`, `src/wm.c` | *Headers:* `include/titlebar.h`, `include/wm.h`

- **32-Bit ARGB Frosted Titlebar (88% Opacity):**
  - 🔴 **Red (`#FF5F56`):** Closes the window via `WM_DELETE_WINDOW` (or `XKillClient` fallback).
  - 🟡 **Yellow (`#FFBD2E`):** Minimizes the window into the Dock.
  - 🟢 **Green (`#27C93F`):** Toggles fullscreen/maximize (guarded by `can_fullscreen`).
- **Keyboard &amp; Mouse Edge Snapping:** Tile windows left/right (`50/50`), maximize (`fullscreen`), or restore to centered floating (`float`) using `Super + Arrow` or mouse drag-to-edge.
- **CSD Auto-Detection:** Detects `_MOTIF_WM_HINTS` and `_GTK_FRAME_EXTENTS` so CSD windows (e.g. Audacious skins, GTK4 apps) are not double-decorated.
- **Unfocused Window Ghost Opacity:** Unfocused windows automatically fade to 85% opacity (`_NET_WM_WINDOW_OPACITY`) after 30 seconds of inactivity and restore to 100% immediately on focus.

### 6. Picom Compositor &amp; 32-Bit ARGB Frosted Glass

*Source:* `src/compositor.c` | *Header:* `include/compositor.h`

- Automatically launches `picom` (searching both `$PATH` and `~/.local/bin/picom`) using `~/.config/muzuwm/picom.conf` or `~/.config/i3/picom.conf`.
- Automatically detects nested `Xephyr` displays and switches to `--backend xrender --no-vsync` for glitch-free rendering in test environments.

### 7. Automatic GTK Theme, Icon Theme &amp; Cursor Synchronization

*Source:* `src/gtk_theme.c`, `src/icon_fetcher.c` | *Headers:* `include/gtk_theme.h`, `include/icon_fetcher.h`

- Parses `~/.gtkrc-2.0`, `~/.config/gtk-3.0/settings.ini`, and `~/.config/gtk-4.0/settings.ini` for `gtk-theme-name`, `gtk-icon-theme-name`, `gtk-cursor-theme-name`, `gtk-cursor-theme-size`, and `gtk-font-name`.
- Exports `GTK_THEME`, `XCURSOR_THEME`, and `XCURSOR_SIZE` to child applications.
- Uses `libXcursor` (`gtk_theme_apply_cursor`) to apply the GTK `left_ptr` arrow cursor directly onto the X11 root window, replacing the legacy X11 `X` cross cursor.

### 8. Zero-Overhead Native Wallpaper Engine &amp; Context Menus

*Source:* `src/wallpaper.c`, `src/menu.c` | *Headers:* `include/wallpaper.h`, `include/menu.h`

- Renders PNG, JPG, and BMP wallpapers directly to the X11 root pixmap (`_XROOTPMAP_ID` / `ESETROOT_PMAP_ID`) and frees client buffers immediately for zero ongoing RAM overhead.
- Supports `--wallpaper <path>`, `~/.config/muzuwm/wallpaper.png`, `~/.config/i3/wallpaper.png`, and Waypaper (`~/.config/waypaper/config.ini`), plus live reload via `_MUZU_RELOAD_WALLPAPER`.

---

## Desktop Controls &amp; Shortcuts Cheat Sheet

### Default Keyboard Shortcuts (`~/.config/muzuwm/shortcuts.conf`)

| Shortcut | Default Action | Description |
| :--- | :--- | :--- |
| `Super` (Tap &amp; Release) | Toggle Launcher | Opens/closes the built-in animated Rofi application launcher. |
| `Super + Left` | `tile_left` | Snaps the focused window to the left half of the screen (ignored if fixed-size). |
| `Super + Right` | `tile_right` | Snaps the focused window to the right half of the screen (ignored if fixed-size). |
| `Super + Up` | `fullscreen` | Toggles maximize/fullscreen on the focused window (ignored if fixed-size). |
| `Super + Down` | `float` | Restores a tiled or maximized window back to centered floating mode. |
| `Super + Return` | `exec wezterm` | Launches `wezterm` (customizable in `shortcuts.conf`). |
| `Super + b` | `exec google-chrome` | Launches `google-chrome` (customizable or set `= none` to unbind). |
| `Super + q` | `close_window` | Gracefully closes the focused window. |
| `Super + 1..4` / `Alt + 1..4` | Switch Workspace | Switches active workspace (`1` to `4`). |
| `Super + Shift + 1..4` | Move to Workspace | Moves the focused window to workspace `1` to `4`. |
| `Alt + F1` | Toggle Launcher | Fallback hotkey for the application launcher. |

### Mouse Interactions

| Action | Target | Description |
| :--- | :--- | :--- |
| **Click &amp; Drag** | Window Titlebar | Moves floating window (detaches tiled/maximized windows automatically). |
| **Drag to Left / Right Edge** | Screen Edges | Snaps resizable window to 50/50 split. |
| **Drag to Top Edge** | Top Screen Edge | Maximizes resizable window. |
| **Double-Click** | Window Titlebar | Toggles tiled/floating mode on resizable windows. |
| **Red / Yellow / Green Buttons** | Titlebar Top-Left | Close / Minimize to Dock / Toggle Maximize. |
| **Left-Click &amp; Drag** | Dock Icons | Reorders pinned applications on the Dock. |
| **Right-Click** | Dock | Opens app actions (`Launch`, `Pin/Unpin`, `Close`) and Dock settings (`Position: Bottom/Left/Right`, `Magnification`, `Auto-Hide`, `Apps Button Corner`). |
| **Mouse Wheel Up / Down** | Top Panel / Launcher | Cycles workspaces on Top Panel, or scrolls app list in Launcher. |

---

## Configuration &amp; Allowlist Files

All configuration files live in `~/.config/muzuwm/` and are automatically created with documented defaults on first launch:

| Path | Purpose |
| :--- | :--- |
| `~/.config/muzuwm/shortcuts.conf` | Custom keyboard shortcuts (`Combo = tile_left \| tile_right \| fullscreen \| float \| close_window \| launcher \| exec <cmd> \| none`). |
| `~/.config/muzuwm/fullscreen_allowlist` | One `WM_CLASS` or window title per line. Only matching resizable windows auto-maximize on launch. |
| `~/.config/muzuwm/auto_allowlist` | One shell command per line to auto-start when MUZUwm launches. |
| `~/.config/muzuwm/dock_apps.ini` | Pinned Dock applications (`Name\|Exec\|Icon`). |
| `~/.config/muzuwm/dock_settings.ini` | Dock preferences (`autohide`, `magnification`, `position`, `size`, `apps_corner`). |
| `~/.config/muzuwm/wallpaper.png` | Default wallpaper image path (also compatible with `~/.config/waypaper/config.ini`). |
| `~/.config/muzuwm/picom.conf` | Optional custom Picom compositor config. |
| `~/.config/muzuwm/tint2rc` | Optional custom top panel config. |

Example `~/.config/muzuwm/shortcuts.conf`:
```ini
Super+Left = tile_left
Super+Right = tile_right
Super+Up = fullscreen
Super+Down = float
Super+Return = exec wezterm
Super+b = exec google-chrome
Super+e = exec thunar
Super+q = close_window
```

---

## Recommended Applications

Because MUZUwm renders custom macOS-style Server-Side Decoration (SSD) titlebars and syncs GTK3/Xcursor themes:
- **File Manager:** **Thunar** (`sudo apt install thunar thunar-archive-plugin gvfs-backends`), **PCManFM**, or **Nemo** (or **Yazi** inside `wezterm`). These integrate cleanly with MUZUwm's single titlebar, GTK theme variables, and the Dock's `~/.local/share/Trash` bin.
- **Terminal:** **WezTerm** (`wezterm`, bound to `Super + Return` by default).

---

## Known Bugs &amp; Limitations

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

## System Architecture &amp; Codebase Map

```
MUZUwm/
├── include/
│   ├── client_props.h    # EWMH/ICCCM property parsers, CSD & fullscreen support detection
│   ├── compositor.h      # Picom compositor lifecycle & Xephyr backend detection
│   ├── config_rules.h    # fullscreen_allowlist, auto_allowlist & shortcuts.conf engine
│   ├── dock.h            # Frosted-glass dock, magnification, drag reorder & Trash/Apps items
│   ├── gtk_theme.h       # GTK theme, icon theme & Xcursor configuration parser
│   ├── icon_fetcher.h    # FreeDesktop icon lookup & _NET_WM_ICON ARGB parser
│   ├── menu.h            # Reusable Cairo context popup menu component
│   ├── muzu.h            # Global macro definitions and constants
│   ├── rofi_launcher.h   # Built-in animated application launcher engine
│   ├── tint2_panel.h     # Liquid Glass top panel, 4 workspaces, clock & power menu
│   ├── titlebar.h        # 32-bit ARGB Cairo titlebar & macOS traffic light controls
│   ├── trigger.h         # Double-tap timer & hotkey event triggers
│   ├── wallpaper.h       # Zero-overhead X11 root window pixmap wallpaper engine
│   └── wm.h              # Client state, tiling reflow & snap layout engine
├── src/
│   ├── client_props.c    # Client properties, initial geometry resolution & size hints
│   ├── compositor.c      # Compositor launcher & config resolution
│   ├── config_rules.c    # Allowlists & custom keyboard shortcut parser/matcher
│   ├── dock.c            # Dock rendering, magnification falloff & item persistence
│   ├── gtk_theme.c       # GTK settings parser, env export & libXcursor root cursor setter
│   ├── icon_fetcher.c    # Icon theme directory traverser and GdkPixbuf loader
│   ├── main.c            # Primary X11 event dispatch loop, EWMH & signal management
│   ├── menu.c            # Cairo context menu drawing and selection events
│   ├── rofi_launcher.c   # Desktop app scanner, fuzzy search & 60fps animation
│   ├── tint2_panel.c     # Top panel rendering, tint2rc parser & workspace hit-testing
│   ├── titlebar.c        # Titlebar Cairo rendering, maximize guard & drag handling
│   ├── trigger.c         # Keyboard timing logic
│   ├── wallpaper.c       # Root pixmap setter supporting PNG, JPG, and BMP
│   └── wm.c              # Core window management and master-stack/half-split reflow
├── tests/                # Automated unit tests, X11 stress tests & RAM budget runner
├── run-xephyr.sh         # Nested X11 test server launch script
└── Makefile              # C11 build configuration with automatic header dependency tracking
```

---

## Prerequisites &amp; Build Instructions

### Dependencies

**Debian / Ubuntu / Linux Mint:**

```bash
sudo apt update
sudo apt install build-essential libcairo2-dev libx11-dev libxext-dev libxcursor-dev \
                 libfontconfig1-dev libgdk-pixbuf-2.0-dev pkg-config picom \
                 x11-server-utils xserver-xephyr
```

**Arch Linux / Manjaro:**

```bash
sudo pacman -S base-devel cairo libx11 libxext libxcursor fontconfig gdk-pixbuf2 \
               pkgconf picom xorg-server-xephyr
```

**Fedora:**

```bash
sudo dnf install @development-tools cairo-devel libX11-devel libXext-devel libXcursor-devel \
                 fontconfig-devel gdk-pixbuf2-devel pkgconf-pkg-config picom xorg-x11-server-Xephyr
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

## Running &amp; Testing

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

This installs the binary to `/usr/local/bin/muzuwm` and registers `/usr/share/xsessions/muzuwm.desktop` so you can select **MUZUwm** directly from your login screen alongside GNOME.

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

## Test Suite &amp; Quality Assurance

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
- [ ] Multi-monitor support via X11 RandR / Xinerama.
- [ ] GUI Settings &amp; Control Center modal.
- [ ] Wayland compositor exploration (`wlroots`).

---

## License

**Acknowledgements &amp; Credits**

This project stands on the shoulders of giants. While architected to serve a custom workflow, parts of the codebase, internal logic, and core mechanics incorporate or derive from several outstanding open-source projects:

* **tinywm** (Nick Welch) — Foundational blueprint for minimalist window management under X11.
* **Openbox** (Dana Jansens, Mikael Magnusson, Derek Foreman, and contributors) — Window manipulation concepts and event-handling mechanics.
* **Rofi** (Dave Davenport, Rasmus Steinke, and contributors) — Application launcher and dynamic menu design patterns.
* **waypaper** (Anufriev Roman) — Wallpaper configuration compatibility.

### Licensing &amp; Derivative Works

In compliance with the licenses of the upstream projects—specifically the GNU General Public License (GPL) governing code derived from Openbox—this entire project is proudly licensed under the **GNU General Public License v3.0 (GPL-3.0)** (or later).
