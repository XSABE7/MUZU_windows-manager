# MUZUwm

> **Status:** `unstable` — lightweight, floating-by-default X11 Window Manager written in C11 (`Xlib`, `Xext`, `Xcursor`, `Cairo`).

MUZUwm focuses strictly on core window management, EWMH/ICCCM compliance, edge snapping, and external shell/panel interoperability (`_NET_WM_STRUT_PARTIAL`, `_NET_WORKAREA`, `_NET_CLIENT_LIST`). Desktop panels, app launchers, docks, and compositors are intentionally kept out of the WM core.

---

## Core Features

- **Floating by Default & Hybrid Snapping:** Windows open floating at their natural size; snap resizable windows left/right (50/50) or fullscreen via keyboard or mouse drag with live snap preview.
- **Borderless Client Management & EWMH Support:** Exposes `_NET_SUPPORTED`, `_NET_SUPPORTING_WM_CHECK`, `_NET_WM_NAME`, `_NET_ACTIVE_WINDOW`, `_NET_CLIENT_LIST`, and `_NET_WORKAREA` (respecting `_NET_WM_STRUT` / `_NET_WM_STRUT_PARTIAL` reserved edges for external bars/shells).
- **Optional `Alt+Tab` Switcher:** Built-in Cairo window switcher, toggleable via `enable_alttab = 1 | 0` in config.
- **Minimal Desktop Context Menu:** Right-clicking empty desktop space provides quick access to `Terminal`, `Internet`, `App`, and `Settings`.
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
| `Super + space` | Launch external launcher (`launcher`) |
| `Alt + Tab` / `Alt + Shift + Tab` | Cycle windows when `enable_alttab = 1` |

---

## Configuration

MUZUwm checks `~/.config/muzuwm/settings.conf`, `~/.config/muzuwm/muzuwm.conf`, and `/etc/muzuwm/muzuwm.conf`:

- `enable_alttab = 1` — Enable (`1`) or disable (`0`) the built-in `Alt+Tab` switcher.
- `~/.config/muzuwm/shortcuts.conf` — Custom keybindings (`none` to unbind).
- `~/.config/muzuwm/fullscreen_allowlist` — Window classes/titles to maximize automatically on map.
- `~/.config/muzuwm/auto_allowlist` — Commands to run at WM startup.

---

## Build & Install

### Dependencies

- **Debian / Ubuntu:** `build-essential pkg-config libcairo2-dev libx11-dev libxext-dev libxcursor-dev libfontconfig1-dev`
- **Arch Linux:** `base-devel pkgconf cairo libx11 libxext libxcursor fontconfig`
- **Fedora:** `@development-tools pkgconf-pkg-config cairo-devel libX11-devel libXext-devel libXcursor-devel fontconfig-devel`

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

`make install` installs `muzuwm`, `muzu-session`, `muzuwm-settings`, `/usr/share/xsessions/muzuwm.desktop`, and `/etc/muzuwm/muzuwm.conf`.

---

## Testing

Run a nested X11 test session:

```bash
./run-xephyr.sh 1280x700
```

Run the automated test suite:

```bash
bash tests/test_runner.sh
```

---

## Codebase (`src/`)

- `main.c` — X11 event loop, EWMH/ICCCM properties, key/mouse bindings, desktop menu
- `wm.c` — Client tracking and layout/snap geometry
- `client_props.c` — `WM_NORMAL_HINTS`, CSD, and fullscreen capability checks
- `config_rules.c` — Config files, `enable_alttab`, allowlists, and shortcut matching
- `struts.c` — `_NET_WM_STRUT` / `_NET_WM_STRUT_PARTIAL` and `_NET_WORKAREA` calculation
- `snap_preview.c` — Edge-snap preview overlay
- `alttab.c` — Optional `Alt+Tab` switcher HUD
- `menu.c` — Desktop right-click menu
- `titlebar.c` — Frame/titlebar helpers

---

## License

Licensed under the **GNU General Public License v3.0 (GPL-3.0)** — see [LICENSE](LICENSE). Based on concepts from **tinywm** and **Openbox**.
