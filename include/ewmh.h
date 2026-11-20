#ifndef MUZU_EWMH_H
#define MUZU_EWMH_H

#include <X11/Xlib.h>
#include "wm.h"

typedef struct {
    int has_fullscreen;
    int has_max_horz;
    int has_max_vert;
    int has_hidden;
} EwmhStateFlags;

// Resolve EWMH _NET_WM_STATE action: 0=REMOVE, 1=ADD, 2=TOGGLE
int ewmh_resolve_state_action(long action, int current_val);

// Parse up to two _NET_WM_STATE atoms from a ClientMessage
EwmhStateFlags ewmh_parse_state_atoms(Atom prop1, Atom prop2,
                                      Atom a_fs, Atom a_max_h, Atom a_max_v, Atom a_hidden);

// Publish _NET_WM_STATE on a client window reflecting fullscreen/maximized/hidden flags
void ewmh_sync_client_state(Display *dpy, Window win, int is_fullscreen, int is_maximized, int is_minimized);

// Check whether a client window already has _NET_WM_STATE_FULLSCREEN set prior to mapping
int ewmh_window_has_fullscreen_state(Display *dpy, Window win);

// Validate an XA_ATOM/32 property buffer and check whether it contains target
int ewmh_atoms_contain(const unsigned char *prop, unsigned long nitems,
                       Atom actual_type, int actual_format, Atom target);

// Validate a 64-bit ClientMessage desktop index against [0, num_workspaces - 1]
int ewmh_parse_desktop_index(long raw, int num_workspaces);

// Returns 1 only when a floating client is allowed to move/resize via ConfigureRequest
int ewmh_allows_client_configure(const Client *c);

// Returns 1 when a client can be interactively dragged/resized (blocked in fullscreen)
int ewmh_can_drag_client(const Client *c);

// Clears is_maximized when moving a maximized window to float; returns 1 if state changed
int ewmh_begin_move_clears_maximized(Client *c);

// Resolve the _NET_ACTIVE_WINDOW target for a focused client (or None if hidden/NULL)
Window ewmh_resolve_active_window(const Client *c);

// Publish _NET_ACTIVE_WINDOW on root (passing None clears active window cleanly)
void ewmh_set_active_window(Display *dpy, Window root, Window win);

// Enter or exit true fullscreen on the active monitor while preserving prior geometry/mode
void ewmh_apply_fullscreen(Display *dpy, Client *c, LayoutConfig *cfg, int enable);

#endif /* MUZU_EWMH_H */
