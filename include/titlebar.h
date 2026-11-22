#ifndef MUZU_TITLEBAR_H
#define MUZU_TITLEBAR_H

#include <X11/Xlib.h>
#include "wm.h"

#define TITLEBAR_HEIGHT 24

cairo_surface_t *titlebar_pre_render(Display *dpy, int width, int is_focused, const GtkTheme *theme);
void titlebar_draw(Display *dpy, Client *c, const GtkTheme *theme);
typedef enum {
    TITLEBAR_CLICK_NONE,
    TITLEBAR_CLICK_CLOSE,
    TITLEBAR_CLICK_MINIMIZE,
    TITLEBAR_CLICK_MAXIMIZE,
    TITLEBAR_CLICK_DRAG
} TitlebarAction;

TitlebarAction titlebar_get_action(int click_x, int width);
TitlebarAction titlebar_get_action_for_client(int click_x, int width, const Client *c);
void client_toggle_maximize(Display *dpy, Client *c, LayoutConfig *cfg);
void titlebar_cleanup(Client *c);

#endif /* MUZU_TITLEBAR_H */
