#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        strncpy(out_buf, DEFAULT_TITLE, maxlen - 1);
    } else {
        strncpy(out_buf, raw_title, maxlen - 1);
    }
    out_buf[maxlen - 1] = '\0';
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
        client_sanitize_title(out_buf, maxlen, (const char *)prop);
        XFree(prop);
        return;
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

int client_is_dialog(Display *dpy, Window win) {
    if (!dpy || win == None) return 0;

    // 1. Check transient_for hint
    Window transient_for = None;
    if (XGetTransientForHint(dpy, win, &transient_for) && transient_for != None) {
        return 1;
    }

    // 2. Check _NET_WM_WINDOW_TYPE
    Atom net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom dialog_atom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    Atom splash_atom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    Atom util_atom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(dpy, win, net_wm_window_type, 0, (~0L), False,
                           XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        Atom *types = (Atom *)prop;
        for (unsigned long i = 0; i < nitems; i++) {
            if (types[i] == dialog_atom || types[i] == splash_atom || types[i] == util_atom) {
                XFree(prop);
                return 1;
            }
        }
        XFree(prop);
    }

    return 0;
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
