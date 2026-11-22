#ifndef CLIENT_PROPS_H
#define CLIENT_PROPS_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stddef.h>
#include <cairo/cairo.h>

#define DEFAULT_TITLE "MUZUwm Window"

// Check if a protocol array contains a target atom
int client_protocols_contain(const Atom *protocols, int count, Atom target);

// Safe copy and sanitize window title with fallback
void client_sanitize_title(char *out_buf, size_t maxlen, const char *raw_title);

// Validate UTF8_STRING property metadata (type/format/nitems) and copy safely
int client_copy_utf8_prop(char *out_buf, size_t maxlen, const unsigned char *prop,
                          unsigned long nitems, Atom actual_type, int actual_format, Atom utf8_string);

// Calculate centered & clamped geometry for floating dialog windows
void client_calculate_dialog_geometry(int screen_w, int screen_h, int win_w, int win_h,
                                     int *out_x, int *out_y, int *out_w, int *out_h);

// Resolve initial floating geometry: preserves client-specified coordinates (e.g. Audacious docked windows)
// and centers unpositioned (0, 0) windows while respecting compact window heights.
void client_resolve_initial_geometry(int screen_w, int screen_h,
                                     int top_inset, int bottom_inset,
                                     int req_x, int req_y, int req_w, int req_h,
                                     int is_csd,
                                     int *out_x, int *out_y, int *out_w, int *out_h);

// Graceful window close via ICCCM WM_DELETE_WINDOW protocol with XKillClient fallback
void client_close(Display *dpy, Window win);

// Retrieve window title resolving _NET_WM_NAME (UTF-8), WM_NAME, WM_CLASS
void client_get_title(Display *dpy, Window win, char *out_buf, size_t maxlen);

// Determine if window is transient or dialog (_NET_WM_WINDOW_TYPE)
int client_is_dialog(Display *dpy, Window win);

typedef enum {
    WIN_TYPE_NORMAL = 0,
    WIN_TYPE_DIALOG,
    WIN_TYPE_UTILITY,
    WIN_TYPE_SPLASH,
    WIN_TYPE_DOCK,
    WIN_TYPE_NOTIFICATION,
    WIN_TYPE_DESKTOP
} WindowType;

// Pure classifier mapping _NET_WM_WINDOW_TYPE atoms and transient hint to WindowType
WindowType client_classify_window_type_atoms(const Atom *types, unsigned long nitems,
                                             Atom a_desktop, Atom a_dock, Atom a_toolbar,
                                             Atom a_menu, Atom a_utility, Atom a_splash,
                                             Atom a_dialog, Atom a_notification, Atom a_normal,
                                             int has_transient);

// Query _NET_WM_WINDOW_TYPE and WM_TRANSIENT_FOR to classify a window
WindowType client_get_window_type(Display *dpy, Window win);

// Returns 1 for normal/dialog/utility/splash clients, 0 for desktop/dock/notification
int client_should_manage_as_normal(WindowType wtype);

// Return WM_TRANSIENT_FOR parent window or None
Window client_get_transient_for(Display *dpy, Window win);

// Center a dialog/transient over its parent and clamp inside the monitor workarea
void client_center_over_parent(int parent_x, int parent_y, int parent_w, int parent_h,
                               int mon_x, int mon_y, int mon_w, int mon_h,
                               int win_w, int win_h,
                               int *out_x, int *out_y, int *out_w, int *out_h);

// Parse Motif WM hints decorations flag (returns 0 if decorations explicitly disabled, 1 otherwise)
int client_parse_motif_decorations(const unsigned long *hints, unsigned long nitems);

// Determine if window uses Client-Side Decorations (CSD / Motif decorations = 0 / _GTK_FRAME_EXTENTS)
int client_is_csd(Display *dpy, Window win);

// Pure policy: returns 0 if a window cannot meaningfully go fullscreen/tile
// (dialogs, fixed-size windows, or windows whose max size is below 800x600), 1 otherwise
int client_size_hints_allow_fullscreen(long flags, int min_w, int min_h, int max_w, int max_h, int is_dialog);

// Query WM_NORMAL_HINTS and dialog state; returns 1 when dpy is NULL or win is None
int client_supports_fullscreen(Display *dpy, Window win);

// Set or clear window opacity using _NET_WM_WINDOW_OPACITY (0xFFFFFFFF = opaque / property removed)
void client_set_opacity(Display *dpy, Window win, unsigned int opacity);

// Resolve ICCCM focus policy from WM_HINTS InputHint and WM_PROTOCOLS WM_TAKE_FOCUS
void client_resolve_focus_model(int has_input_hint, int input_val, int has_take_focus,
                                int *out_set_input, int *out_send_take_focus);

// Query WM_HINTS and WM_PROTOCOLS on a window to decide how focus should be delivered
void client_get_focus_model(Display *dpy, Window win, int *out_set_input, int *out_send_take_focus);

// Send ICCCM WM_TAKE_FOCUS ClientMessage to win
int client_send_take_focus(Display *dpy, Window win, Time timestamp);

#endif // CLIENT_PROPS_H
