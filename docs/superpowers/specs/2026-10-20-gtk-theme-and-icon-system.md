# MUZUwm GTK Theme & Icon Integration Specification
**Date:** 2026-10-20  
**Status:** Approved by Human Partner  
**Topic:** GTK Theming, Freedesktop Icon Theme Resolution, and GTK App Presentation in MUZUwm  

---

## 1. Intent & The Why

MUZUwm is designed as a modern, mouse-first, lightweight X11 window manager. To provide a cohesive and polished desktop experience comparable to full desktop environments without their heavy resource overhead, MUZUwm must:
1. **Respect and Apply GTK Theming:** Automatically load the user's GTK theme, color scheme (dark/light), and font configuration from standard Freedesktop / GNOME configuration files (`~/.config/gtk-3.0/settings.ini`, `~/.config/gtk-4.0/settings.ini`, and GSettings).
2. **Freedesktop Icon Theme Standard:** Replace hardcoded icon search paths with a full icon theme hierarchy resolver that respects `gtk-icon-theme-name` (e.g. `WhiteSur-dark`, `Adwaita`, `Papirus`, `Colloid`), parses `index.theme` for inheritance chains (`Inherits=hicolor,...`), and searches scalable and fixed resolution directories (`apps/48`, `apps/scalable`, `apps/32`, etc.).
3. **Harmonious GTK App Presentation:** Ensure GTK apps spawned under MUZUwm (e.g. file managers, text editors, browsers) load with the matching GTK theme, icon theme, and cursor theme.

---

## 2. Requirements & Architecture

### 2.1 GTK Theme & Configuration Reader (`include/gtk_theme.h`, `src/gtk_theme.c`)
- **Config Sources (in priority order):**
  1. `~/.config/gtk-3.0/settings.ini` and `~/.config/gtk-4.0/settings.ini`
  2. `gsettings get org.gnome.desktop.interface ...` fallback
  3. Safe defaults: `gtk-theme-name = Adwaita-dark`, `gtk-icon-theme-name = hicolor`, `gtk-font-name = Sans 10`.
- **Properties Read:**
  - `theme_name`: Active GTK theme (e.g. `WhiteSur-Dark`, `Orchis-Dark`, `Adwaita`).
  - `icon_theme_name`: Active Icon theme (e.g. `WhiteSur-dark`, `Colloid`, `Adwaita`).
  - `font_name`: UI font name and size (e.g. `Sans 10`, `Cantarell 11`).
  - `is_dark`: Boolean indicator for dark mode (detected via theme name suffix `-dark`/`-Dark`, `prefer-dark`, or `gtk-application-prefer-dark-theme=1`).
- **Color Palette:**
  - `bg_titlebar`: RGB color for window titlebars (e.g. dark: `0.12, 0.14, 0.18`; light: `0.93, 0.94, 0.96`).
  - `fg_titlebar`: RGB color for title text.
  - `accent_color`: Accent color for buttons, highlights, calendar active days.
  - `bg_surface`: Background for Launchpad overlay and Dock container.
  - `border_color`: Subtle separator and border color.
- **Environment Export for GTK Apps:**
  - Set `GTK_THEME` environment variable so all launched GTK apps immediately inherit the active theme.

### 2.2 Freedesktop Icon Theme Resolver (`include/icon_fetcher.h`, `src/icon_fetcher.c`)
- **Theme Search Bases:**
  - `~/.local/share/icons/<theme>`
  - `~/.icons/<theme>`
  - `/usr/share/icons/<theme>`
- **Inheritance (`index.theme`):**
  - Read `Inherits=` from `index.theme` to construct an ordered fallback chain (e.g. `WhiteSur-dark` -> `hicolor`).
  - `hicolor` is always appended as the ultimate Freedesktop fallback.
- **Subdirectory Search Order:**
  - Check `apps/48`, `apps/48x48`, `apps/scalable`, `apps/64`, `apps/32`, `apps/128`, `apps/256`, `apps/16`.
  - Fallback to `/usr/share/pixmaps`.
- **Surface Loading:**
  - PNG icons loaded directly into `cairo_surface_t` with `cairo_image_surface_create_from_png()`.
  - Scalable / Missing icons generate an elegant procedural letter badge matching theme accent colors.

### 2.3 Window Manager UI Theme Integration
- **Titlebars (`src/titlebar.c`):** Use `gtk_theme->bg_titlebar`, `gtk_theme->fg_titlebar`, and theme font.
- **Launchpad (`src/launchpad.c`):** Use `icon_fetcher` configured with active GTK icon theme, theme font, and theme accent colors.
- **Dock (`src/dock.c`):** Use theme background, border, and accent colors.

---

## 3. Constraints & Budget
- **Memory Footprint:** MUZUwm must remain under **30 MB RSS** (total WM + picom < 200 MB).
- **Pure C11:** Zero heavyweight dependencies (no libglib/libgtk linking directly into WM binary; INI parser and Freedesktop resolver written in pure C11 with libcairo and libX11).
