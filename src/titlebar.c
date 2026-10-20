#include <stdio.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include "titlebar.h"

Window titlebar_create(Display *dpy, Window client_win, int width) {
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(dpy, screen);
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | Button1MotionMask;

    Window frame = XCreateWindow(dpy, root, 0, 0, width, TITLEBAR_HEIGHT, 0,
                                 DefaultDepth(dpy, screen), InputOutput,
                                 DefaultVisual(dpy, screen),
                                 CWBackPixel | CWEventMask, &attrs);

    XReparentWindow(dpy, client_win, frame, 0, TITLEBAR_HEIGHT);
    XMapWindow(dpy, frame);
    return frame;
}

void titlebar_draw(Display *dpy, Client *c) {
    if (!dpy || !c || c->frame == None) return;

    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);
    int width = c->width > 0 ? c->width : 100;

    if (!c->titlebar_surface) {
        c->titlebar_surface = cairo_xlib_surface_create(dpy, c->frame, visual, width, TITLEBAR_HEIGHT);
    } else {
        cairo_xlib_surface_set_size(c->titlebar_surface, width, TITLEBAR_HEIGHT);
    }

    cairo_t *cr = cairo_create(c->titlebar_surface);

    // Background: elegant dark modern grey
    cairo_set_source_rgb(cr, 0.12, 0.14, 0.18);
    cairo_paint(cr);

    // Title text (clipped to prevent overlapping close button)
    cairo_save(cr);
    cairo_rectangle(cr, 12, 0, (width > 48 ? width - 40 : 10), TITLEBAR_HEIGHT);
    cairo_clip(cr);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.90, 0.92, 0.95);
    cairo_move_to(cr, 12, 16);
    cairo_show_text(cr, c->title[0] ? c->title : "MUZUwm Window");
    cairo_restore(cr);

    // Close button (red dot on top-right)
    cairo_set_source_rgb(cr, 0.94, 0.27, 0.24);
    cairo_arc(cr, width - 16, TITLEBAR_HEIGHT / 2, 5.0, 0, 2 * 3.14159);
    cairo_fill(cr);

    cairo_destroy(cr);
    cairo_surface_flush(c->titlebar_surface);
    XFlush(dpy);
}

void titlebar_cleanup(Client *c) {
    if (!c) return;
    if (c->titlebar_surface) {
        cairo_surface_finish(c->titlebar_surface);
        cairo_surface_destroy(c->titlebar_surface);
        c->titlebar_surface = NULL;
    }
}

void handle_titlebar_drag(Display *dpy, Client *c, int root_x, int root_y, int screen_w, int screen_h) {
    if (!c) return;
    (void)screen_w;
    (void)screen_h;

    // Detach to floating if currently tiled
    if (c->mode == CLIENT_TILED) {
        c->mode = CLIENT_FLOATING;
    }

    c->x = root_x;
    c->y = root_y;
    Window target = c->frame ? c->frame : c->win;
    XMoveWindow(dpy, target, c->x, c->y);
}

void handle_titlebar_double_click(Display *dpy, Client *c, LayoutConfig *cfg) {
    if (!c) return;
    (void)dpy;
    if (c->mode == CLIENT_FLOATING) {
        c->mode = CLIENT_TILED;
    } else {
        c->mode = CLIENT_FLOATING;
        c->x = cfg->screen_width / 4;
        c->y = cfg->screen_height / 4;
        c->width = cfg->screen_width / 2;
        c->height = cfg->screen_height / 2;
    }
}
