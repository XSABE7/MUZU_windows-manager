#ifndef MUZU_LAUNCHPAD_H
#define MUZU_LAUNCHPAD_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "easter_egg.h"

typedef struct {
    char name[64];
    char exec[128];
    char icon[128];
    cairo_surface_t *icon_surface;
} AppItem;

typedef struct {
    Window win;
    int is_visible;
    AppItem apps[128];
    int app_count;
    char search_query[64];
    int current_month_offset;
    int scroll_y;
    EasterEggState egg;
} Launchpad;

int days_in_month(int year, int month);
int first_weekday_of_month(int year, int month);
void launchpad_init(Display *dpy, Launchpad *lp, int screen_w, int screen_h);
void launchpad_cleanup(Launchpad *lp);
void launchpad_scan_apps(Launchpad *lp);
void launchpad_render(Display *dpy, Launchpad *lp, int screen_w, int screen_h);
void launchpad_toggle(Display *dpy, Launchpad *lp);
void launchpad_handle_key(Display *dpy, Launchpad *lp, XKeyEvent *ev);
void launchpad_handle_click(Display *dpy, Launchpad *lp, unsigned int button, int x, int y);
void launchpad_scroll(Launchpad *lp, int delta_y, int screen_h);

#endif /* MUZU_LAUNCHPAD_H */
