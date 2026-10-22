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
#include <X11/Xutil.h>
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
    if (tint2_cfg_path[0] != '\0') return tint2_cfg_path;

    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char candidate[512];
    // 1. Check ~/.config/muzuwm/tint2rc
    snprintf(candidate, sizeof(candidate), "%s/.config/muzuwm/tint2rc", home);
    if (access(candidate, R_OK) == 0) {
        snprintf(tint2_cfg_path, sizeof(tint2_cfg_path), "%s", candidate);
        return tint2_cfg_path;
    }

    // 2. Check upstream standard ~/.config/tint2/tint2rc
    snprintf(candidate, sizeof(candidate), "%s/.config/tint2/tint2rc", home);
    if (access(candidate, R_OK) == 0) {
        snprintf(tint2_cfg_path, sizeof(tint2_cfg_path), "%s", candidate);
        return tint2_cfg_path;
    }

    // 3. Check system-wide /etc/xdg/tint2/tint2rc
    if (access("/etc/xdg/tint2/tint2rc", R_OK) == 0) {
        snprintf(tint2_cfg_path, sizeof(tint2_cfg_path), "/etc/xdg/tint2/tint2rc");
        return tint2_cfg_path;
    }

    // Default target: ~/.config/muzuwm/tint2rc
    snprintf(tint2_cfg_path, sizeof(tint2_cfg_path), "%s/.config/muzuwm/tint2rc", home);
    return tint2_cfg_path;
}

static int tint2_parse_color(const char *val, double *r, double *g, double *b, double *a) {
    if (!val || val[0] != '#') return 0;
    unsigned int hex = 0;
    int alpha_pct = 100;
    if (sscanf(val, "#%6x %d", &hex, &alpha_pct) >= 1) {
        *r = ((hex >> 16) & 0xFF) / 255.0;
        *g = ((hex >> 8) & 0xFF) / 255.0;
        *b = (hex & 0xFF) / 255.0;
        if (alpha_pct < 0) alpha_pct = 0;
        if (alpha_pct > 100) alpha_pct = 100;
        *a = alpha_pct / 100.0;
        return 1;
    }
    return 0;
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

void tint2_panel_load_config_file(Tint2Panel *panel, const char *path) {
    if (!panel || !path) return;
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        line[strcspn(line, "\r\n")] = '\0';

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        while (*key == ' ' || *key == '\t') key++;
        size_t klen = strlen(key);
        if (klen > 0) {
            char *end_k = key + klen - 1;
            while (end_k > key && (*end_k == ' ' || *end_k == '\t')) *end_k-- = '\0';
        }

        while (*val == ' ' || *val == '\t') val++;
        size_t vlen = strlen(val);
        if (vlen > 0) {
            char *end_v = val + vlen - 1;
            while (end_v > val && (*end_v == ' ' || *end_v == '\t')) *end_v-- = '\0';
        }

        if (strcmp(key, "time1_format") == 0) {
            safe_copy(panel->time1_format, val, sizeof(panel->time1_format));
        } else if (strcmp(key, "time2_format") == 0) {
            safe_copy(panel->time2_format, val, sizeof(panel->time2_format));
        } else if (strcmp(key, "panel_size") == 0) {
            int h = 0;
            if (sscanf(val, "%*s %d", &h) == 1 && h >= 16 && h <= 100) {
                panel->height = h;
            }
        } else if (strcmp(key, "panel_position") == 0) {
            if (strstr(val, "bottom") != NULL) {
                panel->is_top = 0;
            } else {
                panel->is_top = 1;
            }
        } else if (strcmp(key, "background_color") == 0) {
            if (tint2_parse_color(val, &panel->bg_r, &panel->bg_g, &panel->bg_b, &panel->bg_a)) {
                panel->has_custom_bg = 1;
            }
        } else if (strcmp(key, "border_color") == 0) {
            if (tint2_parse_color(val, &panel->border_r, &panel->border_g, &panel->border_b, &panel->border_a)) {
                panel->has_custom_border = 1;
            }
        } else if (strcmp(key, "border_width") == 0) {
            panel->border_width = atof(val);
        } else if (strcmp(key, "rounded") == 0) {
            panel->rounded = atof(val);
        } else if (strcmp(key, "clock_font_color") == 0) {
            if (tint2_parse_color(val, &panel->clock_r, &panel->clock_g, &panel->clock_b, &panel->clock_a)) {
                panel->has_custom_clock_color = 1;
            }
        } else if (strcmp(key, "taskbar_name_font_color") == 0) {
            if (tint2_parse_color(val, &panel->ws_font_r, &panel->ws_font_g, &panel->ws_font_b, &panel->ws_font_a)) {
                panel->has_custom_ws_color = 1;
            }
        } else if (strcmp(key, "taskbar_name_active_font_color") == 0) {
            if (tint2_parse_color(val, &panel->ws_active_font_r, &panel->ws_active_font_g, &panel->ws_active_font_b, &panel->ws_active_font_a)) {
                panel->has_custom_ws_color = 1;
            }
        }
    }
    fclose(f);
}

void tint2_panel_load_config(Tint2Panel *panel) {
    if (!panel) return;
    tint2_panel_load_config_file(panel, tint2_panel_get_config_path());
}

void tint2_panel_update_time(Tint2Panel *panel, time_t now) {
    if (!panel) return;
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    const char *t_fmt = panel->time1_format[0] ? panel->time1_format : "%H:%M:%S";
    const char *d_fmt = panel->time2_format[0] ? panel->time2_format : "%a %d %b %Y";
    strftime(panel->time_str, sizeof(panel->time_str), t_fmt, &tm_info);
    strftime(panel->date_str, sizeof(panel->date_str), d_fmt, &tm_info);
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
        panel->active_title[0] = '\0';
    }
}

void tint2_panel_init(Display *dpy, Tint2Panel *panel, int screen_w, const GtkTheme *theme) {
    if (!panel) return;
    memset(panel, 0, sizeof(Tint2Panel));
    panel->width = (screen_w > 0) ? screen_w : 1920;
    panel->height = TINT2_PANEL_HEIGHT;
    panel->is_top = 1;
    panel->current_workspace = 0;
    panel->hover_workspace = -1;
    panel->hover_power = 0;
    panel->dpy = dpy;
    panel->visual = NULL;
    panel->depth = 0;
    panel->colormap = None;
    panel->theme = theme;
    panel->active_title[0] = '\0';

    tint2_panel_ensure_config();
    tint2_panel_load_config(panel);
    tint2_panel_update_time(panel, time(NULL));

    if (dpy) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);
        int screen_h = DisplayHeight(dpy, screen);
        int win_y = panel->is_top ? 0 : (screen_h - panel->height);

        XVisualInfo vinfo;
        if (XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
            panel->visual = vinfo.visual;
            panel->depth = 32;
            panel->colormap = XCreateColormap(dpy, root, panel->visual, AllocNone);
        } else {
            panel->visual = DefaultVisual(dpy, screen);
            panel->depth = DefaultDepth(dpy, screen);
            panel->colormap = None;
        }

        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                           PointerMotionMask | EnterWindowMask | LeaveWindowMask;

        if (panel->depth == 32 && panel->colormap != None) {
            attrs.colormap = panel->colormap;
            attrs.border_pixel = 0;
            attrs.background_pixel = 0;
            panel->win = XCreateWindow(dpy, root, 0, win_y, panel->width, panel->height, 0,
                                       panel->depth, InputOutput,
                                       panel->visual,
                                       CWOverrideRedirect | CWColormap | CWBorderPixel | CWBackPixel | CWEventMask,
                                       &attrs);
        } else {
            attrs.background_pixel = 0x101216;
            panel->win = XCreateWindow(dpy, root, 0, win_y, panel->width, panel->height, 0,
                                       CopyFromParent, InputOutput, CopyFromParent,
                                       CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);
        }

        XStoreName(dpy, panel->win, "MUZUwm Panel");

        Atom net_wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom net_wm_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
        XChangeProperty(dpy, panel->win, net_wm_type, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&net_wm_dock, 1);

        panel->surface = cairo_xlib_surface_create(dpy, panel->win,
                                                   panel->visual,
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
    if (panel->colormap != None && panel->dpy) {
        XFreeColormap(panel->dpy, panel->colormap);
        panel->colormap = None;
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

    // Right: Keyboard layout indicator, left of the Power button
    int lang_x = TINT2_LANG_X(panel->width);
    if (panel->lang_str[0] != '\0' && x >= lang_x && x < lang_x + TINT2_LANG_W) {
        return TINT2_ACTION_LANG;
    }

    return TINT2_ACTION_NONE;
}

int tint2_panel_handle_motion(Display *dpy, Tint2Panel *panel, int x, int y) {
    if (!panel) return 0;
    Tint2PanelAction act = tint2_panel_get_action_at(panel, x, y);

    int new_hover_ws = -1;
    int new_hover_power = 0;
    int new_hover_lang = 0;

    if (act >= TINT2_ACTION_WORKSPACE_1 && act <= TINT2_ACTION_WORKSPACE_4) {
        new_hover_ws = (int)(act - TINT2_ACTION_WORKSPACE_1);
    } else if (act == TINT2_ACTION_POWER) {
        new_hover_power = 1;
    } else if (act == TINT2_ACTION_LANG) {
        new_hover_lang = 1;
    }

    if (new_hover_ws != panel->hover_workspace || new_hover_power != panel->hover_power ||
        new_hover_lang != panel->hover_lang) {
        panel->hover_workspace = new_hover_ws;
        panel->hover_power = new_hover_power;
        panel->hover_lang = new_hover_lang;
        if (dpy) {
            tint2_panel_render(dpy, panel);
        }
        return 1;
    }
    return 0;
}

void tint2_panel_set_lang(Tint2Panel *panel, const char *short_name) {
    if (!panel) return;
    safe_copy(panel->lang_str, short_name ? short_name : "", sizeof(panel->lang_str));
}

void tint2_panel_render(Display *dpy, Tint2Panel *panel) {
    if (!panel || !panel->surface) return;

    cairo_t *cr = cairo_create(panel->surface);
    if (!cr) return;

    // 1. Background (Custom from tint2rc or Liquid / Acrylic Smoked Glass)
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    if (panel->has_custom_bg) {
        cairo_set_source_rgba(cr, panel->bg_r, panel->bg_g, panel->bg_b, panel->bg_a);
        cairo_rectangle(cr, 0, 0, panel->width, panel->height);
        cairo_fill(cr);
    } else {
        cairo_pattern_t *bg_pat = cairo_pattern_create_linear(0, 0, 0, panel->height);
        cairo_pattern_add_color_stop_rgba(bg_pat, 0.0, 0.09, 0.10, 0.13, 0.88);
        cairo_pattern_add_color_stop_rgba(bg_pat, 1.0, 0.05, 0.06, 0.08, 0.80);
        cairo_set_source(cr, bg_pat);
        cairo_rectangle(cr, 0, 0, panel->width, panel->height);
        cairo_fill(cr);
        cairo_pattern_destroy(bg_pat);
    }
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Border (Custom from tint2rc or bottom specular refraction border)
    if (panel->has_custom_border) {
        cairo_set_source_rgba(cr, panel->border_r, panel->border_g, panel->border_b, panel->border_a);
        cairo_set_line_width(cr, panel->border_width > 0 ? panel->border_width : 1.0);
        double by = panel->is_top ? (panel->height - 0.5) : 0.5;
        cairo_move_to(cr, 0, by);
        cairo_line_to(cr, panel->width, by);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, 0, panel->height - 0.5);
        cairo_line_to(cr, panel->width, panel->height - 0.5);
        cairo_stroke(cr);
    }

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
            if (panel->has_custom_ws_color) {
                cairo_set_source_rgba(cr, panel->ws_active_font_r, panel->ws_active_font_g, panel->ws_active_font_b, panel->ws_active_font_a);
            } else {
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            }
        } else {
            if (panel->has_custom_ws_color) {
                cairo_set_source_rgba(cr, panel->ws_font_r, panel->ws_font_g, panel->ws_font_b, panel->ws_font_a);
            } else {
                cairo_set_source_rgba(cr, 0.82, 0.85, 0.90, 0.85);
            }
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

    char full_clock[256];
    snprintf(full_clock, sizeof(full_clock), "%s   •   %s", panel->date_str, panel->time_str);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.5);
    cairo_text_extents_t cext;
    cairo_text_extents(cr, full_clock, &cext);
    if (panel->has_custom_clock_color) {
        cairo_set_source_rgba(cr, panel->clock_r, panel->clock_g, panel->clock_b, panel->clock_a);
    } else {
        cairo_set_source_rgb(cr, 0.94, 0.96, 0.99);
    }
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

    // 5. Keyboard layout indicator (e.g. "EN" / "TH")
    if (panel->lang_str[0] != '\0') {
        double lx = TINT2_LANG_X(panel->width);
        double ly = 4.0;
        double lw = TINT2_LANG_W;
        double lh = 20.0;
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, panel->hover_lang ? 0.20 : 0.08);
        draw_rounded_rect(cr, lx, ly, lw, lh, 6.0);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, panel->hover_lang ? 0.40 : 0.16);
        cairo_set_line_width(cr, 1.0);
        draw_rounded_rect(cr, lx + 0.5, ly + 0.5, lw - 1.0, lh - 1.0, 6.0);
        cairo_stroke(cr);

        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 11.0);
        cairo_text_extents_t lext;
        cairo_text_extents(cr, panel->lang_str, &lext);
        cairo_set_source_rgb(cr, 0.94, 0.96, 0.99);
        cairo_move_to(cr, lx + (lw - lext.width) / 2.0 - lext.x_bearing, ly + 14.0);
        cairo_show_text(cr, panel->lang_str);
    }

    cairo_destroy(cr);
    cairo_surface_flush(panel->surface);
    if (dpy) XFlush(dpy);
}
