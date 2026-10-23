#ifndef ROFI_LAUNCHER_H
#define ROFI_LAUNCHER_H

#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "gtk_theme.h"

#define ROFI_MAX_APPS 256
#define ROFI_VISIBLE_ROWS 6
#define ROFI_WIDTH 640
#define ROFI_ROW_HEIGHT 52
#define ROFI_HEADER_HEIGHT 64
#define ROFI_TOTAL_HEIGHT (ROFI_HEADER_HEIGHT + (ROFI_VISIBLE_ROWS * ROFI_ROW_HEIGHT) + 16)

typedef struct {
    char name[128];
    char exec[256];
    char comment[256];
    char icon_name[128];
    cairo_surface_t *icon_surface;
} RofiApp;

typedef struct RofiLauncher {
    Window win;
    cairo_surface_t *surface;
    int is_visible;
    int x, y, width, height;

    // Target position for animation
    int target_x, target_y;
    int anim_start_y;
    int is_animating;

    // Applications list
    RofiApp apps[ROFI_MAX_APPS];
    int app_count;

    // Filtered matches
    int matches[ROFI_MAX_APPS];
    int match_count;
    int selected_idx;
    int scroll_offset;

    // Search query
    char query[128];
    int query_len;

    // Drag-to-dock state
    int is_pressing;
    int is_dragging_out;
    int drag_start_root_x, drag_start_root_y;
    int drag_app_idx;

    // Rofi configuration compatibility
    int has_custom_colors;
    double bg_r, bg_g, bg_b;
    double fg_r, fg_g, fg_b;
    double accent_r, accent_g, accent_b;
    char font_name[64];
    char prompt_text[64];
    char config_path[512];
    int use_external_rofi;

    const GtkTheme *theme;
} RofiLauncher;

void rofi_launcher_init(Display *dpy, RofiLauncher *rofi, int screen_w, int screen_h, const GtkTheme *theme);
void rofi_launcher_load_config(RofiLauncher *rofi);
void rofi_launcher_load_config_file(RofiLauncher *rofi, const char *path, const char *base_dir);
const char *rofi_launcher_get_config_path(const RofiLauncher *rofi);
void rofi_launcher_cleanup(RofiLauncher *rofi);
void rofi_launcher_scan_apps(RofiLauncher *rofi);
void rofi_launcher_filter(RofiLauncher *rofi);
void rofi_launcher_set_query(RofiLauncher *rofi, const char *q);
void rofi_launcher_query_append(RofiLauncher *rofi, char c);
void rofi_launcher_query_backspace(RofiLauncher *rofi);
void rofi_launcher_select_next(RofiLauncher *rofi);
void rofi_launcher_select_prev(RofiLauncher *rofi);
void rofi_launcher_scroll(RofiLauncher *rofi, int direction);
void rofi_sanitize_exec(const char *raw, char *clean, size_t max_len);
void rofi_launcher_toggle(Display *dpy, RofiLauncher *rofi);
void rofi_launcher_render(Display *dpy, RofiLauncher *rofi, int screen_w, int screen_h);
void rofi_launcher_handle_key(Display *dpy, RofiLauncher *rofi, XKeyEvent *ev);
void rofi_launcher_handle_click(Display *dpy, RofiLauncher *rofi, unsigned int button, int x, int y);
void rofi_launcher_handle_press(Display *dpy, RofiLauncher *rofi, unsigned int button, int root_x, int root_y);
void rofi_launcher_handle_motion(Display *dpy, RofiLauncher *rofi, int x, int y);
void rofi_launcher_execute_selected(Display *dpy, RofiLauncher *rofi);

#endif /* ROFI_LAUNCHER_H */
