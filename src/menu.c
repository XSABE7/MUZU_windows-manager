#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include "menu.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void safe_copy(char *dst, const char *src, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t i = 0;
    while (i + 1 < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

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

void menu_init(Display *dpy, Menu *menu, const GtkTheme *theme, MenuCallback cb, void *userdata) {
    if (!menu) return;
    memset(menu, 0, sizeof(Menu));
    menu->theme = theme;
    menu->callback = cb;
    menu->userdata = userdata;
    menu->hover_idx = -1;
    menu->width = MENU_MIN_WIDTH;
    menu->height = MENU_PADDING_Y * 2;

    if (dpy) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);

        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.background_pixel = 0x16161a;
        attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                           PointerMotionMask | StructureNotifyMask;

        menu->win = XCreateWindow(dpy, root, 0, 0, menu->width, menu->height, 0,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);

        Atom net_wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom net_wm_popup = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
        XChangeProperty(dpy, menu->win, net_wm_type, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&net_wm_popup, 1);

        menu->surface = cairo_xlib_surface_create(dpy, menu->win,
                                                 DefaultVisual(dpy, screen),
                                                 menu->width, menu->height);
    }
}

void menu_cleanup(Menu *menu) {
    if (!menu) return;
    if (menu->surface) {
        cairo_surface_finish(menu->surface);
        cairo_surface_destroy(menu->surface);
        menu->surface = NULL;
    }
}

void menu_clear(Menu *menu) {
    if (!menu) return;
    menu->item_count = 0;
    menu->hover_idx = -1;
}

void menu_add_item(Menu *menu, const char *label, int action_id) {
    if (!menu || menu->item_count >= MENU_MAX_ITEMS) return;
    MenuItem *item = &menu->items[menu->item_count++];
    safe_copy(item->label, label, sizeof(item->label));
    item->action_id = action_id;
    item->is_separator = 0;
}

void menu_add_separator(Menu *menu) {
    if (!menu || menu->item_count >= MENU_MAX_ITEMS) return;
    MenuItem *item = &menu->items[menu->item_count++];
    item->label[0] = '\0';
    item->action_id = -1;
    item->is_separator = 1;
}

void menu_calculate_geometry(Menu *menu, int cursor_x, int cursor_y, int screen_w, int screen_h) {
    if (!menu) return;

    int total_h = MENU_PADDING_Y * 2;
    int max_text_len = 0;

    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].is_separator) {
            total_h += MENU_SEP_HEIGHT;
        } else {
            total_h += MENU_ROW_HEIGHT;
            int len = (int)strlen(menu->items[i].label);
            if (len > max_text_len) max_text_len = len;
        }
    }

    int calc_w = MENU_MIN_WIDTH;
    if (max_text_len * 9 + 40 > calc_w) {
        calc_w = max_text_len * 9 + 40;
    }

    menu->width = calc_w;
    menu->height = total_h;

    int nx = cursor_x;
    int ny = cursor_y;

    if (nx + menu->width > screen_w) {
        nx = screen_w - menu->width - 4;
    }
    if (ny + menu->height > screen_h) {
        ny = screen_h - menu->height - 4;
    }
    if (nx < 4) nx = 4;
    if (ny < 4) ny = 4;

    menu->x = nx;
    menu->y = ny;
}

int menu_get_item_at(const Menu *menu, int x, int y) {
    if (!menu) return -1;
    if (x < menu->x || x >= menu->x + menu->width ||
        y < menu->y || y >= menu->y + menu->height) {
        return -1;
    }

    int cur_y = menu->y + MENU_PADDING_Y;
    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].is_separator) {
            if (y >= cur_y && y < cur_y + MENU_SEP_HEIGHT) return -1;
            cur_y += MENU_SEP_HEIGHT;
        } else {
            if (y >= cur_y && y < cur_y + MENU_ROW_HEIGHT) return i;
            cur_y += MENU_ROW_HEIGHT;
        }
    }
    return -1;
}

void menu_select_item(Menu *menu, int idx) {
    if (!menu || idx < 0 || idx >= menu->item_count) return;
    if (menu->items[idx].is_separator) return;

    int action = menu->items[idx].action_id;
    menu->is_visible = 0;
    if (menu->callback) {
        menu->callback(action, menu->userdata);
    }
}

void menu_show(Display *dpy, Menu *menu, int cursor_x, int cursor_y, int screen_w, int screen_h) {
    if (!menu) return;
    menu_calculate_geometry(menu, cursor_x, cursor_y, screen_w, screen_h);
    menu->is_visible = 1;
    menu->hover_idx = -1;

    if (dpy && menu->win) {
        XMoveResizeWindow(dpy, menu->win, menu->x, menu->y, menu->width, menu->height);
        if (menu->surface) {
            cairo_xlib_surface_set_size(menu->surface, menu->width, menu->height);
        }
        XMapRaised(dpy, menu->win);
        menu_render(dpy, menu);
        XFlush(dpy);
    }
}

void menu_hide(Display *dpy, Menu *menu) {
    if (!menu) return;
    menu->is_visible = 0;
    menu->hover_idx = -1;
    if (dpy && menu->win) {
        XUnmapWindow(dpy, menu->win);
        XFlush(dpy);
    }
}

void menu_render(Display *dpy, Menu *menu) {
    (void)dpy;
    if (!menu || !menu->surface) return;

    cairo_t *cr = cairo_create(menu->surface);
    if (!cr) return;

    // Background: Deep dark #16161a with 1px border #323238
    cairo_set_source_rgb(cr, 0.086, 0.086, 0.102);
    cairo_paint(cr);

    // Border
    cairo_set_source_rgba(cr, 0.22, 0.22, 0.25, 0.95);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rect(cr, 0.5, 0.5, menu->width - 1.0, menu->height - 1.0, 8.0);
    cairo_stroke(cr);

    double cur_y = MENU_PADDING_Y;
    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].is_separator) {
            cairo_set_source_rgba(cr, 0.25, 0.25, 0.28, 0.7);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, 12.0, cur_y + MENU_SEP_HEIGHT / 2.0);
            cairo_line_to(cr, menu->width - 12.0, cur_y + MENU_SEP_HEIGHT / 2.0);
            cairo_stroke(cr);
            cur_y += MENU_SEP_HEIGHT;
        } else {
            int is_hovered = (i == menu->hover_idx);
            if (is_hovered) {
                cairo_set_source_rgba(cr, 0.20, 0.38, 0.68, 0.55);
                draw_rounded_rect(cr, 6.0, cur_y + 1.0, menu->width - 12.0, MENU_ROW_HEIGHT - 2.0, 6.0);
                cairo_fill(cr);
            }

            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                                   is_hovered ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 12.5);

            if (is_hovered) {
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            } else {
                cairo_set_source_rgb(cr, 0.88, 0.88, 0.90);
            }
            cairo_move_to(cr, 16.0, cur_y + 19.0);
            cairo_show_text(cr, menu->items[i].label);

            cur_y += MENU_ROW_HEIGHT;
        }
    }

    cairo_destroy(cr);
    cairo_surface_flush(menu->surface);
}

void menu_handle_click(Display *dpy, Menu *menu, unsigned int button, int x, int y) {
    if (!menu || !menu->is_visible) return;

    if (button == Button1) {
        int idx = menu_get_item_at(menu, x, y);
        if (idx >= 0) {
            menu_select_item(menu, idx);
            menu_hide(dpy, menu);
        } else {
            menu_hide(dpy, menu);
        }
    } else {
        menu_hide(dpy, menu);
    }
}

void menu_handle_motion(Display *dpy, Menu *menu, int x, int y) {
    if (!menu || !menu->is_visible) return;
    int idx = menu_get_item_at(menu, x, y);
    if (idx != menu->hover_idx) {
        menu->hover_idx = idx;
        if (dpy) menu_render(dpy, menu);
    }
}
