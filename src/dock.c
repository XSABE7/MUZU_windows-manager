#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include "dock.h"
#include "wm.h"
#include "icon_fetcher.h"
#include "rofi_launcher.h"

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

static void draw_rounded_rectangle(cairo_t *cr, double x, double y, double w, double h, double r) {
    if (r * 2.0 > w) r = w / 2.0;
    if (r * 2.0 > h) r = h / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

void dock_calculate_geometry(Dock *dock, DockPosition pos, int is_hidden, int screen_w, int screen_h) {
    if (!dock) return;
    if (dock->size <= 0) dock->size = 48;
    dock->pos = pos;
    dock->is_hidden = is_hidden;

    int icon_size = 32;
    int margin = 8;
    int count = dock->item_count > 0 ? dock->item_count : 1;
    int min_count = dock->minimized_count;
    int content_len = count * (icon_size + margin) + (min_count > 0 ? 16 + min_count * (icon_size + margin) : 0) + 32;
    if (content_len < 260) content_len = 260;

    switch (pos) {
        case DOCK_BOTTOM:
            dock->width = content_len < (int)(screen_w * 0.85) ? content_len : (int)(screen_w * 0.85);
            dock->height = dock->size;
            dock->x = (screen_w - dock->width) / 2;
            dock->y = is_hidden ? screen_h - 4 : screen_h - dock->height - 6;
            break;
        case DOCK_TOP:
            dock->width = content_len < (int)(screen_w * 0.85) ? content_len : (int)(screen_w * 0.85);
            dock->height = dock->size;
            dock->x = (screen_w - dock->width) / 2;
            dock->y = is_hidden ? -dock->height + 4 : 6;
            break;
        case DOCK_LEFT:
            dock->width = dock->size;
            dock->height = content_len < (int)(screen_h * 0.85) ? content_len : (int)(screen_h * 0.85);
            dock->x = is_hidden ? -dock->width + 4 : 6;
            dock->y = (screen_h - dock->height) / 2;
            break;
        case DOCK_RIGHT:
            dock->width = dock->size;
            dock->height = content_len < (int)(screen_h * 0.85) ? content_len : (int)(screen_h * 0.85);
            dock->x = is_hidden ? screen_w - 4 : screen_w - dock->width - 6;
            dock->y = (screen_h - dock->height) / 2;
            break;
    }
}

double dock_get_magnification_scale(const Dock *dock, int item_center) {
    if (!dock || !dock->magnification_enabled || !dock->is_hovered || dock->mouse_pos < 0) {
        return 1.0;
    }
    double dist = fabs(dock->mouse_pos - (double)item_center);
    const double R = 75.0; // Wave influence radius
    if (dist >= R) return 1.0;
    double cosine = cos((M_PI * dist) / (2.0 * R));
    return 1.0 + 0.45 * (cosine * cosine); // Scales smoothly from 1.0 to 1.45
}

void dock_set_magnification(Dock *dock, int enabled) {
    if (!dock) return;
    dock->magnification_enabled = enabled ? 1 : 0;
}

int dock_get_item_at(const Dock *dock, int x, int y) {
    if (!dock || dock->item_count <= 0) return -1;
    int icon_size = 32;
    int margin = 8;
    int is_vertical = (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT);

    if (is_vertical) {
        int launcher_h = dock->item_count * (icon_size + margin);
        int total_h = launcher_h + (dock->minimized_count > 0 ? 16 + dock->minimized_count * (icon_size + margin) : 0);
        int start_y = (dock->height - total_h) / 2;
        if (start_y < 4) start_y = 4;
        int center_x = dock->width / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int center_y = start_y + i * (icon_size + margin) + icon_size / 2;
            double scale = dock_get_magnification_scale(dock, center_y);
            double half_w = (icon_size * scale) / 2.0 + 4.0;
            double half_h = (icon_size * scale) / 2.0 + 4.0;
            if (x >= center_x - half_w && x <= center_x + half_w &&
                y >= center_y - half_h && y <= center_y + half_h) {
                return i;
            }
        }
        return -1;
    } else {
        int launcher_w = dock->item_count * (icon_size + margin);
        int total_w = launcher_w + (dock->minimized_count > 0 ? 16 + dock->minimized_count * (icon_size + margin) : 0);
        int start_x = (dock->width - total_w) / 2;
        if (start_x < 4) start_x = 4;
        int center_y = dock->height / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int center_x = start_x + i * (icon_size + margin) + icon_size / 2;
            double scale = dock_get_magnification_scale(dock, center_x);
            double half_w = (icon_size * scale) / 2.0 + 4.0;
            double half_h = (icon_size * scale) / 2.0 + 4.0;
            if (x >= center_x - half_w && x <= center_x + half_w &&
                y >= center_y - half_h && y <= center_y + half_h) {
                return i;
            }
        }
        return -1;
    }
}

int dock_add_minimized(Dock *dock, Client *c) {
    if (!dock || !c || dock->minimized_count >= DOCK_MAX_MINIMIZED) return 0;
    for (int i = 0; i < dock->minimized_count; i++) {
        if (dock->minimized[i].client == c) return 1;
    }
    int idx = dock->minimized_count;
    dock->minimized[idx].client = c;
    const char *src = c->title[0] ? c->title : "Window";
    safe_copy(dock->minimized[idx].title, src, sizeof(dock->minimized[idx].title));

    if (c->icon_surface) {
        dock->minimized[idx].icon_surface = c->icon_surface;
    } else {
        c->icon_surface = icon_load_surface(c->title);
        dock->minimized[idx].icon_surface = c->icon_surface;
    }
    dock->minimized_count++;
    return 1;
}

void dock_remove_minimized(Dock *dock, Client *c) {
    if (!dock || !c || dock->minimized_count <= 0) return;
    int found = -1;
    for (int i = 0; i < dock->minimized_count; i++) {
        if (dock->minimized[i].client == c) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        dock->minimized[found].icon_surface = NULL;
        for (int i = found; i < dock->minimized_count - 1; i++) {
            dock->minimized[i] = dock->minimized[i + 1];
        }
        dock->minimized_count--;
        memset(&dock->minimized[dock->minimized_count], 0, sizeof(DockMinimizedItem));
    }
}

Client *dock_get_minimized_at(const Dock *dock, int x, int y) {
    if (!dock || dock->minimized_count <= 0) return NULL;
    int icon_size = 32;
    int margin = 8;
    int is_vertical = (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT);

    if (is_vertical) {
        int launcher_h = dock->item_count * (icon_size + margin);
        int total_h = launcher_h + 16 + dock->minimized_count * (icon_size + margin);
        int start_y = (dock->height - total_h) / 2;
        if (start_y < 4) start_y = 4;
        int min_start_y = start_y + launcher_h + 16;
        int center_x = dock->width / 2;

        for (int i = 0; i < dock->minimized_count; i++) {
            int center_y = min_start_y + i * (icon_size + margin) + icon_size / 2;
            double scale = dock_get_magnification_scale(dock, center_y);
            double half_w = (icon_size * scale) / 2.0 + 4.0;
            double half_h = (icon_size * scale) / 2.0 + 4.0;
            if (x >= center_x - half_w && x <= center_x + half_w &&
                y >= center_y - half_h && y <= center_y + half_h) {
                return dock->minimized[i].client;
            }
        }
        return NULL;
    } else {
        int launcher_w = dock->item_count * (icon_size + margin);
        int total_w = launcher_w + 16 + dock->minimized_count * (icon_size + margin);
        int start_x = (dock->width - total_w) / 2;
        if (start_x < 4) start_x = 4;
        int min_start_x = start_x + launcher_w + 16;
        int center_y = dock->height / 2;

        for (int i = 0; i < dock->minimized_count; i++) {
            int center_x = min_start_x + i * (icon_size + margin) + icon_size / 2;
            double scale = dock_get_magnification_scale(dock, center_x);
            double half_w = (icon_size * scale) / 2.0 + 4.0;
            double half_h = (icon_size * scale) / 2.0 + 4.0;
            if (x >= center_x - half_w && x <= center_x + half_w &&
                y >= center_y - half_h && y <= center_y + half_h) {
                return dock->minimized[i].client;
            }
        }
        return NULL;
    }
}

int dock_add_app(Dock *dock, const char *name, const char *exec, const char *icon_name) {
    if (!dock || !name || !exec || dock->item_count >= DOCK_MAX_ITEMS) return -1;

    for (int i = 0; i < dock->item_count; i++) {
        if (strcmp(dock->items[i].exec_cmd, exec) == 0) return i;
    }

    int idx = dock->item_count++;
    safe_copy(dock->items[idx].name, name, sizeof(dock->items[idx].name));
    safe_copy(dock->items[idx].exec_cmd, exec, sizeof(dock->items[idx].exec_cmd));
    safe_copy(dock->items[idx].icon_name, icon_name ? icon_name : "", sizeof(dock->items[idx].icon_name));
    dock->items[idx].is_running = 0;
    dock->items[idx].icon_surface = icon_load_surface(dock->items[idx].icon_name);
    if (!dock->items[idx].icon_surface) {
        dock->items[idx].icon_surface = icon_load_surface("application-x-executable");
    }
    return idx;
}

int dock_remove_app(Dock *dock, int index) {
    if (!dock || index < 0 || index >= dock->item_count) return 0;
    if (strcmp(dock->items[index].exec_cmd, "__ROFI__") == 0) return 0; // Don't remove Apps launcher

    if (dock->items[index].icon_surface) {
        cairo_surface_finish(dock->items[index].icon_surface);
        cairo_surface_destroy(dock->items[index].icon_surface);
        dock->items[index].icon_surface = NULL;
    }

    for (int i = index; i < dock->item_count - 1; i++) {
        dock->items[i] = dock->items[i + 1];
    }
    dock->item_count--;
    dock_save_pinned_apps(dock);
    return 1;
}

void dock_save_pinned_apps(const Dock *dock) {
    const char *home = getenv("HOME");
    if (!home) return;
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.config/muzuwm", home);
    mkdir(dir, 0755);
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/muzuwm/dock_apps.ini", home);

    FILE *f = fopen(path, "w");
    if (!f) return;

    for (int i = 0; i < dock->item_count; i++) {
        if (strcmp(dock->items[i].exec_cmd, "__ROFI__") == 0) continue;
        fprintf(f, "%s|%s|%s\n", dock->items[i].name, dock->items[i].exec_cmd, dock->items[i].icon_name);
    }
    fclose(f);
}

void dock_load_pinned_apps(Dock *dock) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/muzuwm/dock_apps.ini", home);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *name = line;
        char *exec = strchr(name, '|');
        if (!exec) continue;
        *exec++ = '\0';
        char *icon = strchr(exec, '|');
        if (!icon) continue;
        *icon++ = '\0';

        dock_add_app(dock, name, exec, icon);
    }
    fclose(f);
}

void dock_init(Display *dpy, Dock *dock, DockPosition pos, int screen_w, int screen_h, const GtkTheme *theme) {
    if (!dock) return;
    dock->size = 48;
    dock->is_hidden = 1; // Auto-hidden by default
    dock->is_dragging = 0;
    dock->drag_offset_x = 0;
    dock->drag_offset_y = 0;
    dock->hover_state = 0;
    dock->hover_item = -1;
    dock->magnification_enabled = 1;
    dock->mouse_pos = -1.0;
    dock->is_hovered = 0;
    dock->surface = NULL;
    dock->theme = theme;
    dock->item_count = 0;
    dock->minimized_count = 0;

    // Item 0: Apps Launcher (three dots icon)
    dock_add_app(dock, "Apps", "__ROFI__", "view-app-grid");

    // Load user pinned apps
    dock_load_pinned_apps(dock);

    dock_calculate_geometry(dock, pos, dock->is_hidden, screen_w, screen_h);

    if (dpy) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);

        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.background_pixel = BlackPixel(dpy, screen);
        attrs.event_mask = ExposureMask | EnterWindowMask | LeaveWindowMask | ButtonPressMask | PointerMotionMask;

        dock->win = XCreateWindow(dpy, root, dock->x, dock->y, dock->width, dock->height, 0,
                                  DefaultDepth(dpy, screen), InputOutput,
                                  DefaultVisual(dpy, screen),
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);

        XMapRaised(dpy, dock->win);
        dock_render(dpy, dock, screen_w, screen_h);
    }
}

void dock_render(Display *dpy, Dock *dock, int screen_w, int screen_h) {
    if (!dpy || !dock) return;
    (void)screen_w;
    (void)screen_h;
    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);

    if (!dock->surface) {
        dock->surface = cairo_xlib_surface_create(dpy, dock->win, visual, dock->width, dock->height);
    } else {
        cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
    }
    cairo_t *cr = cairo_create(dock->surface);

    int is_vertical = (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT);

    // Solid sleek deep black background
    cairo_set_source_rgba(cr, 0.04, 0.04, 0.04, 0.98);
    draw_rounded_rectangle(cr, 1.0, 1.0, dock->width - 2.0, dock->height - 2.0, 14.0);
    cairo_fill(cr);

    // Subtle crisp border
    cairo_set_source_rgba(cr, 0.22, 0.22, 0.22, 0.70);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rectangle(cr, 1.0, 1.0, dock->width - 2.0, dock->height - 2.0, 14.0);
    cairo_stroke(cr);

    // Render launcher icons
    int icon_size = 32;
    int margin = 8;
    double accent_r = dock->theme ? dock->theme->accent_color.r : 0.25;
    double accent_g = dock->theme ? dock->theme->accent_color.g : 0.55;
    double accent_b = dock->theme ? dock->theme->accent_color.b : 0.95;

    double colors[4][3] = {
        {accent_r, accent_g, accent_b},
        {0.20, 0.75, 0.50},
        {0.45, 0.45, 0.90},
        {0.90, 0.60, 0.20}
    };

    if (is_vertical) {
        int launcher_h = dock->item_count * (icon_size + margin);
        int total_h = launcher_h + (dock->minimized_count > 0 ? 16 + dock->minimized_count * (icon_size + margin) : 0);
        int start_y = (dock->height - total_h) / 2;
        if (start_y < 4) start_y = 4;
        int center_x = dock->width / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int center_y = start_y + i * (icon_size + margin) + icon_size / 2;
            double scale = dock_get_magnification_scale(dock, center_y);
            int eff_size = (int)(icon_size * scale);
            int ix = center_x - eff_size / 2;
            int iy = center_y - eff_size / 2;

            // Hover effect
            if (dock->hover_item == i) {
                cairo_set_source_rgba(cr, 0.25, 0.27, 0.32, 0.60);
                draw_rounded_rectangle(cr, ix - 3, iy - 3, eff_size + 6, eff_size + 6, 8.0);
                cairo_fill(cr);
            }

            if (dock->items[i].icon_surface) {
                icon_render(cr, dock->items[i].icon_surface, dock->items[i].name, ix, iy, eff_size);
            } else {
                cairo_set_source_rgb(cr, colors[i % 4][0], colors[i % 4][1], colors[i % 4][2]);
                draw_rounded_rectangle(cr, ix, iy, eff_size, eff_size, 6.0);
                cairo_fill(cr);

                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 14.0 * scale);
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                cairo_move_to(cr, ix + 10 * scale, iy + 22 * scale);
                char letter[2] = { dock->items[i].name[0], '\0' };
                cairo_show_text(cr, letter);
            }

            // Running indicator dot
            if (dock->items[i].is_running) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.90);
                double dot_x = (dock->pos == DOCK_LEFT) ? (ix + eff_size + 4) : (ix - 4);
                double dot_y = center_y;
                cairo_arc(cr, dot_x, dot_y, 2.0, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }

        // Separator line and Minimized Windows section (vertical)
        if (dock->minimized_count > 0) {
            int sep_y = start_y + launcher_h + 7;
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, (dock->width - 24) / 2, sep_y);
            cairo_line_to(cr, (dock->width + 24) / 2, sep_y);
            cairo_stroke(cr);

            int min_start_y = start_y + launcher_h + 16;
            for (int i = 0; i < dock->minimized_count; i++) {
                int center_y = min_start_y + i * (icon_size + margin) + icon_size / 2;
                double scale = dock_get_magnification_scale(dock, center_y);
                int eff_size = (int)(icon_size * scale);
                int mx = center_x - eff_size / 2;
                int my = center_y - eff_size / 2;

                if (dock->minimized[i].icon_surface) {
                    icon_render(cr, dock->minimized[i].icon_surface, dock->minimized[i].title, mx, my, eff_size);
                } else {
                    cairo_set_source_rgba(cr, 0.35, 0.35, 0.38, 0.85);
                    draw_rounded_rectangle(cr, mx, my, eff_size, eff_size, 6.0);
                    cairo_fill(cr);

                    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_set_font_size(cr, 14.0 * scale);
                    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    char letter[2] = { dock->minimized[i].title[0] ? dock->minimized[i].title[0] : 'W', '\0' };
                    cairo_move_to(cr, mx + 10 * scale, my + 21 * scale);
                    cairo_show_text(cr, letter);
                }
            }
        }
    } else {
        // Horizontal dock
        int launcher_w = dock->item_count * (icon_size + margin);
        int total_w = launcher_w + (dock->minimized_count > 0 ? 16 + dock->minimized_count * (icon_size + margin) : 0);
        int start_x = (dock->width - total_w) / 2;
        if (start_x < 4) start_x = 4;
        int center_y = dock->height / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int center_x = start_x + i * (icon_size + margin) + icon_size / 2;
            double scale = dock_get_magnification_scale(dock, center_x);
            int eff_size = (int)(icon_size * scale);
            int ix = center_x - eff_size / 2;
            int iy = center_y - eff_size / 2;

            if (dock->hover_item == i) {
                cairo_set_source_rgba(cr, 0.25, 0.27, 0.32, 0.60);
                draw_rounded_rectangle(cr, ix - 3, iy - 3, eff_size + 6, eff_size + 6, 8.0);
                cairo_fill(cr);
            }

            if (dock->items[i].icon_surface) {
                icon_render(cr, dock->items[i].icon_surface, dock->items[i].name, ix, iy, eff_size);
            } else {
                cairo_set_source_rgb(cr, colors[i % 4][0], colors[i % 4][1], colors[i % 4][2]);
                draw_rounded_rectangle(cr, ix, iy, eff_size, eff_size, 6.0);
                cairo_fill(cr);

                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 14.0 * scale);
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                cairo_move_to(cr, ix + 10 * scale, iy + 22 * scale);
                char letter[2] = { dock->items[i].name[0], '\0' };
                cairo_show_text(cr, letter);
            }

            // Running indicator dot
            if (dock->items[i].is_running) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.90);
                double dot_x = center_x;
                double dot_y = (dock->pos == DOCK_TOP) ? (iy - 3) : (iy + eff_size + 4);
                cairo_arc(cr, dot_x, dot_y, 2.0, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }

        // Separator line and Minimized Windows section (horizontal)
        if (dock->minimized_count > 0) {
            int sep_x = start_x + launcher_w + 7;
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, sep_x, (dock->height - 24) / 2);
            cairo_line_to(cr, sep_x, (dock->height + 24) / 2);
            cairo_stroke(cr);

            int min_start_x = start_x + launcher_w + 16;
            for (int i = 0; i < dock->minimized_count; i++) {
                int center_x = min_start_x + i * (icon_size + margin) + icon_size / 2;
                double scale = dock_get_magnification_scale(dock, center_x);
                int eff_size = (int)(icon_size * scale);
                int mx = center_x - eff_size / 2;
                int my = center_y - eff_size / 2;

                if (dock->minimized[i].icon_surface) {
                    icon_render(cr, dock->minimized[i].icon_surface, dock->minimized[i].title, mx, my, eff_size);
                } else {
                    cairo_set_source_rgba(cr, 0.35, 0.35, 0.38, 0.85);
                    draw_rounded_rectangle(cr, mx, my, eff_size, eff_size, 6.0);
                    cairo_fill(cr);

                    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_set_font_size(cr, 14.0 * scale);
                    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    char letter[2] = { dock->minimized[i].title[0] ? dock->minimized[i].title[0] : 'W', '\0' };
                    cairo_move_to(cr, mx + 10 * scale, my + 21 * scale);
                    cairo_show_text(cr, letter);
                }
            }
        }
    }

    cairo_destroy(cr);
    cairo_surface_flush(dock->surface);
    if (dpy) XFlush(dpy);
}

void dock_handle_motion(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h) {
    if (!dock) return;

    int in_trigger = 0;
    if (dock->pos == DOCK_BOTTOM && y >= screen_h - dock->size - 10) in_trigger = 1;
    else if (dock->pos == DOCK_TOP && y <= dock->size + 10) in_trigger = 1;
    else if (dock->pos == DOCK_LEFT && x <= dock->size + 10) in_trigger = 1;
    else if (dock->pos == DOCK_RIGHT && x >= screen_w - dock->size - 10) in_trigger = 1;

    if (in_trigger && dock->is_hidden) {
        dock_calculate_geometry(dock, dock->pos, 0, screen_w, screen_h);
        XMoveWindow(dpy, dock->win, dock->x, dock->y);
        XRaiseWindow(dpy, dock->win);
    } else if (!in_trigger && !dock->is_hidden) {
        dock->hover_item = -1;
        dock->is_hovered = 0;
        dock->mouse_pos = -1.0;
        dock_calculate_geometry(dock, dock->pos, 1, screen_w, screen_h);
        XMoveWindow(dpy, dock->win, dock->x, dock->y);
    }
}

void dock_handle_hover(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h) {
    if (!dock) return;
    int is_vertical = (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT);
    dock->is_hovered = 1;
    dock->mouse_pos = is_vertical ? (double)y : (double)x;

    int item = dock_get_item_at(dock, x, y);
    if (item != dock->hover_item || dock->magnification_enabled) {
        dock->hover_item = item;
        dock_render(dpy, dock, screen_w, screen_h);
    }
}

void dock_handle_click(Display *dpy, Dock *dock, RofiLauncher *rofi, int x, int y) {
    if (!dock) return;

    int item = dock_get_item_at(dock, x, y);
    if (item < 0 || item >= dock->item_count) return;

    if (strcmp(dock->items[item].exec_cmd, "__ROFI__") == 0 ||
        strcmp(dock->items[item].exec_cmd, "__LAUNCHPAD__") == 0) {
        if (rofi) rofi_launcher_toggle(dpy, rofi);
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", dock->items[item].exec_cmd, (char *)NULL);
        _exit(1);
    }
}

void dock_set_position(Display *dpy, Dock *dock, DockPosition new_pos, int screen_w, int screen_h) {
    dock_calculate_geometry(dock, new_pos, dock->is_hidden, screen_w, screen_h);
    if (dpy && dock->win) {
        XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
    }
    if (dock->surface) {
        cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
    }
    dock_render(dpy, dock, screen_w, screen_h);
}

void dock_cycle_position(Display *dpy, Dock *dock, int screen_w, int screen_h) {
    if (!dock) return;
    DockPosition next_pos;
    switch (dock->pos) {
        case DOCK_BOTTOM: next_pos = DOCK_LEFT; break;
        case DOCK_LEFT:   next_pos = DOCK_TOP; break;
        case DOCK_TOP:    next_pos = DOCK_RIGHT; break;
        case DOCK_RIGHT:  next_pos = DOCK_BOTTOM; break;
        default:          next_pos = DOCK_BOTTOM; break;
    }
    dock_set_position(dpy, dock, next_pos, screen_w, screen_h);
}

DockPosition dock_calculate_nearest_edge(int center_x, int center_y, int screen_w, int screen_h) {
    int dist_bottom = screen_h - center_y;
    int dist_top = center_y;
    int dist_left = center_x;
    int dist_right = screen_w - center_x;

    int min_dist = dist_bottom;
    DockPosition pos = DOCK_BOTTOM;

    if (dist_top < min_dist) {
        min_dist = dist_top;
        pos = DOCK_TOP;
    }
    if (dist_left < min_dist) {
        min_dist = dist_left;
        pos = DOCK_LEFT;
    }
    if (dist_right < min_dist) {
        min_dist = dist_right;
        pos = DOCK_RIGHT;
    }

    return pos;
}

void dock_handle_drag(Display *dpy, Dock *dock, int root_x, int root_y, int screen_w, int screen_h) {
    if (!dock || !dock->is_dragging) return;
    int nx = root_x - dock->drag_offset_x;
    int ny = root_y - dock->drag_offset_y;

    if (nx + dock->width > screen_w) nx = screen_w - dock->width;
    if (ny + dock->height > screen_h) ny = screen_h - dock->height;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;

    dock->x = nx;
    dock->y = ny;
    if (dpy && dock->win) {
        XMoveWindow(dpy, dock->win, dock->x, dock->y);
    }
}

void dock_end_drag(Display *dpy, Dock *dock, int screen_w, int screen_h) {
    if (!dock) return;
    dock->is_dragging = 0;
    int cx = dock->x + dock->width / 2;
    int cy = dock->y + dock->height / 2;
    DockPosition nearest = dock_calculate_nearest_edge(cx, cy, screen_w, screen_h);
    dock_set_position(dpy, dock, nearest, screen_w, screen_h);
}

void dock_update_running(Dock *dock, const Client *clients) {
    if (!dock) return;
    for (int i = 0; i < dock->item_count; i++) {
        dock->items[i].is_running = 0;
        const char *item_name = dock->items[i].name;

        // Skip Apps launcher (never marked as running window)
        if (strcmp(dock->items[i].exec_cmd, "__ROFI__") == 0 ||
            strcmp(dock->items[i].exec_cmd, "__LAUNCHPAD__") == 0) {
            continue;
        }

        for (const Client *c = clients; c; c = c->next) {
            if (c->title[0] != '\0' && strcasestr(c->title, item_name) != NULL) {
                dock->items[i].is_running = 1;
                break;
            }
        }
    }
}

void dock_cleanup(Dock *dock) {
    if (!dock) return;
    for (int i = 0; i < dock->item_count; i++) {
        if (dock->items[i].icon_surface) {
            cairo_surface_finish(dock->items[i].icon_surface);
            cairo_surface_destroy(dock->items[i].icon_surface);
            dock->items[i].icon_surface = NULL;
        }
    }
    for (int i = 0; i < dock->minimized_count; i++) {
        dock->minimized[i].icon_surface = NULL;
    }
    if (dock->surface) {
        cairo_surface_finish(dock->surface);
        cairo_surface_destroy(dock->surface);
        dock->surface = NULL;
    }
}
