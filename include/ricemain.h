#ifndef MUZU_RICEMAIN_H
#define MUZU_RICEMAIN_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "gtk_theme.h"

#define RICEMAIN_MAX_ITEMS 16
#define RICEMAIN_RADIUS 115
#define RICEMAIN_INNER_RADIUS 38

typedef struct {
    char id[64];
    char title[64];
    char icon[64];
    char command[256];
    int action_id;
} RiceRadialItem;

typedef struct {
    Display *dpy;
    Window win;
    Window root;
    cairo_surface_t *surface;
    const GtkTheme *theme;
    int is_visible;
    int center_x;
    int center_y;
    int width;
    int height;
    int screen_w;
    int screen_h;
    int hover_idx;
    int item_count;
    RiceRadialItem items[RICEMAIN_MAX_ITEMS];
    void (*action_cb)(int action_id, const char *cmd, void *userdata);
    void *userdata;
} RiceRadialMenu;

/* Initialize Python runtime and radial menu system */
int ricemain_init(Display *dpy, Window root, int screen_w, int screen_h,
                  const GtkTheme *theme,
                  void (*action_cb)(int action_id, const char *cmd, void *userdata),
                  void *userdata);

/* Cleanup Python runtime and radial menu resources */
void ricemain_cleanup(void);

/* Reload extensions from disk */
void ricemain_reload(Display *dpy);

/* Handle root window click (ButtonPress). Returns 1 if consumed, 0 otherwise */
int ricemain_handle_root_click(Display *dpy, int button, int root_x, int root_y);

/* Handle X11 events for ricemain windows. Returns 1 if handled, 0 otherwise */
int ricemain_handle_event(Display *dpy, XEvent *ev);

/* Check if radial menu is currently visible */
int ricemain_is_radial_visible(void);

/* Show / hide radial menu */
void ricemain_radial_show(Display *dpy, int x, int y);
void ricemain_radial_hide(Display *dpy);

/* Radial menu item registration */
void ricemain_radial_clear_items(void);
int ricemain_radial_add_item(const char *id, const char *title, const char *icon, const char *cmd, int action_id);

#endif /* MUZU_RICEMAIN_H */
