#ifndef MUZU_DOCK_H
#define MUZU_DOCK_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "gtk_theme.h"

typedef enum { DOCK_BOTTOM, DOCK_TOP, DOCK_LEFT, DOCK_RIGHT } DockPosition;

typedef struct {
    Window win;
    DockPosition pos;
    int size;           // Thickness (e.g. 48px)
    int is_hidden;      // 1 if auto-hidden
    int hover_state;
    int x, y, width, height;
    cairo_surface_t *surface;
    const GtkTheme *theme;
} Dock;

void dock_calculate_geometry(Dock *dock, DockPosition pos, int is_hidden, int screen_w, int screen_h);
void dock_init(Display *dpy, Dock *dock, DockPosition pos, int screen_w, int screen_h, const GtkTheme *theme);
void dock_render(Display *dpy, Dock *dock, int screen_w, int screen_h);
void dock_handle_motion(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h);
void dock_set_position(Display *dpy, Dock *dock, DockPosition new_pos, int screen_w, int screen_h);
void dock_cleanup(Dock *dock);

#endif /* MUZU_DOCK_H */
