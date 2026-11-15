#ifndef MUZU_STRUTS_H
#define MUZU_STRUTS_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "wm.h"

typedef struct {
    int left, right, top, bottom;
    int left_start_y, left_end_y;
    int right_start_y, right_end_y;
    int top_start_x, top_end_x;
    int bottom_start_x, bottom_end_x;
} StrutPartial;

typedef struct StrutList {
    Window win;
    StrutPartial strut;
    struct StrutList *next;
} StrutList;

void strut_list_add_or_update(StrutList **head, Window win, const StrutPartial *strut);
void strut_list_remove(StrutList **head, Window win);
void strut_calculate_insets(const StrutList *head, int screen_w, int screen_h,
                            int *left, int *right, int *top, int *bottom);
int strut_read_window_property(Display *dpy, Window win, StrutPartial *out_strut);
int strut_build_workarea_array(int wa_x, int wa_y, int wa_w, int wa_h,
                               int num_desktops, unsigned long *out_buf, int max_elems);
void strut_apply_workarea_rect_desktops(Display *dpy, Window root,
                                        int wa_x, int wa_y, int wa_w, int wa_h,
                                        int num_desktops);
void strut_apply_workarea_rect(Display *dpy, Window root, int wa_x, int wa_y, int wa_w, int wa_h);
void strut_apply_workarea(Display *dpy, Window root, int screen_w, int screen_h,
                          int left, int right, int top, int bottom,
                          LayoutConfig *cfg);

#endif /* MUZU_STRUTS_H */
