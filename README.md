# MUZUwm (Mouse UserZ Window Manager)

### Modern, Lightweight, Mouse-First Hybrid Tiling &amp; Floating X11 Window Manager
  
> Intuitive, fluid one-handed desktop ergonomics for the user — engineered with pure low-level C11 and Cairo graphics under the hood.

![Language](https://img.shields.io/badge/Language-C11-blue.svg)
![Graphics](https://img.shields.io/badge/Graphics-Cairo%20%2F%20Xlib-orange.svg)
![Platform](https://img.shields.io/badge/Platform-X11%20%2F%20Linux-green.svg)
![Memory Footprint](https://img.shields.io/badge/Memory%20RSS-~15--22%20MB-success.svg)
![Standards](https://img.shields.io/badge/Standards-EWMH%20%2F%20ICCCM-informational.svg)
![License](https://img.shields.io/badge/License-MIT-lightgrey.svg)

---

## Table of Contents

- [Overview &amp; Philosophy](#overview--philosophy)
- [New Systems &amp; Modern Architecture](#new-systems--modern-architecture)
  - [1. Native Animated Rofi-Style Application Launcher](#1-native-animated-rofi-style-application-launcher)
  - [2. Moveable &amp; Magnified Frosted-Glass Dock](#2-moveable--magnified-frosted-glass-dock)
  - [3. macOS-Style Traffic Light Controls &amp; Smart Edge Snapping](#3-macos-style-traffic-light-controls--smart-edge-snapping)
  - [4. Reusable Sleek Context Menu Engine](#4-reusable-sleek-context-menu-engine)
  - [5. Zero-Overhead Native Wallpaper Engine](#5-zero-overhead-native-wallpaper-engine)
  - [6. Automatic GTK Theme &amp; Icon Theme Synchronization](#6-automatic-gtk-theme--icon-theme-synchronization)
- [Desktop Controls &amp; Gestures Cheat Sheet](#desktop-controls--gestures-cheat-sheet)
- [System Architecture &amp; Codebase Map](#system-architecture--codebase-map)
- [Prerequisites &amp; Build Instructions](#prerequisites--build-instructions)
- [Running &amp; Testing](#running--testing)
- [Configuration &amp; Data Paths](#configuration--data-paths)
- [Test Suite &amp; Quality Assurance](#test-suite--quality-assurance)
- [Roadmap](#roadmap)
- [License](#license)

---

## Overview &amp; Philosophy

Traditional tiling window managers (i3, bspwm, dwm) deliver speed and keyboard efficiency but demand memorizing dozens of complex multi-key combinations. Conversely, full-featured desktop environments (GNOME, KDE) offer mouse navigation at the cost of high RAM consumption and background service overhead.

**MUZUwm** closes this gap by providing an **out-of-the-box, mouse-first hybrid window manager**:

1. **Mouse-First &amp; Zero Muscle Memory:** Operate your desktop with one hand. Drag windows, snap to screen edges, minimize to dock, restore, search applications, and configure settings using fluid mouse interactions.
2. **True Out-of-the-Box (OOTB) Usability:** Comes integrated with vector titlebars, a moveable frosted-glass dock, a native animated application launcher, a root wallpaper engine, and context menus—no external daemons or complex dotfiles required.
3. **Ultra Lightweight on X11:** Built with pure **C11**, **Xlib**, and **Cairo Graphics**, maintaining an RSS footprint under **25 MB** and zero idle CPU cycles.
4. **The "Muzu" (むず) Lore:** Named after Japanese slang *Muzu* (short for *Muzukashii* / 難しい, meaning "brutally difficult"). The philosophy dictates that all complexity—X11 event timers, reparenting, FreeDesktop icon resolution, and Cairo compositing—is absorbed by the window manager code so the user enjoys frictionless simplicity.

---

## New Systems &amp; Modern Architecture

MUZUwm has undergone a major evolutionary leap, replacing experimental prototypes with fully modular, production-grade subsystems:

```
+---------------------------------------------------------------------------------+
|                                    MUZUwm                                       |
|                                                                                 |
|  +---------------------------+  +--------------------------+  +--------------+  |
|  | Native Animated Launcher  |  | Frosted-Glass Glass Dock |  | Context Menu |  |
|  | (Rofi drun, Cairo 60fps)  |  | (Magnification, Drag/Snap)|  | (Dock/Root)  |  |
|  +---------------------------+  +--------------------------+  +--------------+  |
|                                                                                 |
|  +---------------------------+  +--------------------------+  +--------------+  |
|  | Traffic Lights & Snapping |  | Zero-Overhead Wallpaper  |  |  GTK & Icons |  |
|  | (Close, Min, Max, 50/50)  |  | (Root Pixmap, PNG/JPG)   |  | (Auto-Theme) |  |
|  +---------------------------+  +--------------------------+  +--------------+  |
|                                                                                 |
|  +---------------------------------------------------------------------------+  |
|  |                  Core Window Management (X11, EWMH, ICCCM)                |  |
|  +---------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------+
```

### 1. Native Animated Rofi-Style Application Launcher

*Source:* `src/rofi_launcher.c` | *Header:* `include/rofi_launcher.h`

Replaces bulky full-screen overlays with an ultra-responsive, standalone application launcher integrated directly into the binary:

- **Instant Activation (&lt; 1ms):** Summon instantly by tapping the `Super` key, pressing `Alt+F1`, or clicking the three-dots "Apps" icon on the Dock.
- **Buttery-Smooth 60fps Micro-Animation:** Slides down smoothly into center screen ($y_{\text{start}} = y_{\text{final}} + 24\text{px} \to y_{\text{final}}$) using dynamic cubic ease-out interpolation.
- **FreeDesktop Application Discovery:** Automatically scans `/usr/share/applications` and `~/.local/share/applications` for `.desktop` entries, parsing Exec, Name, and localized Comment metadata.
- **Live Fuzzy Search:** Type instantly to filter applications in real-time.
- **High-Fidelity Icon Parsing:** Resolves application icons directly from the system icon theme chain (`Adwaita`, `Papirus`, `GNOME`, `hicolor`).
- **Drag-to-Pin Integration:** Click and drag any item out of the launcher directly onto the Dock to pin it dynamically.

### 2. Moveable &amp; Magnified Frosted-Glass Dock

*Source:* `src/dock.c` | *Header:* `include/dock.h`

A versatile, elegant dock component inspired by macOS:

- **macOS-Style Magnification Wave:** Icons enlarge smoothly under the cursor using a distance-based cosine falloff scale formula ($s = 1.0 + \Delta s \cdot \cos^2$), creating a fluid physical wave effect.
- **4-Edge Dynamic Positioning:** Can be freely docked to any screen border (`DOCK_BOTTOM`, `DOCK_LEFT`, `DOCK_TOP`, `DOCK_RIGHT`). Right-click drag the dock to snap it to the nearest edge on release, or cycle through edges via the context menu.
- **Dynamic App Pinning &amp; Persistence:** Drop apps from the launcher onto the dock. Pinned configurations are automatically persisted to `~/.config/muzuwm/dock_apps.ini`.
- **Active Running Indicators:** Displays crisp glowing indicator dots beneath icons for currently running applications.
- **Window Minimization &amp; Restore:** Windows minimized via the titlebar yellow traffic light transition into dedicated dock slots; clicking any minimized slot restores the window to its active workspace.

### 3. macOS-Style Traffic Light Controls &amp; Smart Edge Snapping

*Source:* `src/titlebar.c`, `src/wm.c` | *Headers:* `include/titlebar.h`, `include/wm.h`

- **Vector Traffic Light Titlebar Buttons:**
  - 🔴 **Red (`#FF5F56`):** Closes the client safely via standard `_NET_CLOSE_WINDOW` / `WM_DELETE_WINDOW` protocols.
  - 🟡 **Yellow (`#FFBD2E`):** Minimizes the client directly to the Dock and reflows any remaining tiled windows.
  - 🟢 **Green (`#27C93F`):** Toggles window between Fullscreen/Maximize and restored geometry.
- **Smart Edge Snapping:**
  - Drag window to the **Left or Right screen edge** $\rightarrow$ Snaps cleanly into a 50/50 half-screen split.
  - Drag window to the **Top screen edge** $\rightarrow$ Snaps to full screen.
  - Double-click any titlebar $\rightarrow$ Snaps back to tiled layout or toggles maximize.
- **CSD (Client-Side Decoration) Auto-Detection:** Intelligently detects applications utilizing GTK/Qt headerbars (`_GTK_FRAME_EXTENTS`), suppressing duplicate window borders.

### 4. Reusable Sleek Context Menu Engine

*Source:* `src/menu.c` | *Header:* `include/menu.h`

A lightweight, dark Cairo popup menu component:

- **Dock Context Menu:** Right-click (`Button3`) on the dock to toggle Magnification on/off, change edge orientation, adjust sizing, toggle auto-hide, or unpin applications.
- **Desktop Root Context Menu:** Right-click on empty desktop wallpaper space to open terminal, summon the application launcher, or open the wallpaper selector.

### 5. Zero-Overhead Native Wallpaper Engine

*Source:* `src/wallpaper.c` | *Header:* `include/wallpaper.h`

- **Root Window Pixmap:** Sets the X11 background directly using `XSetWindowBackgroundPixmap` and configures `_XROOTPMAP_ID` and `ESETROOT_PMAP_ID` for full compositor compatibility.
- **Universal Format Support:** Renders PNG, JPG, and BMP image formats with bilinear scaling and centering.
- **Zero Ongoing Resource Consumption:** Once the root pixmap is uploaded to the X server, all image buffers are freed—**0% CPU and 0 MB RAM overhead** thereafter (no external wallpaper daemons required).
- **CLI Flag &amp; Config Integration:** Specify wallpaper via `--wallpaper <path>` / `-w <path>`, or sync automatically with Waypaper (`~/.config/waypaper/config.ini`) or `~/.config/muzuwm/wallpaper.png`.

### 6. Automatic GTK Theme &amp; Icon Theme Synchronization

*Source:* `src/gtk_theme.c`, `src/icon_fetcher.c` | *Headers:* `include/gtk_theme.h`, `include/icon_fetcher.h`

- Auto-detects user GTK themes from `~/.config/gtk-3.0/settings.ini` and `~/.gtkrc-2.0`.
- Automatically extracts titlebar, background, text, and accent colors to ensure MUZUwm decorations match GTK applications seamlessly.
- Exports standard GTK environment variables (`GTK_THEME`, `XDG_CURRENT_DESKTOP=MUZUwm`) on startup.

---

## Desktop Controls &amp; Gestures Cheat Sheet

### Mouse Interactions


| Action                        | Target              | Description                                                 |
| :----------------------------- | :------------------- | :----------------------------------------------------------- |
| **Click &amp; Drag**          | Window Titlebar     | Move / float window freely.                                 |
| **Drag to Left / Right Edge** | Screen Edges        | Snaps window to 50/50 half-screen split.                    |
| **Drag to Top Edge**          | Top Screen Edge     | Maximizes window.                                           |
| **Double-Click**              | Window Titlebar     | Toggles maximize or snaps window back to tiled mode.        |
| **Red Traffic Light**         | Titlebar Top-Left   | Closes window (`WM_DELETE_WINDOW`).                         |
| **Yellow Traffic Light**      | Titlebar Top-Left   | Minimizes window to Dock.                                   |
| **Green Traffic Light**       | Titlebar Top-Left   | Toggles maximize / restore.                                 |
| **Cursor Hover**              | Dock Icons          | Triggers macOS-style magnification wave effect.             |
| **Right-Click**               | Dock                | Opens Dock Context Menu (Magnification, Position, Size).    |
| **Right-Click &amp; Drag**    | Dock                | Drags and snaps dock to nearest screen edge.                |
| **Right-Click**               | Desktop Root        | Opens Desktop Context Menu (Terminal, Launcher, Wallpaper). |
| **Drag &amp; Drop**           | Launcher $\to$ Dock | Pins application to the Dock permanently.                   |


### Keyboard Shortcuts


| Shortcut                                 | Action                                           |
| :---------------------------------------- | :------------------------------------------------ |
| `Super` (Windows Key)                    | Toggle Native Animated Application Launcher      |
| `Alt + F1`                               | Fallback shortcut to toggle Application Launcher |
| `Up` / `Down` or `Ctrl + p` / `Ctrl + n` | Navigate launcher search results                 |
| `Enter`                                  | Launch selected application                      |
| `Escape`                                 | Dismiss launcher or popup context menu           |


---

## System Architecture &amp; Codebase Map

```
MUZUwm/
├── include/
│   ├── client_props.h    # EWMH & ICCCM window property parsers & CSD detection
│   ├── dock.h            # Frosted-glass dock, magnification, edge positioning
│   ├── gtk_theme.h       # GTK theme configuration parser & color definitions
│   ├── icon_fetcher.h    # FreeDesktop icon lookup & _NET_WM_ICON parser
│   ├── menu.h            # Reusable Cairo context popup menu component
│   ├── muzu.h            # Global macro definitions and configurations
│   ├── rofi_launcher.h   # Built-in animated application launcher engine
│   ├── titlebar.h        # Vector titlebar & macOS traffic light controls
│   ├── trigger.h         # Double-tap timer & hotkey event triggers
│   ├── wallpaper.h       # Zero-overhead X11 root window pixmap wallpaper engine
│   └── wm.h              # Master-stack tiling, window client list & layout engine
├── src/
│   ├── client_props.c    # Client window state querying and dialog classification
│   ├── dock.c            # Dock rendering, magnification falloff, and item pinning
│   ├── gtk_theme.c       # Settings.ini parser and palette extraction
│   ├── icon_fetcher.c    # Icon theme directory traverser and PNG loader
│   ├── main.c            # Primary X11 event dispatch loop and signal management
│   ├── menu.c            # Cairo context menu drawing and selection events
│   ├── rofi_launcher.c   # Application scanner, fuzzy search, and 60fps animation
│   ├── titlebar.c        # Titlebar Cairo rendering and traffic light hit testing
│   ├── trigger.c         # Keyboard double-tap timing logic
│   ├── wallpaper.c       # Root pixmap setter supporting PNG, JPG, and BMP
│   └── wm.c              # Core window management, tiling reflow, and edge snapping
├── tests/                # Automated test suite and stress tests
├── run-xephyr.sh         # Nested X11 test server launch script
└── Makefile              # C11 build configuration
```

---

## Prerequisites &amp; Build Instructions

### Dependencies

Install the required development headers on your distribution:

**Debian / Ubuntu / Linux Mint:**

```bash
sudo apt update
sudo apt install build-essential libcairo2-dev libx11-dev libxext-dev \
                 libfontconfig1-dev pkg-config x11-server-utils xterm xserver-xephyr
```

**Arch Linux / Manjaro:**

```bash
sudo pacman -S base-devel cairo libx11 libxext fontconfig pkgconf xorg-server-xephyr xterm
```

**Fedora:**

```bash
sudo dnf install @development-tools cairo-devel libX11-devel libXext-devel \
                 fontconfig-devel pkgconf-pkg-config xorg-x11-server-Xephyr xterm
```

### Compilation

Build the `muzuwm` binary with standard optimizations:

```bash
make clean && make muzuwm
```

Build with AddressSanitizer enabled for memory safety verification:

```bash
make asan
```

---

## Running &amp; Testing

### 1. Isolated Testing via Xephyr (Recommended)

You can test MUZUwm inside a nested, safe X11 container without disrupting your primary desktop session:

```bash
# Launch inside an isolated 1366x768 window
./run-xephyr.sh 1366x768

# Or test in standard 1920x1080 resolution
./run-xephyr.sh 1920x1080
```

### 2. Setting a Custom Wallpaper

Pass any PNG, JPG, or BMP image directly at startup:

```bash
./muzuwm --wallpaper ~/Pictures/wallpaper.png
```

### 3. Installing as a Display Manager Session

To add MUZUwm to your login screen (GDM, LightDM, SDDM), copy the binary and create a desktop entry:

```bash
sudo cp muzuwm /usr/local/bin/

sudo tee /usr/share/xsessions/muzuwm.desktop <<EOF
[Desktop Entry]
Name=MUZUwm
Comment=Mouse UserZ Window Manager
Exec=/usr/local/bin/muzuwm
Type=Application
EOF
```

---

## Configuration &amp; Data Paths

MUZUwm requires zero initial configuration to run, but respects standard XDG directory paths:


| Path                             | Description                                                                        |
| :-------------------------------- | :---------------------------------------------------------------------------------- |
| `~/.config/muzuwm/dock_apps.ini` | Stores pinned Dock applications (updated automatically on drag-and-drop).          |
| `~/.config/waypaper/config.ini`  | Read automatically by the wallpaper engine for seamless GUI wallpaper integration. |
| `~/.config/muzuwm/wallpaper.png` | Fallback wallpaper image path.                                                     |
| `~/.config/gtk-3.0/settings.ini` | System GTK theme configuration, consulted for styling and color synchronization.   |


---

## Test Suite &amp; Quality Assurance

MUZUwm includes a modular test suite covering layout logic, icon resolution, theme parsing, and launcher filtering:

```bash
# Compile and run unit tests
make tests

# Execute the test suite
./test_trigger
./test_client_props
./test_icon_fetcher
./test_gtk_theme
./test_wallpaper
./test_rofi_launcher
./test_menu
```

---

## Roadmap

- [x] High-performance Master/Stack tiling layout with edge snapping (Half / Fullscreen).
- [x] Vector titlebar with macOS traffic lights (Close, Minimize to dock, Maximize/Restore).
- [x] Client-Side Decoration (CSD) detection and integration.
- [x] Built-in animated Rofi-style application launcher with 60fps micro-animation and fuzzy search.
- [x] Moveable frosted-glass dock with magnification wave and drag-to-pin persistence.
- [x] Zero-overhead native root window wallpaper engine (PNG, JPG, BMP).
- [x] Reusable dark context popup menus for Dock and Desktop.
- [x] Automatic GTK theme and icon theme color extraction.
- [ ] Multi-monitor support via X11 RandR / Xinerama.
- [ ] GUI Settings &amp; Control Center modal.
- [ ] Virtual workspace / pager component.
- [ ] Wayland compositor exploration (wlroots).

---

## License

**Acknowledgements &amp; Credits**

This project stands on the shoulders of giants. While architected to serve a custom workflow, parts of the codebase, internal logic, and core mechanics incorporate or derive from several outstanding open-source projects. Immense gratitude goes to the respective authors and maintainers whose original contributions made this software possible:

\* **tinywm** (Nick Welch) — For providing the foundational blueprint and demonstrating how minimalist window management under X11 can be achieved with pure simplicity.

\* **Openbox** (Dana Jansens, Mikael Magnusson, Derek Foreman, and contributors) — For the robust window manipulation concepts, event-handling mechanics, and enduring architecture that laid the groundwork for modern lightweight desktop environments.

\* **Rofi** (Dave Davenport, Rasmus Steinke, and contributors) — For the exceptional design patterns, window switching mechanics, and dynamic menu philosophy integrated across the ecosystem.

\* **waypaper** (Anufriev Roman) — For the modern wallpaper management and backend integration techniques adapted to enhance visual customization.

### Licensing &amp; Derivative Works

In compliance with the licenses of the upstream projects—specifically the GNU General Public License (GPL) governing code derived from Openbox—this entire project is proudly licensed under the **GNU General Public License v3.0 (GPL-3.0)** (or later). 

You are free, encouraged, and warmly welcomed to study, modify, fork, hack, and redistribute this software to suit your own needs. If you build something great with this, improve upon it, or adopt snippets into your own setup, all that is asked is to keep this project free, maintain the copyleft protections, and preserve attribution to the original authors who laid the groundwork.
