#ifndef MUZU_TITLEBAR_H
#define MUZU_TITLEBAR_H

#include <X11/Xlib.h>
#include "wm.h"
#include "gtk_theme.h"

#define TITLEBAR_HEIGHT 24

Window titlebar_create(Display *dpy, Window client_win, int width);
void titlebar_draw(Display *dpy, Client *c, const GtkTheme *theme);
typedef enum {
    TITLEBAR_CLICK_NONE,
    TITLEBAR_CLICK_CLOSE,
    TITLEBAR_CLICK_MINIMIZE,
    TITLEBAR_CLICK_MAXIMIZE,
    TITLEBAR_CLICK_DRAG
} TitlebarAction;

TitlebarAction titlebar_get_action(int click_x, int width);
void client_toggle_maximize(Display *dpy, Client *c, LayoutConfig *cfg);
void titlebar_cleanup(Client *c);
void handle_titlebar_drag(Display *dpy, Client *c, int root_x, int root_y, int screen_w, int screen_h);
void handle_titlebar_double_click(Display *dpy, Client *c, LayoutConfig *cfg);

#endif /* MUZU_TITLEBAR_H */
