#include <string.h>
#include <X11/extensions/Xrandr.h>
#include "monitors.h"

int monitors_deduplicate(Monitor *mons, int count) {
    if (!mons || count <= 0) return 0;
    int out = 0;
    for (int i = 0; i < count; i++) {
        if (mons[i].width <= 0 || mons[i].height <= 0) continue;
        int dup_idx = -1;
        for (int j = 0; j < out; j++) {
            if (mons[j].x == mons[i].x && mons[j].y == mons[i].y &&
                mons[j].width == mons[i].width && mons[j].height == mons[i].height) {
                dup_idx = j;
                break;
            }
        }
        if (dup_idx >= 0) {
            if (mons[i].primary) mons[dup_idx].primary = 1;
        } else {
            mons[out++] = mons[i];
        }
    }
    return out;
}

int monitors_query_xrandr(Display *dpy, Window root, Monitor *out, int max_monitors) {
    if (!dpy || root == None || !out || max_monitors <= 0) return 0;

    int rr_ev = 0, rr_err = 0;
    int count = 0;

    if (XRRQueryExtension(dpy, &rr_ev, &rr_err)) {
        XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
        if (!res) res = XRRGetScreenResources(dpy, root);
        if (res) {
            RROutput primary_out = XRRGetOutputPrimary(dpy, root);
            RRCrtc primary_crtc = None;
            if (primary_out != None) {
                XRROutputInfo *oi = XRRGetOutputInfo(dpy, res, primary_out);
                if (oi) {
                    if (oi->connection == RR_Connected) {
                        primary_crtc = oi->crtc;
                    }
                    XRRFreeOutputInfo(oi);
                }
            }

            for (int i = 0; i < res->ncrtc && count < max_monitors; i++) {
                XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[i]);
                if (!ci) continue;
                if (ci->mode != None && ci->width > 0 && ci->height > 0) {
                    Monitor m;
                    memset(&m, 0, sizeof(m));
                    m.x = ci->x;
                    m.y = ci->y;
                    m.width = (int)ci->width;
                    m.height = (int)ci->height;
                    m.primary = (res->crtcs[i] == primary_crtc) ? 1 : 0;
                    out[count++] = m;
                }
                XRRFreeCrtcInfo(ci);
            }
            XRRFreeScreenResources(res);
            count = monitors_deduplicate(out, count);
        }
    }

    if (count == 0) {
        int scr = DefaultScreen(dpy);
        memset(&out[0], 0, sizeof(Monitor));
        out[0].x = 0;
        out[0].y = 0;
        out[0].width = DisplayWidth(dpy, scr);
        out[0].height = DisplayHeight(dpy, scr);
        out[0].primary = 1;
        count = 1;
    }

    return count;
}

static int fallback_monitor_index(const Monitor *mons, int count) {
    for (int i = 0; i < count; i++) {
        if (mons[i].primary) return i;
    }
    return 0;
}

int monitor_find_for_point(const Monitor *mons, int count, int px, int py) {
    if (!mons || count <= 0) return 0;
    for (int i = 0; i < count; i++) {
        if (px >= mons[i].x && px < mons[i].x + mons[i].width &&
            py >= mons[i].y && py < mons[i].y + mons[i].height) {
            return i;
        }
    }
    return fallback_monitor_index(mons, count);
}

int monitor_find_for_rect(const Monitor *mons, int count, int rx, int ry, int rw, int rh) {
    if (!mons || count <= 0) return 0;
    if (rw <= 0) rw = 1;
    if (rh <= 0) rh = 1;

    long best_area = -1;
    int best_idx = fallback_monitor_index(mons, count);

    for (int i = 0; i < count; i++) {
        int x1 = rx > mons[i].x ? rx : mons[i].x;
        int y1 = ry > mons[i].y ? ry : mons[i].y;
        int x2 = (rx + rw) < (mons[i].x + mons[i].width) ? (rx + rw) : (mons[i].x + mons[i].width);
        int y2 = (ry + rh) < (mons[i].y + mons[i].height) ? (ry + rh) : (mons[i].y + mons[i].height);
        if (x2 > x1 && y2 > y1) {
            long area = (long)(x2 - x1) * (long)(y2 - y1);
            if (area > best_area) {
                best_area = area;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

static int ranges_overlap(int a1, int a2, int b1, int b2) {
    return (a1 <= b2) && (b1 <= a2);
}

void monitor_apply_struts(Monitor *mons, int count, const StrutList *struts, int total_w, int total_h) {
    if (!mons || count <= 0) return;

    for (int i = 0; i < count; i++) {
        mons[i].left_inset = 0;
        mons[i].right_inset = 0;
        mons[i].top_inset = 0;
        mons[i].bottom_inset = 0;
    }

    for (const StrutList *s = struts; s; s = s->next) {
        const StrutPartial *sp = &s->strut;
        for (int i = 0; i < count; i++) {
            int mx1 = mons[i].x;
            int mx2 = mons[i].x + mons[i].width - 1;
            int my1 = mons[i].y;
            int my2 = mons[i].y + mons[i].height - 1;

            if (sp->top > 0) {
                int has_partial = (sp->top_end_x > sp->top_start_x);
                int match = has_partial ? ranges_overlap(mx1, mx2, sp->top_start_x, sp->top_end_x)
                                        : (mons[i].y == 0);
                if (match && sp->top > mons[i].top_inset) {
                    int local_top = sp->top - mons[i].y;
                    if (local_top > mons[i].top_inset) mons[i].top_inset = local_top;
                }
            }
            if (sp->bottom > 0) {
                int has_partial = (sp->bottom_end_x > sp->bottom_start_x);
                int match = has_partial ? ranges_overlap(mx1, mx2, sp->bottom_start_x, sp->bottom_end_x)
                                        : (total_h <= 0 || mons[i].y + mons[i].height >= total_h);
                if (match) {
                    int dist_from_bottom = (total_h > 0) ? (total_h - (mons[i].y + mons[i].height)) : 0;
                    int local_bottom = sp->bottom - dist_from_bottom;
                    if (local_bottom > mons[i].bottom_inset) mons[i].bottom_inset = local_bottom;
                }
            }
            if (sp->left > 0) {
                int has_partial = (sp->left_end_y > sp->left_start_y);
                int match = has_partial ? ranges_overlap(my1, my2, sp->left_start_y, sp->left_end_y)
                                        : (mons[i].x == 0);
                if (match) {
                    int local_left = sp->left - mons[i].x;
                    if (local_left > mons[i].left_inset) mons[i].left_inset = local_left;
                }
            }
            if (sp->right > 0) {
                int has_partial = (sp->right_end_y > sp->right_start_y);
                int match = has_partial ? ranges_overlap(my1, my2, sp->right_start_y, sp->right_end_y)
                                        : (total_w <= 0 || mons[i].x + mons[i].width >= total_w);
                if (match) {
                    int dist_from_right = (total_w > 0) ? (total_w - (mons[i].x + mons[i].width)) : 0;
                    int local_right = sp->right - dist_from_right;
                    if (local_right > mons[i].right_inset) mons[i].right_inset = local_right;
                }
            }
        }
    }

    for (int i = 0; i < count; i++) {
        if (mons[i].left_inset < 0) mons[i].left_inset = 0;
        if (mons[i].right_inset < 0) mons[i].right_inset = 0;
        if (mons[i].top_inset < 0) mons[i].top_inset = 0;
        if (mons[i].bottom_inset < 0) mons[i].bottom_inset = 0;
        if (mons[i].left_inset + mons[i].right_inset > mons[i].width - 64 && mons[i].width > 64) {
            mons[i].left_inset = mons[i].width / 4;
            mons[i].right_inset = mons[i].width / 4;
        }
        if (mons[i].top_inset + mons[i].bottom_inset > mons[i].height - 64 && mons[i].height > 64) {
            mons[i].top_inset = mons[i].height / 4;
            mons[i].bottom_inset = mons[i].height / 4;
        }
    }
}

void monitor_to_layout_config(const Monitor *mon, int master_count, int current_workspace, LayoutConfig *out_cfg) {
    if (!mon || !out_cfg) return;
    out_cfg->mon_x = mon->x;
    out_cfg->mon_y = mon->y;
    out_cfg->screen_width = mon->width;
    out_cfg->screen_height = mon->height;
    out_cfg->left_inset = mon->left_inset;
    out_cfg->right_inset = mon->right_inset;
    out_cfg->top_inset = mon->top_inset;
    out_cfg->bottom_inset = mon->bottom_inset;
    out_cfg->master_count = master_count;
    out_cfg->current_workspace = current_workspace;
    out_cfg->border_width = 0;
}
