#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include "dock.h"
#include "icon_fetcher.h"
#include "launchpad.h"

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
    int total_w = dock->item_count * (icon_size + margin);
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

void dock_init(Display *dpy, Dock *dock, DockPosition pos, int screen_w, int screen_h, const GtkTheme *theme) {
    dock->size = 48;
    dock->is_hidden = 1; // Auto-hidden by default
    dock->hover_state = 0;
    dock->hover_item = -1;
    dock->surface = NULL;
    dock->theme = theme;
    dock->item_count = 4;

    // Item 0: Web
    snprintf(dock->items[0].name, sizeof(dock->items[0].name), "Web");
    snprintf(dock->items[0].icon_name, sizeof(dock->items[0].icon_name), "web-browser");
    snprintf(dock->items[0].exec_cmd, sizeof(dock->items[0].exec_cmd), "x-www-browser");
    dock->items[0].icon_surface = icon_load_surface("web-browser");
    if (!dock->items[0].icon_surface) dock->items[0].icon_surface = icon_load_surface("firefox");

    // Item 1: Files
    snprintf(dock->items[1].name, sizeof(dock->items[1].name), "Files");
    snprintf(dock->items[1].icon_name, sizeof(dock->items[1].icon_name), "system-file-manager");
    snprintf(dock->items[1].exec_cmd, sizeof(dock->items[1].exec_cmd), "nautilus ~");
    dock->items[1].icon_surface = icon_load_surface("system-file-manager");
    if (!dock->items[1].icon_surface) dock->items[1].icon_surface = icon_load_surface("org.gnome.Nautilus");

    // Item 2: Terminal
    snprintf(dock->items[2].name, sizeof(dock->items[2].name), "Terminal");
    snprintf(dock->items[2].icon_name, sizeof(dock->items[2].icon_name), "utilities-terminal");
    snprintf(dock->items[2].exec_cmd, sizeof(dock->items[2].exec_cmd), "x-terminal-emulator");
    dock->items[2].icon_surface = icon_load_surface("utilities-terminal");
    if (!dock->items[2].icon_surface) dock->items[2].icon_surface = icon_load_surface("org.gnome.Terminal");

    // Item 3: Apps (Launchpad)
    snprintf(dock->items[3].name, sizeof(dock->items[3].name), "Apps");
    snprintf(dock->items[3].icon_name, sizeof(dock->items[3].icon_name), "view-app-grid");
    snprintf(dock->items[3].exec_cmd, sizeof(dock->items[3].exec_cmd), "__LAUNCHPAD__");
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

    // Container background from GTK Theme
    if (dock->theme) {
        cairo_set_source_rgba(cr, dock->theme->bg_surface.r, dock->theme->bg_surface.g, dock->theme->bg_surface.b, 0.94);
    } else {
        cairo_set_source_rgba(cr, 0.12, 0.15, 0.20, 0.94);
    }
    cairo_rectangle(cr, 0, 0, dock->width, dock->height);
    cairo_fill(cr);

    // Subtle border
    if (dock->theme) {
        cairo_set_source_rgba(cr, dock->theme->border_color.r, dock->theme->border_color.g, dock->theme->border_color.b, 0.80);
    } else {
        cairo_set_source_rgba(cr, 0.30, 0.38, 0.50, 0.80);
    }
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, 0, 0, dock->width, dock->height);
    cairo_stroke(cr);

    // Render launcher icons
    int icon_size = 32;
    int margin = 8;
    int total_w = dock->item_count * (icon_size + margin);
    int start_x = (dock->width - total_w) / 2;
    if (start_x < 4) start_x = 4;

    double accent_r = dock->theme ? dock->theme->accent_color.r : 0.25;
    double accent_g = dock->theme ? dock->theme->accent_color.g : 0.55;
    double accent_b = dock->theme ? dock->theme->accent_color.b : 0.95;

    double colors[4][3] = {
        {accent_r, accent_g, accent_b}, // Web (Theme Accent)
        {0.20, 0.75, 0.50}, // Files
        {0.45, 0.45, 0.90}, // Terminal
        {0.90, 0.60, 0.20}  // Settings/Apps
    };

    for (int i = 0; i < dock->item_count; i++) {
        int ix = start_x + i * (icon_size + margin);
        int iy = (dock->height - icon_size) / 2;

        // Hover effect: draw soft glowing rounded rectangle
        if (dock->hover_item == i) {
            cairo_set_source_rgba(cr, accent_r, accent_g, accent_b, 0.35);
            cairo_rectangle(cr, ix - 3, iy - 3, icon_size + 6, icon_size + 6);
            cairo_fill(cr);

            // Active underline indicator
            cairo_set_source_rgb(cr, accent_r, accent_g, accent_b);
            cairo_set_line_width(cr, 2.0);
            cairo_move_to(cr, ix + 4, iy + icon_size + 3);
            cairo_line_to(cr, ix + icon_size - 4, iy + icon_size + 3);
            cairo_stroke(cr);
        }

        if (dock->items[i].icon_surface) {
            icon_render(cr, dock->items[i].icon_surface, dock->items[i].name, ix, iy, icon_size);
        } else {
            // Fallback themed badge
            cairo_set_source_rgb(cr, colors[i % 4][0], colors[i % 4][1], colors[i % 4][2]);
            cairo_rectangle(cr, ix, iy, icon_size, icon_size);
            cairo_fill(cr);

            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 14.0);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_move_to(cr, ix + 10, iy + 22);
            char letter[2] = { dock->items[i].name[0], '\0' };
            cairo_show_text(cr, letter);
        }
    }

    cairo_destroy(cr);
    cairo_surface_flush(dock->surface);
    XFlush(dpy);
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

void dock_handle_click(Display *dpy, Dock *dock, struct Launchpad *lp, int x, int y) {
    if (!dock) return;
    int item = dock_get_item_at(dock, x, y);
    if (item < 0 || item >= dock->item_count) return;

    if (strcmp(dock->items[item].exec_cmd, "__LAUNCHPAD__") == 0) {
        if (lp) launchpad_toggle(dpy, (Launchpad *)lp);
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
    XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
    if (dock->surface) {
        cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
    }
    dock_render(dpy, dock, screen_w, screen_h);
}

void dock_cleanup(Dock *dock) {
    if (!dock) return;
    for (int i = 0; i < dock->item_count; i++) {
        if (dock->items[i].icon_surface) {
            cairo_surface_destroy(dock->items[i].icon_surface);
            dock->items[i].icon_surface = NULL;
        }
    }
    dock->item_count = 0;
    if (dock->surface) {
        cairo_surface_finish(dock->surface);
        cairo_surface_destroy(dock->surface);
        dock->surface = NULL;
    }
}
