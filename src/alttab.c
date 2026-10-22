#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo/cairo-xlib.h>
#include "alttab.h"
#include "icon_fetcher.h"

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

void alttab_init(Display *dpy, AltTab *at, const GtkTheme *theme) {
    (void)dpy;
    if (!at) return;
    memset(at, 0, sizeof(AltTab));
    at->theme = theme;
}

static void sanitize_app_name(const char *raw, char *out, size_t out_sz) {
    if (!raw || !out || out_sz == 0) return;
    size_t i = 0;
    while (raw[i] && i + 1 < out_sz) {
        if (raw[i] == '-' || raw[i] == ':') {
            // Cut off window title specifics (e.g. "Terminal - bash" -> "Terminal")
            break;
        }
        out[i] = raw[i];
        i++;
    }
    // Trim trailing whitespace and numbers (e.g. "Terminal 1" -> "Terminal")
    while (i > 0 && (out[i - 1] == ' ' || out[i - 1] == '\t' || (out[i - 1] >= '0' && out[i - 1] <= '9'))) {
        i--;
    }
    out[i] = '\0';
    if (out[0] == '\0') {
        snprintf(out, out_sz, "Application");
    }
}

void alttab_populate(Display *dpy, AltTab *at, Client *clients) {
    if (!at) return;
    at->app_count = 0;
    at->selected_idx = 0;

    for (Client *c = clients; c; c = c->next) {
        char cls[128] = {0};
        char name[128] = {0};

        if (dpy && c->win) {
            XClassHint hint = {0};
            if (XGetClassHint(dpy, c->win, &hint)) {
                if (hint.res_class) strncpy(cls, hint.res_class, sizeof(cls) - 1);
                if (hint.res_name) strncpy(name, hint.res_name, sizeof(name) - 1);
                if (hint.res_class) XFree(hint.res_class);
                if (hint.res_name) XFree(hint.res_name);
            }
        }

        if (cls[0] == '\0') {
            sanitize_app_name(c->title, cls, sizeof(cls));
        }
        if (name[0] == '\0') {
            snprintf(name, sizeof(name), "%s", cls);
        }

        // Check if this application class already exists in at->apps
        int found = -1;
        for (int i = 0; i < at->app_count; i++) {
            if (strcasecmp(at->apps[i].app_class, cls) == 0) {
                found = i;
                break;
            }
        }

        if (found >= 0) {
            AltTabApp *app = &at->apps[found];
            if (app->win_count < ALTTAB_MAX_WINS_PER_APP) {
                app->wins[app->win_count] = c->win;
            }
            app->win_count++;
            if (!app->icon_surface && c->icon_surface) {
                app->icon_surface = c->icon_surface;
            }
        } else if (at->app_count < ALTTAB_MAX_APPS) {
            AltTabApp *app = &at->apps[at->app_count];
            memset(app, 0, sizeof(AltTabApp));
            snprintf(app->app_class, sizeof(app->app_class), "%s", cls);
            snprintf(app->app_name, sizeof(app->app_name), "%s", name);
            app->wins[0] = c->win;
            app->win_count = 1;
            if (c->icon_surface) {
                app->icon_surface = c->icon_surface;
            } else if (dpy) {
                app->icon_surface = icon_load_surface(cls);
            }
            at->app_count++;
        }
    }
}

void alttab_calculate_geometry(AltTab *at, int screen_w, int screen_h) {
    if (!at) return;
    int count = (at->app_count > 0) ? at->app_count : 1;
    at->width = count * ALTTAB_ITEM_WIDTH + ALTTAB_PADDING_X * 2;
    at->height = ALTTAB_ITEM_HEIGHT + ALTTAB_PADDING_Y * 2;
    at->x = (screen_w - at->width) / 2;
    at->y = (screen_h - at->height) / 2;
}

void alttab_select_next(AltTab *at) {
    if (!at || at->app_count <= 0) return;
    at->selected_idx = (at->selected_idx + 1) % at->app_count;
}

void alttab_select_prev(AltTab *at) {
    if (!at || at->app_count <= 0) return;
    at->selected_idx = (at->selected_idx - 1 + at->app_count) % at->app_count;
}

Window alttab_get_selected_window(const AltTab *at) {
    if (!at || at->app_count <= 0 || at->selected_idx < 0 || at->selected_idx >= at->app_count) {
        return None;
    }
    if (at->apps[at->selected_idx].win_count <= 0) {
        return None;
    }
    return at->apps[at->selected_idx].wins[0];
}

void alttab_render(Display *dpy, AltTab *at) {
    (void)dpy;
    if (!at || !at->surface) return;

    cairo_t *cr = cairo_create(at->surface);
    if (!cr) return;

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Background HUD frame with frosted glass aesthetics
    cairo_set_source_rgba(cr, 0.08, 0.11, 0.17, 0.90);
    draw_rounded_rect(cr, 0.5, 0.5, at->width - 1.0, at->height - 1.0, 16.0);
    cairo_fill_preserve(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.18);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    // Render items
    for (int i = 0; i < at->app_count; i++) {
        const AltTabApp *app = &at->apps[i];
        double ix = ALTTAB_PADDING_X + i * ALTTAB_ITEM_WIDTH;
        double iy = ALTTAB_PADDING_Y;
        double iw = ALTTAB_ITEM_WIDTH;
        double ih = ALTTAB_ITEM_HEIGHT;

        int is_selected = (i == at->selected_idx);
        if (is_selected) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.18);
            draw_rounded_rect(cr, ix + 4.0, iy + 4.0, iw - 8.0, ih - 8.0, 12.0);
            cairo_fill_preserve(cr);

            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.35);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        }

        // Draw icon
        double icon_size = 36.0;
        double icon_x = ix + (iw - icon_size) / 2.0;
        double icon_y = iy + 10.0;

        if (app->icon_surface) {
            cairo_save(cr);
            int sw = cairo_image_surface_get_width(app->icon_surface);
            int sh = cairo_image_surface_get_height(app->icon_surface);
            if (sw > 0 && sh > 0) {
                cairo_translate(cr, icon_x, icon_y);
                cairo_scale(cr, icon_size / sw, icon_size / sh);
                cairo_set_source_surface(cr, app->icon_surface, 0, 0);
                cairo_paint(cr);
            }
            cairo_restore(cr);
        } else {
            // Fallback app square with initial
            cairo_set_source_rgba(cr, 0.22, 0.35, 0.55, 0.85);
            draw_rounded_rect(cr, icon_x, icon_y, icon_size, icon_size, 8.0);
            cairo_fill(cr);

            char initial[2] = {app->app_name[0] ? app->app_name[0] : 'A', '\0'};
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 18.0);
            cairo_text_extents_t te;
            cairo_text_extents(cr, initial, &te);
            cairo_move_to(cr, icon_x + (icon_size - te.width) / 2.0 - te.x_bearing,
                          icon_y + (icon_size - te.height) / 2.0 - te.y_bearing);
            cairo_show_text(cr, initial);
        }

        // Window indicator dots row
        int dots = app->win_count;
        if (dots > ALTTAB_MAX_DOTS) dots = ALTTAB_MAX_DOTS;
        if (dots > 0) {
            double dot_r = 2.5;
            double dot_gap = 3.5;
            double total_dots_w = dots * (dot_r * 2.0) + (dots - 1) * dot_gap;
            double dot_x = ix + (iw - total_dots_w) / 2.0 + dot_r;
            double dot_y = icon_y + icon_size + 9.0;

            for (int d = 0; d < dots; d++) {
                if (is_selected) {
                    cairo_set_source_rgba(cr, 0.22, 0.74, 1.0, 0.98);
                } else {
                    cairo_set_source_rgba(cr, 0.72, 0.78, 0.88, 0.65);
                }
                cairo_arc(cr, dot_x + d * (dot_r * 2.0 + dot_gap), dot_y, dot_r, 0, 2.0 * M_PI);
                cairo_fill(cr);
            }
        }

        // App title
        cairo_set_source_rgba(cr, 0.90, 0.93, 0.98, is_selected ? 1.0 : 0.75);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, is_selected ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0);

        char label[16];
        strncpy(label, app->app_name, sizeof(label) - 1);
        label[sizeof(label) - 1] = '\0';
        if (strlen(app->app_name) >= sizeof(label) - 1) {
            label[12] = '.';
            label[13] = '.';
            label[14] = '.';
            label[15] = '\0';
        }

        cairo_text_extents_t te;
        cairo_text_extents(cr, label, &te);
        double tx = ix + (iw - te.width) / 2.0 - te.x_bearing;
        double ty = iy + ih - 10.0;
        cairo_move_to(cr, tx, ty);
        cairo_show_text(cr, label);
    }

    cairo_destroy(cr);
}

void alttab_show(Display *dpy, AltTab *at, int screen_w, int screen_h) {
    if (!at || !dpy || at->app_count <= 0) return;

    alttab_calculate_geometry(at, screen_w, screen_h);

    if (!at->win) {
        XSetWindowAttributes wa;
        wa.override_redirect = True;
        wa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask;

        at->win = XCreateWindow(dpy, DefaultRootWindow(dpy),
                                at->x, at->y, at->width, at->height, 0,
                                CopyFromParent, InputOutput, CopyFromParent,
                                CWOverrideRedirect | CWEventMask, &wa);

        at->surface = cairo_xlib_surface_create(dpy, at->win,
                                                DefaultVisual(dpy, DefaultScreen(dpy)),
                                                at->width, at->height);
    } else {
        XMoveResizeWindow(dpy, at->win, at->x, at->y, at->width, at->height);
        cairo_xlib_surface_set_size(at->surface, at->width, at->height);
    }

    XMapRaised(dpy, at->win);
    at->is_visible = 1;
    alttab_render(dpy, at);
}

void alttab_hide(Display *dpy, AltTab *at) {
    if (!at) return;
    if (at->win && dpy) {
        XUnmapWindow(dpy, at->win);
    }
    at->is_visible = 0;
}

void alttab_cleanup(AltTab *at) {
    if (!at) return;
    if (at->surface) {
        cairo_surface_finish(at->surface);
        cairo_surface_destroy(at->surface);
        at->surface = NULL;
    }
    at->win = None;
    at->is_visible = 0;
}
