#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "client_props.h"
#include "icon_fetcher.h"

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

int client_size_hints_allow_fullscreen(long flags, int min_w, int min_h, int max_w, int max_h, int is_dialog) {
    if (is_dialog) return 0;
    int has_max = (flags & PMaxSize) && max_w > 0 && max_h > 0;
    if (has_max && (flags & PMinSize) && min_w == max_w && min_h == max_h) return 0;
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

cairo_surface_t *client_get_icon(Display *dpy, Window win, const char *title, int target_size) {
    if (!dpy || win == None) return NULL;
    if (target_size <= 0) target_size = 32;

    // 1. Try _NET_WM_ICON property
    Atom net_wm_icon = XInternAtom(dpy, "_NET_WM_ICON", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(dpy, win, net_wm_icon, 0, (~0L), False,
                           XA_CARDINAL, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        if (nitems >= 2 && actual_format == 32) {
            cairo_surface_t *surf = icon_create_from_net_wm_icon((const unsigned long *)prop, nitems, target_size);
            XFree(prop);
            if (surf) return surf;
        } else {
            XFree(prop);
        }
    }

    // 2. Try WM_CLASS
    XClassHint ch;
    if (XGetClassHint(dpy, win, &ch)) {
        cairo_surface_t *surf = NULL;
        if (ch.res_name && ch.res_name[0] != '\0') {
            char name_lower[128];
            snprintf(name_lower, sizeof(name_lower), "%s", ch.res_name);
            for (char *p = name_lower; *p; p++) *p = (char)tolower((unsigned char)*p);
            surf = icon_load_surface(name_lower);
        }
        if (!surf && ch.res_class && ch.res_class[0] != '\0') {
            char class_lower[128];
            snprintf(class_lower, sizeof(class_lower), "%s", ch.res_class);
            for (char *p = class_lower; *p; p++) *p = (char)tolower((unsigned char)*p);
            surf = icon_load_surface(class_lower);
        }
        if (ch.res_name) XFree(ch.res_name);
        if (ch.res_class) XFree(ch.res_class);
        if (surf) return surf;
    }

    // 3. Fallback to title keywords
    if (title && title[0] != '\0') {
        if (strcasestr(title, "firefox")) return icon_load_surface("firefox");
        if (strcasestr(title, "chrome") || strcasestr(title, "chromium")) return icon_load_surface("google-chrome");
        if (strcasestr(title, "nautilus") || strcasestr(title, "files")) return icon_load_surface("system-file-manager");
        if (strcasestr(title, "terminal") || strcasestr(title, "bash")) return icon_load_surface("utilities-terminal");
    }

    return NULL;
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
