#ifndef MUZU_MENU_H
#define MUZU_MENU_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "gtk_theme.h"

#define MENU_MAX_ITEMS 24
#define MENU_MIN_WIDTH 180
#define MENU_ROW_HEIGHT 28
#define MENU_SEP_HEIGHT 8
#define MENU_PADDING_Y 6

typedef void (*MenuCallback)(int action_id, void *userdata);

typedef struct {
    char label[64];
    int action_id;
    int is_separator;
} MenuItem;

typedef struct Menu {
    Window win;
    cairo_surface_t *surface;
    int is_visible;
    int x, y, width, height;
    MenuItem items[MENU_MAX_ITEMS];
    int item_count;
    int hover_idx;
    MenuCallback callback;
    void *userdata;
    const GtkTheme *theme;
} Menu;

void menu_init(Display *dpy, Menu *menu, const GtkTheme *theme, MenuCallback cb, void *userdata);
void menu_cleanup(Menu *menu);
void menu_clear(Menu *menu);
void menu_add_item(Menu *menu, const char *label, int action_id);
void menu_add_separator(Menu *menu);
void menu_calculate_geometry(Menu *menu, int cursor_x, int cursor_y, int screen_w, int screen_h);
int menu_get_item_at(const Menu *menu, int x, int y);
void menu_select_item(Menu *menu, int idx);
void menu_show(Display *dpy, Menu *menu, int cursor_x, int cursor_y, int screen_w, int screen_h);
void menu_hide(Display *dpy, Menu *menu);
void menu_render(Display *dpy, Menu *menu);
void menu_handle_click(Display *dpy, Menu *menu, unsigned int button, int x, int y);
void menu_handle_motion(Display *dpy, Menu *menu, int x, int y);

#endif /* MUZU_MENU_H */
