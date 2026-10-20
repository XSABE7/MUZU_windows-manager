#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
        int start_x = (dock->width - icon_size) / 2;

        if (x < start_x - 6 || x > start_x + icon_size + 6) return -1;

        for (int i = 0; i < dock->item_count; i++) {
            int iy = start_y + i * (icon_size + margin);
            if (y >= iy - 4 && y <= iy + icon_size + 4) {
                return i;
            }
        }
        return -1;
    } else {
        int launcher_w = dock->item_count * (icon_size + margin);
        int total_w = launcher_w + (dock->minimized_count > 0 ? 16 + dock->minimized_count * (icon_size + margin) : 0);
        int start_x = (dock->width - total_w) / 2;
        if (start_x < 4) start_x = 4;
        int start_y = (dock->height - icon_size) / 2;

        if (y < start_y - 6 || y > start_y + icon_size + 6) return -1;

        for (int i = 0; i < dock->item_count; i++) {
            int ix = start_x + i * (icon_size + margin);
            if (x >= ix - 4 && x <= ix + icon_size + 4) {
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
    size_t len = strlen(src);
    if (len >= sizeof(dock->minimized[idx].title)) {
        len = sizeof(dock->minimized[idx].title) - 1;
    }
    memcpy(dock->minimized[idx].title, src, len);
    dock->minimized[idx].title[len] = '\0';
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
        int start_x = (dock->width - icon_size) / 2;

        if (x < start_x - 6 || x > start_x + icon_size + 6) return NULL;

        for (int i = 0; i < dock->minimized_count; i++) {
            int my = min_start_y + i * (icon_size + margin);
            if (y >= my - 4 && y <= my + icon_size + 4) {
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
        int start_y = (dock->height - icon_size) / 2;

        if (y < start_y - 6 || y > start_y + icon_size + 6) return NULL;

        for (int i = 0; i < dock->minimized_count; i++) {
            int mx = min_start_x + i * (icon_size + margin);
            if (x >= mx - 4 && x <= mx + icon_size + 4) {
                return dock->minimized[i].client;
            }
        }
        return NULL;
    }
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
    dock->surface = NULL;
    dock->theme = theme;
    dock->item_count = 4;
    dock->minimized_count = 0;

    // Item 0: Web
    snprintf(dock->items[0].name, sizeof(dock->items[0].name), "Web");
    snprintf(dock->items[0].icon_name, sizeof(dock->items[0].icon_name), "web-browser");
    snprintf(dock->items[0].exec_cmd, sizeof(dock->items[0].exec_cmd), "x-www-browser");
    dock->items[0].is_running = 0;
    dock->items[0].icon_surface = icon_load_surface("web-browser");
    if (!dock->items[0].icon_surface) dock->items[0].icon_surface = icon_load_surface("firefox");

    // Item 1: Files
    snprintf(dock->items[1].name, sizeof(dock->items[1].name), "Files");
    snprintf(dock->items[1].icon_name, sizeof(dock->items[1].icon_name), "system-file-manager");
    snprintf(dock->items[1].exec_cmd, sizeof(dock->items[1].exec_cmd), "nautilus ~");
    dock->items[1].is_running = 0;
    dock->items[1].icon_surface = icon_load_surface("system-file-manager");
    if (!dock->items[1].icon_surface) dock->items[1].icon_surface = icon_load_surface("org.gnome.Nautilus");

    // Item 2: Terminal
    snprintf(dock->items[2].name, sizeof(dock->items[2].name), "Terminal");
    snprintf(dock->items[2].icon_name, sizeof(dock->items[2].icon_name), "utilities-terminal");
    snprintf(dock->items[2].exec_cmd, sizeof(dock->items[2].exec_cmd), "x-terminal-emulator");
    dock->items[2].is_running = 0;
    dock->items[2].icon_surface = icon_load_surface("utilities-terminal");
    if (!dock->items[2].icon_surface) dock->items[2].icon_surface = icon_load_surface("org.gnome.Terminal");

    // Item 3: Apps (Launcher)
    snprintf(dock->items[3].name, sizeof(dock->items[3].name), "Apps");
    snprintf(dock->items[3].icon_name, sizeof(dock->items[3].icon_name), "view-app-grid");
    snprintf(dock->items[3].exec_cmd, sizeof(dock->items[3].exec_cmd), "__ROFI__");
    dock->items[3].is_running = 0;
    dock->items[3].icon_surface = icon_load_surface("view-app-grid");
    if (!dock->items[3].icon_surface) dock->items[3].icon_surface = icon_load_surface("start-here");

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
        {accent_r, accent_g, accent_b}, // Web (Theme Accent)
        {0.20, 0.75, 0.50}, // Files
        {0.45, 0.45, 0.90}, // Terminal
        {0.90, 0.60, 0.20}  // Settings/Apps
    };

    if (is_vertical) {
        int launcher_h = dock->item_count * (icon_size + margin);
        int total_h = launcher_h + (dock->minimized_count > 0 ? 16 + dock->minimized_count * (icon_size + margin) : 0);
        int start_y = (dock->height - total_h) / 2;
        if (start_y < 4) start_y = 4;
        int start_x = (dock->width - icon_size) / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int ix = start_x;
            int iy = start_y + i * (icon_size + margin);

            // Hover effect
            if (dock->hover_item == i) {
                cairo_set_source_rgba(cr, 0.25, 0.27, 0.32, 0.60);
                draw_rounded_rectangle(cr, ix - 3, iy - 3, icon_size + 6, icon_size + 6, 8.0);
                cairo_fill(cr);
            }

            if (dock->items[i].icon_surface) {
                icon_render(cr, dock->items[i].icon_surface, dock->items[i].name, ix, iy, icon_size);
            } else {
                cairo_set_source_rgb(cr, colors[i % 4][0], colors[i % 4][1], colors[i % 4][2]);
                draw_rounded_rectangle(cr, ix, iy, icon_size, icon_size, 6.0);
                cairo_fill(cr);

                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 14.0);
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                cairo_move_to(cr, ix + 10, iy + 22);
                char letter[2] = { dock->items[i].name[0], '\0' };
                cairo_show_text(cr, letter);
            }

            // Running indicator dot
            if (dock->items[i].is_running) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.90);
                double dot_x = (dock->pos == DOCK_LEFT) ? (ix + icon_size + 4) : (ix - 4);
                double dot_y = iy + icon_size / 2.0;
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
                int mx = start_x;
                int my = min_start_y + i * (icon_size + margin);

                if (dock->minimized[i].icon_surface) {
                    icon_render(cr, dock->minimized[i].icon_surface, dock->minimized[i].title, mx, my, icon_size);
                } else {
                    cairo_set_source_rgba(cr, 0.28, 0.33, 0.42, 0.85);
                    draw_rounded_rectangle(cr, mx, my, icon_size, icon_size, 6.0);
                    cairo_fill(cr);

                    cairo_set_source_rgba(cr, 0.60, 0.65, 0.75, 0.80);
                    cairo_set_line_width(cr, 1.0);
                    draw_rounded_rectangle(cr, mx, my, icon_size, icon_size, 6.0);
                    cairo_stroke(cr);

                    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_set_font_size(cr, 13.0);
                    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    char letter[2] = { dock->minimized[i].title[0] ? dock->minimized[i].title[0] : 'W', '\0' };
                    cairo_move_to(cr, mx + 10, my + 21);
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
        int start_y = (dock->height - icon_size) / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int ix = start_x + i * (icon_size + margin);
            int iy = start_y;

            // Hover effect
            if (dock->hover_item == i) {
                cairo_set_source_rgba(cr, 0.25, 0.27, 0.32, 0.60);
                draw_rounded_rectangle(cr, ix - 3, iy - 3, icon_size + 6, icon_size + 6, 8.0);
                cairo_fill(cr);
            }

            if (dock->items[i].icon_surface) {
                icon_render(cr, dock->items[i].icon_surface, dock->items[i].name, ix, iy, icon_size);
            } else {
                cairo_set_source_rgb(cr, colors[i % 4][0], colors[i % 4][1], colors[i % 4][2]);
                draw_rounded_rectangle(cr, ix, iy, icon_size, icon_size, 6.0);
                cairo_fill(cr);

                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 14.0);
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                cairo_move_to(cr, ix + 10, iy + 22);
                char letter[2] = { dock->items[i].name[0], '\0' };
                cairo_show_text(cr, letter);
            }

            // Running indicator dot
            if (dock->items[i].is_running) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.90);
                double dot_x = ix + icon_size / 2.0;
                double dot_y = (dock->pos == DOCK_TOP) ? (iy - 4) : (iy + icon_size + 4);
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
                int mx = min_start_x + i * (icon_size + margin);
                int my = start_y;

                if (dock->minimized[i].icon_surface) {
                    icon_render(cr, dock->minimized[i].icon_surface, dock->minimized[i].title, mx, my, icon_size);
                } else {
                    cairo_set_source_rgba(cr, 0.28, 0.33, 0.42, 0.85);
                    draw_rounded_rectangle(cr, mx, my, icon_size, icon_size, 6.0);
                    cairo_fill(cr);

                    cairo_set_source_rgba(cr, 0.60, 0.65, 0.75, 0.80);
                    cairo_set_line_width(cr, 1.0);
                    draw_rounded_rectangle(cr, mx, my, icon_size, icon_size, 6.0);
                    cairo_stroke(cr);

                    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_set_font_size(cr, 13.0);
                    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    char letter[2] = { dock->minimized[i].title[0] ? dock->minimized[i].title[0] : 'W', '\0' };
                    cairo_move_to(cr, mx + 10, my + 21);
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
        dock_calculate_geometry(dock, dock->pos, 1, screen_w, screen_h);
        XMoveWindow(dpy, dock->win, dock->x, dock->y);
    }
}

void dock_handle_hover(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h) {
    if (!dock) return;
    int item = dock_get_item_at(dock, x, y);
    if (item != dock->hover_item) {
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
        if (rofi) rofi_launcher_toggle(dpy, (RofiLauncher *)rofi);
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
    }
    for (const Client *c = clients; c; c = c->next) {
        for (int i = 0; i < dock->item_count; i++) {
            if (strcmp(dock->items[i].exec_cmd, "__LAUNCHPAD__") == 0) continue;
            if (strcasestr(c->title, dock->items[i].name) ||
                (dock->items[i].icon_name[0] && strcasestr(c->title, dock->items[i].icon_name))) {
                dock->items[i].is_running = 1;
            }
            if (i == 0 && (strcasestr(c->title, "firefox") || strcasestr(c->title, "chromium") || strcasestr(c->title, "chrome") || strcasestr(c->title, "browser"))) {
                dock->items[0].is_running = 1;
            }
            if (i == 1 && (strcasestr(c->title, "nautilus") || strcasestr(c->title, "thunar") || strcasestr(c->title, "pcmanfm") || strcasestr(c->title, "dolphin") || strcasestr(c->title, "Files"))) {
                dock->items[1].is_running = 1;
            }
            if (i == 2 && (strcasestr(c->title, "terminal") || strcasestr(c->title, "alacritty") || strcasestr(c->title, "kitty") || strcasestr(c->title, "xterm") || strcasestr(c->title, "urxvt") || strcasestr(c->title, "bash") || strcasestr(c->title, "zsh"))) {
                dock->items[2].is_running = 1;
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
    dock->item_count = 0;
    for (int i = 0; i < dock->minimized_count; i++) {
        dock->minimized[i].icon_surface = NULL;
    }
    dock->minimized_count = 0;
    if (dock->surface) {
        cairo_surface_finish(dock->surface);
        cairo_surface_destroy(dock->surface);
        dock->surface = NULL;
    }
}
