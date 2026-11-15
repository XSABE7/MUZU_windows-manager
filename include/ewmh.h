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

// Enter or exit true fullscreen on the active monitor while preserving prior geometry/mode
void ewmh_apply_fullscreen(Display *dpy, Client *c, LayoutConfig *cfg, int enable);

#endif /* MUZU_EWMH_H */
