# Built-in Rofi-Style Application Launcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a native, lightweight, Rofi-style application launcher into MUZUwm that opens instantly on a single Super press or Dock Apps click, while completely retiring the old Launchpad.

**Architecture:** Implement `rofi_launcher.h` and `rofi_launcher.c` providing application scanning, instant query substring filtering, keyboard/mouse navigation, and Cairo vector rendering styled after classic Rofi drun. Connect single Super keypress and Dock Apps button to toggle the launcher, and remove legacy `launchpad.c`/`easter_egg.c`.

**Tech Stack:** C11 (`-std=c11 -Wall -Wextra -pedantic`), Xlib, Cairo vector graphics, FreeDesktop desktop/icon standards.

**Spec:** [docs/superpowers/specs/2026-10-20-builtin-rofi-launcher.md](file:///home/peach/git/MUZUwm/docs/superpowers/specs/2026-10-20-builtin-rofi-launcher.md)

## Global Constraints
- Pure C11 with zero compiler warnings (`-Wall -Wextra -pedantic -std=c11 -O2`).
- Total WM RSS memory footprint must remain under 30 MB (current: ~22 MB).
- Instant open response (< 1ms map latency) without spawning external processes to display the UI.
- Single Super keypress (`XK_Super_L` or `XK_Super_R`) opens launcher; Escape closes.
- Clicking "Apps" (item 3) in Dock opens launcher.
- Clean teardown of all Cairo surfaces, windows, and allocated strings with zero leaks.

## Review Focus
1. Rapid Super keypress / Release interleaving: Ensure single Super tap cleanly toggles visibility without getting stuck in a grab loop.
2. Filter buffer bounds & non-ASCII/special key safety: Typing backspaces when buffer is empty or rapid arrow navigation when list is empty must not cause underflow or out-of-bounds array access.
3. Desktop entry exec parsing: Safely sanitize `Exec=` lines (stripping `%u`, `%F`, etc.) and ensure child process launching uses `fork() + setsid() + execl()` without leaving zombie processes.
4. Window focus & outside click dismissal: Clicking outside the launcher or losing focus must cleanly close the launcher and restore input focus to the active client.
5. Complete Launchpad removal: Ensure no orphaned header includes, stale test files, or dead variables remain.

---

### Task 1: Rofi Launcher Core Engine & Desktop App Scanner
**Files:**
- Create: `include/rofi_launcher.h`
- Create: `src/rofi_launcher.c`
- Test: `tests/test_rofi_launcher.c`

- [ ] **Step 1.1**: Write failing unit test `tests/test_rofi_launcher.c` verifying app scanning, filtering logic (`rofi_launcher_filter`), navigation (`rofi_launcher_select_next`, `rofi_launcher_select_prev`), and query editing (append char, backspace).
- [ ] **Step 1.2**: Define data structures and API signatures in `include/rofi_launcher.h`.
- [ ] **Step 1.3**: Implement `src/rofi_launcher.c` with application directory scanner (`/usr/share/applications` & `~/.local/share/applications`), fuzzy/substring search matcher, and query manipulation.
- [ ] **Step 1.4**: Compile and run `tests/test_rofi_launcher.c` to verify engine passes.
- [ ] **Step 1.5**: Commit Task 1.

---

### Task 2: Cairo Vector UI Rendering & Interactive Navigation with Smooth Animation
**Files:**
- Modify: `include/rofi_launcher.h`
- Modify: `src/rofi_launcher.c`
- Modify: `tests/test_rofi_launcher.c`

- [ ] **Step 2.1**: Implement `rofi_launcher_render()` and smooth open animation in `src/rofi_launcher.c`:
  - Centered modal window ($640 \times 440\text{px}$) with sleek dark background (`#121214`), subtle 1px border (`#323238`), and $r=12\text{px}$ pill geometry.
  - Smooth 80-100ms slide-up & ease-out cubic micro-animation on open ($y_{\text{start}} = y_{\text{final}} + 24\text{px} \to y_{\text{final}}$) ensuring high responsiveness with zero visual lag.
  - Search input box with prompt symbol, live text query, and blinking/rendered cursor.
  - Scrolling vertical list of matching apps (up to 6 visible rows).
  - High-res 32x32 icon, application name (bold, bright), and subtitle/comment (subtle gray).
  - Selected row highlight pill background (`rgba(50, 100, 200, 0.45)`).
- [ ] **Step 2.2**: Implement `rofi_launcher_handle_key()`, `rofi_launcher_handle_click()`, and `rofi_launcher_handle_motion()`:
  - Key events: `Up`/`Down`, `Ctrl+P`/`Ctrl+N`, `Return` (execute), `Escape` (close), typing (append), `BackSpace`.
  - Mouse events: hover highlight, click execute, outside click dismiss.
- [ ] **Step 2.3**: Expand `tests/test_rofi_launcher.c` to test hit testing, selection scrolling bounds, and key handling.
- [ ] **Step 2.4**: Run tests and verify clean pass.
- [ ] **Step 2.5**: Commit Task 2.

---

### Task 3: Retirement of Legacy Launchpad & Easter Egg
**Files:**
- Remove: `src/launchpad.c`
- Remove: `include/launchpad.h`
- Remove: `src/easter_egg.c`
- Remove: `include/easter_egg.h`
- Remove: `tests/test_calendar.c`
- Remove: `tests/test_easter_egg.c`
- Modify: `Makefile`
- Modify: `tests/test_runner.sh`

- [ ] **Step 3.1**: Remove legacy Launchpad and Easter Egg source/header files.
- [ ] **Step 3.2**: Update `Makefile` to remove `launchpad.o`, `easter_egg.o`, `test_calendar`, and `test_easter_egg`, and add `rofi_launcher.o` and `test_rofi_launcher`.
- [ ] **Step 3.3**: Update `tests/test_runner.sh` to run `test_rofi_launcher` instead of `test_calendar` and `test_easter_egg`.
- [ ] **Step 3.4**: Commit Task 3.

---

### Task 4: Main WM Integration (Single Super Press & Dock Apps Button)
**Files:**
- Modify: `src/main.c`
- Modify: `src/dock.c`
- Modify: `include/dock.h`

- [ ] **Step 4.1**: Update `include/dock.h` and `src/dock.c` to take `RofiLauncher *` instead of `Launchpad *` and invoke `rofi_launcher_toggle()` on Item 3 ("Apps").
- [ ] **Step 4.2**: Update `src/main.c`:
  - Initialize `RofiLauncher rofi_launcher;` with `rofi_launcher_init()`.
  - Handle single Super keypress: when `ev.xkey.keycode == super_kc`, toggle `rofi_launcher`.
  - Route keyboard and mouse events to `rofi_launcher` when visible.
  - Teardown cleanly in `main()` with `rofi_launcher_cleanup()`.
- [ ] **Step 4.3**: Compile full project with `make clean && make`.
- [ ] **Step 4.4**: Commit Task 4.

---

### Task 5: Native Zero-Overhead Wallpaper Engine
**Files:**
- Create: `include/wallpaper.h`
- Create: `src/wallpaper.c`
- Test: `tests/test_wallpaper.c`

- [ ] **Step 5.1**: Write failing test `tests/test_wallpaper.c` verifying `wallpaper_parse_waypaper_config()` and `wallpaper_set_root()`.
- [ ] **Step 5.2**: Implement `include/wallpaper.h` and `src/wallpaper.c`:
  - INI parser reading `wallpaper` from `~/.config/waypaper/config.ini` or fallback path.
  - Cairo surface loader scaling image to screen aspect ratio (`cairo_scale()`).
  - Native X11 root pixmap creation (`XCreatePixmap`), drawing to pixmap, setting `_XROOTPMAP_ID` and `ESETROOT_PMAP_ID` atoms, and applying `XSetWindowBackgroundPixmap(dpy, root, pixmap)`.
  - Zero background daemon requirement: Xorg holds pixmap directly with 0% CPU overhead.
- [ ] **Step 5.3**: Wire `wallpaper_init(dpy, screen_w, screen_h)` into `src/main.c`.
- [ ] **Step 5.4**: Run `test_wallpaper` and verify clean pass.
- [ ] **Step 5.5**: Commit Task 5.

---

### Task 6: End-to-End Verification, Memory Footprint & Xephyr Stress Test
**Files:**
- Test: `tests/test_runner.sh`
- Test: `tests/test_leak_and_race.sh`

- [ ] **Step 6.1**: Run `make tests && bash tests/test_runner.sh` to verify all unit tests pass and RSS is well below ceiling.
- [ ] **Step 6.2**: Update `tests/test_leak_and_race.sh` to test single Super toggle instead of Launchpad hot corner.
- [ ] **Step 6.3**: Run `bash tests/test_leak_and_race.sh` in Xephyr to verify 0 crashes, 0 leaks, and RSS < 30 MB.
- [ ] **Step 6.4**: Commit Task 6.
