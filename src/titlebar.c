#include <stdio.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include "titlebar.h"

cairo_surface_t *titlebar_pre_render(Display *dpy, int width, int is_focused, const GtkTheme *theme) {
    (void)dpy;
    if (width <= 0) width = 100;
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, TITLEBAR_HEIGHT);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        return surf;
    }
    cairo_t *cr = cairo_create(surf);

    // Liquid Glass frosted background gradient
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, 0, TITLEBAR_HEIGHT);
    if (is_focused) {
        double r = theme ? theme->bg_titlebar.r : 0.086;
        double g = theme ? theme->bg_titlebar.g : 0.094;
        double b = theme ? theme->bg_titlebar.b : 0.117;
        cairo_pattern_add_color_stop_rgba(pat, 0.0, r * 1.4, g * 1.4, b * 1.4, 0.85);
        cairo_pattern_add_color_stop_rgba(pat, 1.0, r * 0.8, g * 0.8, b * 0.8, 0.72);
    } else {
        cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.07, 0.08, 0.10, 0.60);
        cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.05, 0.06, 0.08, 0.50);
    }
    cairo_set_source(cr, pat);
    cairo_rectangle(cr, 0, 0, width, TITLEBAR_HEIGHT);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    // Top highlight refraction rim
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, is_focused ? 0.22 : 0.10);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, 0.5);
    cairo_line_to(cr, width, 0.5);
    cairo_stroke(cr);

    // Bottom shadow divider
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, TITLEBAR_HEIGHT - 0.5);
    cairo_line_to(cr, width, TITLEBAR_HEIGHT - 0.5);
    cairo_stroke(cr);

    // Outer subtle border
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
    cairo_rectangle(cr, 0.5, 0.5, width - 1.0, TITLEBAR_HEIGHT - 1.0);
    cairo_stroke(cr);

    cairo_destroy(cr);
    return surf;
}

Window titlebar_create(Display *dpy, Window client_win, int width) {
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(dpy, screen);
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | Button1MotionMask | EnterWindowMask | LeaveWindowMask;

    Window frame = XCreateWindow(dpy, root, 0, 0, width, TITLEBAR_HEIGHT, 0,
                                 DefaultDepth(dpy, screen), InputOutput,
                                 DefaultVisual(dpy, screen),
                                 CWBackPixel | CWEventMask, &attrs);

    XStoreName(dpy, frame, "MUZUwm Titlebar");
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
    int focused = c->is_focused;

    // Blit pre-rendered Liquid Glass surface from cache
    if (!c->titlebar_cache || c->titlebar_cache_w != width || c->titlebar_cache_focused != focused) {
        if (c->titlebar_cache) {
            cairo_surface_destroy(c->titlebar_cache);
            c->titlebar_cache = NULL;
        }
        c->titlebar_cache = titlebar_pre_render(dpy, width, focused, theme);
        c->titlebar_cache_w = width;
        c->titlebar_cache_focused = focused;
    }

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    if (c->titlebar_cache && cairo_surface_status(c->titlebar_cache) == CAIRO_STATUS_SUCCESS) {
        cairo_set_source_surface(cr, c->titlebar_cache, 0, 0);
        cairo_paint(cr);
    } else {
        // Fallback if cache creation failed
        cairo_set_source_rgb(cr, 0.12, 0.14, 0.18);
        cairo_paint(cr);
    }
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

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
    if (!focused) {
        cairo_set_source_rgba(cr, 0.60, 0.62, 0.65, 0.70);
    } else if (theme) {
        cairo_set_source_rgb(cr, theme->fg_titlebar.r, theme->fg_titlebar.g, theme->fg_titlebar.b);
    } else {
        cairo_set_source_rgb(cr, 0.90, 0.92, 0.95);
    }
    cairo_move_to(cr, 64, 16);
    char display_title[320];
    if (c->slot >= 1 && c->slot <= 10) {
        snprintf(display_title, sizeof(display_title), "[%d] %s",
                 c->slot % 10, c->title[0] ? c->title : "MUZUwm Window");
    } else {
        snprintf(display_title, sizeof(display_title), "%s",
                 c->title[0] ? c->title : "MUZUwm Window");
    }
    cairo_show_text(cr, display_title);
    cairo_restore(cr);

    // Control Buttons: X (Close), M (Maximize), Z (Minimize / Disappear)
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);

    // 1. 'X' at x = 14 (Close)
    if (!focused) {
        cairo_set_source_rgba(cr, 0.45, 0.47, 0.52, 0.50);
    } else {
        cairo_set_source_rgb(cr, 1.00, 0.37, 0.34); // #FF5F56
    }
    cairo_text_extents_t ext_x;
    cairo_text_extents(cr, "X", &ext_x);
    cairo_move_to(cr, 14.0 - ext_x.width / 2.0 - ext_x.x_bearing,
                      (TITLEBAR_HEIGHT / 2.0) - ext_x.height / 2.0 - ext_x.y_bearing);
    cairo_show_text(cr, "X");

    // 2. 'M' at x = 30 (Maximize or Disabled Gray)
    if (!focused) {
        cairo_set_source_rgba(cr, 0.45, 0.47, 0.52, 0.50);
    } else if (c->can_fullscreen) {
        cairo_set_source_rgb(cr, 0.15, 0.79, 0.25); // #27C93F
    } else {
        cairo_set_source_rgba(cr, 0.45, 0.47, 0.52, 0.45); // Disabled
    }
    cairo_text_extents_t ext_m;
    cairo_text_extents(cr, "M", &ext_m);
    cairo_move_to(cr, 30.0 - ext_m.width / 2.0 - ext_m.x_bearing,
                      (TITLEBAR_HEIGHT / 2.0) - ext_m.height / 2.0 - ext_m.y_bearing);
    cairo_show_text(cr, "M");

    // 3. 'Z' at x = 46 (Disappear / Minimize)
    if (!focused) {
        cairo_set_source_rgba(cr, 0.45, 0.47, 0.52, 0.50);
    } else {
        cairo_set_source_rgb(cr, 1.00, 0.74, 0.18); // #FFBD2E
    }
    cairo_text_extents_t ext_z;
    cairo_text_extents(cr, "Z", &ext_z);
    cairo_move_to(cr, 46.0 - ext_z.width / 2.0 - ext_z.x_bearing,
                      (TITLEBAR_HEIGHT / 2.0) - ext_z.height / 2.0 - ext_z.y_bearing);
    cairo_show_text(cr, "Z");

    cairo_destroy(cr);
    cairo_surface_flush(c->titlebar_surface);
    if (dpy) XFlush(dpy);
}

TitlebarAction titlebar_get_action_for_client(int click_x, int width, const Client *c) {
    (void)width;
    if (click_x < 0) return TITLEBAR_CLICK_NONE;
    if (click_x < 22) return TITLEBAR_CLICK_CLOSE;     /* X */
    if (click_x < 38) {                               /* M */
        if (c && !c->can_fullscreen) return TITLEBAR_CLICK_NONE;
        return TITLEBAR_CLICK_MAXIMIZE;
    }
    if (click_x < 56) return TITLEBAR_CLICK_MINIMIZE;  /* Z */
    return TITLEBAR_CLICK_DRAG;
}

TitlebarAction titlebar_get_action(int click_x, int width) {
    return titlebar_get_action_for_client(click_x, width, NULL);
}

void client_toggle_maximize(Display *dpy, Client *c, LayoutConfig *cfg) {
    if (!c || !cfg) return;
    if (!c->can_fullscreen) return;

    if (!c->is_maximized) {
        c->saved_x = c->x;
        c->saved_y = c->y;
        c->saved_width = c->width;
        c->saved_height = c->height;
        c->saved_mode = c->mode;

        c->mode = CLIENT_FLOATING;
        c->snap_state = SNAP_TOP;
        c->x = cfg->mon_x + cfg->left_inset;
        c->y = cfg->mon_y + cfg->top_inset;
        c->width = cfg->screen_width - cfg->left_inset - cfg->right_inset;
        c->height = cfg->screen_height - cfg->top_inset - cfg->bottom_inset;
        if (c->width < 34) c->width = 34;
        if (c->height < 34) c->height = 34;
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
        int max_w = cfg->screen_width - cfg->left_inset - cfg->right_inset;
        int max_h = cfg->screen_height - cfg->top_inset - cfg->bottom_inset;
        int bad_saved = (c->saved_width <= 0 || c->saved_width >= max_w ||
                         c->saved_height <= 0 || c->saved_height >= max_h);
        if (bad_saved) {
            // Window started auto-maximized: restore to a centered floating size
            c->saved_width = cfg->screen_width * 3 / 5;
            c->saved_height = cfg->screen_height * 3 / 5;
            c->saved_x = cfg->mon_x + cfg->left_inset + (max_w - c->saved_width) / 2;
            c->saved_y = cfg->mon_y + cfg->top_inset + (max_h - c->saved_height) / 2;
            c->saved_mode = CLIENT_FLOATING;
        }
        c->x = c->saved_x;
        c->y = c->saved_y;
        c->width = (c->saved_width > 0) ? c->saved_width : 600;
        c->height = (c->saved_height > 0) ? c->saved_height : 400;
        c->mode = c->saved_mode;
        c->snap_state = SNAP_NONE;
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
    if (c->titlebar_cache) {
        cairo_surface_destroy(c->titlebar_cache);
        c->titlebar_cache = NULL;
    }
}

void handle_titlebar_drag(Display *dpy, Client *c, int root_x, int root_y, int screen_w, int screen_h) {
    if (!c) return;

    // Detach to floating if currently tiled
    if (c->mode == CLIENT_TILED) {
        c->mode = CLIENT_FLOATING;
        c->snap_state = SNAP_NONE;
    }

    if (c->is_maximized) {
        c->is_maximized = 0;
        c->snap_state = SNAP_NONE;
        c->width = (c->saved_width > 0 && c->saved_width < screen_w) ? c->saved_width : (screen_w / 2);
        c->height = (c->saved_height > 0 && c->saved_height < screen_h) ? c->saved_height : (screen_h * 2 / 3);
        if (dpy) {
            Window target = c->frame ? c->frame : c->win;
            if (target != None) {
                XResizeWindow(dpy, target, c->width, c->height);
                if (c->frame) {
                    int ch = c->height - TITLEBAR_HEIGHT;
                    if (ch < 10) ch = 10;
                    XResizeWindow(dpy, c->win, c->width, ch);
                }
            }
        }
    }

    c->x = root_x;
    c->y = root_y;
    if (dpy) {
        Window target = c->frame ? c->frame : c->win;
        if (target != None) {
            XMoveWindow(dpy, target, c->x, c->y);
        }
    }
}

void handle_titlebar_double_click(Display *dpy, Client *c, LayoutConfig *cfg) {
    if (!c || !c->can_fullscreen) return;
    (void)dpy;
    if (c->mode == CLIENT_FLOATING) {
        c->mode = CLIENT_TILED;
        c->snap_state = SNAP_NONE;
    } else {
        c->mode = CLIENT_FLOATING;
        c->snap_state = SNAP_NONE;
        c->x = cfg->screen_width / 4;
        c->y = cfg->screen_height / 4;
        c->width = cfg->screen_width / 2;
        c->height = cfg->screen_height / 2;
    }
}
