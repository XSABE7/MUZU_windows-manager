#ifndef MUZU_ICON_FETCHER_H
#define MUZU_ICON_FETCHER_H

#include <cairo/cairo.h>
#include <stddef.h>

// Set the active icon theme (e.g. "WhiteSur-dark", "Adwaita", "hicolor")
void icon_theme_set(const char *theme_name);

// Resolve an icon name using the active Freedesktop icon theme hierarchy (supports PNG, SVG, XPM, WebP, JPEG, etc.)
int icon_find_themed_png(const char *icon_name, char *out_path, size_t maxlen);
int icon_find_themed(const char *icon_name, char *out_path, size_t maxlen);

// Fallback search without theme
int icon_find_png(const char *icon_name, char *out_path, size_t maxlen);

// Load an icon into a Cairo image surface (supports PNG, SVG, XPM, WebP, JPEG, etc.)
cairo_surface_t *icon_load_surface(const char *icon_name);

// Create a Cairo ARGB32 image surface from EWMH _NET_WM_ICON property data
cairo_surface_t *icon_create_from_net_wm_icon(const unsigned long *data, unsigned long nitems, int target_size);

// Render an icon onto a Cairo context at (x, y) with size (size x size)
void icon_render(cairo_t *cr, cairo_surface_t *icon, const char *fallback_name, int x, int y, int size);

#endif // MUZU_ICON_FETCHER_H
