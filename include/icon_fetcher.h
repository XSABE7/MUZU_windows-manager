#ifndef MUZU_ICON_FETCHER_H
#define MUZU_ICON_FETCHER_H

#include <cairo/cairo.h>
#include <stddef.h>

// Resolve an icon name or path to an existing PNG file path
// Returns 1 if found, 0 otherwise
int icon_find_png(const char *icon_name, char *out_path, size_t maxlen);

// Load an icon into a Cairo image surface (PNG)
// Returns valid cairo_surface_t * or NULL if not found/error
cairo_surface_t *icon_load_surface(const char *icon_name);

// Render an icon onto a Cairo context at (x, y) with size (size x size)
// If icon is NULL, renders a stylish fallback icon with the initial letter of fallback_name
void icon_render(cairo_t *cr, cairo_surface_t *icon, const char *fallback_name, int x, int y, int size);

#endif // MUZU_ICON_FETCHER_H
