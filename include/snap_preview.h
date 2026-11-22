#ifndef MUZU_SNAP_PREVIEW_H
#define MUZU_SNAP_PREVIEW_H

#include <X11/Xlib.h>
#include "wm.h"

#define SNAP_THRESHOLD 24

typedef struct {
    Window win;
    int is_visible;
    SnapState current_snap;
    int x, y, width, height;
} SnapPreview;

SnapState snap_preview_detect_zone_in_monitor(int root_x, int root_y,
                                            int mon_x, int mon_y, int mon_w, int mon_h,
                                            int top_inset, int threshold_px);
void snap_preview_calculate_rect_in_monitor(SnapState snap,
                                            int mon_x, int mon_y, int mon_w, int mon_h,
                                            int left_inset, int right_inset, int top_inset, int bottom_inset,
                                            int *out_x, int *out_y, int *out_w, int *out_h);
void snap_preview_update_in_monitor(Display *dpy, SnapPreview *preview, SnapState snap,
                                    int mon_x, int mon_y, int mon_w, int mon_h,
                                    int left_inset, int right_inset, int top_inset, int bottom_inset);

SnapState snap_preview_detect_zone(int root_x, int root_y, int screen_w, int top_inset, int threshold_px);
void snap_preview_calculate_rect(SnapState snap, int screen_w, int screen_h,
                                 int left_inset, int right_inset, int top_inset, int bottom_inset,
                                 int *out_x, int *out_y, int *out_w, int *out_h);
void snap_preview_init(Display *dpy, Window root, SnapPreview *preview);
void snap_preview_update(Display *dpy, SnapPreview *preview, SnapState snap,
                         int screen_w, int screen_h,
                         int left_inset, int right_inset, int top_inset, int bottom_inset);
void snap_preview_hide(Display *dpy, SnapPreview *preview);
void snap_preview_destroy(Display *dpy, SnapPreview *preview);

#endif /* MUZU_SNAP_PREVIEW_H */
