#include <X11/Xatom.h>
#include "workspace.h"

int workspace_clamp_index(int ws, int num_ws) {
    if (num_ws <= 0 || ws < 0 || ws >= num_ws) return -1;
    return ws;
}

void workspace_publish_ewmh(Display *dpy, Window root, int num_ws, int current_ws) {
    if (!dpy || root == None || num_ws <= 0) return;

    Atom net_num_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    Atom net_cur_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);

    unsigned long n = (unsigned long)num_ws;
    unsigned long cur = (unsigned long)(current_ws >= 0 && current_ws < num_ws ? current_ws : 0);

    XChangeProperty(dpy, root, net_num_desktops, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&n, 1);
    XChangeProperty(dpy, root, net_cur_desktop, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&cur, 1);
}

void workspace_set_client_desktop(Display *dpy, Window win, int ws) {
    if (!dpy || win == None) return;
    Atom net_wm_desktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    unsigned long val = (unsigned long)(ws >= 0 ? ws : 0);
    XChangeProperty(dpy, win, net_wm_desktop, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&val, 1);
}

static void map_client_windows(Display *dpy, Client *c) {
    if (!dpy || !c) return;
    if (c->frame != None) {
        XMapWindow(dpy, c->frame);
        if (c->win != None) XMapWindow(dpy, c->win);
    } else if (c->win != None) {
        XMapWindow(dpy, c->win);
    }
}

static void unmap_client_windows(Display *dpy, Client *c) {
    if (!dpy || !c) return;
    Window target = (c->frame != None) ? c->frame : c->win;
    if (target != None) XUnmapWindow(dpy, target);
    if (c->frame != None && c->win != None) {
        XUnmapWindow(dpy, c->win);
    }
}

int workspace_switch(Display *dpy, Window root, Client *clients, LayoutConfig *cfg,
                     int target_ws, int num_ws, Client **out_focus) {
    if (!cfg) return 0;
    if (workspace_clamp_index(target_ws, num_ws) < 0) return 0;
    if (target_ws == cfg->current_workspace) return 0;

    cfg->current_workspace = target_ws;

    Client *first_visible = NULL;
    for (Client *c = clients; c; c = c->next) {
        if (c->workspace == target_ws) {
            c->ws_hidden = 0;
            if (!c->is_minimized) {
                map_client_windows(dpy, c);
                if (!first_visible) first_visible = c;
            }
        } else {
            c->ws_hidden = 1;
            if (!c->is_minimized) {
                unmap_client_windows(dpy, c);
            }
        }
    }

    if (dpy && root != None) {
        workspace_publish_ewmh(dpy, root, num_ws, target_ws);
    }
    if (out_focus) *out_focus = first_visible;
    return 1;
}

int workspace_move_client(Display *dpy, Client *c, Client *clients, LayoutConfig *cfg,
                          int target_ws, int num_ws, Client **out_focus) {
    if (!c || !cfg) return 0;
    if (workspace_clamp_index(target_ws, num_ws) < 0) return 0;
    if (c->workspace == target_ws) return 0;

    c->workspace = target_ws;
    workspace_set_client_desktop(dpy, c->win, target_ws);

    Client *next_focus = NULL;
    if (target_ws != cfg->current_workspace) {
        c->ws_hidden = 1;
        if (!c->is_minimized) {
            unmap_client_windows(dpy, c);
        }
        for (Client *curr = clients; curr; curr = curr->next) {
            if (curr != c && !curr->is_minimized && curr->workspace == cfg->current_workspace) {
                next_focus = curr;
                break;
            }
        }
    } else {
        c->ws_hidden = 0;
        if (!c->is_minimized) {
            map_client_windows(dpy, c);
        }
        next_focus = c;
    }

    if (out_focus) *out_focus = next_focus;
    return 1;
}
