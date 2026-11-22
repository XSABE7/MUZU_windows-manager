#include <X11/Xatom.h>
#include "ewmh.h"

typedef struct {
    Display *dpy;
    Atom net_wm_state;
    Atom net_wm_state_fs;
    Atom net_wm_state_max_h;
    Atom net_wm_state_max_v;
    Atom net_wm_state_hidden;
    Atom net_wm_state_focused;
    Atom net_frame_extents;
    Atom net_allowed_actions;
    Atom net_action_move;
    Atom net_action_resize;
    Atom net_action_min;
    Atom net_action_max_h;
    Atom net_action_max_v;
    Atom net_action_fs;
    Atom net_action_close;
    Atom net_action_desk;
    Atom wm_state;
    Atom net_active;
} EwmhAtoms;

static EwmhAtoms g_ewmh_atoms;

static const EwmhAtoms *ewmh_atoms(Display *dpy) {
    if (g_ewmh_atoms.dpy != dpy) {
        g_ewmh_atoms.dpy = dpy;
        g_ewmh_atoms.net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
        g_ewmh_atoms.net_wm_state_fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
        g_ewmh_atoms.net_wm_state_max_h = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        g_ewmh_atoms.net_wm_state_max_v = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
        g_ewmh_atoms.net_wm_state_hidden = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
        g_ewmh_atoms.net_wm_state_focused = XInternAtom(dpy, "_NET_WM_STATE_FOCUSED", False);
        g_ewmh_atoms.net_frame_extents = XInternAtom(dpy, "_NET_FRAME_EXTENTS", False);
        g_ewmh_atoms.net_allowed_actions = XInternAtom(dpy, "_NET_WM_ALLOWED_ACTIONS", False);
        g_ewmh_atoms.net_action_move = XInternAtom(dpy, "_NET_WM_ACTION_MOVE", False);
        g_ewmh_atoms.net_action_resize = XInternAtom(dpy, "_NET_WM_ACTION_RESIZE", False);
        g_ewmh_atoms.net_action_min = XInternAtom(dpy, "_NET_WM_ACTION_MINIMIZE", False);
        g_ewmh_atoms.net_action_max_h = XInternAtom(dpy, "_NET_WM_ACTION_MAXIMIZE_HORZ", False);
        g_ewmh_atoms.net_action_max_v = XInternAtom(dpy, "_NET_WM_ACTION_MAXIMIZE_VERT", False);
        g_ewmh_atoms.net_action_fs = XInternAtom(dpy, "_NET_WM_ACTION_FULLSCREEN", False);
        g_ewmh_atoms.net_action_close = XInternAtom(dpy, "_NET_WM_ACTION_CLOSE", False);
        g_ewmh_atoms.net_action_desk = XInternAtom(dpy, "_NET_WM_ACTION_CHANGE_DESKTOP", False);
        g_ewmh_atoms.wm_state = XInternAtom(dpy, "WM_STATE", False);
        g_ewmh_atoms.net_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    }
    return &g_ewmh_atoms;
}

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

void ewmh_sync_client_state_ex(Display *dpy, Window win, int is_fullscreen, int is_maximized,
                               int is_minimized, int is_focused) {
    if (!dpy || win == None) return;

    const EwmhAtoms *at = ewmh_atoms(dpy);
    Atom atoms[5];
    int count = 0;

    if (is_fullscreen) {
        atoms[count++] = at->net_wm_state_fs;
    }
    if (is_maximized) {
        atoms[count++] = at->net_wm_state_max_h;
        atoms[count++] = at->net_wm_state_max_v;
    }
    if (is_minimized) {
        atoms[count++] = at->net_wm_state_hidden;
    }
    if (is_focused && !is_minimized) {
        atoms[count++] = at->net_wm_state_focused;
    }

    if (count == 0) {
        XDeleteProperty(dpy, win, at->net_wm_state);
    } else {
        XChangeProperty(dpy, win, at->net_wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)atoms, count);
    }
}

void ewmh_sync_client_state(Display *dpy, Window win, int is_fullscreen, int is_maximized, int is_minimized) {
    ewmh_sync_client_state_ex(dpy, win, is_fullscreen, is_maximized, is_minimized, 0);
}

void ewmh_set_frame_extents(Display *dpy, Window win, int left, int right, int top, int bottom) {
    if (!dpy || win == None) return;
    const EwmhAtoms *at = ewmh_atoms(dpy);
    unsigned long extents[4] = {
        (unsigned long)(left > 0 ? left : 0),
        (unsigned long)(right > 0 ? right : 0),
        (unsigned long)(top > 0 ? top : 0),
        (unsigned long)(bottom > 0 ? bottom : 0)
    };
    XChangeProperty(dpy, win, at->net_frame_extents, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)extents, 4);
}

void ewmh_set_allowed_actions(Display *dpy, Window win, int can_fullscreen) {
    if (!dpy || win == None) return;
    const EwmhAtoms *at = ewmh_atoms(dpy);
    Atom atoms[8];
    int count = 0;
    atoms[count++] = at->net_action_move;
    atoms[count++] = at->net_action_min;
    atoms[count++] = at->net_action_close;
    atoms[count++] = at->net_action_desk;
    if (can_fullscreen) {
        atoms[count++] = at->net_action_resize;
        atoms[count++] = at->net_action_max_h;
        atoms[count++] = at->net_action_max_v;
        atoms[count++] = at->net_action_fs;
    }
    XChangeProperty(dpy, win, at->net_allowed_actions, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)atoms, count);
}

void ewmh_set_wm_state(Display *dpy, Window win, long state) {
    if (!dpy || win == None) return;
    const EwmhAtoms *at = ewmh_atoms(dpy);
    unsigned long data[2] = { (unsigned long)state, (unsigned long)None };
    XChangeProperty(dpy, win, at->wm_state, at->wm_state, 32, PropModeReplace,
                    (unsigned char *)data, 2);
}

int ewmh_window_has_maximized_state(Display *dpy, Window win) {
    if (!dpy || win == None) return 0;

    const EwmhAtoms *at = ewmh_atoms(dpy);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    int found = 0;

    if (XGetWindowProperty(dpy, win, at->net_wm_state, 0, (~0L), False,
                           XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        if (ewmh_atoms_contain(prop, nitems, actual_type, actual_format, at->net_wm_state_max_h) ||
            ewmh_atoms_contain(prop, nitems, actual_type, actual_format, at->net_wm_state_max_v)) {
            found = 1;
        }
        XFree(prop);
    }
    return found;
}

int ewmh_atoms_contain(const unsigned char *prop, unsigned long nitems,
                       Atom actual_type, int actual_format, Atom target) {
    if (!prop || nitems == 0 || target == None) return 0;
    if (actual_type != XA_ATOM || actual_format != 32) return 0;

    const Atom *atoms = (const Atom *)prop;
    for (unsigned long i = 0; i < nitems; i++) {
        if (atoms[i] == target) return 1;
    }
    return 0;
}

int ewmh_parse_desktop_index(long raw, int num_workspaces) {
    if (num_workspaces <= 0 || raw < 0 || raw >= (long)num_workspaces) return -1;
    return (int)raw;
}

int ewmh_allows_client_configure(const Client *c) {
    if (!c) return 0;
    return (c->mode == CLIENT_FLOATING && !c->is_maximized && !c->is_fullscreen) ? 1 : 0;
}

int ewmh_can_drag_client(const Client *c) {
    if (!c || c->is_fullscreen) return 0;
    return 1;
}

int ewmh_begin_move_clears_maximized(Client *c) {
    if (!c || !c->is_maximized) return 0;
    c->is_maximized = 0;
    return 1;
}

int ewmh_window_has_fullscreen_state(Display *dpy, Window win) {
    if (!dpy || win == None) return 0;

    const EwmhAtoms *at = ewmh_atoms(dpy);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    int found = 0;

    if (XGetWindowProperty(dpy, win, at->net_wm_state, 0, (~0L), False,
                           XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        found = ewmh_atoms_contain(prop, nitems, actual_type, actual_format, at->net_wm_state_fs);
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
        c->x = cfg->mon_x;
        c->y = cfg->mon_y;
        c->width = cfg->screen_width;
        c->height = cfg->screen_height;

        if (dpy && c->win != None) {
            XSetWindowBorderWidth(dpy, c->win, 0);
            XMoveResizeWindow(dpy, c->win, c->x, c->y, c->width, c->height);
            XRaiseWindow(dpy, c->win);
            ewmh_sync_client_state_ex(dpy, c->win, c->is_fullscreen, c->is_maximized,
                                      c->is_minimized, c->is_focused);
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
            int bw = (cfg->border_width > 0) ? cfg->border_width : 0;
            XSetWindowBorderWidth(dpy, c->win, (unsigned int)bw);
            XMoveResizeWindow(dpy, c->win, c->x, c->y, c->width, c->height);
            ewmh_sync_client_state_ex(dpy, c->win, c->is_fullscreen, c->is_maximized,
                                      c->is_minimized, c->is_focused);
        }
    }
}

Window ewmh_resolve_active_window(const Client *c) {
    if (!c || c->is_minimized || c->ws_hidden) return None;
    return c->win;
}

void ewmh_set_active_window(Display *dpy, Window root, Window win) {
    if (!dpy || root == None) return;
    const EwmhAtoms *at = ewmh_atoms(dpy);
    XChangeProperty(dpy, root, at->net_active, XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&win, 1);
}
