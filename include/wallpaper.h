#ifndef MUZU_WALLPAPER_H
#define MUZU_WALLPAPER_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>

// Parse wallpaper path from ~/.config/waypaper/config.ini or ~/.config/muzuwm/wallpaper.png
// Returns 1 if valid wallpaper path found, 0 if not found
int wallpaper_get_configured_path(char *path_out, size_t max_len);

// Set root window background to image at image_path (PNG supported) or fallback dark gradient
// Sets _XROOTPMAP_ID and ESETROOT_PMAP_ID atoms for desktop compositor compatibility
// Returns 1 on success, 0 on failure
int wallpaper_set_root(Display *dpy, int screen_w, int screen_h, const char *image_path);

// Initialize wallpaper on WM startup (reads config, sets root pixmap, 0% background overhead)
void wallpaper_init(Display *dpy, int screen_w, int screen_h);

#endif /* MUZU_WALLPAPER_H */
