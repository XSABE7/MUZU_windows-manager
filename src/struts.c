#include <stdlib.h>
#include <string.h>
#include "struts.h"

#define STRUT_MAX_VALUE 32768UL

int strut_clamp_value(unsigned long raw) {
    if (raw > STRUT_MAX_VALUE) return (int)STRUT_MAX_VALUE;
    return (int)raw;
}

void strut_list_add_or_update(StrutList **head, Window win, const StrutPartial *strut) {
    if (!head || !strut) return;
    for (StrutList *curr = *head; curr; curr = curr->next) {
        if (curr->win == win) {
            curr->strut = *strut;
            return;
        }
    }
    StrutList *node = malloc(sizeof(StrutList));
    if (!node) return;
    node->win = win;
    node->strut = *strut;
    node->next = *head;
    *head = node;
}

void strut_list_remove(StrutList **head, Window win) {
    if (!head || !*head) return;
    StrutList **curr = head;
    while (*curr) {
        if ((*curr)->win == win) {
            StrutList *to_free = *curr;
            *curr = (*curr)->next;
            free(to_free);
            return;
        }
        curr = &(*curr)->next;
    }
}

void strut_calculate_insets(const StrutList *head, int screen_w, int screen_h,
                            int *left, int *right, int *top, int *bottom) {
    int max_l = 0, max_r = 0, max_t = 0, max_b = 0;
    for (const StrutList *curr = head; curr; curr = curr->next) {
        if (curr->strut.left > max_l) max_l = curr->strut.left;
        if (curr->strut.right > max_r) max_r = curr->strut.right;
        if (curr->strut.top > max_t) max_t = curr->strut.top;
        if (curr->strut.bottom > max_b) max_b = curr->strut.bottom;
    }

    if (screen_w > 0) {
        if (max_l > screen_w) max_l = screen_w;
        if (max_r > screen_w) max_r = screen_w;
        if (max_l + max_r > screen_w) {
            max_l = screen_w / 2;
            max_r = screen_w / 2;
        }
    }
    if (screen_h > 0) {
        if (max_t > screen_h) max_t = screen_h;
        if (max_b > screen_h) max_b = screen_h;
        if (max_t + max_b > screen_h) {
            max_t = screen_h / 2;
            max_b = screen_h / 2;
        }
    }

    if (left) *left = max_l;
    if (right) *right = max_r;
    if (top) *top = max_t;
    if (bottom) *bottom = max_b;
}

int strut_read_window_property(Display *dpy, Window win, StrutPartial *out_strut) {
    if (!dpy || win == None || !out_strut) return 0;
    memset(out_strut, 0, sizeof(StrutPartial));

    Atom net_wm_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    Atom net_wm_strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    // Try _NET_WM_STRUT_PARTIAL (12 cardinals)
    if (XGetWindowProperty(dpy, win, net_wm_strut_partial, 0, 12, False,
                           XA_CARDINAL, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        if (nitems >= 12 && actual_format == 32) {
            const unsigned long *v = (const unsigned long *)prop;
            out_strut->left = strut_clamp_value(v[0]);
            out_strut->right = strut_clamp_value(v[1]);
            out_strut->top = strut_clamp_value(v[2]);
            out_strut->bottom = strut_clamp_value(v[3]);
            out_strut->left_start_y = strut_clamp_value(v[4]);
            out_strut->left_end_y = strut_clamp_value(v[5]);
            out_strut->right_start_y = strut_clamp_value(v[6]);
            out_strut->right_end_y = strut_clamp_value(v[7]);
            out_strut->top_start_x = strut_clamp_value(v[8]);
            out_strut->top_end_x = strut_clamp_value(v[9]);
            out_strut->bottom_start_x = strut_clamp_value(v[10]);
            out_strut->bottom_end_x = strut_clamp_value(v[11]);
            XFree(prop);
            return 1;
        }
        XFree(prop);
    }

    // Fallback to legacy _NET_WM_STRUT (4 cardinals)
    if (XGetWindowProperty(dpy, win, net_wm_strut, 0, 4, False,
                           XA_CARDINAL, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        if (nitems >= 4 && actual_format == 32) {
            const unsigned long *v = (const unsigned long *)prop;
            out_strut->left = strut_clamp_value(v[0]);
            out_strut->right = strut_clamp_value(v[1]);
            out_strut->top = strut_clamp_value(v[2]);
            out_strut->bottom = strut_clamp_value(v[3]);
            XFree(prop);
            return 1;
        }
        XFree(prop);
    }

    return 0;
}

int strut_build_workarea_array(int wa_x, int wa_y, int wa_w, int wa_h,
                               int num_desktops, unsigned long *out_buf, int max_elems) {
    if (!out_buf || max_elems < 4) return 0;
    if (wa_w < 100) wa_w = 100;
    if (wa_h < 100) wa_h = 100;
    int count = (num_desktops > 0) ? num_desktops : 1;
    int max_desktops = max_elems / 4;
    if (count > max_desktops) count = max_desktops;

    unsigned long x = (unsigned long)(wa_x > 0 ? wa_x : 0);
    unsigned long y = (unsigned long)(wa_y > 0 ? wa_y : 0);
    unsigned long w = (unsigned long)wa_w;
    unsigned long h = (unsigned long)wa_h;

    for (int i = 0; i < count; i++) {
        out_buf[i * 4 + 0] = x;
        out_buf[i * 4 + 1] = y;
        out_buf[i * 4 + 2] = w;
        out_buf[i * 4 + 3] = h;
    }
    return count * 4;
}

void strut_apply_workarea_rect_desktops(Display *dpy, Window root,
                                        int wa_x, int wa_y, int wa_w, int wa_h,
                                        int num_desktops) {
    if (!dpy || root == None) return;

    Atom net_workarea = XInternAtom(dpy, "_NET_WORKAREA", False);
    unsigned long workarea[64];
    int nelems = strut_build_workarea_array(wa_x, wa_y, wa_w, wa_h, num_desktops, workarea, 64);
    if (nelems > 0) {
        XChangeProperty(dpy, root, net_workarea, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)workarea, nelems);
    }
}

void strut_apply_workarea_rect(Display *dpy, Window root, int wa_x, int wa_y, int wa_w, int wa_h) {
    strut_apply_workarea_rect_desktops(dpy, root, wa_x, wa_y, wa_w, wa_h, 1);
}

void strut_apply_workarea(Display *dpy, Window root, int screen_w, int screen_h,
                          int left, int right, int top, int bottom,
                          LayoutConfig *cfg) {
    if (cfg) {
        cfg->top_inset = top;
        cfg->bottom_inset = bottom;
    }
    if (!dpy || root == None) return;

    int wa_x = left;
    int wa_y = top;
    int wa_w = screen_w - left - right;
    int wa_h = screen_h - top - bottom;
    strut_apply_workarea_rect(dpy, root, wa_x, wa_y, wa_w, wa_h);
}
