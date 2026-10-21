# Code Review Fixes and Dock Top Edge Removal Specification

**Date:** 2026-10-21  
**Target:** MUZUwm (Release Candidate / Dev_Debug)  
**Status:** Approved  

## 1. Intent & Context

Following code review of recent changes to compositor configuration, dock behaviors, and launcher subsystems, 7 key findings were identified ranging from high severity (launch failures, visual overlaps) to medium and low severity (leakage, nondeterministic tests, undefined pointer arithmetic). In addition, the user specified that the top position for the dock must be removed/disabled to prevent layout conflict with the top panel.

## 2. Goals & Decisions

### 2.1 Disable Dock Top Position (User Directed)
- The user instructed: *"การย้าย dock อยู่ตำแหน่งด้านบนปิดทิ้งไป"* (Disable/remove moving dock to the top position).
- The dock context menu must no longer provide "Position: Top".
- Position cycling via `dock_cycle_position()` must transition: `DOCK_BOTTOM -> DOCK_LEFT -> DOCK_RIGHT -> DOCK_BOTTOM`.
- Screen edge snapping via `dock_calculate_nearest_edge()` must only compute distances to bottom, left, and right edges.
- Legacy configuration files or saved settings specifying `pos = top` must safely fall back to `DOCK_BOTTOM`.
- Any geometry calculation or rendering for dock top position must be removed or stubbed to prevent overlapping the panel at `y = 0..28`.

### 2.2 Picom ~/.local/bin Support
- Picom binary lookup in `compositor.c` must inspect `$HOME/.local/bin/picom` in addition to `/usr/bin/picom`, `/usr/local/bin/picom`, and `$PATH`.
- When launching picom via `fork()`, the detected executable binary path must be retained and used for execution (`execl(picom_bin, ...)`), with `PATH` and `LD_LIBRARY_PATH` properly populated if running from `~/.local/bin`.

### 2.3 Dock EWMH Identity
- The dock X11 window must advertise `_NET_WM_WINDOW_TYPE = _NET_WM_WINDOW_TYPE_DOCK`.
- The dock X11 window must set its window name (`WM_NAME` and `_NET_WM_NAME`) to `"MUZUwm Dock"`.
- This ensures compositor shadow exclusions (`name = 'MUZUwm Dock'` and `_NET_WM_WINDOW_TYPE@:a *= 'DOCK'`) match correctly, and external window management tools can identify the window.

### 2.4 Rofi Launcher Icon Surface Leak Prevention
- In `rofi_launcher_scan_apps()`, before resetting `rofi->app_count = 0`, all existing cached `icon_surface` objects across `apps[0..rofi->app_count-1]` must be destroyed using `cairo_surface_finish()` and `cairo_surface_destroy()`.

### 2.5 Deterministic Compositor Unit Testing
- `tests/test_compositor.c` must not depend on or be polluted by an existing configuration on the host machine (`~/.config/picom/picom.conf` or `~/.config/muzuwm/picom.conf`).
- The test harness must set a temporary isolated `$HOME` directory (e.g. `/tmp/muzuwm_test_compositor_XXXXXX`), run the test, and clean up the temporary directory on exit.

### 2.6 Guard Against Negative Pointer Arithmetic in Config Parsers
- In `src/tint2_panel.c` and `src/rofi_launcher.c`, ensure string length is strictly greater than 0 before performing `key + strlen(key) - 1` or `val + strlen(val) - 1` trimming.
- Empty keys or empty values (e.g. lines with `=foo` or `foo:`) must not cause undefined pointer arithmetic.

### 2.7 Clean Leak Sanitizer Gate for test_layout
- `tests/test_layout.c` must free all temporary client list allocations after each test case using `client_remove()` / `free_clients()`.
- Running `make asan` on `test_layout` must produce zero leaked bytes.

## 3. Non-Goals
- Altering the bottom panel behavior or changing how tint2 panel parses non-empty values.
- Introducing new external dependencies.
