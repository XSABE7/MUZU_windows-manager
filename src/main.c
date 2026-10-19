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

static int running = 1;

static void sig_handler(int sig) {
    (void)sig;
    running = 0;
}

static int x_error_handler(Display *dpy, XErrorEvent *e) {
    (void)dpy;
    (void)e;
    return 0; // Ignore non-fatal X errors (e.g. BadWindow when client crashes)
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "MUZUwm: Cannot open display\n");
        return 1;
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
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

    // Select root window events: window redirection, mouse motion, substructure changes
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask |
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                            KeyPressMask | KeyReleaseMask);

    // Grab Super key (Mod4Mask)
    KeyCode super_kc = XKeysymToKeycode(dpy, XK_Super_L);
    if (super_kc == 0) super_kc = XKeysymToKeycode(dpy, XK_Super_R);
    if (super_kc != 0) {
        XGrabKey(dpy, super_kc, AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    }

    // Grab Alt+F1 as fallback trigger
    KeyCode alt_f1_kc = XKeysymToKeycode(dpy, XK_F1);
    XGrabKey(dpy, alt_f1_kc, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);

    Client *drag_client = NULL;
    int drag_start_x = 0;
    int drag_start_y = 0;
    int client_start_x = 0;
    int client_start_y = 0;

    XEvent ev;
    while (running && !XNextEvent(dpy, &ev)) {
        switch (ev.type) {
            case MapRequest: {
                Window w = ev.xmaprequest.window;
                if (w == launchpad.win || w == dock.win) {
                    XMapWindow(dpy, w);
                    break;
                }

                // Check if already managed
                if (!client_find(clients, w)) {
                    // Create titlebar frame
                    Window frame = titlebar_create(dpy, w, 600);
                    client_add(&clients, w, CLIENT_TILED);
                    Client *c = client_find(clients, w);
                    if (c) c->frame = frame;

                    // Reflow tiled windows
                    layout_calculate(&cfg, clients, 0);
                    for (Client *curr = clients; curr; curr = curr->next) {
                        if (curr->mode == CLIENT_TILED) {
                            Window target = curr->frame ? curr->frame : curr->win;
                            XMoveResizeWindow(dpy, target, curr->x, curr->y, curr->width, curr->height);
                            if (curr->frame) {
                                XResizeWindow(dpy, curr->win, curr->width, curr->height - TITLEBAR_HEIGHT);
                                titlebar_draw(dpy, curr->frame, "Application", curr->width);
                            }
                        }
                    }
                }
                XMapWindow(dpy, w);
                break;
            }

            case UnmapNotify:
            case DestroyNotify: {
                Window w = (ev.type == DestroyNotify) ? ev.xdestroywindow.window : ev.xunmap.window;
                Client *c = client_find(clients, w);
                if (c) {
                    if (c->frame) XDestroyWindow(dpy, c->frame);
                    client_remove(&clients, w);

                    // Reflow remaining tiled windows
                    layout_calculate(&cfg, clients, 0);
                    for (Client *curr = clients; curr; curr = curr->next) {
                        if (curr->mode == CLIENT_TILED) {
                            Window target = curr->frame ? curr->frame : curr->win;
                            XMoveResizeWindow(dpy, target, curr->x, curr->y, curr->width, curr->height);
                            if (curr->frame) {
                                XResizeWindow(dpy, curr->win, curr->width, curr->height - TITLEBAR_HEIGHT);
                                titlebar_draw(dpy, curr->frame, "Application", curr->width);
                            }
                        }
                    }
                }
                break;
            }

            case KeyPress: {
                // If Launchpad visible, forward keys to Launchpad
                if (launchpad.is_visible) {
                    launchpad_handle_key(dpy, &launchpad, &ev.xkey);
                    break;
                }

                // Check Super Double-tap
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
                    launchpad_handle_click(dpy, &launchpad, ev.xbutton.x, ev.xbutton.y);
                    break;
                }

                // Check if titlebar clicked
                for (Client *curr = clients; curr; curr = curr->next) {
                    if (curr->frame && ev.xbutton.window == curr->frame) {
                        // Check if close button clicked (right side of titlebar)
                        if (ev.xbutton.x >= curr->width - 24) {
                            XDestroyWindow(dpy, curr->win);
                            break;
                        }

                        // Drag start
                        if (ev.xbutton.button == Button1) {
                            drag_client = curr;
                            drag_start_x = ev.xbutton.x_root;
                            drag_start_y = ev.xbutton.y_root;
                            client_start_x = curr->x;
                            client_start_y = curr->y;
                        }
                        break;
                    }
                }
                break;
            }

            case MotionNotify: {
                // Check Hot corner
                if (trigger_check_hot_corner(ev.xmotion.x_root, ev.xmotion.y_root, DEFAULT_HOT_CORNER_SIZE)) {
                    if (!launchpad.is_visible) {
                        launchpad_toggle(dpy, &launchpad);
                    }
                }

                // Handle window dragging
                if (drag_client) {
                    int dx = ev.xmotion.x_root - drag_start_x;
                    int dy = ev.xmotion.y_root - drag_start_y;
                    handle_titlebar_drag(dpy, drag_client, client_start_x + dx, client_start_y + dy, screen_w, screen_h);
                }

                // Dock hover check
                dock_handle_motion(dpy, &dock, ev.xmotion.x_root, ev.xmotion.y_root, screen_w, screen_h);
                break;
            }

            case ButtonRelease: {
                if (drag_client) {
                    // Check edge snapping to retile / maximize
                    if (ev.xbutton.y_root <= 5) {
                        // Drag to top -> Maximize (or re-tile)
                        drag_client->mode = CLIENT_TILED;
                        layout_calculate(&cfg, clients, 0);
                        for (Client *curr = clients; curr; curr = curr->next) {
                            if (curr->mode == CLIENT_TILED) {
                                Window target = curr->frame ? curr->frame : curr->win;
                                XMoveResizeWindow(dpy, target, curr->x, curr->y, curr->width, curr->height);
                                if (curr->frame) {
                                    XResizeWindow(dpy, curr->win, curr->width, curr->height - TITLEBAR_HEIGHT);
                                    titlebar_draw(dpy, curr->frame, "Application", curr->width);
                                }
                            }
                        }
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
                            titlebar_draw(dpy, curr->frame, "Application", curr->width);
                        }
                    }
                }
                break;
            }
        }
    }

    XCloseDisplay(dpy);
    return 0;
}
