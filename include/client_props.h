#ifndef CLIENT_PROPS_H
#define CLIENT_PROPS_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stddef.h>

#define DEFAULT_TITLE "MUZUwm Window"

// Check if a protocol array contains a target atom
int client_protocols_contain(const Atom *protocols, int count, Atom target);

// Safe copy and sanitize window title with fallback
void client_sanitize_title(char *out_buf, size_t maxlen, const char *raw_title);

// Calculate centered & clamped geometry for floating dialog windows
void client_calculate_dialog_geometry(int screen_w, int screen_h, int win_w, int win_h,
                                     int *out_x, int *out_y, int *out_w, int *out_h);

// Graceful window close via ICCCM WM_DELETE_WINDOW protocol with XKillClient fallback
void client_close(Display *dpy, Window win);

// Retrieve window title resolving _NET_WM_NAME (UTF-8), WM_NAME, WM_CLASS
void client_get_title(Display *dpy, Window win, char *out_buf, size_t maxlen);

// Determine if window is transient or dialog (_NET_WM_WINDOW_TYPE)
int client_is_dialog(Display *dpy, Window win);

// Parse Motif WM hints decorations flag (returns 0 if decorations explicitly disabled, 1 otherwise)
int client_parse_motif_decorations(const unsigned long *hints, unsigned long nitems);

// Determine if window uses Client-Side Decorations (CSD / Motif decorations = 0 / _GTK_FRAME_EXTENTS)
int client_is_csd(Display *dpy, Window win);

#endif // CLIENT_PROPS_H
