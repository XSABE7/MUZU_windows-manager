#ifndef MUZU_LAUNCHPAD_H
#define MUZU_LAUNCHPAD_H

#include <X11/Xlib.h>
#include "easter_egg.h"

typedef struct {
    char name[64];
    char exec[128];
} AppItem;

typedef struct {
    Window win;
    int is_visible;
    AppItem apps[128];
    int app_count;
    char search_query[64];
    int current_month_offset;
    EasterEggState egg;
} Launchpad;

int days_in_month(int year, int month);
int first_weekday_of_month(int year, int month);
void launchpad_init(Display *dpy, Launchpad *lp, int screen_w, int screen_h);
void launchpad_scan_apps(Launchpad *lp);
void launchpad_render(Display *dpy, Launchpad *lp, int screen_w, int screen_h);
void launchpad_toggle(Display *dpy, Launchpad *lp);
void launchpad_handle_key(Display *dpy, Launchpad *lp, XKeyEvent *ev);
void launchpad_handle_click(Display *dpy, Launchpad *lp, int x, int y);

#endif /* MUZU_LAUNCHPAD_H */
