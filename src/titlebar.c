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

void titlebar_draw(Display *dpy, Client *c, const GtkTheme *theme) {
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

    // Background from GTK Theme (or fallback to dark grey)
    if (theme) {
        cairo_set_source_rgb(cr, theme->bg_titlebar.r, theme->bg_titlebar.g, theme->bg_titlebar.b);
    } else {
        cairo_set_source_rgb(cr, 0.12, 0.14, 0.18);
    }
    cairo_paint(cr);

    // Subtle bottom border
    if (theme) {
        cairo_set_source_rgba(cr, theme->border_color.r, theme->border_color.g, theme->border_color.b, 0.6);
    } else {
        cairo_set_source_rgba(cr, 0.25, 0.28, 0.35, 0.6);
    }
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, TITLEBAR_HEIGHT - 0.5);
    cairo_line_to(cr, width, TITLEBAR_HEIGHT - 0.5);
    cairo_stroke(cr);

    // Title text (shifted to right of traffic lights, clipped to width)
    cairo_save(cr);
    cairo_rectangle(cr, 62, 0, (width > 80 ? width - 72 : 10), TITLEBAR_HEIGHT);
    cairo_clip(cr);
    const char *font_fam = (theme && theme->font_family[0]) ? theme->font_family : "sans-serif";
    double font_sz = (theme && theme->font_size > 0.0) ? theme->font_size : 11.0;
    if (font_sz < 9.0) font_sz = 9.0;
    if (font_sz > 14.0) font_sz = 14.0;
    cairo_select_font_face(cr, font_fam, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, font_sz);
    if (theme) {
        cairo_set_source_rgb(cr, theme->fg_titlebar.r, theme->fg_titlebar.g, theme->fg_titlebar.b);
    } else {
        cairo_set_source_rgb(cr, 0.90, 0.92, 0.95);
    }
    cairo_move_to(cr, 64, 16);
    cairo_show_text(cr, c->title[0] ? c->title : "MUZUwm Window");
    cairo_restore(cr);

    // macOS Traffic Light Controls (Top-Left)
    // 1. Red (Close)
    cairo_set_source_rgb(cr, 1.00, 0.37, 0.34); // #FF5F56
    cairo_arc(cr, 14, TITLEBAR_HEIGHT / 2, 5.0, 0, 2 * 3.14159);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.80, 0.15, 0.15, 0.8);
    cairo_set_line_width(cr, 0.6);
    cairo_stroke(cr);

    // 2. Yellow (Minimize)
    cairo_set_source_rgb(cr, 1.00, 0.74, 0.18); // #FFBD2E
    cairo_arc(cr, 30, TITLEBAR_HEIGHT / 2, 5.0, 0, 2 * 3.14159);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.75, 0.50, 0.10, 0.8);
    cairo_set_line_width(cr, 0.6);
    cairo_stroke(cr);

    // 3. Green (Maximize)
    cairo_set_source_rgb(cr, 0.15, 0.79, 0.25); // #27C93F
    cairo_arc(cr, 46, TITLEBAR_HEIGHT / 2, 5.0, 0, 2 * 3.14159);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.10, 0.55, 0.15, 0.8);
    cairo_set_line_width(cr, 0.6);
    cairo_stroke(cr);

    cairo_destroy(cr);
    cairo_surface_flush(c->titlebar_surface);
    if (dpy) XFlush(dpy);
}

TitlebarAction titlebar_get_action(int click_x, int width) {
    (void)width;
    if (click_x < 0) return TITLEBAR_CLICK_NONE;
    if (click_x < 22) return TITLEBAR_CLICK_CLOSE;
    if (click_x < 38) return TITLEBAR_CLICK_MINIMIZE;
    if (click_x < 56) return TITLEBAR_CLICK_MAXIMIZE;
    return TITLEBAR_CLICK_DRAG;
}

void client_toggle_maximize(Display *dpy, Client *c, LayoutConfig *cfg) {
    if (!c || !cfg) return;

    if (!c->is_maximized) {
        c->saved_x = c->x;
        c->saved_y = c->y;
        c->saved_width = c->width;
        c->saved_height = c->height;
        c->saved_mode = c->mode;

        c->mode = CLIENT_FLOATING;
        c->x = 0;
        c->y = 0;
        c->width = cfg->screen_width;
        c->height = cfg->screen_height;
        c->is_maximized = 1;

        if (dpy) {
            Window target = c->frame ? c->frame : c->win;
            XMoveResizeWindow(dpy, target, c->x, c->y, c->width, c->height);
            if (c->frame) {
                int ch = c->height - TITLEBAR_HEIGHT;
                if (ch < 10) ch = 10;
                XResizeWindow(dpy, c->win, c->width, ch);
            }
        }
    } else {
        c->x = c->saved_x;
        c->y = c->saved_y;
        c->width = (c->saved_width > 0) ? c->saved_width : 600;
        c->height = (c->saved_height > 0) ? c->saved_height : 400;
        c->mode = c->saved_mode;
        c->is_maximized = 0;

        if (dpy) {
            Window target = c->frame ? c->frame : c->win;
            XMoveResizeWindow(dpy, target, c->x, c->y, c->width, c->height);
            if (c->frame) {
                int ch = c->height - TITLEBAR_HEIGHT;
                if (ch < 10) ch = 10;
                XResizeWindow(dpy, c->win, c->width, ch);
            }
        }
    }
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
