# MUZUwm

Lightweight, floating-by-default X11 Window Manager written in C11 (`Xlib`, `Xext`, `Xcursor`, `Xrandr`, `Cairo`).

MUZUwm focuses strictly on core window management, EWMH/ICCCM compliance, multi-monitor snapping, multi-workspace support, and external shell/panel interoperability (`_NET_WM_STRUT_PARTIAL`, `_NET_WORKAREA`, `_NET_CLIENT_LIST`). Desktop panels, app launchers, docks, and compositors are intentionally kept out of the WM core.

---

## Core Features

- **Floating by Default & Hybrid Snapping:** Windows open floating at their natural size; snap resizable windows left/right (50/50) or fullscreen via keyboard or mouse drag with live snap preview.
- **Click-to-Focus, Focus-Follows-Mouse & Focus Borders:** Supports both click-to-focus (replaying clicks to clients) and pointer focus-follows-mouse (`focus_follows_mouse = 1` by default), with configurable focused/unfocused border colors (`border_width`, `focus_border_color`, `unfocus_border_color`).
- **XRandR Multi-Monitor & Per-Monitor Struts:** Detects connected monitors via `XRandR` (`RRScreenChangeNotify`), calculates per-monitor insets/snap zones, and respects external panel/dock struts.
- **Multi-Workspace Support:** Configurable workspaces (`workspace_count = 4` by default, up to 10) with `_NET_NUMBER_OF_DESKTOPS`, `_NET_CURRENT_DESKTOP`, and `_NET_WM_DESKTOP`.
- **Borderless Client Management & EWMH Support:** Exposes `_NET_SUPPORTED`, `_NET_SUPPORTING_WM_CHECK`, `_NET_WM_NAME`, `_NET_ACTIVE_WINDOW`, `_NET_CLIENT_LIST`, `_NET_WORKAREA`, `_NET_WM_STATE` (`ClientMessage`), `_NET_CLOSE_WINDOW`, `_NET_WM_WINDOW_TYPE`, and `WM_TRANSIENT_FOR` dialog stacking.
- **Optional `Alt+Tab` / `Alt+\`` Switcher:** Built-in Cairo window switcher (including intra-app switching with `Alt+\``), toggleable via `enable_alttab = 1 | 0` in config.
- **Toggleable Desktop Context Menu & Emergency `Ctrl+Alt + 2x Delete`:** Right-clicking empty desktop space opens quick actions (`Terminal`, `Internet`, `App`, `Fallback Launcher (mzml)`, `Settings`). Can be disabled (`enable_desktop_menu = 0`) when using `mzshell`, while pressing `Ctrl+Alt+Delete` twice within 500 ms is hardcoded to force-open the menu in emergencies.
- **Bundled Fallback Launcher (`mzml`):** Standalone C11 + Cairo `.desktop` application launcher installed alongside `muzuwm`; `Super+space` and `App` try `mzshell` first and automatically fall back to `mzml` on clean installs.
- **Configurable Rules & Shortcuts:** Per-class `fullscreen_allowlist`, startup `auto_allowlist`, `shortcuts.conf`, and `muzuwm.conf` / `settings.conf`.

---

## Controls

### Mouse

| Action | Description |
| :--- | :--- |
| `Super + Left-Drag` | Move floating window; drag to left, right, or top screen edge to snap |
| `Super + Right-Drag` | Resize resizable window |
| `Desktop Right-Click` | Open desktop context menu (`Terminal`, `Internet`, `App`, `Settings`) |

### Default Shortcuts (`config/muzuwm.conf` / `~/.config/muzuwm/shortcuts.conf`)

| Shortcut | Action |
| :--- | :--- |
| `Super + Left` / `Right` | Snap window to left / right half (`tile_left` / `tile_right`) |
| `Super + Up` / `Super + m` | Toggle fullscreen / maximize (`fullscreen`) |
| `Super + Down` | Restore to floating (`float`) |
| `Super + q` / `Super + x` | Close focused window (`close_window`) |
| `Super + z` | Minimize / iconify focused window (`minimize`) |
| `Super + 1..0` | Recall / focus client slot |
| `Super + Return` | Launch terminal (`wezterm` / `alacritty` / `xterm`) |
| `Super + b` | Launch browser |
| `Super + space` | Launch external launcher (`mzshell`, falls back to `mzml`) |
| `Ctrl + Alt + 2x Delete` | Hardcoded emergency trigger: force-open desktop context menu |
| `Super + Ctrl + 1..4` | Switch to workspace 1..4 (`workspace_1..4`) |
| `Super + Shift + 1..4` | Move focused window to workspace 1..4 (`move_workspace_1..4`) |
| `Super + Page_Up` / `Page_Down` | Switch to previous / next workspace (`workspace_prev` / `workspace_next`) |
| `Alt + Tab` / `Alt + Shift + Tab` | Cycle applications when `enable_alttab = 1` |
| `Alt + \`` / `Alt + Shift + \`` | Cycle windows within the active application when `enable_alttab = 1` |

---

## Configuration

MUZUwm checks `~/.config/muzuwm/settings.conf`, `~/.config/muzuwm/muzuwm.conf`, and `/etc/muzuwm/muzuwm.conf`:

- `enable_alttab = 1` — Enable (`1`) or disable (`0`) the built-in `Alt+Tab` / `Alt+\`` switcher.
- `workspace_count = 4` — Number of virtual desktops (`1` to `10`).
- `focus_follows_mouse = 1` — Enable (`1`) or disable (`0`) pointer focus-follows-mouse (click-to-focus is always active).
- `enable_desktop_menu = 1` — Enable (`1`) or disable (`0`) the desktop right-click menu (`Ctrl+Alt + 2x Delete` always overrides).
- `border_width = 2` — Window border width in pixels (`0` to `16`).
- `focus_border_color = #89b4fa` — Focused window border hex color.
- `unfocus_border_color = #45475a` — Unfocused window border hex color.
- `~/.config/muzuwm/shortcuts.conf` — Custom keybindings (`none` to unbind).
- `~/.config/muzuwm/fullscreen_allowlist` — Window classes/titles to maximize automatically on map.
- `~/.config/muzuwm/auto_allowlist` — Commands to run at WM startup.

---

## Build & Install

### Dependencies

- **Debian / Ubuntu:** `build-essential pkg-config libcairo2-dev libx11-dev libxext-dev libxcursor-dev libxrandr-dev libfontconfig1-dev`
- **Arch Linux:** `base-devel pkgconf cairo libx11 libxext libxcursor libxrandr fontconfig`
- **Fedora:** `@development-tools pkgconf-pkg-config cairo-devel libX11-devel libXext-devel libXcursor-devel libXrandr-devel fontconfig-devel`

### Compile

```bash
make clean && make
```

With AddressSanitizer:

```bash
make asan
```

### Install / Uninstall

```bash
sudo make install
sudo make uninstall
```

`make install` installs `muzuwm`, `mzml`, `muzu-session`, `muzuwm-settings`, `/usr/share/xsessions/muzuwm.desktop`, and `/etc/muzuwm/muzuwm.conf`.

---

## Testing

Run a nested X11 test session:

```bash
./run-xephyr.sh 1280x700
```

Run the automated unit + Xvfb ASan/UBSan integration test suite:

```bash
bash tests/test_runner.sh
```

Run the headless Xvfb integration suite directly (requires `Xvfb`):

```bash
bash tests/test_integration_xvfb.sh
```

---

## Codebase (`src/`)

- `main.c` — `poll()`-driven X11 event loop, signal/child lifecycle, EWMH/ICCCM properties, key/mouse bindings, desktop menu
- `wm.c` — Client tracking and layout/snap geometry
- `client_props.c` — `WM_NORMAL_HINTS`, `_NET_WM_WINDOW_TYPE`, `WM_TRANSIENT_FOR`, CSD, and fullscreen capability checks
- `ewmh.c` — EWMH `ClientMessage` state resolution, fullscreen transitions, and active window/desktop helpers
- `monitors.c` — `XRandR` multi-monitor detection and per-monitor strut mapping
- `workspace.c` — Multi-workspace switching, client moving, and EWMH desktop publishing
- `config_rules.c` — Config files, `enable_alttab`, `workspace_count`, allowlists, and shortcut matching
- `struts.c` — `_NET_WM_STRUT` / `_NET_WM_STRUT_PARTIAL` and per-desktop `_NET_WORKAREA` calculation
- `snap_preview.c` — Edge-snap preview overlay
- `alttab.c` — Optional `Alt+Tab` / `Alt+\`` switcher HUD
- `menu.c` — Desktop right-click menu
- `titlebar.c` — Frame/titlebar helpers

---

## Support & Community

- Discord: https://discord.gg/jc3QngErz8

---

## License

Licensed under the **GNU General Public License v3.0 (GPL-3.0)** — see [LICENSE](LICENSE). Based on concepts from **tinywm** and **Openbox**.
