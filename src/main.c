#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include "wm.h"
#include "titlebar.h"
#include "client_props.h"
#include "config_rules.h"
#include "struts.h"
#include "snap_preview.h"

static int running = 1;
static Display *dpy = NULL;
static Window root = None;

static void sig_handler(int sig) {
    (void)sig;
    running = 0;
}

static int x_error_handler(Display *d, XErrorEvent *ee) {
    (void)d; (void)ee;
    return 0; // Ignore non-fatal X errors
}

static void set_focused_client(Display *d, Client *c, Client *head) {
    if (!d) return;
    for (Client *curr = head; curr; curr = curr->next) {
        int was_focused = curr->is_focused;
        curr->is_focused = (curr == c);
        if (curr->frame != None && was_focused != curr->is_focused) {
            titlebar_draw(d, curr, NULL);
        }
    }
    if (c) {
        XSetInputFocus(d, c->win, RevertToPointerRoot, CurrentTime);
        Atom net_active = XInternAtom(d, "_NET_ACTIVE_WINDOW", False);
        XChangeProperty(d, root, net_active, XA_WINDOW, 32, PropModeReplace,
                        (unsigned char *)&c->win, 1);
    } else {
        XSetInputFocus(d, root, RevertToPointerRoot, CurrentTime);
    }
}

static void reflow_all(Display *d, Client *clients, LayoutConfig *cfg) {
    if (!d || !cfg) return;
    int count = client_count_tiled(clients);
    layout_calculate(cfg, clients, count);

    for (Client *c = clients; c; c = c->next) {
        if (c->is_minimized || c->workspace != cfg->current_workspace) continue;
        if (c->mode == CLIENT_TILED || c->is_maximized) {
            if (c->frame != None) {
                int ch = c->height - TITLEBAR_HEIGHT;
                if (ch < 10) ch = 10;
                XMoveResizeWindow(d, c->frame, c->x, c->y, c->width, c->height);
                XMoveResizeWindow(d, c->win, 0, TITLEBAR_HEIGHT, c->width, ch);
                titlebar_draw(d, c, NULL);
            } else {
                XMoveResizeWindow(d, c->win, c->x, c->y, c->width, c->height);
            }
        }
    }
    XFlush(d);
}

static void client_call_back_slot(Display *d, Client *clients, LayoutConfig *cfg, int slot) {
    if (!d || !clients || slot <= 0) return;
    Client *target_c = client_find_by_slot(clients, slot);
    if (!target_c) return;

    if (target_c->is_minimized) {
        target_c->is_minimized = 0;
        Window target = target_c->frame != None ? target_c->frame : target_c->win;
        XMapWindow(d, target);
        if (target_c->frame != None && target_c->win != None) {
            XMapWindow(d, target_c->win);
        }
        XRaiseWindow(d, target);
        set_focused_client(d, target_c, clients);
        reflow_all(d, clients, cfg);
    } else {
        Window target = target_c->frame != None ? target_c->frame : target_c->win;
        XRaiseWindow(d, target);
        set_focused_client(d, target_c, clients);
    }
}

static void client_manage(Display *d, Window w, Client **clients, LayoutConfig *cfg) {
    if (!d || w == None || client_find(*clients, w)) return;

    XWindowAttributes wa;
    if (!XGetWindowAttributes(d, w, &wa) || wa.override_redirect) return;

    int is_csd = client_is_csd(d, w);
    int can_fs = client_supports_fullscreen(d, w);

    int init_x = 0, init_y = cfg->top_inset, win_w = 600, win_h = 400;
    client_resolve_initial_geometry(cfg->screen_width, cfg->screen_height,
                                    cfg->top_inset, cfg->bottom_inset,
                                    wa.x, wa.y, wa.width, wa.height,
                                    is_csd,
                                    &init_x, &init_y, &win_w, &win_h);

    Window frame = is_csd ? None : titlebar_create(d, w, win_w);
    client_add(clients, w, CLIENT_FLOATING);
    Client *c = client_find(*clients, w);
    if (!c) return;

    c->frame = frame;
    c->is_csd = is_csd;
    c->workspace = cfg->current_workspace;
    c->x = init_x;
    c->y = init_y;
    c->width = win_w;
    c->height = is_csd ? win_h : (win_h + TITLEBAR_HEIGHT);
    c->can_fullscreen = can_fs;
    client_get_title(d, w, c->title, sizeof(c->title));

    XAddToSaveSet(d, w);
    XSelectInput(d, w, PropertyChangeMask | StructureNotifyMask | FocusChangeMask | EnterWindowMask);

    XMapWindow(d, w);
    if (c->frame != None) {
        int ch = c->height - TITLEBAR_HEIGHT;
        if (ch < 10) ch = 10;
        XMoveResizeWindow(d, c->frame, c->x, c->y, c->width, c->height);
        XMoveResizeWindow(d, w, 0, TITLEBAR_HEIGHT, c->width, ch);
        titlebar_draw(d, c, NULL);
        XMapWindow(d, c->frame);
        XRaiseWindow(d, c->frame);
    } else {
        XMoveResizeWindow(d, w, c->x, c->y, c->width, c->height);
    }

    set_focused_client(d, c, *clients);
}

static void client_teardown(Display *d, Client **clients, Window w, LayoutConfig *cfg,
                            Client **drag_client, SnapPreview *preview) {
    if (!d || !clients || w == None) return;
    Client *c = client_find_by_any(*clients, w);
    if (!c) return;

    if (drag_client && *drag_client == c) {
        *drag_client = NULL;
        XUngrabPointer(d, CurrentTime);
        if (preview) snap_preview_hide(d, preview);
    }

    Window frame = c->frame;
    Window win = c->win;

    if (frame != None) {
        XUnmapWindow(d, frame);
        XReparentWindow(d, win, root, c->x, c->y);
        XDestroyWindow(d, frame);
    }

    int was_focused = c->is_focused;
    client_remove(clients, win);

    if (was_focused) {
        Client *next_focus = NULL;
        for (Client *curr = *clients; curr; curr = curr->next) {
            if (!curr->is_minimized && curr->workspace == cfg->current_workspace) {
                next_focus = curr;
                break;
            }
        }
        set_focused_client(d, next_focus, *clients);
    }
    reflow_all(d, *clients, cfg);
}


int main(void) {
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGHUP, sig_handler);

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "muzuwm: cannot open display\n");
        return 1;
    }

    int screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);

    XSetErrorHandler(x_error_handler);
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask |
                            StructureNotifyMask | ButtonPressMask | PropertyChangeMask);
    XSync(dpy, False);

    // EWMH Supported Atoms
    Atom net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    Atom net_wm_strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
    Atom net_wm_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    Atom net_workarea = XInternAtom(dpy, "_NET_WORKAREA", False);
    Atom net_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);

    Atom supported[] = { net_supported, net_wm_strut, net_wm_strut_partial,
                         net_workarea, net_active, net_wm_name };
    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)supported, sizeof(supported) / sizeof(Atom));

    LayoutConfig cfg = {
        .screen_width = screen_w,
        .screen_height = screen_h,
        .master_count = 1,
        .top_inset = 0,
        .bottom_inset = 0,
        .current_workspace = 0
    };

    Client *clients = NULL;
    Client *drag_client = NULL;
    int drag_start_x = 0, drag_start_y = 0;
    int client_start_x = 0, client_start_y = 0;

    StrutList *strut_list = NULL;
    SnapPreview snap_preview;
    snap_preview_init(dpy, root, &snap_preview);

    ShortcutConfig sc;
    config_rules_ensure_files();
    shortcuts_load(&sc);
    shortcuts_grab_keys(dpy, root, &sc);

    // Grab Super + 1..0 for calling back window slots 1-10
    const KeySym num_keys[10] = { XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9, XK_0 };
    const unsigned int num_locks[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (int k = 0; k < 10; k++) {
        KeyCode kc = XKeysymToKeycode(dpy, num_keys[k]);
        if (kc != 0) {
            for (int l = 0; l < 4; l++) {
                XGrabKey(dpy, kc, Mod4Mask | num_locks[l], root, True, GrabModeAsync, GrabModeAsync);
            }
        }
    }

    // Initial Workarea publish
    strut_apply_workarea(dpy, root, screen_w, screen_h, 0, 0, 0, 0, &cfg);

    // Discover existing windows
    Window root_ret, parent_ret, *children = NULL;
    unsigned int nchildren = 0;
    if (XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &nchildren) && children) {
        for (unsigned int i = 0; i < nchildren; i++) {
            StrutPartial sp;
            if (strut_read_window_property(dpy, children[i], &sp)) {
                strut_list_add_or_update(&strut_list, children[i], &sp);
                XSelectInput(dpy, children[i], PropertyChangeMask | StructureNotifyMask);
            } else {
                XWindowAttributes wa;
                if (XGetWindowAttributes(dpy, children[i], &wa) &&
                    wa.map_state == IsViewable && !wa.override_redirect) {
                    client_manage(dpy, children[i], &clients, &cfg);
                }
            }
        }
        XFree(children);
        int l = 0, r = 0, t = 0, b = 0;
        strut_calculate_insets(strut_list, screen_w, screen_h, &l, &r, &t, &b);
        strut_apply_workarea(dpy, root, screen_w, screen_h, l, r, t, b, &cfg);
        reflow_all(dpy, clients, &cfg);
    }

    XEvent ev;
    while (running) {
        XNextEvent(dpy, &ev);

        switch (ev.type) {
            case KeyPress: {
                KeySym sym = XLookupKeysym(&ev.xkey, 0);
                unsigned int mods = ev.xkey.state & (Mod1Mask | Mod4Mask | ControlMask | ShiftMask);

                // Super + 1..0: Call back / focus window slot 1-10
                if ((mods & Mod4Mask) && !(mods & (Mod1Mask | ControlMask | ShiftMask))) {
                    int slot = 0;
                    if (sym >= XK_1 && sym <= XK_9) {
                        slot = (int)(sym - XK_1 + 1);
                    } else if (sym == XK_0) {
                        slot = 10;
                    }
                    if (slot > 0) {
                        client_call_back_slot(dpy, clients, &cfg, slot);
                        break;
                    }
                }

                const ShortcutBinding *b = shortcuts_match_event(&sc, ev.xkey.state, sym);
                Client *focused = NULL;
                for (Client *c = clients; c; c = c->next) {
                    if (c->is_focused) { focused = c; break; }
                }

                if (b) {
                    switch (b->type) {
                        case SHORTCUT_ACT_TILE_LEFT:
                            if (focused && focused->can_fullscreen) {
                                focused->mode = CLIENT_TILED;
                                focused->snap_state = SNAP_LEFT;
                                reflow_all(dpy, clients, &cfg);
                            }
                            break;
                        case SHORTCUT_ACT_TILE_RIGHT:
                            if (focused && focused->can_fullscreen) {
                                focused->mode = CLIENT_TILED;
                                focused->snap_state = SNAP_RIGHT;
                                reflow_all(dpy, clients, &cfg);
                            }
                            break;
                        case SHORTCUT_ACT_FULLSCREEN:
                            if (focused && focused->can_fullscreen) {
                                client_toggle_maximize(dpy, focused, &cfg);
                                if (focused->frame != None) titlebar_draw(dpy, focused, NULL);
                                reflow_all(dpy, clients, &cfg);
                            }
                            break;
                        case SHORTCUT_ACT_FLOAT:
                            if (focused) {
                                focused->mode = CLIENT_FLOATING;
                                focused->snap_state = SNAP_NONE;
                                reflow_all(dpy, clients, &cfg);
                            }
                            break;
                        case SHORTCUT_ACT_CLOSE:
                            if (focused) client_close(dpy, focused->win);
                            break;
                        case SHORTCUT_ACT_LAUNCHER:
                            config_spawn_command("rofi -show drun");
                            break;
                        case SHORTCUT_ACT_EXEC:
                            config_spawn_command(b->exec_cmd);
                            break;
                        default:
                            break;
                    }
                }
                break;
            }

            case MapRequest: {
                Window w = ev.xmaprequest.window;
                StrutPartial sp;
                if (strut_read_window_property(dpy, w, &sp)) {
                    strut_list_add_or_update(&strut_list, w, &sp);
                    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask);
                    int l = 0, r = 0, t = 0, b = 0;
                    strut_calculate_insets(strut_list, screen_w, screen_h, &l, &r, &t, &b);
                    strut_apply_workarea(dpy, root, screen_w, screen_h, l, r, t, b, &cfg);
                    XMapWindow(dpy, w);
                    reflow_all(dpy, clients, &cfg);
                } else {
                    client_manage(dpy, w, &clients, &cfg);
                }
                break;
            }

            case ConfigureRequest: {
                XConfigureRequestEvent *cre = &ev.xconfigurerequest;
                Client *c = client_find(clients, cre->window);
                if (c && c->mode == CLIENT_FLOATING && !c->is_maximized) {
                    if (cre->value_mask & CWX) c->x = cre->x;
                    if (cre->value_mask & CWY) c->y = cre->y;
                    if (cre->value_mask & CWWidth) c->width = cre->width > 10 ? cre->width : 10;
                    if (cre->value_mask & CWHeight) {
                        int req_h = cre->height > 10 ? cre->height : 10;
                        c->height = c->frame != None ? (req_h + TITLEBAR_HEIGHT) : req_h;
                    }
                    if (c->frame != None) {
                        int ch = c->height - TITLEBAR_HEIGHT;
                        if (ch < 10) ch = 10;
                        XMoveResizeWindow(dpy, c->frame, c->x, c->y, c->width, c->height);
                        XMoveResizeWindow(dpy, c->win, 0, TITLEBAR_HEIGHT, c->width, ch);
                        titlebar_draw(dpy, c, NULL);
                    } else {
                        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->width, c->height);
                    }
                } else {
                    XWindowChanges wc;
                    wc.x = cre->x; wc.y = cre->y;
                    wc.width = cre->width; wc.height = cre->height;
                    wc.border_width = cre->border_width;
                    wc.sibling = cre->above;
                    wc.stack_mode = cre->detail;
                    XConfigureWindow(dpy, cre->window, cre->value_mask, &wc);
                }
                break;
            }

            case DestroyNotify:
            case UnmapNotify: {
                Window w = (ev.type == DestroyNotify) ? ev.xdestroywindow.window : ev.xunmap.window;
                Client *c = client_find_by_any(clients, w);
                if (c) {
                    if (ev.type == UnmapNotify && c->is_minimized) {
                        break;
                    }
                    client_teardown(dpy, &clients, w, &cfg, &drag_client, &snap_preview);
                } else {
                    // Check if strut window unmapped/crashed - Review Focus #1
                    strut_list_remove(&strut_list, w);
                    int l = 0, r = 0, t = 0, b = 0;
                    strut_calculate_insets(strut_list, screen_w, screen_h, &l, &r, &t, &b);
                    strut_apply_workarea(dpy, root, screen_w, screen_h, l, r, t, b, &cfg);
                    reflow_all(dpy, clients, &cfg);
                }
                break;
            }

            case PropertyNotify: {
                Atom atom = ev.xproperty.atom;
                Window w = ev.xproperty.window;
                if (atom == net_wm_strut || atom == net_wm_strut_partial) {
                    StrutPartial sp;
                    if (strut_read_window_property(dpy, w, &sp)) {
                        strut_list_add_or_update(&strut_list, w, &sp);
                    } else {
                        strut_list_remove(&strut_list, w);
                    }
                    int l = 0, r = 0, t = 0, b = 0;
                    strut_calculate_insets(strut_list, screen_w, screen_h, &l, &r, &t, &b);
                    strut_apply_workarea(dpy, root, screen_w, screen_h, l, r, t, b, &cfg);
                    reflow_all(dpy, clients, &cfg);
                } else {
                    Client *c = client_find_by_any(clients, w);
                    if (c) {
                        if (atom == XA_WM_NAME || atom == net_wm_name) {
                            client_get_title(dpy, c->win, c->title, sizeof(c->title));
                            if (c->frame != None) titlebar_draw(dpy, c, NULL);
                        } else if (atom == XA_WM_NORMAL_HINTS) {
                            c->can_fullscreen = client_supports_fullscreen(dpy, c->win);
                            if (c->frame != None) titlebar_draw(dpy, c, NULL);
                        }
                    }
                }
                break;
            }

            case ButtonPress: {
                Client *c = client_find_by_any(clients, ev.xbutton.window);
                if (c) {
                    set_focused_client(dpy, c, clients);
                    Window target = c->frame != None ? c->frame : c->win;
                    XRaiseWindow(dpy, target);

                    if (c->frame != None && ev.xbutton.window == c->frame) {
                        TitlebarAction act = titlebar_get_action_for_client(ev.xbutton.x, c->width, c);
                        if (act == TITLEBAR_CLICK_CLOSE) {
                            client_close(dpy, c->win);
                        } else if (act == TITLEBAR_CLICK_MAXIMIZE) {
                            client_toggle_maximize(dpy, c, &cfg);
                            titlebar_draw(dpy, c, NULL);
                            reflow_all(dpy, clients, &cfg);
                        } else if (act == TITLEBAR_CLICK_MINIMIZE) {
                            // 'Z': Make window disappear
                            c->is_minimized = 1;
                            Window unmap_target = c->frame != None ? c->frame : c->win;
                            XUnmapWindow(dpy, unmap_target);
                            if (c->frame != None && c->win != None) {
                                XUnmapWindow(dpy, c->win);
                            }
                            if (c->is_focused) {
                                c->is_focused = 0;
                                Client *next_focus = NULL;
                                for (Client *curr = clients; curr; curr = curr->next) {
                                    if (!curr->is_minimized) {
                                        next_focus = curr;
                                        break;
                                    }
                                }
                                set_focused_client(dpy, next_focus, clients);
                            }
                            reflow_all(dpy, clients, &cfg);
                        } else if (ev.xbutton.button == Button1) {
                            drag_client = c;
                            drag_start_x = ev.xbutton.x_root;
                            drag_start_y = ev.xbutton.y_root;
                            client_start_x = c->x;
                            client_start_y = c->y;
                            XGrabPointer(dpy, root, False,
                                         ButtonReleaseMask | PointerMotionMask,
                                         GrabModeAsync, GrabModeAsync,
                                         None, None, CurrentTime);
                        }
                    } else if (ev.xbutton.window == c->win) {
                        XAllowEvents(dpy, ReplayPointer, CurrentTime);
                    }
                }
                break;
            }

            case MotionNotify: {
                if (drag_client) {
                    int dx = ev.xmotion.x_root - drag_start_x;
                    int dy = ev.xmotion.y_root - drag_start_y;
                    if (abs(dx) > 4 || abs(dy) > 4) {
                        if (drag_client->mode == CLIENT_TILED) {
                            drag_client->mode = CLIENT_FLOATING;
                            drag_client->snap_state = SNAP_NONE;
                            reflow_all(dpy, clients, &cfg);
                        }
                        drag_client->x = client_start_x + dx;
                        drag_client->y = client_start_y + dy;
                        Window target = drag_client->frame != None ? drag_client->frame : drag_client->win;
                        XMoveWindow(dpy, target, drag_client->x, drag_client->y);

                        // Review Focus #2: Suppress snap preview on fixed-size windows
                        if (drag_client->can_fullscreen) {
                            SnapState snap = snap_preview_detect_zone(ev.xmotion.x_root, ev.xmotion.y_root,
                                                                     screen_w, cfg.top_inset, SNAP_THRESHOLD);
                            snap_preview_update(dpy, &snap_preview, snap, screen_w, screen_h,
                                                0, 0, cfg.top_inset, cfg.bottom_inset);
                        } else {
                            snap_preview_hide(dpy, &snap_preview);
                        }
                    }
                }
                break;
            }

            case ButtonRelease: {
                if (drag_client) {
                    XUngrabPointer(dpy, CurrentTime);
                    SnapState snap = snap_preview.current_snap;
                    snap_preview_hide(dpy, &snap_preview);

                    if (drag_client->can_fullscreen) {
                        if (snap == SNAP_TOP) {
                            client_toggle_maximize(dpy, drag_client, &cfg);
                            if (drag_client->frame != None) titlebar_draw(dpy, drag_client, NULL);
                            reflow_all(dpy, clients, &cfg);
                        } else if (snap == SNAP_LEFT || snap == SNAP_RIGHT) {
                            drag_client->mode = CLIENT_TILED;
                            drag_client->snap_state = snap;
                            reflow_all(dpy, clients, &cfg);
                        }
                    }
                    drag_client = NULL;
                }
                break;
            }

            case Expose: {
                Client *c = client_find_by_any(clients, ev.xexpose.window);
                if (c && c->frame != None && ev.xexpose.window == c->frame) {
                    titlebar_draw(dpy, c, NULL);
                }
                break;
            }

            default:
                break;
        }
    }

    // Teardown
    snap_preview_destroy(dpy, &snap_preview);
    while (clients) {
        client_teardown(dpy, &clients, clients->win, &cfg, NULL, NULL);
    }
    XCloseDisplay(dpy);
    return 0;
}
