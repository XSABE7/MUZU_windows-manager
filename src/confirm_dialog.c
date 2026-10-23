#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include "confirm_dialog.h"
#include "tint2_panel.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void draw_rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    if (r * 2.0 > w) r = w / 2.0;
    if (r * 2.0 > h) r = h / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

int power_action_needs_confirm(int action_id) {
    switch (action_id) {
        case POWER_ACTION_SUSPEND:
        case POWER_ACTION_REBOOT:
        case POWER_ACTION_POWEROFF:
        case POWER_ACTION_LOGOUT:
            return 1;
        default:
            return 0;
    }
}

void confirm_dialog_init(Display *dpy, ConfirmDialog *d, const GtkTheme *theme) {
    if (!d) return;
    memset(d, 0, sizeof(*d));
    d->dpy = dpy;
    d->theme = theme;
    d->colormap = None;
    d->width = CONFIRM_DIALOG_WIDTH;
    d->height = CONFIRM_DIALOG_HEIGHT;
}

void confirm_dialog_prepare(ConfirmDialog *d, int action_id, int screen_w, int screen_h) {
    if (!d) return;
    d->action_id = action_id;
    d->hover_btn = CONFIRM_BTN_NONE;

    const char *title = "Are you sure?";
    const char *msg = "";
    const char *label = "OK";
    switch (action_id) {
        case POWER_ACTION_SUSPEND:
            title = "Suspend now?";
            msg = "The computer will go to sleep.";
            label = "Suspend";
            break;
        case POWER_ACTION_REBOOT:
            title = "Restart now?";
            msg = "Unsaved work in open apps may be lost.";
            label = "Restart";
            break;
        case POWER_ACTION_POWEROFF:
            title = "Power off now?";
            msg = "Unsaved work in open apps may be lost.";
            label = "Power Off";
            break;
        case POWER_ACTION_LOGOUT:
            title = "Log out now?";
            msg = "All open apps will be closed.";
            label = "Log Out";
            break;
        default:
            break;
    }
    snprintf(d->title, sizeof(d->title), "%s", title);
    snprintf(d->message, sizeof(d->message), "%s", msg);
    snprintf(d->confirm_label, sizeof(d->confirm_label), "%s", label);

    d->width = CONFIRM_DIALOG_WIDTH;
    d->height = CONFIRM_DIALOG_HEIGHT;
    d->x = (screen_w - d->width) / 2;
    d->y = (screen_h - d->height) / 2;
    if (d->x < 0) d->x = 0;
    if (d->y < 0) d->y = 0;
}

void confirm_dialog_button_rect(const ConfirmDialog *d, ConfirmButton btn,
                                int *x, int *y, int *w, int *h) {
    int bw = (CONFIRM_DIALOG_WIDTH - CONFIRM_DIALOG_PAD * 2 - CONFIRM_DIALOG_BTN_GAP) / 2;
    int by = (d ? d->height : CONFIRM_DIALOG_HEIGHT) - CONFIRM_DIALOG_PAD - CONFIRM_DIALOG_BTN_H;
    int bx = CONFIRM_DIALOG_PAD;
    if (btn == CONFIRM_BTN_OK) bx += bw + CONFIRM_DIALOG_BTN_GAP;
    if (x) *x = bx;
    if (y) *y = by;
    if (w) *w = bw;
    if (h) *h = CONFIRM_DIALOG_BTN_H;
}

ConfirmButton confirm_dialog_button_at(const ConfirmDialog *d, int x, int y) {
    if (!d) return CONFIRM_BTN_NONE;
    const ConfirmButton btns[2] = { CONFIRM_BTN_CANCEL, CONFIRM_BTN_OK };
    for (int i = 0; i < 2; i++) {
        int bx, by, bw, bh;
        confirm_dialog_button_rect(d, btns[i], &bx, &by, &bw, &bh);
        if (x >= bx && x < bx + bw && y >= by && y < by + bh) return btns[i];
    }
    return CONFIRM_BTN_NONE;
}

int confirm_dialog_handle_motion(Display *dpy, ConfirmDialog *d, int x, int y) {
    if (!d) return 0;
    int hb = (int)confirm_dialog_button_at(d, x, y);
    if (hb == d->hover_btn) return 0;
    d->hover_btn = hb;
    if (dpy && d->is_visible) confirm_dialog_render(dpy, d);
    return 1;
}

static void draw_button(cairo_t *cr, int bx, int by, int bw, int bh,
                        const char *label, int is_primary, int is_hovered) {
    if (is_primary) {
        cairo_set_source_rgba(cr, 0.88, 0.26, 0.26, is_hovered ? 0.95 : 0.78);
    } else {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, is_hovered ? 0.20 : 0.10);
    }
    draw_rounded_rect(cr, bx, by, bw, bh, 8.0);
    cairo_fill(cr);

    if (is_primary) {
        cairo_set_source_rgba(cr, 1.0, 0.55, 0.55, is_hovered ? 0.9 : 0.6);
    } else {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, is_hovered ? 0.35 : 0.18);
    }
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rect(cr, bx + 0.5, by + 0.5, bw - 1.0, bh - 1.0, 8.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.0);
    cairo_text_extents_t te;
    cairo_text_extents(cr, label, &te);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.96);
    cairo_move_to(cr, bx + (bw - te.width) / 2.0 - te.x_bearing,
                  by + (bh - te.height) / 2.0 - te.y_bearing);
    cairo_show_text(cr, label);
}

void confirm_dialog_render(Display *dpy, ConfirmDialog *d) {
    if (!d || !d->surface) return;
    cairo_t *cr = cairo_create(d->surface);
    if (!cr) return;

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Frosted dark glass card
    cairo_pattern_t *bg = cairo_pattern_create_linear(0, 0, 0, d->height);
    cairo_pattern_add_color_stop_rgba(bg, 0.0, 0.11, 0.12, 0.16, 0.94);
    cairo_pattern_add_color_stop_rgba(bg, 1.0, 0.07, 0.08, 0.11, 0.94);
    cairo_set_source(cr, bg);
    draw_rounded_rect(cr, 0.5, 0.5, d->width - 1.0, d->height - 1.0, 14.0);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(bg);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.18);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Power glyph
    double icx = CONFIRM_DIALOG_PAD + 18.0;
    double icy = 42.0;
    cairo_set_source_rgba(cr, 0.88, 0.26, 0.26, 0.25);
    cairo_arc(cr, icx, icy, 18.0, 0, 2.0 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1.0, 0.82, 0.82);
    cairo_set_line_width(cr, 2.0);
    cairo_arc(cr, icx, icy + 1.0, 8.0, -0.32 * M_PI, 1.32 * M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, icx, icy - 9.0);
    cairo_line_to(cr, icx, icy);
    cairo_stroke(cr);

    // Title + message
    double tx = CONFIRM_DIALOG_PAD + 48.0;
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 15.0);
    cairo_set_source_rgb(cr, 0.96, 0.97, 0.99);
    cairo_move_to(cr, tx, 38.0);
    cairo_show_text(cr, d->title);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgba(cr, 0.80, 0.84, 0.90, 0.85);
    cairo_move_to(cr, tx, 60.0);
    cairo_show_text(cr, d->message);

    int bx, by, bw, bh;
    confirm_dialog_button_rect(d, CONFIRM_BTN_CANCEL, &bx, &by, &bw, &bh);
    draw_button(cr, bx, by, bw, bh, "Cancel", 0, d->hover_btn == CONFIRM_BTN_CANCEL);
    confirm_dialog_button_rect(d, CONFIRM_BTN_OK, &bx, &by, &bw, &bh);
    draw_button(cr, bx, by, bw, bh, d->confirm_label, 1, d->hover_btn == CONFIRM_BTN_OK);

    cairo_destroy(cr);
    cairo_surface_flush(d->surface);
    if (dpy) XFlush(dpy);
}

void confirm_dialog_show(Display *dpy, ConfirmDialog *d, int action_id, int screen_w, int screen_h) {
    if (!d) return;
    confirm_dialog_prepare(d, action_id, screen_w, screen_h);
    if (!dpy) {
        d->is_visible = 1;
        return;
    }
    d->dpy = dpy;

    if (!d->win) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);
        XVisualInfo vinfo;
        XSetWindowAttributes attrs;
        memset(&attrs, 0, sizeof(attrs));
        attrs.override_redirect = True;
        attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                           PointerMotionMask | KeyPressMask | LeaveWindowMask;

        if (XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
            d->visual = vinfo.visual;
            d->depth = 32;
            d->colormap = XCreateColormap(dpy, root, d->visual, AllocNone);
            attrs.colormap = d->colormap;
            attrs.border_pixel = 0;
            attrs.background_pixel = 0;
            d->win = XCreateWindow(dpy, root, d->x, d->y, d->width, d->height, 0,
                                   d->depth, InputOutput, d->visual,
                                   CWOverrideRedirect | CWColormap | CWBorderPixel |
                                   CWBackPixel | CWEventMask, &attrs);
        } else {
            d->visual = DefaultVisual(dpy, screen);
            d->depth = DefaultDepth(dpy, screen);
            attrs.background_pixel = 0x15171c;
            d->win = XCreateWindow(dpy, root, d->x, d->y, d->width, d->height, 0,
                                   CopyFromParent, InputOutput, CopyFromParent,
                                   CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);
        }
        XStoreName(dpy, d->win, "Confirm");
        d->surface = cairo_xlib_surface_create(dpy, d->win, d->visual, d->width, d->height);
    } else {
        XMoveResizeWindow(dpy, d->win, d->x, d->y, d->width, d->height);
        cairo_xlib_surface_set_size(d->surface, d->width, d->height);
    }

    XMapRaised(dpy, d->win);
    d->is_visible = 1;
    confirm_dialog_render(dpy, d);
}

void confirm_dialog_hide(Display *dpy, ConfirmDialog *d) {
    if (!d) return;
    if (dpy && d->win) XUnmapWindow(dpy, d->win);
    d->is_visible = 0;
    d->hover_btn = CONFIRM_BTN_NONE;
}

void confirm_dialog_cleanup(ConfirmDialog *d) {
    if (!d) return;
    if (d->surface) {
        cairo_surface_finish(d->surface);
        cairo_surface_destroy(d->surface);
        d->surface = NULL;
    }
    if (d->dpy && d->win) {
        XDestroyWindow(d->dpy, d->win);
    }
    d->win = None;
    if (d->dpy && d->colormap != None) {
        XFreeColormap(d->dpy, d->colormap);
    }
    d->colormap = None;
    d->is_visible = 0;
}
