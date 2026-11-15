#ifndef MUZU_WORKSPACE_H
#define MUZU_WORKSPACE_H

#include <X11/Xlib.h>
#include "wm.h"

#define DEFAULT_WORKSPACE_COUNT 4
#define MAX_WORKSPACE_COUNT 10

// Return ws if 0 <= ws < num_ws, otherwise -1
int workspace_clamp_index(int ws, int num_ws);

// Publish _NET_NUMBER_OF_DESKTOPS and _NET_CURRENT_DESKTOP on root
void workspace_publish_ewmh(Display *dpy, Window root, int num_ws, int current_ws);

// Publish _NET_WM_DESKTOP on a client window
void workspace_set_client_desktop(Display *dpy, Window win, int ws);

// Switch active workspace, hiding inactive-workspace clients and showing target-workspace clients
int workspace_switch(Display *dpy, Window root, Client *clients, LayoutConfig *cfg,
                     int target_ws, int num_ws, Client **out_focus);

// Move a single client to target_ws, hiding it if target_ws != cfg->current_workspace
int workspace_move_client(Display *dpy, Client *c, Client *clients, LayoutConfig *cfg,
                          int target_ws, int num_ws, Client **out_focus);

#endif /* MUZU_WORKSPACE_H */
