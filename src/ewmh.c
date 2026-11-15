#include <X11/Xatom.h>
#include "ewmh.h"
#include "titlebar.h"

int ewmh_resolve_state_action(long action, int current_val) {
    if (action == 0) return 0; // _NET_WM_STATE_REMOVE
    if (action == 1) return 1; // _NET_WM_STATE_ADD
    if (action == 2) return current_val ? 0 : 1; // _NET_WM_STATE_TOGGLE
    return current_val;
}

EwmhStateFlags ewmh_parse_state_atoms(Atom prop1, Atom prop2,
                                      Atom a_fs, Atom a_max_h, Atom a_max_v, Atom a_hidden) {
    EwmhStateFlags flags = {0, 0, 0, 0};
    Atom props[2] = { prop1, prop2 };
    for (int i = 0; i < 2; i++) {
        Atom p = props[i];
        if (p == None) continue;
        if (a_fs != None && p == a_fs) flags.has_fullscreen = 1;
        if (a_max_h != None && p == a_max_h) flags.has_max_horz = 1;
        if (a_max_v != None && p == a_max_v) flags.has_max_vert = 1;
        if (a_hidden != None && p == a_hidden) flags.has_hidden = 1;
    }
    return flags;
}

void ewmh_sync_client_state(Display *dpy, Window win, int is_fullscreen, int is_maximized, int is_minimized) {
    if (!dpy || win == None) return;

    Atom net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom atoms[4];
    int count = 0;

    if (is_fullscreen) {
        atoms[count++] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    }
    if (is_maximized) {
        atoms[count++] = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        atoms[count++] = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    }
    if (is_minimized) {
        atoms[count++] = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
    }

    if (count == 0) {
        XDeleteProperty(dpy, win, net_wm_state);
    } else {
        XChangeProperty(dpy, win, net_wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)atoms, count);
    }
}

int ewmh_window_has_fullscreen_state(Display *dpy, Window win) {
    if (!dpy || win == None) return 0;

    Atom net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom net_wm_fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    int found = 0;

    if (XGetWindowProperty(dpy, win, net_wm_state, 0, (~0L), False,
                           XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        Atom *atoms = (Atom *)prop;
        for (unsigned long i = 0; i < nitems; i++) {
            if (atoms[i] == net_wm_fs) {
                found = 1;
                break;
            }
        }
        XFree(prop);
    }
    return found;
}

void ewmh_apply_fullscreen(Display *dpy, Client *c, LayoutConfig *cfg, int enable) {
    if (!c || !cfg) return;

    if (enable && !c->is_fullscreen) {
        c->saved_fs_x = c->x;
        c->saved_fs_y = c->y;
        c->saved_fs_w = c->width;
        c->saved_fs_h = c->height;
        c->saved_fs_mode = c->mode;
        c->saved_fs_snap = c->snap_state;
        c->saved_fs_maximized = c->is_maximized;

        c->is_fullscreen = 1;
        c->mode = CLIENT_FLOATING;
        c->snap_state = SNAP_NONE;
        c->x = 0;
        c->y = 0;
        c->width = cfg->screen_width;
        c->height = cfg->screen_height;

        if (dpy && c->win != None) {
            if (c->frame != None) {
                XMoveResizeWindow(dpy, c->frame, c->x, c->y, c->width, c->height);
                XMoveResizeWindow(dpy, c->win, 0, 0, c->width, c->height);
                XRaiseWindow(dpy, c->frame);
            } else {
                XMoveResizeWindow(dpy, c->win, c->x, c->y, c->width, c->height);
                XRaiseWindow(dpy, c->win);
            }
            ewmh_sync_client_state(dpy, c->win, c->is_fullscreen, c->is_maximized, c->is_minimized);
        }
    } else if (!enable && c->is_fullscreen) {
        c->is_fullscreen = 0;
        c->x = c->saved_fs_x;
        c->y = c->saved_fs_y;
        c->width = (c->saved_fs_w > 10) ? c->saved_fs_w : 600;
        c->height = (c->saved_fs_h > 10) ? c->saved_fs_h : 400;
        c->mode = c->saved_fs_mode;
        c->snap_state = c->saved_fs_snap;
        c->is_maximized = c->saved_fs_maximized;

        if (dpy && c->win != None) {
            if (c->frame != None) {
                int ch = c->height - TITLEBAR_HEIGHT;
                if (ch < 10) ch = 10;
                XMoveResizeWindow(dpy, c->frame, c->x, c->y, c->width, c->height);
                XMoveResizeWindow(dpy, c->win, 0, TITLEBAR_HEIGHT, c->width, ch);
            } else {
                XMoveResizeWindow(dpy, c->win, c->x, c->y, c->width, c->height);
            }
            ewmh_sync_client_state(dpy, c->win, c->is_fullscreen, c->is_maximized, c->is_minimized);
        }
    }
}
