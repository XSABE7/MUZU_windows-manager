#ifndef MUZU_DOCK_H
#define MUZU_DOCK_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "gtk_theme.h"

typedef struct RofiLauncher RofiLauncher;
struct Client;       // Forward declaration
typedef struct Client Client;

typedef enum { DOCK_BOTTOM, DOCK_TOP, DOCK_LEFT, DOCK_RIGHT } DockPosition;

#define DOCK_MAX_ITEMS 32
#define DOCK_MAX_MINIMIZED 16

typedef struct {
    char name[64];
    char icon_name[64];
    char exec_cmd[256];
    int is_running;
    cairo_surface_t *icon_surface;
} DockItem;

typedef struct {
    Client *client;
    char title[64];
    cairo_surface_t *icon_surface;
} DockMinimizedItem;

typedef struct {
    Window win;
    DockPosition pos;
    int size;           // Thickness (e.g. 48px)
    int is_hidden;      // 1 if auto-hidden
    int autohide_enabled; // 1 = auto-hide on cursor leave, 0 = always visible on screen
    int is_dragging;    // 1 if currently being dragged by user
    int drag_offset_x, drag_offset_y; // Drag anchor offset
    int hover_state;
    int hover_item;     // -1 if none, 0..item_count-1
    int hover_minimized; // -1 if none, 0..minimized_count-1
    int magnification_enabled; // 1 = macOS magnification wave on, 0 = off
    double mouse_pos;   // Cursor coordinate along dock axis
    int is_hovered;     // 1 if cursor is currently within dock bounds
    int apps_at_left;   // 0 = 3-dots Apps button at right corner (default), 1 = left corner
    int press_item;     // Index of item currently held by left-click (-1 if none)
    int press_x, press_y; // Initial press coordinates for reorder drag threshold
    int is_reordering;  // 1 if actively dragging an app to reorder
    int item_count;
    DockItem items[DOCK_MAX_ITEMS];
    int minimized_count;
    DockMinimizedItem minimized[DOCK_MAX_MINIMIZED];
    int x, y, width, height;
    cairo_surface_t *surface;
    Window tooltip_win;
    cairo_surface_t *tooltip_surface;
    const GtkTheme *theme;
} Dock;

void dock_calculate_geometry(Dock *dock, DockPosition pos, int is_hidden, int screen_w, int screen_h);
void dock_init(Display *dpy, Dock *dock, DockPosition pos, int screen_w, int screen_h, const GtkTheme *theme);
void dock_render(Display *dpy, Dock *dock, int screen_w, int screen_h);
void dock_render_tooltip(Display *dpy, Dock *dock, int screen_w, int screen_h);
void dock_hide_tooltip(Display *dpy, Dock *dock);
int dock_get_item_center_coord(const Dock *dock, int index, int start_pos, int icon_size, int margin);
int dock_get_minimized_center_coord(const Dock *dock, int index, int min_start_pos, int icon_size, int margin);
void dock_handle_motion(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h);
void dock_handle_hover(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h);
int dock_get_item_at(const Dock *dock, int x, int y);
int dock_add_app(Dock *dock, const char *name, const char *exec, const char *icon_name);
int dock_remove_app(Dock *dock, int index);
int dock_reorder_item(Dock *dock, int from_idx, int to_idx);
void dock_enforce_special_items(Dock *dock);
void dock_set_apps_corner(Dock *dock, int at_left);
void dock_empty_trash(void);
void dock_save_pinned_apps(const Dock *dock);
void dock_load_pinned_apps(Dock *dock);
void dock_save_settings(const Dock *dock);
void dock_load_settings(Dock *dock);
void dock_set_autohide(Dock *dock, int enabled, int screen_w, int screen_h, Display *dpy);
void dock_set_magnification(Dock *dock, int enabled);
double dock_get_magnification_scale(const Dock *dock, int item_center);
int dock_add_minimized(Dock *dock, Client *c);
void dock_remove_minimized(Dock *dock, Client *c);
Client *dock_get_minimized_at(const Dock *dock, int x, int y);
void dock_handle_click(Display *dpy, Dock *dock, RofiLauncher *rofi, int x, int y);
void dock_set_position(Display *dpy, Dock *dock, DockPosition new_pos, int screen_w, int screen_h);
void dock_cycle_position(Display *dpy, Dock *dock, int screen_w, int screen_h);
DockPosition dock_calculate_nearest_edge(int center_x, int center_y, int screen_w, int screen_h);
void dock_handle_drag(Display *dpy, Dock *dock, int root_x, int root_y, int screen_w, int screen_h);
void dock_end_drag(Display *dpy, Dock *dock, int screen_w, int screen_h);
void dock_update_running(Dock *dock, const Client *clients);
void dock_cleanup(Dock *dock);

#endif /* MUZU_DOCK_H */
