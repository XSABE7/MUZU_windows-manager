#ifndef MUZU_TINT2_PANEL_H
#define MUZU_TINT2_PANEL_H

#include <time.h>
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "wm.h"
#include "gtk_theme.h"

#define TINT2_PANEL_HEIGHT 28
#define TINT2_NUM_WORKSPACES 4

typedef enum {
    TINT2_ACTION_NONE = 0,
    TINT2_ACTION_WORKSPACE_1 = 1,
    TINT2_ACTION_WORKSPACE_2 = 2,
    TINT2_ACTION_WORKSPACE_3 = 3,
    TINT2_ACTION_WORKSPACE_4 = 4,
    TINT2_ACTION_CLOCK = 10,
    TINT2_ACTION_POWER = 20
} Tint2PanelAction;

typedef enum {
    POWER_ACTION_SUSPEND = 301,
    POWER_ACTION_REBOOT = 302,
    POWER_ACTION_POWEROFF = 303,
    POWER_ACTION_LOCK = 304,
    POWER_ACTION_LOGOUT = 305
} PowerMenuAction;

typedef struct {
    Window win;
    cairo_surface_t *surface;
    int width;
    int height;
    int current_workspace;
    int workspace_has_windows[TINT2_NUM_WORKSPACES];
    int workspace_window_count[TINT2_NUM_WORKSPACES];
    int hover_workspace;
    int hover_power;
    char time_str[32];
    char date_str[64];
    char active_title[128];
    const GtkTheme *theme;
} Tint2Panel;

void tint2_panel_init(Display *dpy, Tint2Panel *panel, int screen_w, const GtkTheme *theme);
void tint2_panel_cleanup(Tint2Panel *panel);
void tint2_panel_update_time(Tint2Panel *panel, time_t now);
void tint2_panel_update_workspaces(Tint2Panel *panel, Client *clients, int current_ws, const char *focused_title);
void tint2_panel_render(Display *dpy, Tint2Panel *panel);
Tint2PanelAction tint2_panel_get_action_at(const Tint2Panel *panel, int x, int y);
int tint2_panel_handle_motion(Display *dpy, Tint2Panel *panel, int x, int y);
const char *tint2_panel_get_config_path(void);

#endif /* MUZU_TINT2_PANEL_H */
