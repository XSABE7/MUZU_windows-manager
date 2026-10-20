#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <fontconfig/fontconfig.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "muzu.h"
#include "wm.h"
#include "titlebar.h"
#include "trigger.h"
#include "launchpad.h"
#include "dock.h"
#include "client_props.h"
#include "gtk_theme.h"

static int running = 1;
static GtkTheme current_theme;

static void sig_handler(int sig) {
    (void)sig;
    running = 0;
}

static int x_error_handler(Display *dpy, XErrorEvent *e) {
    (void)dpy;
    (void)e;
    return 0; // Ignore non-fatal X errors
}

static void reflow_all(Display *dpy, Client *clients, LayoutConfig *cfg) {
    layout_calculate(cfg, clients, 0);
    for (Client *curr = clients; curr; curr = curr->next) {
        if (curr->is_minimized) continue;
        if (curr->mode == CLIENT_TILED) {
            Window target = curr->frame ? curr->frame : curr->win;
            XMoveResizeWindow(dpy, target, curr->x, curr->y, curr->width, curr->height);
            if (curr->frame) {
                int client_h = curr->height - TITLEBAR_HEIGHT;
                if (client_h < 10) client_h = 10;
                XResizeWindow(dpy, curr->win, curr->width, client_h);
                titlebar_draw(dpy, curr, &current_theme);
            }
        }
    }
    XFlush(dpy);
}

static void client_teardown(Display *dpy, Client **clients, Window w, LayoutConfig *cfg,
                            Client **drag_client, Window *last_click_win, Dock *dock) {
    Client *c = client_find_by_any(*clients, w);
    if (!c) return;

    if (drag_client && *drag_client == c) {
        *drag_client = NULL;
        XUngrabPointer(dpy, CurrentTime);
    }
    if (last_click_win && (*last_click_win == c->frame || *last_click_win == c->win)) {
        *last_click_win = None;
    }

    if (dock) {
        dock_remove_minimized(dock, c);
        dock_render(dpy, dock, cfg->screen_width, cfg->screen_height);
    }

    // Stop receiving events for this client to prevent processing stale events
    XSelectInput(dpy, c->win, NoEventMask);
    XRemoveFromSaveSet(dpy, c->win);

    // Free Cairo titlebar surface
    titlebar_cleanup(c);

    // Safely unparent child window back to root before destroying frame
    if (c->frame != None) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);
        XUnmapWindow(dpy, c->frame);
        XReparentWindow(dpy, c->win, root, c->x, c->y);
        XDestroyWindow(dpy, c->frame);
        c->frame = None;
    }

    client_remove(clients, c->win);
    reflow_all(dpy, *clients, cfg);
    XFlush(dpy);
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "MUZUwm: Cannot open display\n");
        return 1;
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGCHLD, SIG_IGN); // Automatically reap zombie child processes
    XSetErrorHandler(x_error_handler);

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);

    LayoutConfig cfg = {
        .screen_width = screen_w,
        .screen_height = screen_h,
        .master_count = 1
    };

    Client *clients = NULL;
    TriggerState trigger = {0};
    Launchpad launchpad;
    Dock dock;

    // Load GTK theme configuration & export env for GTK applications
    gtk_theme_init(&current_theme);
    gtk_theme_export_env(&current_theme);

    // Initialize UI subsystems
    launchpad_init(dpy, &launchpad, screen_w, screen_h, &current_theme);
    dock_init(dpy, &dock, DOCK_BOTTOM, screen_w, screen_h, &current_theme);

    // Select root window events
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask |
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                            KeyPressMask | KeyReleaseMask);

    // Grab Super key (Mod4Mask)
    KeyCode super_kc = XKeysymToKeycode(dpy, XK_Super_L);
    if (super_kc == 0) super_kc = XKeysymToKeycode(dpy, XK_Super_R);
    if (super_kc != 0) {
        XGrabKey(dpy, super_kc, 0, root, True, GrabModeAsync, GrabModeAsync);
    }

    // Grab Alt+F1 as fallback trigger
    KeyCode alt_f1_kc = XKeysymToKeycode(dpy, XK_F1);
    XGrabKey(dpy, alt_f1_kc, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);

    // EWMH supported atoms setup
    Atom net_wm_moveresize = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
    Atom net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom net_close_window = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);

    Atom supported_atoms[] = {
        net_supported,
        net_wm_name,
        net_wm_moveresize,
        net_active_window,
        net_close_window
    };
    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)supported_atoms,
                    sizeof(supported_atoms) / sizeof(Atom));

    Client *drag_client = NULL;
    int drag_start_x = 0;
    int drag_start_y = 0;
    int client_start_x = 0;
    int client_start_y = 0;
    Time last_titlebar_click_time = 0;
    Window last_titlebar_click_win = None;

    struct pollfd pfd = {
        .fd = ConnectionNumber(dpy),
        .events = POLLIN,
        .revents = 0
    };

    XEvent ev;
    while (running) {
        while (running && XPending(dpy)) {
            XNextEvent(dpy, &ev);
            switch (ev.type) {
            case MapRequest: {
                Window w = ev.xmaprequest.window;
                if (w == launchpad.win || w == dock.win) {
                    XMapWindow(dpy, w);
                    break;
                }

                XWindowAttributes wa = {0};
                if (XGetWindowAttributes(dpy, w, &wa) && wa.override_redirect) {
                    XMapWindow(dpy, w);
                    break;
                }

                if (!client_find(clients, w)) {
                    int is_dialog = client_is_dialog(dpy, w);
                    int is_csd = client_is_csd(dpy, w);
                    ClientMode mode = is_dialog ? CLIENT_FLOATING : CLIENT_TILED;

                    int win_w = 600, win_h = 400;
                    if (wa.width > 0 && wa.height > 0) {
                        win_w = wa.width;
                        win_h = wa.height;
                    }

                    int init_x = 0, init_y = 0;
                    if (is_dialog) {
                        client_calculate_dialog_geometry(screen_w, screen_h, win_w, win_h,
                                                         &init_x, &init_y, &win_w, &win_h);
                    }

                    Window frame = is_csd ? None : titlebar_create(dpy, w, win_w);
                    client_add(&clients, w, mode);
                    Client *c = client_find(clients, w);
                    if (c) {
                        c->frame = frame;
                        c->is_csd = is_csd;
                        c->x = init_x;
                        c->y = init_y;
                        c->width = win_w;
                        c->height = is_csd ? win_h : (win_h + TITLEBAR_HEIGHT);
                        client_get_title(dpy, w, c->title, sizeof(c->title));
                    }
                    XAddToSaveSet(dpy, w);
                    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask | FocusChangeMask);

                    if (is_dialog) {
                        if (frame != None) {
                            XMoveResizeWindow(dpy, frame, init_x, init_y, win_w, win_h + TITLEBAR_HEIGHT);
                            XResizeWindow(dpy, w, win_w, win_h);
                            titlebar_draw(dpy, c, &current_theme);
                        } else {
                            XMoveResizeWindow(dpy, w, init_x, init_y, win_w, win_h);
                        }
                    } else {
                        reflow_all(dpy, clients, &cfg);
                    }
                }
                XMapWindow(dpy, w);
                Client *c = client_find(clients, w);
                if (c && c->frame) {
                    XRaiseWindow(dpy, c->frame);
                }
                XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
                break;
            }

            case ClientMessage: {
                if (ev.xclient.message_type == net_wm_moveresize) {
                    if (ev.xclient.format != 32) break;
                    Window target_win = ev.xclient.window;
                    if (target_win == None) break;
                    Client *c = client_find_by_any(clients, target_win);
                    if (c) {
                        long direction = ev.xclient.data.l[2];
                        if (direction == 11 && drag_client == c) {
                            XUngrabPointer(dpy, CurrentTime);
                            drag_client = NULL;
                            break;
                        }
                        if ((direction == 8 || direction == 10) && !drag_client) {
                            XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
                            Window raise_target = c->frame ? c->frame : c->win;
                            XRaiseWindow(dpy, raise_target);

                            if (c->mode == CLIENT_TILED) {
                                c->mode = CLIENT_FLOATING;
                                reflow_all(dpy, clients, &cfg);
                            }

                            drag_client = c;
                            drag_start_x = ev.xclient.data.l[0];
                            drag_start_y = ev.xclient.data.l[1];
                            client_start_x = c->x;
                            client_start_y = c->y;

                            XGrabPointer(dpy, root, False,
                                         ButtonReleaseMask | PointerMotionMask,
                                         GrabModeAsync, GrabModeAsync,
                                         None, None, CurrentTime);
                        }
                    }
                } else if (ev.xclient.message_type == net_close_window) {
                    if (ev.xclient.window != None) {
                        client_close(dpy, ev.xclient.window);
                    }
                } else if (ev.xclient.message_type == net_active_window) {
                    Client *c = client_find_by_any(clients, ev.xclient.window);
                    if (c) {
                        XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
                        Window raise_target = c->frame ? c->frame : c->win;
                        XRaiseWindow(dpy, raise_target);
                    }
                }
                break;
            }

            case UnmapNotify: {
                Window w = ev.xunmap.window;
                if (w == launchpad.win || w == dock.win) {
                    break;
                }
                Client *c = client_find_by_any(clients, w);
                if (c && c->is_minimized) {
                    break;
                }
                client_teardown(dpy, &clients, w, &cfg, &drag_client, &last_titlebar_click_win, &dock);
                break;
            }

            case DestroyNotify: {
                Window w = ev.xdestroywindow.window;
                if (w == launchpad.win || w == dock.win) {
                    break;
                }
                client_teardown(dpy, &clients, w, &cfg, &drag_client, &last_titlebar_click_win, &dock);
                break;
            }

            case KeyPress: {
                if (launchpad.is_visible) {
                    launchpad_handle_key(dpy, &launchpad, &ev.xkey);
                    break;
                }

                KeySym sym = XLookupKeysym(&ev.xkey, 0);
                if (sym == XK_Super_L || sym == XK_Super_R) {
                    if (trigger_check_super(&trigger, ev.xkey.time)) {
                        launchpad_toggle(dpy, &launchpad);
                    }
                } else if (sym == XK_F1 && (ev.xkey.state & Mod1Mask)) {
                    launchpad_toggle(dpy, &launchpad);
                }
                break;
            }

            case ButtonPress: {
                if (launchpad.is_visible) {
                    launchpad_handle_click(dpy, &launchpad, ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
                    break;
                }

                if (ev.xbutton.window == dock.win) {
                    if (ev.xbutton.button == Button1) {
                        Client *min_c = dock_get_minimized_at(&dock, ev.xbutton.x, ev.xbutton.y);
                        if (min_c) {
                            min_c->is_minimized = 0;
                            Window target = min_c->frame ? min_c->frame : min_c->win;
                            XMapWindow(dpy, target);
                            if (min_c->frame && min_c->win) {
                                XMapWindow(dpy, min_c->win);
                            }
                            XRaiseWindow(dpy, target);
                            XSetInputFocus(dpy, min_c->win, RevertToPointerRoot, CurrentTime);
                            dock_remove_minimized(&dock, min_c);
                            reflow_all(dpy, clients, &cfg);
                            dock_render(dpy, &dock, screen_w, screen_h);
                        } else {
                            dock_handle_click(dpy, &dock, &launchpad, ev.xbutton.x, ev.xbutton.y);
                        }
                    }
                    break;
                }

                for (Client *curr = clients; curr; curr = curr->next) {
                    if (curr->frame && ev.xbutton.window == curr->frame) {
                        XSetInputFocus(dpy, curr->win, RevertToPointerRoot, CurrentTime);
                        XRaiseWindow(dpy, curr->frame);

                        TitlebarAction action = titlebar_get_action(ev.xbutton.x, curr->width);
                        if (action == TITLEBAR_CLICK_CLOSE) {
                            client_close(dpy, curr->win);
                            break;
                        } else if (action == TITLEBAR_CLICK_MINIMIZE) {
                            curr->is_minimized = 1;
                            Window target = curr->frame ? curr->frame : curr->win;
                            XUnmapWindow(dpy, target);
                            dock_add_minimized(&dock, curr);
                            reflow_all(dpy, clients, &cfg);
                            dock_render(dpy, &dock, screen_w, screen_h);
                            break;
                        } else if (action == TITLEBAR_CLICK_MAXIMIZE) {
                            client_toggle_maximize(dpy, curr, &cfg);
                            if (!curr->is_maximized && curr->mode == CLIENT_TILED) {
                                reflow_all(dpy, clients, &cfg);
                            }
                            break;
                        }

                        // Check double click on titlebar
                        if (ev.xbutton.time - last_titlebar_click_time <= 250 &&
                            ev.xbutton.window == last_titlebar_click_win) {
                            handle_titlebar_double_click(dpy, curr, &cfg);
                            reflow_all(dpy, clients, &cfg);
                            last_titlebar_click_time = 0;
                            last_titlebar_click_win = None;
                            break;
                        }
                        last_titlebar_click_time = ev.xbutton.time;
                        last_titlebar_click_win = ev.xbutton.window;

                        // Start dragging with pointer grab
                        if (ev.xbutton.button == Button1) {
                            drag_client = curr;
                            drag_start_x = ev.xbutton.x_root;
                            drag_start_y = ev.xbutton.y_root;
                            client_start_x = curr->x;
                            client_start_y = curr->y;

                            // If tiled, detach to floating and reflow remaining tiled windows!
                            if (curr->mode == CLIENT_TILED) {
                                curr->mode = CLIENT_FLOATING;
                                reflow_all(dpy, clients, &cfg);
                            }

                            XGrabPointer(dpy, root, False,
                                         ButtonReleaseMask | PointerMotionMask,
                                         GrabModeAsync, GrabModeAsync,
                                         None, None, CurrentTime);
                        }
                        break;
                    }
                }
                break;
            }

            case MotionNotify: {
                if (trigger_check_hot_corner(ev.xmotion.x_root, ev.xmotion.y_root, DEFAULT_HOT_CORNER_SIZE)) {
                    if (!launchpad.is_visible) {
                        launchpad_toggle(dpy, &launchpad);
                    }
                }

                if (drag_client) {
                    int dx = ev.xmotion.x_root - drag_start_x;
                    int dy = ev.xmotion.y_root - drag_start_y;
                    handle_titlebar_drag(dpy, drag_client, client_start_x + dx, client_start_y + dy, screen_w, screen_h);
                }

                if (ev.xmotion.window == dock.win) {
                    dock_handle_hover(dpy, &dock, ev.xmotion.x, ev.xmotion.y, screen_w, screen_h);
                }

                dock_handle_motion(dpy, &dock, ev.xmotion.x_root, ev.xmotion.y_root, screen_w, screen_h);
                break;
            }

            case LeaveNotify: {
                if (ev.xcrossing.window == dock.win) {
                    if (dock.hover_item != -1) {
                        dock.hover_item = -1;
                        dock_render(dpy, &dock, screen_w, screen_h);
                    }
                }
                break;
            }

            case ButtonRelease: {
                if (drag_client) {
                    XUngrabPointer(dpy, CurrentTime);

                    // Edge snapping: top maximizes, left/right retiles
                    if (ev.xbutton.y_root <= 10 || ev.xbutton.x_root <= 10 || ev.xbutton.x_root >= screen_w - 10) {
                        drag_client->mode = CLIENT_TILED;
                        reflow_all(dpy, clients, &cfg);
                    }
                    drag_client = NULL;
                }
                break;
            }

            case Expose: {
                if (ev.xexpose.window == dock.win) {
                    dock_render(dpy, &dock, screen_w, screen_h);
                } else if (ev.xexpose.window == launchpad.win) {
                    launchpad_render(dpy, &launchpad, screen_w, screen_h);
                } else {
                    for (Client *curr = clients; curr; curr = curr->next) {
                        if (curr->frame && ev.xexpose.window == curr->frame) {
                            titlebar_draw(dpy, curr, &current_theme);
                        }
                    }
                }
                break;
            }

            case PropertyNotify: {
                if (ev.xproperty.atom == XA_WM_NAME || ev.xproperty.atom == net_wm_name) {
                    Client *c = client_find_by_any(clients, ev.xproperty.window);
                    if (c && c->frame) {
                        client_get_title(dpy, c->win, c->title, sizeof(c->title));
                        titlebar_draw(dpy, c, &current_theme);
                    }
                }
                break;
            }
        }
    }

        if (running && !XPending(dpy)) {
            poll(&pfd, 1, 50);
        }
    }

    while (clients) {
        client_teardown(dpy, &clients, clients->win, &cfg, &drag_client, &last_titlebar_click_win, &dock);
    }
    dock_cleanup(&dock);
    launchpad_cleanup(&launchpad);

    while (XPending(dpy)) {
        XNextEvent(dpy, &ev);
    }

    XSync(dpy, True);
    XCloseDisplay(dpy);
    cairo_debug_reset_static_data();
    FcFini();
    fprintf(stderr, "MUZUwm: Clean shutdown complete.\n");
    return 0;
}
