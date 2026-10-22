#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cairo/cairo-xlib.h>
#include "mission_viewer.h"
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

void mission_viewer_init(Display *dpy, MissionViewer *mv, const GtkTheme *theme) {
    (void)dpy;
    if (!mv) return;
    memset(mv, 0, sizeof(MissionViewer));
    mv->theme = theme;
    mv->hover_idx = -1;
}

void mission_viewer_populate(Display *dpy, MissionViewer *mv, Client *clients, int current_workspace) {
    if (!mv) return;

    // Free previously owned icon surfaces
    for (int i = 0; i < mv->slot_count; i++) {
        if (mv->slots[i].owns_icon && mv->slots[i].icon) {
            cairo_surface_finish(mv->slots[i].icon);
            cairo_surface_destroy(mv->slots[i].icon);
            mv->slots[i].icon = NULL;
        }
    }

    mv->slot_count = 0;
    mv->selected_idx = 0;
    mv->hover_idx = -1;

    for (Client *c = clients; c; c = c->next) {
        if (current_workspace >= 0 && c->workspace != current_workspace) {
            continue;
        }
        if (mv->slot_count >= MISSION_MAX_SLOTS) {
            break;
        }

        MissionSlot *slot = &mv->slots[mv->slot_count];
        memset(slot, 0, sizeof(MissionSlot));
        slot->win = c->win;
        slot->client = c;
        snprintf(slot->title, sizeof(slot->title), "%s", c->title);
        if (c->icon_surface) {
            slot->icon = c->icon_surface;
            slot->owns_icon = 0;
        } else if (dpy) {
            slot->icon = icon_load_surface(c->title);
            slot->owns_icon = (slot->icon != NULL);
        }
        mv->slot_count++;
    }
}

void mission_viewer_calculate_layout(MissionViewer *mv, int screen_w, int screen_h) {
    if (!mv || mv->slot_count <= 0) return;

    mv->screen_w = screen_w;
    mv->screen_h = screen_h;

    int n = mv->slot_count;
    double margin_x = screen_w * 0.08;
    double margin_y = screen_h * 0.10;
    double avail_w = screen_w - margin_x * 2.0;
    double avail_h = screen_h - margin_y * 2.0;

    if (n == 1) {
        int w = (avail_w < 720.0) ? (int)avail_w : 720;
        int h = (avail_h < 480.0) ? (int)avail_h : 480;
        mv->slots[0].x = (screen_w - w) / 2;
        mv->slots[0].y = (screen_h - h) / 2;
        mv->slots[0].w = w;
        mv->slots[0].h = h;
        return;
    }

    int cols = (int)ceil(sqrt(n * (avail_w / avail_h)));
    if (cols < 2) cols = 2;
    if (cols > n) cols = n;
    int rows = (int)ceil((double)n / cols);

    double gap_x = 24.0;
    double gap_y = 24.0;

    double slot_w = (avail_w - (cols - 1) * gap_x) / cols;
    double slot_h = (avail_h - (rows - 1) * gap_y) / rows;

    for (int r = 0; r < rows; r++) {
        int start_idx = r * cols;
        int end_idx = (r == rows - 1) ? n : (start_idx + cols);
        int items_in_row = end_idx - start_idx;
        double row_w = items_in_row * slot_w + (items_in_row - 1) * gap_x;
        double row_start_x = (screen_w - row_w) / 2.0;
        double row_y = margin_y + r * (slot_h + gap_y);

        for (int c = 0; c < items_in_row; c++) {
            int idx = start_idx + c;
            mv->slots[idx].x = (int)(row_start_x + c * (slot_w + gap_x));
            mv->slots[idx].y = (int)row_y;
            mv->slots[idx].w = (int)slot_w;
            mv->slots[idx].h = (int)slot_h;
        }
    }
}

int mission_viewer_get_slot_at(const MissionViewer *mv, int x, int y) {
    if (!mv) return -1;
    for (int i = 0; i < mv->slot_count; i++) {
        const MissionSlot *s = &mv->slots[i];
        if (x >= s->x && x <= s->x + s->w && y >= s->y && y <= s->y + s->h) {
            return i;
        }
    }
    return -1;
}

void mission_viewer_select_next(MissionViewer *mv) {
    if (!mv || mv->slot_count <= 0) return;
    mv->selected_idx = (mv->selected_idx + 1) % mv->slot_count;
}

void mission_viewer_select_prev(MissionViewer *mv) {
    if (!mv || mv->slot_count <= 0) return;
    mv->selected_idx = (mv->selected_idx - 1 + mv->slot_count) % mv->slot_count;
}

Client *mission_viewer_get_selected(const MissionViewer *mv) {
    if (!mv || mv->slot_count <= 0 || mv->selected_idx < 0 || mv->selected_idx >= mv->slot_count) {
        return NULL;
    }
    return mv->slots[mv->selected_idx].client;
}

void mission_viewer_render(Display *dpy, MissionViewer *mv) {
    (void)dpy;
    if (!mv || !mv->surface) return;

    cairo_t *cr = cairo_create(mv->surface);
    if (!cr) return;

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Dark semi-transparent frosted backdrop
    cairo_set_source_rgba(cr, 0.05, 0.07, 0.12, 0.88);
    cairo_paint(cr);

    // Top banner
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.90);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16.0);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "Mission Control", &te);
    cairo_move_to(cr, (mv->screen_w - te.width) / 2.0 - te.x_bearing, 40.0);
    cairo_show_text(cr, "Mission Control");

    cairo_set_source_rgba(cr, 0.65, 0.72, 0.82, 0.70);
    cairo_set_font_size(cr, 12.0);
    cairo_text_extents(cr, "Click a window or press [Enter] to switch • [Esc] to exit", &te);
    cairo_move_to(cr, (mv->screen_w - te.width) / 2.0 - te.x_bearing, 62.0);
    cairo_show_text(cr, "Click a window or press [Enter] to switch • [Esc] to exit");

    // Render slots
    for (int i = 0; i < mv->slot_count; i++) {
        const MissionSlot *s = &mv->slots[i];
        int is_active = (i == mv->selected_idx || i == mv->hover_idx);

        double x = s->x;
        double y = s->y;
        double w = s->w;
        double h = s->h;

        // Card frame
        if (is_active) {
            cairo_set_source_rgba(cr, 0.14, 0.20, 0.32, 0.95);
        } else {
            cairo_set_source_rgba(cr, 0.10, 0.13, 0.20, 0.85);
        }
        draw_rounded_rect(cr, x, y, w, h, 10.0);
        cairo_fill_preserve(cr);

        if (is_active) {
            cairo_set_source_rgba(cr, 0.24, 0.65, 1.0, 0.90);
            cairo_set_line_width(cr, 2.0);
        } else {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.15);
            cairo_set_line_width(cr, 1.0);
        }
        cairo_stroke(cr);

        // Header bar in card
        double header_h = 30.0;
        cairo_save(cr);
        draw_rounded_rect(cr, x, y, w, header_h, 10.0);
        cairo_clip(cr);
        cairo_set_source_rgba(cr, 0.18, 0.22, 0.32, 0.80);
        cairo_rectangle(cr, x, y, w, header_h);
        cairo_fill(cr);
        cairo_restore(cr);

        // Title and icon
        double tx = x + 12.0;
        if (s->icon) {
            cairo_save(cr);
            int iw = cairo_image_surface_get_width(s->icon);
            int ih = cairo_image_surface_get_height(s->icon);
            if (iw > 0 && ih > 0) {
                cairo_translate(cr, tx, y + 7.0);
                cairo_scale(cr, 16.0 / iw, 16.0 / ih);
                cairo_set_source_surface(cr, s->icon, 0, 0);
                cairo_paint(cr);
            }
            cairo_restore(cr);
            tx += 22.0;
        }

        cairo_set_source_rgba(cr, 0.95, 0.97, 1.0, 0.95);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 11.0);

        char title_buf[48];
        strncpy(title_buf, s->title, sizeof(title_buf) - 1);
        title_buf[sizeof(title_buf) - 1] = '\0';
        cairo_move_to(cr, tx, y + 20.0);
        cairo_show_text(cr, title_buf);

        // Thumbnail preview container
        double body_margin = 8.0;
        double bx = x + body_margin;
        double by = y + header_h + 4.0;
        double bw = w - body_margin * 2.0;
        double bh = h - header_h - body_margin - 4.0;

        cairo_set_source_rgba(cr, 0.04, 0.06, 0.09, 0.75);
        draw_rounded_rect(cr, bx, by, bw, bh, 6.0);
        cairo_fill_preserve(cr);

        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    cairo_destroy(cr);
}

void mission_viewer_show(Display *dpy, MissionViewer *mv, int screen_w, int screen_h) {
    if (!mv || !dpy || mv->slot_count <= 0) return;

    mission_viewer_calculate_layout(mv, screen_w, screen_h);

    if (!mv->win) {
        XSetWindowAttributes wa;
        wa.override_redirect = True;
        wa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | PointerMotionMask;

        mv->win = XCreateWindow(dpy, DefaultRootWindow(dpy),
                                0, 0, screen_w, screen_h, 0,
                                CopyFromParent, InputOutput, CopyFromParent,
                                CWOverrideRedirect | CWEventMask, &wa);

        mv->surface = cairo_xlib_surface_create(dpy, mv->win,
                                                DefaultVisual(dpy, DefaultScreen(dpy)),
                                                screen_w, screen_h);
    } else {
        XMoveResizeWindow(dpy, mv->win, 0, 0, screen_w, screen_h);
        cairo_xlib_surface_set_size(mv->surface, screen_w, screen_h);
    }

    XMapRaised(dpy, mv->win);
    mv->is_visible = 1;
    mission_viewer_render(dpy, mv);
}

void mission_viewer_hide(Display *dpy, MissionViewer *mv) {
    if (!mv) return;
    if (mv->win && dpy) {
        XUnmapWindow(dpy, mv->win);
    }
    mv->is_visible = 0;
}

void mission_viewer_cleanup(MissionViewer *mv) {
    if (!mv) return;
    for (int i = 0; i < mv->slot_count; i++) {
        if (mv->slots[i].owns_icon && mv->slots[i].icon) {
            cairo_surface_finish(mv->slots[i].icon);
            cairo_surface_destroy(mv->slots[i].icon);
            mv->slots[i].icon = NULL;
        }
    }
    mv->slot_count = 0;
    if (mv->surface) {
        cairo_surface_finish(mv->surface);
        cairo_surface_destroy(mv->surface);
        mv->surface = NULL;
    }
    mv->win = None;
    mv->is_visible = 0;
}
