#ifndef MUZU_WM_H
#define MUZU_WM_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <time.h>

typedef enum {
    CLIENT_TILED,
    CLIENT_FLOATING
} ClientMode;

typedef enum {
    SNAP_NONE = 0,
    SNAP_LEFT,
    SNAP_RIGHT,
    SNAP_TOP
} SnapState;

typedef struct Client {
    Window win;
    Window frame;
    int x, y, width, height;
    ClientMode mode;
    SnapState snap_state;
    int is_csd;
    int is_minimized;
    int is_maximized;
    int saved_x, saved_y, saved_width, saved_height;
    ClientMode saved_mode;
    char title[256];
    cairo_surface_t *titlebar_surface;
    cairo_surface_t *titlebar_cache;
    int titlebar_cache_w;
    int titlebar_cache_focused;
    cairo_surface_t *icon_surface;
    time_t last_unfocus_time;
    int is_ghost;
    int is_hovered;
    int is_focused;
    int workspace;
    struct Client *next;
} Client;

typedef struct {
    int screen_width;
    int screen_height;
    int master_count;
    int top_inset;
    int current_workspace;
} LayoutConfig;

void layout_calculate(const LayoutConfig *cfg, Client *head, int count);
void client_add(Client **head, Window win, ClientMode mode);
void client_remove(Client **head, Window win);
Client *client_find(Client *head, Window win);
Client *client_find_by_any(Client *head, Window win);
int client_count_tiled(Client *head);

#endif /* MUZU_WM_H */
