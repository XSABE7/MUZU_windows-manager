#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "muzu.h"
#include "wm.h"
#include "titlebar.h"
#include "trigger.h"
#include "launchpad.h"
#include "dock.h"
#include "client_props.h"

static int running = 1;

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
        if (curr->mode == CLIENT_TILED) {
            Window target = curr->frame ? curr->frame : curr->win;
            XMoveResizeWindow(dpy, target, curr->x, curr->y, curr->width, curr->height);
            if (curr->frame) {
                int client_h = curr->height - TITLEBAR_HEIGHT;
                if (client_h < 10) client_h = 10;
                XResizeWindow(dpy, curr->win, curr->width, client_h);
                titlebar_draw(dpy, curr->frame, curr->title[0] ? curr->title : "MUZUwm Window", curr->width);
            }
        }
    }
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

    // Initialize UI subsystems
    launchpad_init(dpy, &launchpad, screen_w, screen_h);
    dock_init(dpy, &dock, DOCK_BOTTOM, screen_w, screen_h);

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

    Client *drag_client = NULL;
    int drag_start_x = 0;
    int drag_start_y = 0;
    int client_start_x = 0;
    int client_start_y = 0;
    Time last_titlebar_click_time = 0;
    Window last_titlebar_click_win = None;

    XEvent ev;
    while (running && !XNextEvent(dpy, &ev)) {
        switch (ev.type) {
            case MapRequest: {
                Window w = ev.xmaprequest.window;
                if (w == launchpad.win || w == dock.win) {
                    XMapWindow(dpy, w);
                    break;
                }

                if (!client_find(clients, w)) {
                    int is_dialog = client_is_dialog(dpy, w);
                    ClientMode mode = is_dialog ? CLIENT_FLOATING : CLIENT_TILED;

                    int win_w = 600, win_h = 400;
                    XWindowAttributes wa;
                    if (XGetWindowAttributes(dpy, w, &wa) && wa.width > 0 && wa.height > 0) {
                        win_w = wa.width;
                        win_h = wa.height;
                    }

                    int init_x = 0, init_y = 0;
                    if (is_dialog) {
                        client_calculate_dialog_geometry(screen_w, screen_h, win_w, win_h,
                                                         &init_x, &init_y, &win_w, &win_h);
                    }

                    Window frame = titlebar_create(dpy, w, win_w);
                    client_add(&clients, w, mode);
                    Client *c = client_find(clients, w);
                    if (c) {
                        c->frame = frame;
                        c->x = init_x;
                        c->y = init_y;
                        c->width = win_w;
                        c->height = win_h + TITLEBAR_HEIGHT;
                        client_get_title(dpy, w, c->title, sizeof(c->title));
                    }
                    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask);

                    if (is_dialog) {
                        XMoveResizeWindow(dpy, frame, init_x, init_y, win_w, win_h + TITLEBAR_HEIGHT);
                        XResizeWindow(dpy, w, win_w, win_h);
                        titlebar_draw(dpy, frame, c && c->title[0] ? c->title : "MUZUwm Window", win_w);
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

            case UnmapNotify: {
                // Safely hide frame without destroying client
                Window w = ev.xunmap.window;
                Client *c = client_find(clients, w);
                if (c && c->frame) {
                    XUnmapWindow(dpy, c->frame);
                }
                break;
            }

            case DestroyNotify: {
                Window w = ev.xdestroywindow.window;
                Client *c = client_find(clients, w);
                if (c) {
                    if (c->frame) XDestroyWindow(dpy, c->frame);
                    client_remove(&clients, w);
                    reflow_all(dpy, clients, &cfg);
                }
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

                for (Client *curr = clients; curr; curr = curr->next) {
                    if (curr->frame && ev.xbutton.window == curr->frame) {
                        XSetInputFocus(dpy, curr->win, RevertToPointerRoot, CurrentTime);
                        XRaiseWindow(dpy, curr->frame);

                        // Close button clicked (red dot on right)
                        if (ev.xbutton.x >= curr->width - 24) {
                            client_close(dpy, curr->win);
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

                dock_handle_motion(dpy, &dock, ev.xmotion.x_root, ev.xmotion.y_root, screen_w, screen_h);
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
                            titlebar_draw(dpy, curr->frame, curr->title[0] ? curr->title : "MUZUwm Window", curr->width);
                        }
                    }
                }
                break;
            }

            case PropertyNotify: {
                Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
                if (ev.xproperty.atom == XA_WM_NAME || ev.xproperty.atom == net_wm_name) {
                    Client *c = client_find(clients, ev.xproperty.window);
                    if (c && c->frame) {
                        client_get_title(dpy, c->win, c->title, sizeof(c->title));
                        titlebar_draw(dpy, c->frame, c->title[0] ? c->title : "MUZUwm Window", c->width);
                    }
                }
                break;
            }
        }
    }

    XCloseDisplay(dpy);
    return 0;
}
