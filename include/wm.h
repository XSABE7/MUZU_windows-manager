#ifndef MUZU_WM_H
#define MUZU_WM_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>

typedef enum {
    CLIENT_TILED,
    CLIENT_FLOATING
} ClientMode;

typedef struct Client {
    Window win;
    Window frame;
    int x, y, width, height;
    ClientMode mode;
    int is_csd;
    int is_minimized;
    int is_maximized;
    int saved_x, saved_y, saved_width, saved_height;
    ClientMode saved_mode;
    char title[256];
    cairo_surface_t *titlebar_surface;
    struct Client *next;
} Client;

typedef struct {
    int screen_width;
    int screen_height;
    int master_count;
} LayoutConfig;

void layout_calculate(const LayoutConfig *cfg, Client *head, int count);
void client_add(Client **head, Window win, ClientMode mode);
void client_remove(Client **head, Window win);
Client *client_find(Client *head, Window win);
Client *client_find_by_any(Client *head, Window win);
int client_count_tiled(Client *head);

#endif /* MUZU_WM_H */
