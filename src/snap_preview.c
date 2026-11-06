#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "snap_preview.h"

SnapState snap_preview_detect_zone(int root_x, int root_y, int screen_w, int top_inset, int threshold_px) {
    int th = threshold_px > 0 ? threshold_px : SNAP_THRESHOLD;
    if (root_y <= top_inset + th) {
        return SNAP_TOP;
    }
    if (root_x <= th) {
        return SNAP_LEFT;
    }
    if (root_x >= screen_w - th) {
        return SNAP_RIGHT;
    }
    return SNAP_NONE;
}

void snap_preview_calculate_rect(SnapState snap, int screen_w, int screen_h,
                                 int left_inset, int right_inset, int top_inset, int bottom_inset,
                                 int *out_x, int *out_y, int *out_w, int *out_h) {
    int avail_w = screen_w - left_inset - right_inset;
    int avail_h = screen_h - top_inset - bottom_inset;
    if (avail_w < 100) avail_w = 100;
    if (avail_h < 100) avail_h = 100;

    int x = 0, y = 0, w = 0, h = 0;

    switch (snap) {
        case SNAP_LEFT:
            x = left_inset;
            y = top_inset;
            w = avail_w / 2;
            h = avail_h;
            break;
        case SNAP_RIGHT:
            x = left_inset + (avail_w / 2);
            y = top_inset;
            w = avail_w - (avail_w / 2);
            h = avail_h;
            break;
        case SNAP_TOP:
            x = left_inset;
            y = top_inset;
            w = avail_w;
            h = avail_h;
            break;
        case SNAP_NONE:
        default:
            break;
    }

    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

void snap_preview_init(Display *dpy, Window root, SnapPreview *preview) {
    if (!preview) return;
    preview->win = None;
    preview->is_visible = 0;
    preview->current_snap = SNAP_NONE;
    preview->x = 0;
    preview->y = 0;
    preview->width = 100;
    preview->height = 100;

    if (!dpy || root == None) return;

    int screen = DefaultScreen(dpy);
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = BlackPixel(dpy, screen);
    attrs.border_pixel = WhitePixel(dpy, screen);

    preview->win = XCreateWindow(dpy, root, 0, 0, 100, 100, 2,
                                 CopyFromParent, InputOutput, CopyFromParent,
                                 CWOverrideRedirect | CWBackPixel | CWBorderPixel,
                                 &attrs);

    // Set 40% opacity via _NET_WM_WINDOW_OPACITY if compositor is active
    Atom net_wm_opacity = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long opacity = (unsigned long)(0.40 * 0xFFFFFFFF);
    XChangeProperty(dpy, preview->win, net_wm_opacity, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&opacity, 1);
}

void snap_preview_update(Display *dpy, SnapPreview *preview, SnapState snap,
                         int screen_w, int screen_h,
                         int left_inset, int right_inset, int top_inset, int bottom_inset) {
    if (!preview) return;
    if (snap == SNAP_NONE) {
        snap_preview_hide(dpy, preview);
        return;
    }

    int x = 0, y = 0, w = 0, h = 0;
    snap_preview_calculate_rect(snap, screen_w, screen_h,
                                left_inset, right_inset, top_inset, bottom_inset,
                                &x, &y, &w, &h);

    preview->current_snap = snap;
    preview->x = x;
    preview->y = y;
    preview->width = w;
    preview->height = h;

    if (dpy && preview->win != None) {
        XMoveResizeWindow(dpy, preview->win, x, y, w, h);
        if (!preview->is_visible) {
            XMapRaised(dpy, preview->win);
            preview->is_visible = 1;
        } else {
            XRaiseWindow(dpy, preview->win);
        }
    }
}

void snap_preview_hide(Display *dpy, SnapPreview *preview) {
    if (!preview) return;
    if (dpy && preview->win != None && preview->is_visible) {
        XUnmapWindow(dpy, preview->win);
    }
    preview->is_visible = 0;
    preview->current_snap = SNAP_NONE;
}

void snap_preview_destroy(Display *dpy, SnapPreview *preview) {
    if (!preview) return;
    snap_preview_hide(dpy, preview);
    if (dpy && preview->win != None) {
        XDestroyWindow(dpy, preview->win);
        preview->win = None;
    }
}
