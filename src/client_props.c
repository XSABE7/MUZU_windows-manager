#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "client_props.h"

int client_protocols_contain(const Atom *protocols, int count, Atom target) {
    if (!protocols || count <= 0) return 0;
    for (int i = 0; i < count; i++) {
        if (protocols[i] == target) {
            return 1;
        }
    }
    return 0;
}

void client_sanitize_title(char *out_buf, size_t maxlen, const char *raw_title) {
    if (!out_buf || maxlen == 0) return;

    if (!raw_title || raw_title[0] == '\0') {
        snprintf(out_buf, maxlen, "%s", DEFAULT_TITLE);
    } else {
        snprintf(out_buf, maxlen, "%s", raw_title);
    }
}

int client_copy_utf8_prop(char *out_buf, size_t maxlen, const unsigned char *prop,
                          unsigned long nitems, Atom actual_type, int actual_format, Atom utf8_string) {
    if (!out_buf || maxlen == 0 || !prop || nitems == 0) return 0;
    if (actual_format != 8 || (utf8_string != None && actual_type != utf8_string)) return 0;

    size_t copy_len = (nitems < maxlen - 1) ? (size_t)nitems : (maxlen - 1);
    size_t actual_len = 0;
    while (actual_len < copy_len && prop[actual_len] != '\0') {
        out_buf[actual_len] = (char)prop[actual_len];
        actual_len++;
    }
    out_buf[actual_len] = '\0';
    if (actual_len == 0) {
        client_sanitize_title(out_buf, maxlen, NULL);
    }
    return 1;
}

void client_calculate_dialog_geometry(int screen_w, int screen_h, int win_w, int win_h,
                                     int *out_x, int *out_y, int *out_w, int *out_h) {
    int w = win_w;
    int h = win_h;

    // Fallback for uninitialized dimensions
    if (w <= 0 || h <= 0) {
        w = 600;
        h = 400;
    }

    // Clamp maximum dimensions to screen with margin
    if (w > screen_w) {
        w = screen_w - 40;
    }
    if (h > screen_h) {
        h = screen_h - 60;
    }

    // Minimum boundary guards
    if (w < 200) w = 200;
    if (h < 150) h = 150;

    int x = (screen_w - w) / 2;
    int y = (screen_h - h) / 2;

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

void client_resolve_initial_geometry(int screen_w, int screen_h,
                                     int top_inset, int bottom_inset,
                                     int req_x, int req_y, int req_w, int req_h,
                                     int is_csd,
                                     int *out_x, int *out_y, int *out_w, int *out_h) {
    int w = (req_w > 0) ? req_w : 600;
    int h = (req_h > 0) ? req_h : 400;
    int titlebar_h = is_csd ? 0 : 24;
    int avail_h = screen_h - top_inset - bottom_inset - titlebar_h;

    if (w > screen_w - 20) w = screen_w - 20;
    if (avail_h > 50 && h > avail_h) h = avail_h;
    if (w < 10) w = 10;
    if (h < 10) h = 10;

    int x, y;
    if ((req_x > 0 || req_y > 0) && req_x < screen_w - 20 && req_y < screen_h - 20) {
        x = req_x;
        y = (req_y < top_inset) ? top_inset : req_y;
    } else {
        x = (screen_w - w) / 2;
        y = top_inset + (avail_h - h) / 2;
        if (x < 0) x = 0;
        if (y < top_inset) y = top_inset;
    }

    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

void client_close(Display *dpy, Window win) {
    if (!dpy || win == None) return;

    Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    Atom *protocols = NULL;
    int count = 0;
    int supports_delete = 0;

    if (XGetWMProtocols(dpy, win, &protocols, &count) && protocols) {
        supports_delete = client_protocols_contain(protocols, count, wm_delete);
        XFree(protocols);
    }

    if (supports_delete) {
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = ClientMessage;
        ev.xclient.window = win;
        ev.xclient.message_type = wm_protocols;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = wm_delete;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, win, False, NoEventMask, &ev);
    } else {
        XKillClient(dpy, win);
    }
    XFlush(dpy);
}

void client_get_title(Display *dpy, Window win, char *out_buf, size_t maxlen) {
    if (!dpy || win == None || !out_buf || maxlen == 0) return;

    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    // 1. Try _NET_WM_NAME (UTF-8)
    if (XGetWindowProperty(dpy, win, net_wm_name, 0, (~0L), False,
                           utf8_string, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        int ok = client_copy_utf8_prop(out_buf, maxlen, prop, nitems, actual_type, actual_format, utf8_string);
        XFree(prop);
        if (ok) return;
    }

    // 2. Try WM_NAME (ICCCM)
    XTextProperty text_prop;
    if (XGetWMName(dpy, win, &text_prop) && text_prop.value && text_prop.nitems > 0) {
        client_sanitize_title(out_buf, maxlen, (const char *)text_prop.value);
        if (text_prop.value) XFree(text_prop.value);
        return;
    }

    // 3. Try WM_CLASS
    XClassHint ch;
    if (XGetClassHint(dpy, win, &ch)) {
        if (ch.res_name && ch.res_name[0] != '\0') {
            client_sanitize_title(out_buf, maxlen, ch.res_name);
        } else if (ch.res_class && ch.res_class[0] != '\0') {
            client_sanitize_title(out_buf, maxlen, ch.res_class);
        } else {
            client_sanitize_title(out_buf, maxlen, NULL);
        }
        if (ch.res_name) XFree(ch.res_name);
        if (ch.res_class) XFree(ch.res_class);
        return;
    }

    // 4. Default fallback
    client_sanitize_title(out_buf, maxlen, NULL);
}

WindowType client_classify_window_type_atoms(const Atom *types, unsigned long nitems,
                                             Atom a_desktop, Atom a_dock, Atom a_toolbar,
                                             Atom a_menu, Atom a_utility, Atom a_splash,
                                             Atom a_dialog, Atom a_notification, Atom a_normal,
                                             int has_transient) {
    (void)a_normal;
    if (types && nitems > 0) {
        // Priority 1: Desktop / Dock / Notification (never managed as regular tiled windows)
        for (unsigned long i = 0; i < nitems; i++) {
            if (a_desktop != None && types[i] == a_desktop) return WIN_TYPE_DESKTOP;
            if (a_dock != None && types[i] == a_dock) return WIN_TYPE_DOCK;
            if (a_notification != None && types[i] == a_notification) return WIN_TYPE_NOTIFICATION;
        }
        // Priority 2: Splash / Dialog
        for (unsigned long i = 0; i < nitems; i++) {
            if (a_splash != None && types[i] == a_splash) return WIN_TYPE_SPLASH;
            if (a_dialog != None && types[i] == a_dialog) return WIN_TYPE_DIALOG;
        }
        // Priority 3: Utility / Toolbar / Menu
        for (unsigned long i = 0; i < nitems; i++) {
            if ((a_utility != None && types[i] == a_utility) ||
                (a_toolbar != None && types[i] == a_toolbar) ||
                (a_menu != None && types[i] == a_menu)) {
                return WIN_TYPE_UTILITY;
            }
        }
    }
    return has_transient ? WIN_TYPE_DIALOG : WIN_TYPE_NORMAL;
}

Window client_get_transient_for(Display *dpy, Window win) {
    if (!dpy || win == None) return None;
    Window transient_for = None;
    if (XGetTransientForHint(dpy, win, &transient_for) && transient_for != None) {
        return transient_for;
    }
    return None;
}

WindowType client_get_window_type(Display *dpy, Window win) {
    if (!dpy || win == None) return WIN_TYPE_NORMAL;

    int has_transient = (client_get_transient_for(dpy, win) != None);

    Atom net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom a_desktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    Atom a_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    Atom a_toolbar = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
    Atom a_menu = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
    Atom a_utility = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    Atom a_splash = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    Atom a_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    Atom a_notify = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    Atom a_normal = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    WindowType result = has_transient ? WIN_TYPE_DIALOG : WIN_TYPE_NORMAL;

    if (XGetWindowProperty(dpy, win, net_wm_window_type, 0, (~0L), False,
                           XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        if (actual_type == XA_ATOM && actual_format == 32 && nitems > 0) {
            result = client_classify_window_type_atoms((const Atom *)prop, nitems,
                                                       a_desktop, a_dock, a_toolbar, a_menu,
                                                       a_utility, a_splash, a_dialog, a_notify,
                                                       a_normal, has_transient);
        }
        XFree(prop);
    }

    return result;
}

int client_should_manage_as_normal(WindowType wtype) {
    return (wtype != WIN_TYPE_DOCK &&
            wtype != WIN_TYPE_NOTIFICATION &&
            wtype != WIN_TYPE_DESKTOP);
}

void client_center_over_parent(int parent_x, int parent_y, int parent_w, int parent_h,
                               int mon_x, int mon_y, int mon_w, int mon_h,
                               int win_w, int win_h,
                               int *out_x, int *out_y, int *out_w, int *out_h) {
    int w = (win_w > 0) ? win_w : 600;
    int h = (win_h > 0) ? win_h : 400;

    if (mon_w > 40 && w > mon_w - 20) w = mon_w - 20;
    if (mon_h > 60 && h > mon_h - 20) h = mon_h - 20;
    if (w < 10) w = 10;
    if (h < 10) h = 10;

    int x = parent_x + (parent_w - w) / 2;
    int y = parent_y + (parent_h - h) / 2;

    if (x < mon_x) x = mon_x;
    if (y < mon_y) y = mon_y;
    if (mon_w > 0 && x + w > mon_x + mon_w) x = mon_x + mon_w - w;
    if (mon_h > 0 && y + h > mon_y + mon_h) y = mon_y + mon_h - h;
    if (x < mon_x) x = mon_x;
    if (y < mon_y) y = mon_y;

    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

int client_is_dialog(Display *dpy, Window win) {
    if (!dpy || win == None) return 0;
    WindowType t = client_get_window_type(dpy, win);
    return (t == WIN_TYPE_DIALOG || t == WIN_TYPE_SPLASH || t == WIN_TYPE_UTILITY);
}

int client_parse_motif_decorations(const unsigned long *hints, unsigned long nitems) {
    if (!hints || nitems < 3) return 1; // Default to decorated

    unsigned long flags = hints[0];
    unsigned long decorations = hints[2];

    // MWM_HINTS_DECORATIONS = (1L << 1)
    if (flags & (1L << 1)) {
        return (decorations != 0) ? 1 : 0;
    }
    return 1;
}

int client_is_csd(Display *dpy, Window win) {
    if (!dpy || win == None) return 0;

    // 1. Check _MOTIF_WM_HINTS
    Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(dpy, win, motif_hints, 0, (~0L), False,
                           AnyPropertyType, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        if (nitems >= 3 && actual_format == 32) {
            unsigned long *hints = (unsigned long *)prop;
            int decor = client_parse_motif_decorations(hints, nitems);
            XFree(prop);
            if (decor == 0) return 1; // CSD
        } else {
            XFree(prop);
        }
    }

    // 2. Check _GTK_FRAME_EXTENTS
    Atom gtk_frame_extents = XInternAtom(dpy, "_GTK_FRAME_EXTENTS", False);
    prop = NULL;
    if (XGetWindowProperty(dpy, win, gtk_frame_extents, 0, 4, False,
                           XA_CARDINAL, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        if (nitems == 4 && actual_format == 32) {
            unsigned long *extents = (unsigned long *)prop;
            if (extents[0] > 0 || extents[1] > 0 || extents[2] > 0 || extents[3] > 0) {
                XFree(prop);
                return 1;
            }
        }
        XFree(prop);
    }

    return 0;
}

int client_size_hints_allow_fullscreen(long flags, int min_w, int min_h, int max_w, int max_h, int is_dialog) {
    if (is_dialog) return 0;
    int has_max = (flags & PMaxSize) && max_w > 0 && max_h > 0;
    if (has_max && (flags & PMinSize) && (min_w == max_w || min_h == max_h)) return 0;
    if (has_max && (max_w < 800 || max_h < 600)) return 0;
    return 1;
}

int client_supports_fullscreen(Display *dpy, Window win) {
    if (!dpy || win == None) return 1;
    int is_dialog = client_is_dialog(dpy, win);
    XSizeHints hints;
    long supplied = 0;
    memset(&hints, 0, sizeof(hints));
    if (!XGetWMNormalHints(dpy, win, &hints, &supplied)) {
        return client_size_hints_allow_fullscreen(0, 0, 0, 0, 0, is_dialog);
    }
    return client_size_hints_allow_fullscreen(hints.flags, hints.min_width, hints.min_height,
                                              hints.max_width, hints.max_height, is_dialog);
}

void client_set_opacity(Display *dpy, Window win, unsigned int opacity) {
    if (!dpy || win == None) return;
    static Atom net_opacity = None;
    if (net_opacity == None) {
        net_opacity = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    }
    if (opacity >= 0xFFFFFFFF) {
        XDeleteProperty(dpy, win, net_opacity);
    } else {
        unsigned long val = opacity;
        XChangeProperty(dpy, win, net_opacity, XA_CARDINAL, 32, PropModeReplace,
                        (unsigned char *)&val, 1);
    }
}
