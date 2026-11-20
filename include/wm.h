#ifndef MUZU_WM_H
#define MUZU_WM_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <time.h>

typedef struct {
    double r, g, b, a;
} ColorRGBA;

typedef struct {
    char theme_name[128];
    char icon_theme_name[128];
    char cursor_theme_name[128];
    int cursor_theme_size;
    char font_family[64];
    double font_size;
    int is_dark;
    ColorRGBA bg_titlebar;
    ColorRGBA fg_titlebar;
    ColorRGBA accent_color;
    ColorRGBA bg_surface;
    ColorRGBA border_color;
} GtkTheme;

typedef enum {
    CLIENT_TILED,
    CLIENT_FLOATING
} ClientMode;

typedef enum {
    SNAP_NONE = 0,
    SNAP_LEFT,
    SNAP_RIGHT,
    SNAP_TOP
} SnapState;

typedef struct Client {
    Window win;
    Window frame;
    int x, y, width, height;
    ClientMode mode;
    SnapState snap_state;
    int is_csd;
    int is_minimized;
    int is_maximized;
    int is_fullscreen;
    int saved_x, saved_y, saved_width, saved_height;
    ClientMode saved_mode;
    int saved_fs_x, saved_fs_y, saved_fs_w, saved_fs_h;
    ClientMode saved_fs_mode;
    SnapState saved_fs_snap;
    int saved_fs_maximized;
    char title[256];
    cairo_surface_t *titlebar_surface;
    cairo_surface_t *titlebar_cache;
    int titlebar_cache_w;
    int titlebar_cache_focused;
    cairo_surface_t *icon_surface;
    time_t last_unfocus_time;
    int is_ghost;
    int is_hovered;
    int is_focused;
    int can_fullscreen;
    int slot;
    int workspace;
    int ws_hidden;
    Window transient_for;
    struct Client *next;
} Client;

typedef struct {
    int mon_x;
    int mon_y;
    int screen_width;
    int screen_height;
    int master_count;
    int left_inset;
    int right_inset;
    int top_inset;
    int bottom_inset;
    int current_workspace;
} LayoutConfig;

typedef struct {
    Client *client;
    int is_resize;
    int start_x;
    int start_y;
    int orig_x;
    int orig_y;
    int orig_w;
    int orig_h;
} DragState;

void layout_calculate_clients(const LayoutConfig *cfg, Client **tiled, int tiled_count);
void layout_calculate(const LayoutConfig *cfg, Client *head, int count);
void client_add(Client **head, Window win, ClientMode mode);
void client_remove(Client **head, Window win);
Client *client_find(Client *head, Window win);
Client *client_find_by_any(Client *head, Window win);
Client *client_find_by_slot(Client *head, int slot);
int client_count_tiled(Client *head);

int wm_find_readable_path(const char *const *candidates, size_t count, char *out, size_t out_len);
int wm_wait_x11_or_signal(int x11_fd, int sig_fd);
int wm_drag_commit_if_grabbed(DragState *drag, Client *c, int is_resize,
                              int start_x, int start_y, int grab_status);
int wm_drag_prepare_resize(Client *c);
int wm_drag_should_cancel(const DragState *drag, const Client *c);
void wm_drag_reset(DragState *drag);

#endif /* MUZU_WM_H */
