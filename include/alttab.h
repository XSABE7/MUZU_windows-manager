#ifndef MUZU_ALTTAB_H
#define MUZU_ALTTAB_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "wm.h"
#include "gtk_theme.h"

#define ALTTAB_MAX_APPS 32
#define ALTTAB_MAX_WINS_PER_APP 32
#define ALTTAB_MAX_DOTS 8
#define ALTTAB_ITEM_WIDTH 90
#define ALTTAB_ITEM_HEIGHT 100
#define ALTTAB_PADDING_X 20
#define ALTTAB_PADDING_Y 16

typedef struct {
    char app_class[128];
    char app_name[128];
    cairo_surface_t *icon_surface;
    Window wins[ALTTAB_MAX_WINS_PER_APP];
    int win_count;
} AltTabApp;

typedef struct {
    Window win;
    cairo_surface_t *surface;
    int is_visible;
    int x, y, width, height;
    AltTabApp apps[ALTTAB_MAX_APPS];
    int app_count;
    int selected_idx;
    const GtkTheme *theme;
} AltTab;

void alttab_init(Display *dpy, AltTab *at, const GtkTheme *theme);
void alttab_populate(Display *dpy, AltTab *at, Client *clients);
void alttab_calculate_geometry(AltTab *at, int screen_w, int screen_h);
void alttab_select_next(AltTab *at);
void alttab_select_prev(AltTab *at);
void alttab_render(Display *dpy, AltTab *at);
Window alttab_get_selected_window(const AltTab *at);
void alttab_show(Display *dpy, AltTab *at, int screen_w, int screen_h);
void alttab_hide(Display *dpy, AltTab *at);
void alttab_cleanup(AltTab *at);

#endif /* MUZU_ALTTAB_H */
