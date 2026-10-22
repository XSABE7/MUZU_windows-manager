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

typedef enum {
    MENU_ACTION_NONE = 0,

    // Dock context menu actions
    DOCK_ACTION_TOGGLE_MAGNIFICATION = 1,
    DOCK_ACTION_TOGGLE_AUTOHIDE,
    DOCK_ACTION_POS_BOTTOM,
    DOCK_ACTION_POS_LEFT,
    DOCK_ACTION_POS_TOP,
    DOCK_ACTION_POS_RIGHT,
    DOCK_ACTION_SIZE_SMALL,
    DOCK_ACTION_SIZE_MEDIUM,
    DOCK_ACTION_SIZE_LARGE,
    DOCK_ACTION_MOVE,
    DOCK_ACTION_REMOVE_APP,
    DOCK_ACTION_TOGGLE_APPS_CORNER,
    DOCK_ACTION_EMPTY_TRASH,
    DOCK_ACTION_CLOSE_APP,
    DOCK_ACTION_KILL_APP,
    DOCK_ACTION_CLOSE_ALL_WINDOWS,
    DOCK_ACTION_DETACH_ALL_WINDOWS,
    DOCK_ACTION_LAUNCH_APP,

    // Desktop context menu actions
    DESKTOP_ACTION_WALLPAPER_BROWSE = 101,
    DESKTOP_ACTION_WALLPAPER_PATH,
    DESKTOP_ACTION_TERMINAL,
    DESKTOP_ACTION_APPLICATIONS,
    DESKTOP_ACTION_SETTINGS
} MenuActionId;

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
