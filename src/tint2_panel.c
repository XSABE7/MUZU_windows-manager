#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include "tint2_panel.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static char tint2_cfg_path[512] = {0};

static void safe_copy(char *dst, const char *src, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
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

const char *tint2_panel_get_config_path(void) {
    if (tint2_cfg_path[0] == '\0') {
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(tint2_cfg_path, sizeof(tint2_cfg_path), "%s/.config/muzuwm/tint2rc", home);
    }
    return tint2_cfg_path;
}

static void tint2_panel_ensure_config(void) {
    const char *path = tint2_panel_get_config_path();
    if (access(path, F_OK) == 0) return;

    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.config/muzuwm", home);
    mkdir(dir, 0755);

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f,
        "# MUZUwm Tint2 Panel Configuration\n"
        "panel_items = T:C:P\n"
        "panel_size = 100%% 28\n"
        "panel_position = top center horizontal\n"
        "taskbar_mode = multi_desktop\n"
        "time1_format = %%H:%%M:%%S\n"
        "time2_format = %%a %%d %%b %%Y\n"
        "power_options = suspend,reboot,poweroff,lock,logout\n"
    );
    fclose(f);
}

void tint2_panel_update_time(Tint2Panel *panel, time_t now) {
    if (!panel) return;
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(panel->time_str, sizeof(panel->time_str), "%H:%M:%S", &tm_info);
    strftime(panel->date_str, sizeof(panel->date_str), "%a %d %b %Y", &tm_info);
}

void tint2_panel_update_workspaces(Tint2Panel *panel, Client *clients, int current_ws, const char *focused_title) {
    if (!panel) return;
    if (current_ws < 0) current_ws = 0;
    if (current_ws >= TINT2_NUM_WORKSPACES) current_ws = TINT2_NUM_WORKSPACES - 1;
    panel->current_workspace = current_ws;

    for (int i = 0; i < TINT2_NUM_WORKSPACES; i++) {
        panel->workspace_has_windows[i] = 0;
        panel->workspace_window_count[i] = 0;
    }

    for (Client *curr = clients; curr; curr = curr->next) {
        int ws = curr->workspace;
        if (ws >= 0 && ws < TINT2_NUM_WORKSPACES) {
            panel->workspace_has_windows[ws] = 1;
            panel->workspace_window_count[ws]++;
        }
    }

    if (focused_title && focused_title[0] != '\0') {
        safe_copy(panel->active_title, focused_title, sizeof(panel->active_title));
    } else {
        safe_copy(panel->active_title, "MUZUwm Desktop", sizeof(panel->active_title));
    }
}

void tint2_panel_init(Display *dpy, Tint2Panel *panel, int screen_w, const GtkTheme *theme) {
    if (!panel) return;
    memset(panel, 0, sizeof(Tint2Panel));
    panel->width = (screen_w > 0) ? screen_w : 1920;
    panel->height = TINT2_PANEL_HEIGHT;
    panel->current_workspace = 0;
    panel->hover_workspace = -1;
    panel->hover_power = 0;
    panel->theme = theme;
    safe_copy(panel->active_title, "MUZUwm Desktop", sizeof(panel->active_title));

    tint2_panel_ensure_config();
    tint2_panel_update_time(panel, time(NULL));

    if (dpy) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);

        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.background_pixel = 0x101216;
        attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                           PointerMotionMask | EnterWindowMask | LeaveWindowMask;

        panel->win = XCreateWindow(dpy, root, 0, 0, panel->width, panel->height, 0,
                                   CopyFromParent, InputOutput, CopyFromParent,
                                   CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);

        XStoreName(dpy, panel->win, "MUZUwm Panel");

        Atom net_wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom net_wm_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
        XChangeProperty(dpy, panel->win, net_wm_type, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&net_wm_dock, 1);

        panel->surface = cairo_xlib_surface_create(dpy, panel->win,
                                                   DefaultVisual(dpy, screen),
                                                   panel->width, panel->height);
        XMapRaised(dpy, panel->win);
        tint2_panel_render(dpy, panel);
    }
}

void tint2_panel_cleanup(Tint2Panel *panel) {
    if (!panel) return;
    if (panel->surface) {
        cairo_surface_finish(panel->surface);
        cairo_surface_destroy(panel->surface);
        panel->surface = NULL;
    }
}

Tint2PanelAction tint2_panel_get_action_at(const Tint2Panel *panel, int x, int y) {
    if (!panel || x < 0 || x >= panel->width || y < 0 || y >= panel->height) {
        return TINT2_ACTION_NONE;
    }

    // Left: Workspaces 1..4 pills (x = 8 + i * 40, width = 34)
    for (int i = 0; i < TINT2_NUM_WORKSPACES; i++) {
        int wx = 8 + i * 40;
        if (x >= wx && x < wx + 34) {
            return (Tint2PanelAction)(TINT2_ACTION_WORKSPACE_1 + i);
        }
    }

    // Center: Clock & Date capsule
    int clock_w = 240;
    int clock_x = (panel->width - clock_w) / 2;
    if (x >= clock_x && x < clock_x + clock_w) {
        return TINT2_ACTION_CLOCK;
    }

    // Right: Power Options button (x = width - 88 .. width - 8)
    if (x >= panel->width - 88 && x < panel->width - 6) {
        return TINT2_ACTION_POWER;
    }

    return TINT2_ACTION_NONE;
}

int tint2_panel_handle_motion(Display *dpy, Tint2Panel *panel, int x, int y) {
    if (!panel) return 0;
    Tint2PanelAction act = tint2_panel_get_action_at(panel, x, y);

    int new_hover_ws = -1;
    int new_hover_power = 0;

    if (act >= TINT2_ACTION_WORKSPACE_1 && act <= TINT2_ACTION_WORKSPACE_4) {
        new_hover_ws = (int)(act - TINT2_ACTION_WORKSPACE_1);
    } else if (act == TINT2_ACTION_POWER) {
        new_hover_power = 1;
    }

    if (new_hover_ws != panel->hover_workspace || new_hover_power != panel->hover_power) {
        panel->hover_workspace = new_hover_ws;
        panel->hover_power = new_hover_power;
        if (dpy) {
            tint2_panel_render(dpy, panel);
        }
        return 1;
    }
    return 0;
}

void tint2_panel_render(Display *dpy, Tint2Panel *panel) {
    if (!panel || !panel->surface) return;

    cairo_t *cr = cairo_create(panel->surface);
    if (!cr) return;

    // 1. Liquid / Acrylic Smoked Glass Background
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_pattern_t *bg_pat = cairo_pattern_create_linear(0, 0, 0, panel->height);
    cairo_pattern_add_color_stop_rgba(bg_pat, 0.0, 0.09, 0.10, 0.13, 0.88);
    cairo_pattern_add_color_stop_rgba(bg_pat, 1.0, 0.05, 0.06, 0.08, 0.80);
    cairo_set_source(cr, bg_pat);
    cairo_rectangle(cr, 0, 0, panel->width, panel->height);
    cairo_fill(cr);
    cairo_pattern_destroy(bg_pat);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Bottom specular refraction border
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, panel->height - 0.5);
    cairo_line_to(cr, panel->width, panel->height - 0.5);
    cairo_stroke(cr);

    // 2. Left Section: Workspaces Manager (1 .. 4)
    for (int i = 0; i < TINT2_NUM_WORKSPACES; i++) {
        double wx = 8.0 + i * 40.0;
        double wy = 4.0;
        double ww = 34.0;
        double wh = 20.0;
        int is_active = (i == panel->current_workspace);
        int is_hovered = (i == panel->hover_workspace);

        if (is_active) {
            cairo_set_source_rgba(cr, 0.22, 0.48, 0.88, 0.78);
            draw_rounded_rect(cr, wx, wy, ww, wh, 6.0);
            cairo_fill(cr);

            cairo_set_source_rgba(cr, 0.50, 0.75, 1.0, 0.85);
            cairo_set_line_width(cr, 1.0);
            draw_rounded_rect(cr, wx + 0.5, wy + 0.5, ww - 1.0, wh - 1.0, 6.0);
            cairo_stroke(cr);
        } else if (is_hovered) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.18);
            draw_rounded_rect(cr, wx, wy, ww, wh, 6.0);
            cairo_fill(cr);
        } else {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.07);
            draw_rounded_rect(cr, wx, wy, ww, wh, 6.0);
            cairo_fill(cr);
        }

        // Workspace number label
        char ws_lbl[8];
        snprintf(ws_lbl, sizeof(ws_lbl), "%d", i + 1);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 11.5);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, ws_lbl, &ext);
        if (is_active) {
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        } else {
            cairo_set_source_rgba(cr, 0.82, 0.85, 0.90, 0.85);
        }
        cairo_move_to(cr, wx + (ww - ext.width) / 2.0 - ext.x_bearing, wy + 14.0);
        cairo_show_text(cr, ws_lbl);

        // Occupancy dot if workspace has open windows
        if (panel->workspace_has_windows[i]) {
            if (is_active) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
            } else {
                cairo_set_source_rgba(cr, 0.45, 0.72, 1.0, 0.90);
            }
            cairo_arc(cr, wx + ww - 5.5, wy + 5.5, 2.2, 0, 2.0 * M_PI);
            cairo_fill(cr);
        }
    }

    // Active window title next to workspace pills
    int clock_w = 240;
    int clock_x = (panel->width - clock_w) / 2;
    int title_x = 8 + TINT2_NUM_WORKSPACES * 40 + 10;
    int max_title_w = clock_x - title_x - 12;
    if (max_title_w > 40) {
        cairo_save(cr);
        cairo_rectangle(cr, title_x, 0, max_title_w, panel->height);
        cairo_clip(cr);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.5);
        cairo_set_source_rgba(cr, 0.78, 0.82, 0.88, 0.85);
        cairo_move_to(cr, title_x, 18.0);
        cairo_show_text(cr, panel->active_title);
        cairo_restore(cr);
    }

    // 3. Center Section: Date + Digital Clock with Seconds (HH:MM:SS)
    draw_rounded_rect(cr, clock_x, 3.0, clock_w, 22.0, 7.0);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.06);
    cairo_fill(cr);

    char full_clock[128];
    snprintf(full_clock, sizeof(full_clock), "%s   •   %s", panel->date_str, panel->time_str);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.5);
    cairo_text_extents_t cext;
    cairo_text_extents(cr, full_clock, &cext);
    cairo_set_source_rgb(cr, 0.94, 0.96, 0.99);
    cairo_move_to(cr, clock_x + (clock_w - cext.width) / 2.0 - cext.x_bearing, 18.0);
    cairo_show_text(cr, full_clock);

    // 4. Right Section: Power Options Button
    double px = panel->width - 88.0;
    double py = 4.0;
    double pw = 80.0;
    double ph = 20.0;

    if (panel->hover_power) {
        cairo_set_source_rgba(cr, 0.92, 0.28, 0.28, 0.55);
    } else {
        cairo_set_source_rgba(cr, 0.85, 0.24, 0.24, 0.28);
    }
    draw_rounded_rect(cr, px, py, pw, ph, 6.0);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 1.0, 0.45, 0.45, panel->hover_power ? 0.85 : 0.45);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rect(cr, px + 0.5, py + 0.5, pw - 1.0, ph - 1.0, 6.0);
    cairo_stroke(cr);

    // Vector Power Symbol (arc + vertical bar)
    double icx = px + 14.0;
    double icy = py + 10.5;
    cairo_set_source_rgb(cr, 1.0, 0.92, 0.92);
    cairo_set_line_width(cr, 1.6);
    cairo_arc(cr, icx, icy, 4.8, -0.32 * M_PI, 1.32 * M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, icx, icy - 6.0);
    cairo_line_to(cr, icx, icy - 0.5);
    cairo_stroke(cr);

    // "Power" text
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 1.0, 0.94, 0.94);
    cairo_move_to(cr, px + 25.0, py + 14.0);
    cairo_show_text(cr, "Power");

    cairo_destroy(cr);
    cairo_surface_flush(panel->surface);
    if (dpy) XFlush(dpy);
}
