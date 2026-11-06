#include <stdlib.h>
#include <string.h>
#include "struts.h"

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
            out_strut->left = (int)v[0];
            out_strut->right = (int)v[1];
            out_strut->top = (int)v[2];
            out_strut->bottom = (int)v[3];
            out_strut->left_start_y = (int)v[4];
            out_strut->left_end_y = (int)v[5];
            out_strut->right_start_y = (int)v[6];
            out_strut->right_end_y = (int)v[7];
            out_strut->top_start_x = (int)v[8];
            out_strut->top_end_x = (int)v[9];
            out_strut->bottom_start_x = (int)v[10];
            out_strut->bottom_end_x = (int)v[11];
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
            out_strut->left = (int)v[0];
            out_strut->right = (int)v[1];
            out_strut->top = (int)v[2];
            out_strut->bottom = (int)v[3];
            XFree(prop);
            return 1;
        }
        XFree(prop);
    }

    return 0;
}

void strut_apply_workarea(Display *dpy, Window root, int screen_w, int screen_h,
                          int left, int right, int top, int bottom,
                          LayoutConfig *cfg) {
    if (cfg) {
        cfg->top_inset = top;
        cfg->bottom_inset = bottom;
    }
    if (!dpy || root == None) return;

    Atom net_workarea = XInternAtom(dpy, "_NET_WORKAREA", False);
    int wa_x = left;
    int wa_y = top;
    int wa_w = screen_w - left - right;
    int wa_h = screen_h - top - bottom;
    if (wa_w < 100) wa_w = 100;
    if (wa_h < 100) wa_h = 100;

    unsigned long workarea[4] = {
        (unsigned long)wa_x,
        (unsigned long)wa_y,
        (unsigned long)wa_w,
        (unsigned long)wa_h
    };

    XChangeProperty(dpy, root, net_workarea, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)workarea, 4);
}
