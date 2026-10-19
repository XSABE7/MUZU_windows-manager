#ifndef MUZU_WM_H
#define MUZU_WM_H

#include <X11/Xlib.h>

typedef enum {
    CLIENT_TILED,
    CLIENT_FLOATING
} ClientMode;

typedef struct Client {
    Window win;
    Window frame;
    int x, y, width, height;
    ClientMode mode;
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
int client_count_tiled(Client *head);

#endif /* MUZU_WM_H */
