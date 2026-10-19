# MUZUwm Real Icon Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement real desktop application icon resolution, loading, and rendering in MUZUwm Launchpad (and Dock) using Cairo PNG surfaces, inspired by Rofi's icon fetcher.

**Architecture:** Create `include/icon_fetcher.h` and `src/icon_fetcher.c` implementing XDG standard icon search (pixmaps, hicolor, gnome, user local), Cairo PNG surface creation (`cairo_image_surface_create_from_png`), aspect-preserving rendering with scaling, and procedural fallback letter icons. Update `AppItem` in `include/launchpad.h` to store parsed `Icon=` and cached `cairo_surface_t *icon_surface`.

**Tech Stack:** C11, Xlib, Cairo (`cairo-xlib`, libpng native in Cairo), POSIX.

**Spec:** Adapted from Rofi's `rofi-icon-fetcher.c` and `MUZUwm.md`.

## Global Constraints

- Must stay in pure C11 with Cairo and Xlib.
- Total memory footprint must remain under 200 MB (measured < 10 MB RSS).
- All unit tests and benchmarks must pass cleanly.
- Preserves 1366x768 layout responsiveness.

## Review Focus

1. **Non-Existent / Malformed Icons:** If `Icon=` contains a non-existent name or invalid PNG, must fall back cleanly without crashing or aborting.
2. **Surface Memory Leak:** When Launchpad rescan or shutdown occurs, cached Cairo surfaces must be cleanly freed via `cairo_surface_destroy`.
3. **Aspect Ratio & Boundary Clipping:** Icons of varying source resolutions (e.g. 16x16, 48x48, 256x256) must scale proportionally to fit exactly inside the 38x38 app icon slot.
4. **NoDisplay Filtering:** System `.desktop` files marked with `NoDisplay=true` or `Hidden=true` must be omitted from the app grid.
5. **Fast Startup:** Path resolution must stop immediately on the first found file without scanning unnecessary directories.

---

### Task 1: Icon Fetcher Module & Unit Tests

**Files:**
- Create: `include/icon_fetcher.h`
- Create: `src/icon_fetcher.c`
- Create: `tests/test_icon_fetcher.c`
- Modify: `Makefile`

- [ ] **Step 1: Write failing unit test for `icon_fetcher`**
  - Test `icon_find_png()` with absolute paths, nonexistent names, and standard system icon names.
  - Test fallback icon rendering helper.
  - Compile with new test target in Makefile.

- [ ] **Step 2: Implement `src/icon_fetcher.c` and `include/icon_fetcher.h`**
  - Implement `icon_find_png(const char *icon_name, char *out_path, size_t maxlen)`.
  - Implement `icon_load_surface(const char *icon_name)`.
  - Implement `icon_render(cairo_t *cr, cairo_surface_t *icon, const char *fallback_name, int x, int y, int size)`.

- [ ] **Step 3: Run unit tests and verify pass**
  - Run `./test_icon_fetcher` and ensure assertions pass.

- [ ] **Step 4: Commit Task 1 changes**
  - Git commit: `feat: implement icon_fetcher module with XDG search and Cairo rendering`

---

### Task 2: Launchpad Integration with Real Icons & NoDisplay Filtering

**Files:**
- Modify: `include/launchpad.h:12-25,40-50`
- Modify: `src/launchpad.c:66-107,175-210,390-410`
- Modify: `Makefile`

- [ ] **Step 1: Update `AppItem` struct and `.desktop` scanner**
  - Add `char icon[128]` and `cairo_surface_t *icon_surface` to `AppItem`.
  - In `launchpad_scan_apps()`, parse `Icon=` from `.desktop`.
  - Filter out files with `NoDisplay=true` or `Hidden=true`.
  - Load and cache `cairo_surface_t *` for each scanned app via `icon_load_surface()`.

- [ ] **Step 2: Update Launchpad rendering**
  - In `launchpad_render()`, call `icon_render()` at `(gx, gy, 38)` for each app item.
  - Clean up surfaces in `launchpad_cleanup()` or re-scan.

- [ ] **Step 3: Verify build and unit tests**
  - Run `make all test_calendar test_icon_fetcher`.
  - Verify clean compilation with zero warnings.

- [ ] **Step 4: Commit Task 2 changes**
  - Git commit: `feat(launchpad): render real application icons and filter NoDisplay apps`

---

### Task 3: Full Test Suite Verification & RAM Benchmark

**Files:**
- Modify: `tests/test_runner.sh`
- Modify: `.gitignore`

- [ ] **Step 1: Update `tests/test_runner.sh` to include `test_icon_fetcher`**
- [ ] **Step 2: Add `test_icon_fetcher` to `.gitignore`**
- [ ] **Step 3: Run all tests and measure memory consumption**
  - Verify unit tests pass: `test_layout`, `test_trigger`, `test_easter_egg`, `test_calendar`, `test_client_props`, `test_icon_fetcher`.
  - Verify RSS memory remains well under 200 MB (target < 10 MB).
- [ ] **Step 4: Commit Task 3 changes**
  - Git commit: `test: add test_icon_fetcher to test runner and update gitignore`
