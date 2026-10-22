#ifndef MUZU_MISSION_VIEWER_H
#define MUZU_MISSION_VIEWER_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "wm.h"
#include "gtk_theme.h"

#define MISSION_MAX_SLOTS 64

typedef struct {
    Window win;
    Client *client;
    char title[256];
    cairo_surface_t *icon;
    int x, y, w, h;
    int is_hovered;
} MissionSlot;

typedef struct {
    Window win;
    cairo_surface_t *surface;
    int is_visible;
    int screen_w, screen_h;
    MissionSlot slots[MISSION_MAX_SLOTS];
    int slot_count;
    int selected_idx;
    int hover_idx;
    const GtkTheme *theme;
} MissionViewer;

void mission_viewer_init(Display *dpy, MissionViewer *mv, const GtkTheme *theme);
void mission_viewer_populate(Display *dpy, MissionViewer *mv, Client *clients, int current_workspace);
void mission_viewer_calculate_layout(MissionViewer *mv, int screen_w, int screen_h);
int mission_viewer_get_slot_at(const MissionViewer *mv, int x, int y);
void mission_viewer_select_next(MissionViewer *mv);
void mission_viewer_select_prev(MissionViewer *mv);
Client *mission_viewer_get_selected(const MissionViewer *mv);
void mission_viewer_render(Display *dpy, MissionViewer *mv);
void mission_viewer_show(Display *dpy, MissionViewer *mv, int screen_w, int screen_h);
void mission_viewer_hide(Display *dpy, MissionViewer *mv);
void mission_viewer_cleanup(MissionViewer *mv);

#endif /* MUZU_MISSION_VIEWER_H */
