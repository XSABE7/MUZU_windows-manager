#ifndef MUZU_MONITORS_H
#define MUZU_MONITORS_H

#include <X11/Xlib.h>
#include "wm.h"
#include "struts.h"

#define MAX_MONITORS 16

typedef struct {
    int x, y, width, height;
    int left_inset, right_inset, top_inset, bottom_inset;
    int primary;
} Monitor;

// Remove invalid (<= 0) entries and collapse mirrored/identical rectangles (preserving primary)
int monitors_deduplicate(Monitor *mons, int count);

// Query active XRandR CRTCs; falls back to DisplayWidth/DisplayHeight if XRandR unavailable
int monitors_query_xrandr(Display *dpy, Window root, Monitor *out, int max_monitors);

// Return index of monitor containing (px, py), or primary/0 fallback
int monitor_find_for_point(const Monitor *mons, int count, int px, int py);

// Return index of monitor with largest intersection area with rectangle, or primary/0 fallback
int monitor_find_for_rect(const Monitor *mons, int count, int rx, int ry, int rw, int rh);

// Compute per-monitor insets from _NET_WM_STRUT_PARTIAL / _NET_WM_STRUT list
void monitor_apply_struts(Monitor *mons, int count, const StrutList *struts, int total_w, int total_h);

// Populate a LayoutConfig scoped to a single monitor
void monitor_to_layout_config(const Monitor *mon, int master_count, int current_workspace, LayoutConfig *out_cfg);

#endif /* MUZU_MONITORS_H */
