# Dock Magnification, Drag-and-Drop Pinning, and Context Menus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement macOS-style dock magnification, Rofi-to-Dock drag-and-drop app pinning, default empty dock (only 3 dots), and right-click context menus for both Dock customization and Desktop Wallpaper path setting.

**Architecture:** Build a reusable Cairo-based popup `Menu` subsystem (`menu.h`/`menu.c`). Upgrade `dock.c` with default single-item layout (3 dots "Apps"), macOS magnification wave calculation, and right-click Dock decoration menu. Add mouse drag detection to `rofi_launcher.c` to drop apps onto `dock.win`. Wire right-click on root window to show Desktop Wallpaper context menu.

**Tech Stack:** C11 (`-std=c11 -Wall -Wextra -pedantic`), Xlib, Cairo vector graphics, Waypaper config parser.

**Spec:** [docs/superpowers/specs/2026-10-20-dock-magnification-dnd-context-menus.md](file:///home/peach/git/MUZUwm/docs/superpowers/specs/2026-10-20-dock-magnification-dnd-context-menus.md)

## Global Constraints
- Pure C11 with zero compiler warnings (`-Wall -Wextra -pedantic -std=c11 -O2`).
- Total WM RSS memory footprint under 20 MB (< 30 MB budget).
- Magnification effect must be smooth and glitch-free, with fallback to normal scaling if disabled.
- Pinned dock apps must persist to disk (`~/.config/muzuwm/dock_apps.ini`).
- Clean menu dismissals when clicking outside or pressing Escape.

## Review Focus
1. Magnification bounds and hit testing: Ensure clicking magnified icons activates the exact intended icon even when width/positions dynamically expand.
2. Drag-and-Drop lifecycle from Rofi: Ensure drag-out does not freeze pointer grab if released outside both Rofi and Dock.
3. Menu lifecycle & outside clicks: Clicking anywhere outside a context menu must dismiss it without swallowing clicks intended for clients.
4. Dock item persistence: Malformed or unreadable `dock_apps.ini` must not crash or leak memory.
5. Desktop right-click on multi-monitor / root edges: Ensure menu stays clamped within screen boundaries.

---

### Task 1: Reusable Context Menu Subsystem
**Files:**
- Create: `include/menu.h`
- Create: `src/menu.c`
- Test: `tests/test_menu.c`

- [ ] **Step 1.1**: Write failing unit test `tests/test_menu.c` verifying item addition, hit testing, and action dispatch.
- [ ] **Step 1.2**: Implement `include/menu.h` and `src/menu.c` with sleek dark Cairo rendering, border radius, hover highlight, and keyboard/mouse event handling.
- [ ] **Step 1.3**: Compile and run `tests/test_menu.c` to verify passing.
- [ ] **Step 1.4**: Commit Task 1.

---

### Task 2: Minimal Dock (Only 3 Dots) & Dynamic Item Pinning / Persistence
**Files:**
- Modify: `include/dock.h`
- Modify: `src/dock.c`
- Test: `tests/test_layout.c`

- [ ] **Step 2.1**: Update `dock_init()` to initialize with only 1 item: Item 0 = "Apps" (three dots icon `...`).
- [ ] **Step 2.2**: Implement `dock_add_app()`, `dock_remove_app()`, `dock_load_pinned_apps()`, and `dock_save_pinned_apps()` in `src/dock.c`.
- [ ] **Step 2.3**: Update `tests/test_layout.c` to reflect default 1-item dock and test dynamic app addition/removal.
- [ ] **Step 2.4**: Verify tests pass.
- [ ] **Step 2.5**: Commit Task 2.

---

### Task 3: macOS-Style Dock Magnification Wave Effect
**Files:**
- Modify: `include/dock.h`
- Modify: `src/dock.c`
- Test: `tests/test_layout.c`

- [ ] **Step 3.1**: Add `magnification_enabled` flag (default 1) and calculate dynamic scale factors per icon using cosine wave falloff ($s = 1.0 \to 1.45$).
- [ ] **Step 3.2**: Update `dock_render()` and `dock_get_item_at()` to render and hit-test magnified icons centered within the dock geometry.
- [ ] **Step 3.3**: Add unit tests in `tests/test_layout.c` verifying magnification math and boundary hit testing.
- [ ] **Step 3.4**: Commit Task 3.

---

### Task 4: Drag & Drop App from Rofi to Dock
**Files:**
- Modify: `include/rofi_launcher.h`
- Modify: `src/rofi_launcher.c`
- Modify: `src/main.c`

- [ ] **Step 4.1**: In `rofi_launcher.c`, detect drag threshold (> 8px) on `ButtonPress` + `MotionNotify` to enter drag-to-pin mode.
- [ ] **Step 4.2**: In `src/main.c`, when dragging an app from Rofi and releasing over `dock.win`, call `dock_add_app()` to pin the app and dismiss Rofi.
- [ ] **Step 4.3**: Commit Task 4.

---

### Task 5: Dock Right-Click Decoration Menu & Desktop Wallpaper Menu
**Files:**
- Modify: `src/dock.c`
- Modify: `src/wallpaper.c`
- Modify: `include/wallpaper.h`
- Modify: `src/main.c`

- [ ] **Step 5.1**: On Dock right-click (`Button3`), display the Dock Context Menu:
  - Toggle Magnification (On/Off)
  - Change Position (Bottom, Left, Top, Right)
  - Change Size (Small, Medium, Large)
  - Toggle Auto-Hide (On/Off)
  - Move Dock (starts drag)
  - If right-clicking pinned app: "Remove from Dock"
- [ ] **Step 5.2**: On empty Desktop root window right-click (`Button3`), display the Desktop Context Menu:
  - "Change Wallpaper..." (runs `waypaper` or opens wallpaper prompt)
  - "Open Terminal"
  - "Applications"
- [ ] **Step 5.3**: Wire menu events in `src/main.c` event loop.
- [ ] **Step 5.4**: Commit Task 5.

---

### Task 6: End-to-End Test Suite, Xephyr Verification & Memory Benchmark
**Files:**
- Modify: `Makefile`
- Test: `tests/test_runner.sh`
- Test: `tests/test_leak_and_race.sh`

- [ ] **Step 6.1**: Update `Makefile` with `src/menu.c` and `test_menu`.
- [ ] **Step 6.2**: Run `make tests && bash tests/test_runner.sh` ensuring all tests pass and RSS < 20 MB.
- [ ] **Step 6.3**: Run `bash tests/test_leak_and_race.sh` verifying 0 crashes and 0 leaks.
- [ ] **Step 6.4**: Commit Task 6.
