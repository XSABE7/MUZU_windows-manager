#include <stdio.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include "dock.h"

void dock_calculate_geometry(Dock *dock, DockPosition pos, int is_hidden, int screen_w, int screen_h) {
    if (!dock) return;
    if (dock->size <= 0) dock->size = 48;
    dock->pos = pos;
    dock->is_hidden = is_hidden;

    switch (pos) {
        case DOCK_BOTTOM:
            dock->width = screen_w * 0.40;
            dock->height = dock->size;
            dock->x = (screen_w - dock->width) / 2;
            dock->y = is_hidden ? screen_h - 4 : screen_h - dock->height - 6;
            break;
        case DOCK_TOP:
            dock->width = screen_w * 0.40;
            dock->height = dock->size;
            dock->x = (screen_w - dock->width) / 2;
            dock->y = is_hidden ? -dock->height + 4 : 6;
            break;
        case DOCK_LEFT:
            dock->width = dock->size;
            dock->height = screen_h * 0.50;
            dock->x = is_hidden ? -dock->width + 4 : 6;
            dock->y = (screen_h - dock->height) / 2;
            break;
        case DOCK_RIGHT:
            dock->width = dock->size;
            dock->height = screen_h * 0.50;
            dock->x = is_hidden ? screen_w - 4 : screen_w - dock->width - 6;
            dock->y = (screen_h - dock->height) / 2;
            break;
    }
}

void dock_init(Display *dpy, Dock *dock, DockPosition pos, int screen_w, int screen_h) {
    dock->size = 48;
    dock->is_hidden = 0;
    dock->hover_state = 0;
    dock_calculate_geometry(dock, pos, 0, screen_w, screen_h);

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = BlackPixel(dpy, screen);
    attrs.event_mask = ExposureMask | EnterWindowMask | LeaveWindowMask | ButtonPressMask;

    dock->win = XCreateWindow(dpy, root, dock->x, dock->y, dock->width, dock->height, 0,
                              DefaultDepth(dpy, screen), InputOutput,
                              DefaultVisual(dpy, screen),
                              CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);

    XMapRaised(dpy, dock->win);
    dock_render(dpy, dock, screen_w, screen_h);
}

void dock_render(Display *dpy, Dock *dock, int screen_w, int screen_h) {
    (void)screen_w;
    (void)screen_h;
    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);

    cairo_surface_t *surface = cairo_xlib_surface_create(dpy, dock->win, visual, dock->width, dock->height);
    cairo_t *cr = cairo_create(surface);

    // Pill container background
    cairo_set_source_rgba(cr, 0.12, 0.15, 0.20, 0.92);
    cairo_rectangle(cr, 0, 0, dock->width, dock->height);
    cairo_fill(cr);

    // Subtle border
    cairo_set_source_rgba(cr, 0.30, 0.38, 0.50, 0.80);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, 0, 0, dock->width, dock->height);
    cairo_stroke(cr);

    // Render sample favorite app slots (Files, Terminal, Browser, Settings)
    int icon_size = 32;
    int margin = 8;
    int icons = 4;
    int start_x = (dock->width - (icons * (icon_size + margin))) / 2;

    double colors[4][3] = {
        {0.25, 0.55, 0.95}, // Web
        {0.20, 0.75, 0.50}, // Files
        {0.45, 0.45, 0.90}, // Terminal
        {0.90, 0.60, 0.20}  // Settings
    };

    for (int i = 0; i < icons; i++) {
        cairo_set_source_rgb(cr, colors[i][0], colors[i][1], colors[i][2]);
        cairo_rectangle(cr, start_x + i * (icon_size + margin), (dock->height - icon_size) / 2, icon_size, icon_size);
        cairo_fill(cr);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    XFlush(dpy);
}

void dock_handle_motion(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h) {
    (void)x;
    (void)y;
    // When hovering near trigger boundary, reveal dock
    if (dock->is_hidden) {
        dock_calculate_geometry(dock, dock->pos, 0, screen_w, screen_h);
        XMoveWindow(dpy, dock->win, dock->x, dock->y);
    }
}

void dock_set_position(Display *dpy, Dock *dock, DockPosition new_pos, int screen_w, int screen_h) {
    dock_calculate_geometry(dock, new_pos, dock->is_hidden, screen_w, screen_h);
    XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
    dock_render(dpy, dock, screen_w, screen_h);
}
