# GTK Theme & Freedesktop Icon System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a lightweight, zero-dependency GTK theming and Freedesktop icon theme resolution system in MUZUwm, allowing window titlebars, dock, launchpad, and launched GTK apps to look unified and visually cohesive.

**Architecture:** Pure C11 INI parser and Freedesktop icon theme hierarchy traverser (`index.theme` inheritance + multi-size resolution). Theme colors and fonts dynamically populate a `GtkTheme` palette used by Cairo rendering across all UI elements, while exporting `GTK_THEME` to ensure child GTK applications render with consistent theme and icons.

**Tech Stack:** C11, Xlib, Cairo, Pkg-config, Fontconfig.

**Spec:** [docs/superpowers/specs/2026-10-20-gtk-theme-and-icon-system.md](file:///home/peach/git/MUZUwm/docs/superpowers/specs/2026-10-20-gtk-theme-and-icon-system.md)

## Global Constraints

- **Language & Standard:** Strict C11 (`-std=c11 -Wall -Wextra -pedantic`).
- **Dependencies:** Pure C11 with libcairo, libX11, libfontconfig. No heavy GLib or GTK runtime library dependencies in the WM binary.
- **Memory Ceiling:** Total MUZUwm RSS memory must remain < 30 MB.
- **Responsiveness:** Icon lookup must be cached or fast, zero UI stutter.

## Review Focus

1. **Missing or Corrupt INI Files:** If `settings.ini` is missing, unreadable, or contains empty values, fallback gracefully to `Adwaita-dark` and `hicolor`.
2. **Circular `Inherits=` in `index.theme`:** Icon theme inheritance chain must guard against cycles (e.g. Theme A inherits B, B inherits A) with max depth limit.
3. **Missing Theme Directory:** If the configured GTK theme or icon theme does not exist on disk, fallback smoothly to standard system icons and default dark palette.
4. **Non-Standard Font Strings:** `gtk-font-name` strings like `"Cantarell 11"` or `"Ubuntu Bold 10"` must be safely parsed into font family and size without buffer overflows.
5. **Memory Leaks in Icon Theme Chain:** Dynamically allocated icon theme inheritance lists must be cleanly freed on cleanup.

---

### Task 1: GTK Theme & Settings Parser

**Files:**
- Create: `include/gtk_theme.h`
- Create: `src/gtk_theme.c`
- Create: `tests/test_gtk_theme.c`
- Modify: `Makefile`

**Interfaces:**
- Produces:
  ```c
  typedef struct {
      double r, g, b, a;
  } ColorRGBA;

  typedef struct {
      char theme_name[128];
      char icon_theme_name[128];
      char font_family[64];
      double font_size;
      int is_dark;
      ColorRGBA bg_titlebar;
      ColorRGBA fg_titlebar;
      ColorRGBA accent_color;
      ColorRGBA bg_surface;
      ColorRGBA border_color;
  } GtkTheme;

  void gtk_theme_init(GtkTheme *theme);
  void gtk_theme_load_from_file(GtkTheme *theme, const char *filepath);
  void gtk_theme_export_env(const GtkTheme *theme);
  ```

- [ ] **Step 1: Write failing unit test `tests/test_gtk_theme.c`**
  Test INI parsing of `gtk-theme-name`, `gtk-icon-theme-name`, `gtk-font-name`, dark mode detection, and color palette generation.
- [ ] **Step 2: Run test to verify it fails**
  Run: `make test_gtk_theme && ./test_gtk_theme` (Expected: FAIL - file not found)
- [ ] **Step 3: Implement `include/gtk_theme.h` and `src/gtk_theme.c`**
  Implement lightweight INI reader for `settings.ini`, font parser, dark mode detector, and color palette generator.
- [ ] **Step 4: Run test to verify it passes**
  Run: `make test_gtk_theme && ./test_gtk_theme` (Expected: PASS)
- [ ] **Step 5: Commit**
  `git add include/gtk_theme.h src/gtk_theme.c tests/test_gtk_theme.c Makefile`
  `git commit -m "feat: add GTK theme settings parser and color palette loader"`

---

### Task 2: Freedesktop Icon Theme Resolver with Inheritance

**Files:**
- Modify: `include/icon_fetcher.h`
- Modify: `src/icon_fetcher.c`
- Modify: `tests/test_icon_fetcher.c`

**Interfaces:**
- Consumes: `icon_theme_name` from `GtkTheme`
- Produces:
  ```c
  void icon_theme_set(const char *theme_name);
  int icon_find_themed_png(const char *icon_name, char *out_path, size_t maxlen);
  ```

- [ ] **Step 1: Write test for themed icon lookup in `tests/test_icon_fetcher.c`**
  Add test verifying resolving icons using theme name (e.g. `WhiteSur-dark` or `hicolor`), parsing `index.theme` `Inherits=`, and falling back to standard sizes.
- [ ] **Step 2: Run test to verify it fails**
  Run: `make test_icon_fetcher && ./test_icon_fetcher` (Expected: FAIL)
- [ ] **Step 3: Implement icon theme hierarchy resolver in `src/icon_fetcher.c`**
  Parse `index.theme` for `Inherits=`, scan `~/.local/share/icons`, `~/.icons`, and `/usr/share/icons`, search `apps/48`, `apps/scalable`, `apps/32`, etc., and fallback to `/usr/share/pixmaps`.
- [ ] **Step 4: Run test to verify it passes**
  Run: `make test_icon_fetcher && ./test_icon_fetcher` (Expected: PASS)
- [ ] **Step 5: Commit**
  `git add include/icon_fetcher.h src/icon_fetcher.c tests/test_icon_fetcher.c`
  `git commit -m "feat: implement Freedesktop icon theme resolver with inheritance"`

---

### Task 3: Theming Integration in Titlebars, Launchpad, and Dock

**Files:**
- Modify: `include/titlebar.h`
- Modify: `src/titlebar.c`
- Modify: `include/launchpad.h`
- Modify: `src/launchpad.c`
- Modify: `include/dock.h`
- Modify: `src/dock.c`

**Interfaces:**
- Consumes: `GtkTheme` palette in titlebar, launchpad, and dock draw routines.

- [ ] **Step 1: Update Titlebars (`include/titlebar.h`, `src/titlebar.c`)**
  Use `theme->bg_titlebar` and `theme->fg_titlebar` instead of hardcoded dark grey.
- [ ] **Step 2: Update Launchpad (`include/launchpad.h`, `src/launchpad.c`)**
  Initialize `icon_theme_set(theme->icon_theme_name)` before app scanning. Use `theme->bg_surface`, `theme->accent_color`, and `theme->font_family` for digital clock, search input, and calendar.
- [ ] **Step 3: Update Dock (`include/dock.h`, `src/dock.c`)**
  Use `theme->bg_surface`, `theme->border_color`, and `theme->accent_color`.
- [ ] **Step 4: Run unit tests**
  Run: `make clean && make all test_layout test_trigger test_easter_egg test_calendar test_client_props test_icon_fetcher && bash tests/test_runner.sh`
- [ ] **Step 5: Commit**
  `git add include/titlebar.h src/titlebar.c include/launchpad.h src/launchpad.c include/dock.h src/dock.c`
  `git commit -m "feat: integrate GTK theme colors and typography across UI"`

---

### Task 4: Main WM Wiring, Environment Setup, and Verification

**Files:**
- Modify: `src/main.c`
- Modify: `Makefile`
- Modify: `tests/test_runner.sh`

**Interfaces:**
- Initialize `GtkTheme gtk_theme` in `main()`
- Export `gtk_theme_export_env(&gtk_theme)` so all child processes launched by MUZUwm inherit the GTK theme and icon theme.

- [ ] **Step 1: Wire `gtk_theme` in `src/main.c`**
  Call `gtk_theme_init(&gtk_theme)` and `gtk_theme_export_env(&gtk_theme)`.
- [ ] **Step 2: Update build and automated test suite**
  Update `Makefile` and `tests/test_runner.sh` to include `test_gtk_theme`.
- [ ] **Step 3: Run full verification suite and memory benchmark**
  Run all 7 unit tests and verify RSS < 30 MB.
- [ ] **Step 4: Test interactive GTK app in Xephyr**
  Run `./test_leak_and_race.sh` to verify zero regressions or memory leaks.
- [ ] **Step 5: Commit**
  `git add src/main.c Makefile tests/test_runner.sh`
  `git commit -m "feat: complete GTK theme and icon system integration in MUZUwm"`
