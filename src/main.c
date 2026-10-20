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
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include "muzu.h"
#include "wm.h"
#include "titlebar.h"
#include "trigger.h"
#include "rofi_launcher.h"
#include "wallpaper.h"
#include "dock.h"
#include "client_props.h"
#include "gtk_theme.h"
#include "menu.h"

static int running = 1;
static GtkTheme current_theme;

typedef struct {
    Display *dpy;
    Window root;
    Dock *dock;
    RofiLauncher *rofi;
    int screen_w, screen_h;
    int right_clicked_dock_item;
} WmContext;

static void handle_menu_action(int action_id, void *userdata) {
    WmContext *ctx = (WmContext *)userdata;
    if (!ctx) return;
    Display *dpy = ctx->dpy;
    Dock *dock = ctx->dock;
    int screen_w = ctx->screen_w;
    int screen_h = ctx->screen_h;

    switch (action_id) {
        case DOCK_ACTION_TOGGLE_MAGNIFICATION:
            dock->magnification_enabled = !dock->magnification_enabled;
            dock_render(dpy, dock, screen_w, screen_h);
            break;

        case DOCK_ACTION_TOGGLE_AUTOHIDE:
            dock_set_autohide(dock, !dock->autohide_enabled, screen_w, screen_h, dpy);
            break;

        case DOCK_ACTION_POS_BOTTOM:
            dock_set_position(dpy, dock, DOCK_BOTTOM, screen_w, screen_h);
            break;
        case DOCK_ACTION_POS_LEFT:
            dock_set_position(dpy, dock, DOCK_LEFT, screen_w, screen_h);
            break;
        case DOCK_ACTION_POS_TOP:
            dock_set_position(dpy, dock, DOCK_TOP, screen_w, screen_h);
            break;
        case DOCK_ACTION_POS_RIGHT:
            dock_set_position(dpy, dock, DOCK_RIGHT, screen_w, screen_h);
            break;

        case DOCK_ACTION_SIZE_SMALL:
            dock->size = 36;
            dock_calculate_geometry(dock, dock->pos, dock->is_hidden, screen_w, screen_h);
            if (dock->win) {
                XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
                cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
            }
            dock_render(dpy, dock, screen_w, screen_h);
            break;

        case DOCK_ACTION_SIZE_MEDIUM:
            dock->size = 48;
            dock_calculate_geometry(dock, dock->pos, dock->is_hidden, screen_w, screen_h);
            if (dock->win) {
                XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
                cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
            }
            dock_render(dpy, dock, screen_w, screen_h);
            break;

        case DOCK_ACTION_SIZE_LARGE:
            dock->size = 64;
            dock_calculate_geometry(dock, dock->pos, dock->is_hidden, screen_w, screen_h);
            if (dock->win) {
                XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
                cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
            }
            dock_render(dpy, dock, screen_w, screen_h);
            break;

        case DOCK_ACTION_MOVE:
            dock->drag_offset_x = dock->width / 2;
            dock->drag_offset_y = dock->height / 2;
            if (XGrabPointer(dpy, ctx->root, False,
                             ButtonReleaseMask | PointerMotionMask,
                             GrabModeAsync, GrabModeAsync,
                             None, None, CurrentTime) == GrabSuccess) {
                dock->is_dragging = 1;
            }
            break;

        case DOCK_ACTION_REMOVE_APP:
            if (ctx->right_clicked_dock_item > 0 && ctx->right_clicked_dock_item < dock->item_count) {
                dock_remove_app(dock, ctx->right_clicked_dock_item);
                dock_calculate_geometry(dock, dock->pos, dock->is_hidden, screen_w, screen_h);
                if (dock->win) {
                    XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
                    cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
                }
                dock_render(dpy, dock, screen_w, screen_h);
            }
            break;

        case DESKTOP_ACTION_WALLPAPER_BROWSE: {
            pid_t pid = fork();
            if (pid == 0) {
                setsid();
                const char *script =
                    "if which waypaper >/dev/null 2>&1; then\n"
                    "  waypaper\n"
                    "elif which zenity >/dev/null 2>&1; then\n"
                    "  FILE=$(zenity --file-selection --title=\"Select Wallpaper\" --file-filter=\"Images | *.png *.jpg *.jpeg *.bmp *.webp\" 2>/dev/null)\n"
                    "  if [ -n \"$FILE\" ] && [ -f \"$FILE\" ]; then\n"
                    "    mkdir -p \"$HOME/.config/muzuwm\"\n"
                    "    cp -f \"$FILE\" \"$HOME/.config/muzuwm/wallpaper.png\"\n"
                    "    xprop -root -format _MUZU_RELOAD_WALLPAPER 32c -set _MUZU_RELOAD_WALLPAPER 1 2>/dev/null || true\n"
                    "  fi\n"
                    "fi\n";
                execl("/bin/sh", "sh", "-c", script, NULL);
                _exit(0);
            }
            break;
        }

        case DESKTOP_ACTION_WALLPAPER_PATH: {
            pid_t pid = fork();
            if (pid == 0) {
                setsid();
                const char *script =
                    "if which zenity >/dev/null 2>&1; then\n"
                    "  FILE=$(zenity --entry --title=\"Set Wallpaper Path\" --text=\"Enter full image path:\" 2>/dev/null)\n"
                    "  if [ -n \"$FILE\" ] && [ -f \"$FILE\" ]; then\n"
                    "    mkdir -p \"$HOME/.config/muzuwm\"\n"
                    "    cp -f \"$FILE\" \"$HOME/.config/muzuwm/wallpaper.png\"\n"
                    "    xprop -root -format _MUZU_RELOAD_WALLPAPER 32c -set _MUZU_RELOAD_WALLPAPER 1 2>/dev/null || true\n"
                    "  fi\n"
                    "fi\n";
                execl("/bin/sh", "sh", "-c", script, NULL);
                _exit(0);
            }
            break;
        }

        case DESKTOP_ACTION_TERMINAL: {
            pid_t pid = fork();
            if (pid == 0) {
                setsid();
                execl("/bin/sh", "sh", "-c", "x-terminal-emulator || alacritty || gnome-terminal || kitty || xterm", NULL);
                _exit(0);
            }
            break;
        }

        case DESKTOP_ACTION_APPLICATIONS:
            if (ctx->rofi) {
                rofi_launcher_toggle(dpy, ctx->rofi);
            }
            break;

        default:
            break;
    }
}

static void sig_handler(int sig) {
    (void)sig;
    running = 0;
}

static int x_error_handler(Display *dpy, XErrorEvent *e) {
    (void)dpy;
    (void)e;
    return 0; // Ignore non-fatal X errors
}

static Dock *g_dock = NULL;
static RofiLauncher *g_rofi = NULL;
static Menu *g_menu = NULL;

static void raise_system_overlays(Display *dpy) {
    if (!dpy) return;
    if (g_dock && g_dock->win) {
        XRaiseWindow(dpy, g_dock->win);
    }
    if (g_rofi && g_rofi->win && g_rofi->is_visible) {
        XRaiseWindow(dpy, g_rofi->win);
    }
    if (g_menu && g_menu->win && g_menu->is_visible) {
        XRaiseWindow(dpy, g_menu->win);
    }
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
    raise_system_overlays(dpy);
    XFlush(dpy);
}

static Client *focused_client = NULL;

static void set_focused_client(Display *dpy, Client *c) {
    if (focused_client == c) {
        if (c && c->win) {
            XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
        }
        return;
    }

    Client *old = focused_client;
    focused_client = c;

    if (old) {
        old->is_focused = 0;
        old->last_unfocus_time = time(NULL);
        if (old->win != None) {
            XGrabButton(dpy, AnyButton, AnyModifier, old->win, False,
                        ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
        }
        if (old->frame != None) {
            titlebar_draw(dpy, old, &current_theme);
        }
    }

    if (c) {
        c->is_focused = 1;
        if (c->win != None) {
            XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
            if (c->is_ghost) {
                client_set_opacity(dpy, c->win, 0xFFFFFFFF);
                c->is_ghost = 0;
            }
            XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
        }
        if (c->frame != None) {
            titlebar_draw(dpy, c, &current_theme);
        }
    }
}

static void client_teardown(Display *dpy, Client **clients, Window w, LayoutConfig *cfg,
                            Client **drag_client, Window *last_click_win, Dock *dock) {
    Client *c = client_find_by_any(*clients, w);
    if (!c) return;

    if (focused_client == c) {
        focused_client = NULL;
    }

    if (drag_client && *drag_client == c) {
        *drag_client = NULL;
        XUngrabPointer(dpy, CurrentTime);
    }
    if (last_click_win && (*last_click_win == c->frame || *last_click_win == c->win)) {
        *last_click_win = None;
    }

    // Stop receiving events for this client to prevent processing stale events
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
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

    if (dock) {
        dock_remove_minimized(dock, c);
    }

    client_remove(clients, c->win);
    if (!focused_client && *clients) {
        for (Client *o = *clients; o; o = o->next) {
            if (!o->is_minimized) {
                set_focused_client(dpy, o);
                break;
            }
        }
    }
    reflow_all(dpy, *clients, cfg);
    if (dock) {
        dock_update_running(dock, *clients);
        dock_render(dpy, dock, cfg->screen_width, cfg->screen_height);
    }
    XFlush(dpy);
}

int main(int argc, char **argv) {
    const char *cli_wallpaper = NULL;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--wallpaper") == 0) && i + 1 < argc) {
            cli_wallpaper = argv[++i];
        }
    }

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
    RofiLauncher rofi_launcher;
    Dock dock = {0};

    // Load GTK theme configuration & export env for GTK applications
    gtk_theme_init(&current_theme);
    gtk_theme_export_env(&current_theme);

    // Initialize Wallpaper (Zero-Overhead Root Window Pixmap)
    if (cli_wallpaper) {
        wallpaper_set_root(dpy, screen_w, screen_h, cli_wallpaper);
    } else {
        wallpaper_init(dpy, screen_w, screen_h);
    }

    // Initialize UI subsystems
    rofi_launcher_init(dpy, &rofi_launcher, screen_w, screen_h, &current_theme);
    dock_init(dpy, &dock, DOCK_BOTTOM, screen_w, screen_h, &current_theme);

    Menu context_menu = {0};
    WmContext wm_ctx = {
        .dpy = dpy,
        .root = root,
        .dock = &dock,
        .rofi = &rofi_launcher,
        .screen_w = screen_w,
        .screen_h = screen_h,
        .right_clicked_dock_item = -1
    };
    menu_init(dpy, &context_menu, &current_theme, handle_menu_action, &wm_ctx);

    g_dock = &dock;
    g_rofi = &rofi_launcher;
    g_menu = &context_menu;

    // Select root window events
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask |
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                            KeyPressMask | KeyReleaseMask | PropertyChangeMask);

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
    Atom net_muzu_reload_wallpaper = XInternAtom(dpy, "_MUZU_RELOAD_WALLPAPER", False);
    Atom net_wm_moveresize = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
    Atom net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom net_wm_icon = XInternAtom(dpy, "_NET_WM_ICON", False);
    Atom net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom net_close_window = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);

    Atom supported_atoms[] = {
        net_supported,
        net_wm_name,
        net_wm_icon,
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
                if (w == rofi_launcher.win || w == dock.win || w == context_menu.win) {
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
                    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask | FocusChangeMask | EnterWindowMask | LeaveWindowMask);

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
                if (c) {
                    if (c->frame) {
                        XRaiseWindow(dpy, c->frame);
                    }
                    if (!c->icon_surface) {
                        c->icon_surface = client_get_icon(dpy, c->win, c->title, 32);
                    }
                    set_focused_client(dpy, c);
                } else {
                    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
                }
                dock_update_running(&dock, clients);
                dock_render(dpy, &dock, screen_w, screen_h);
                raise_system_overlays(dpy);
                break;
            }

            case ConfigureRequest: {
                XConfigureRequestEvent *cre = &ev.xconfigurerequest;
                XWindowChanges wc;
                wc.x = cre->x;
                wc.y = cre->y;
                wc.width = cre->width;
                wc.height = cre->height;
                wc.border_width = cre->border_width;
                wc.sibling = cre->above;
                wc.stack_mode = cre->detail;
                XConfigureWindow(dpy, cre->window, cre->value_mask, &wc);
                raise_system_overlays(dpy);
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
                            set_focused_client(dpy, c);
                            Window raise_target = c->frame ? c->frame : c->win;
                            XRaiseWindow(dpy, raise_target);
                            raise_system_overlays(dpy);

                            if (c->mode == CLIENT_TILED) {
                                c->mode = CLIENT_FLOATING;
                                c->snap_state = SNAP_NONE;
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
                        if (c->is_minimized) {
                            c->is_minimized = 0;
                            Window target = c->frame ? c->frame : c->win;
                            XMapWindow(dpy, target);
                            if (c->frame && c->win) {
                                XMapWindow(dpy, c->win);
                            }
                            dock_remove_minimized(&dock, c);
                            reflow_all(dpy, clients, &cfg);
                            dock_update_running(&dock, clients);
                            dock_render(dpy, &dock, screen_w, screen_h);
                        }
                        set_focused_client(dpy, c);
                        Window raise_target = c->frame ? c->frame : c->win;
                        XRaiseWindow(dpy, raise_target);
                        raise_system_overlays(dpy);
                    }
                }
                break;
            }

            case UnmapNotify: {
                Window w = ev.xunmap.window;
                if (w == rofi_launcher.win || w == dock.win || w == context_menu.win) {
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
                if (w == rofi_launcher.win || w == dock.win || w == context_menu.win) {
                    break;
                }
                client_teardown(dpy, &clients, w, &cfg, &drag_client, &last_titlebar_click_win, &dock);
                break;
            }

            case KeyPress: {
                if (context_menu.is_visible) {
                    KeySym sym = XLookupKeysym(&ev.xkey, 0);
                    if (sym == XK_Escape) {
                        menu_hide(dpy, &context_menu);
                        break;
                    }
                }

                if (rofi_launcher.is_visible) {
                    rofi_launcher_handle_key(dpy, &rofi_launcher, &ev.xkey);
                    break;
                }

                KeySym sym = XLookupKeysym(&ev.xkey, 0);
                if (sym == XK_Super_L || sym == XK_Super_R) {
                    rofi_launcher_toggle(dpy, &rofi_launcher);
                } else if (sym == XK_F1 && (ev.xkey.state & Mod1Mask)) {
                    rofi_launcher_toggle(dpy, &rofi_launcher);
                }
                break;
            }

            case ButtonPress: {
                if (context_menu.is_visible) {
                    if (ev.xbutton.window == context_menu.win) {
                        menu_handle_click(dpy, &context_menu, ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
                    } else {
                        menu_hide(dpy, &context_menu);
                    }
                    break;
                }

                if (rofi_launcher.is_visible) {
                    rofi_launcher_handle_press(dpy, &rofi_launcher, ev.xbutton.button,
                                               ev.xbutton.x_root, ev.xbutton.y_root);
                    break;
                }

                if (ev.xbutton.window == dock.win) {
                    if (ev.xbutton.button == Button3) {
                        int clicked_item = dock_get_item_at(&dock, ev.xbutton.x, ev.xbutton.y);
                        wm_ctx.right_clicked_dock_item = clicked_item;

                        menu_clear(&context_menu);
                        if (clicked_item > 0 && clicked_item < dock.item_count) {
                            char remove_lbl[128];
                            snprintf(remove_lbl, sizeof(remove_lbl), "Remove '%s' from Dock", dock.items[clicked_item].name);
                            menu_add_item(&context_menu, remove_lbl, DOCK_ACTION_REMOVE_APP);
                            menu_add_separator(&context_menu);
                        }

                        menu_add_item(&context_menu, dock.magnification_enabled ? "Magnification: On ✓" : "Magnification: Off", DOCK_ACTION_TOGGLE_MAGNIFICATION);
                        menu_add_item(&context_menu, dock.autohide_enabled ? "Auto-Hide: On ✓" : "Auto-Hide: Off", DOCK_ACTION_TOGGLE_AUTOHIDE);
                        menu_add_separator(&context_menu);
                        menu_add_item(&context_menu, "Position: Bottom", DOCK_ACTION_POS_BOTTOM);
                        menu_add_item(&context_menu, "Position: Left", DOCK_ACTION_POS_LEFT);
                        menu_add_item(&context_menu, "Position: Top", DOCK_ACTION_POS_TOP);
                        menu_add_item(&context_menu, "Position: Right", DOCK_ACTION_POS_RIGHT);
                        menu_add_separator(&context_menu);
                        menu_add_item(&context_menu, "Size: Small (36px)", DOCK_ACTION_SIZE_SMALL);
                        menu_add_item(&context_menu, "Size: Medium (48px)", DOCK_ACTION_SIZE_MEDIUM);
                        menu_add_item(&context_menu, "Size: Large (64px)", DOCK_ACTION_SIZE_LARGE);
                        menu_add_separator(&context_menu);
                        menu_add_item(&context_menu, "Move Dock", DOCK_ACTION_MOVE);

                        menu_show(dpy, &context_menu, ev.xbutton.x_root, ev.xbutton.y_root, screen_w, screen_h);
                        break;
                    }
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
                            raise_system_overlays(dpy);
                            set_focused_client(dpy, min_c);
                            dock_remove_minimized(&dock, min_c);
                            reflow_all(dpy, clients, &cfg);
                            dock_update_running(&dock, clients);
                            dock_render(dpy, &dock, screen_w, screen_h);
                        } else {
                            dock_handle_click(dpy, &dock, &rofi_launcher, ev.xbutton.x, ev.xbutton.y);
                        }
                    }
                    break;
                }

                if (ev.xbutton.window == root && ev.xbutton.button == Button3) {
                    menu_clear(&context_menu);
                    menu_add_item(&context_menu, "Choose Wallpaper (File Browser)...", DESKTOP_ACTION_WALLPAPER_BROWSE);
                    menu_add_item(&context_menu, "Set Wallpaper Path...", DESKTOP_ACTION_WALLPAPER_PATH);
                    menu_add_separator(&context_menu);
                    menu_add_item(&context_menu, "Open Terminal", DESKTOP_ACTION_TERMINAL);
                    menu_add_item(&context_menu, "Applications", DESKTOP_ACTION_APPLICATIONS);

                    menu_show(dpy, &context_menu, ev.xbutton.x_root, ev.xbutton.y_root, screen_w, screen_h);
                    break;
                }

                Client *clicked_c = client_find_by_any(clients, ev.xbutton.window);
                if (clicked_c) {
                    if (clicked_c->frame && ev.xbutton.window == clicked_c->frame) {
                        set_focused_client(dpy, clicked_c);
                        XRaiseWindow(dpy, clicked_c->frame);
                        raise_system_overlays(dpy);

                        if (ev.xbutton.button == Button1) {
                            TitlebarAction action = titlebar_get_action(ev.xbutton.x, clicked_c->width);
                            if (action == TITLEBAR_CLICK_CLOSE) {
                                client_close(dpy, clicked_c->win);
                                break;
                            } else if (action == TITLEBAR_CLICK_MINIMIZE) {
                                if (dock_add_minimized(&dock, clicked_c)) {
                                    clicked_c->is_minimized = 1;
                                    Window target = clicked_c->frame ? clicked_c->frame : clicked_c->win;
                                    XUnmapWindow(dpy, target);
                                    if (focused_client == clicked_c) {
                                        focused_client = NULL;
                                        for (Client *o = clients; o; o = o->next) {
                                            if (!o->is_minimized) {
                                                set_focused_client(dpy, o);
                                                break;
                                            }
                                        }
                                    }
                                    reflow_all(dpy, clients, &cfg);
                                    dock_render(dpy, &dock, screen_w, screen_h);
                                }
                                break;
                            } else if (action == TITLEBAR_CLICK_MAXIMIZE) {
                                client_toggle_maximize(dpy, clicked_c, &cfg);
                                titlebar_draw(dpy, clicked_c, &current_theme);
                                if (!clicked_c->is_maximized && clicked_c->mode == CLIENT_TILED) {
                                    reflow_all(dpy, clients, &cfg);
                                }
                                break;
                            }
                        }

                        // Check double click on titlebar
                        if (ev.xbutton.time - last_titlebar_click_time <= 250 &&
                            ev.xbutton.window == last_titlebar_click_win) {
                            handle_titlebar_double_click(dpy, clicked_c, &cfg);
                            reflow_all(dpy, clients, &cfg);
                            last_titlebar_click_time = 0;
                            last_titlebar_click_win = None;
                            break;
                        }
                        last_titlebar_click_time = ev.xbutton.time;
                        last_titlebar_click_win = ev.xbutton.window;

                        // Start dragging with pointer grab
                        if (ev.xbutton.button == Button1) {
                            drag_client = clicked_c;
                            drag_start_x = ev.xbutton.x_root;
                            drag_start_y = ev.xbutton.y_root;
                            client_start_x = clicked_c->x;
                            client_start_y = clicked_c->y;

                            // If tiled, detach to floating and reflow remaining tiled windows!
                            if (clicked_c->mode == CLIENT_TILED) {
                                clicked_c->mode = CLIENT_FLOATING;
                                clicked_c->snap_state = SNAP_NONE;
                                reflow_all(dpy, clients, &cfg);
                            }

                            XGrabPointer(dpy, root, False,
                                         ButtonReleaseMask | PointerMotionMask,
                                         GrabModeAsync, GrabModeAsync,
                                         None, None, CurrentTime);
                        }
                        break;
                    } else if (ev.xbutton.window == clicked_c->win) {
                        set_focused_client(dpy, clicked_c);
                        Window raise_target = clicked_c->frame ? clicked_c->frame : clicked_c->win;
                        XRaiseWindow(dpy, raise_target);
                        raise_system_overlays(dpy);
                        XAllowEvents(dpy, ReplayPointer, CurrentTime);
                        break;
                    }
                }
                break;
            }

            case MotionNotify: {
                if (context_menu.is_visible && ev.xmotion.window == context_menu.win) {
                    menu_handle_motion(dpy, &context_menu, ev.xmotion.x, ev.xmotion.y);
                }

                if (dock.is_dragging) {
                    dock_handle_drag(dpy, &dock, ev.xmotion.x_root, ev.xmotion.y_root, screen_w, screen_h);
                    break;
                }

                if (rofi_launcher.is_visible) {
                    rofi_launcher_handle_motion(dpy, &rofi_launcher,
                                                ev.xmotion.x_root - rofi_launcher.x,
                                                ev.xmotion.y_root - rofi_launcher.y);
                    if (rofi_launcher.is_dragging_out) {
                        dock_handle_motion(dpy, &dock, ev.xmotion.x_root, ev.xmotion.y_root, screen_w, screen_h);
                        if (ev.xmotion.x_root >= dock.x && ev.xmotion.x_root <= dock.x + dock.width &&
                            ev.xmotion.y_root >= dock.y && ev.xmotion.y_root <= dock.y + dock.height) {
                            dock_handle_hover(dpy, &dock, ev.xmotion.x_root - dock.x, ev.xmotion.y_root - dock.y, screen_w, screen_h);
                        } else if (dock.is_hovered) {
                            dock.is_hovered = 0;
                            dock.hover_item = -1;
                            dock_render(dpy, &dock, screen_w, screen_h);
                        }
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

            case EnterNotify: {
                if (ev.xcrossing.window == dock.win) {
                    if (dock.autohide_enabled && dock.is_hidden) {
                        dock.is_hidden = 0;
                        dock_calculate_geometry(&dock, dock.pos, 0, screen_w, screen_h);
                        XMoveResizeWindow(dpy, dock.win, dock.x, dock.y, dock.width, dock.height);
                        XRaiseWindow(dpy, dock.win);
                        dock_render(dpy, &dock, screen_w, screen_h);
                    }
                    break;
                }

                Client *c = client_find_by_any(clients, ev.xcrossing.window);
                if (c) {
                    c->is_hovered = 1;
                    if (c->is_ghost) {
                        client_set_opacity(dpy, c->win, 0xFFFFFFFF);
                    }
                }
                break;
            }

            case LeaveNotify: {
                if (ev.xcrossing.window == dock.win && !rofi_launcher.is_dragging_out) {
                    if (dock.hover_item != -1 || dock.is_hovered) {
                        dock.hover_item = -1;
                        dock.is_hovered = 0;
                        dock.mouse_pos = -1.0;
                        if (!dock.is_hidden) {
                            dock_render(dpy, &dock, screen_w, screen_h);
                        }
                    }
                    if (dock.autohide_enabled && !dock.is_hidden && !dock.is_dragging) {
                        dock.is_hidden = 1;
                        dock_calculate_geometry(&dock, dock.pos, 1, screen_w, screen_h);
                        XMoveResizeWindow(dpy, dock.win, dock.x, dock.y, dock.width, dock.height);
                    }
                    break;
                }

                Client *c = client_find_by_any(clients, ev.xcrossing.window);
                if (c && c != focused_client) {
                    int rx = ev.xcrossing.x_root;
                    int ry = ev.xcrossing.y_root;
                    if (rx < c->x || rx >= c->x + c->width || ry < c->y || ry >= c->y + c->height) {
                        c->is_hovered = 0;
                        time_t now = time(NULL);
                        if (c->last_unfocus_time > 0 && (now - c->last_unfocus_time >= 20)) {
                            client_set_opacity(dpy, c->win, 0xA6AAAAAA);
                            c->is_ghost = 1;
                        }
                    }
                }
                break;
            }

            case ButtonRelease: {
                if (rofi_launcher.is_pressing && ev.xbutton.button == Button1) {
                    XUngrabPointer(dpy, CurrentTime);
                    int was_dragging = rofi_launcher.is_dragging_out;
                    int app_idx = rofi_launcher.drag_app_idx;
                    rofi_launcher.is_pressing = 0;
                    rofi_launcher.is_dragging_out = 0;

                    if (was_dragging) {
                        int rx = ev.xbutton.x_root;
                        int ry = ev.xbutton.y_root;
                        if (rx >= dock.x && rx <= dock.x + dock.width &&
                            ry >= dock.y && ry <= dock.y + dock.height) {
                            if (app_idx >= 0 && app_idx < rofi_launcher.app_count) {
                                const RofiApp *app = &rofi_launcher.apps[app_idx];
                                char clean_cmd[256];
                                rofi_sanitize_exec(app->exec, clean_cmd, sizeof(clean_cmd));
                                dock_add_app(&dock, app->name, clean_cmd, app->icon_name);
                                dock_save_pinned_apps(&dock);
                                dock_calculate_geometry(&dock, dock.pos, dock.is_hidden, screen_w, screen_h);
                                if (dock.win) {
                                    XMoveResizeWindow(dpy, dock.win, dock.x, dock.y, dock.width, dock.height);
                                    cairo_xlib_surface_set_size(dock.surface, dock.width, dock.height);
                                    dock_render(dpy, &dock, screen_w, screen_h);
                                }
                                rofi_launcher_toggle(dpy, &rofi_launcher);
                            }
                        }
                    } else {
                        rofi_launcher_execute_selected(dpy, &rofi_launcher);
                    }
                    break;
                }

                if (dock.is_dragging && (ev.xbutton.button == Button1 || ev.xbutton.button == Button3)) {
                    XUngrabPointer(dpy, CurrentTime);
                    dock_end_drag(dpy, &dock, screen_w, screen_h);
                    break;
                }

                if (drag_client) {
                    XUngrabPointer(dpy, CurrentTime);

                    const int SNAP_THRESHOLD = 16;
                    if (ev.xbutton.y_root <= SNAP_THRESHOLD) {
                        // Top edge -> Maximize!
                        if (!drag_client->is_maximized) {
                            client_toggle_maximize(dpy, drag_client, &cfg);
                            if (drag_client->frame) {
                                titlebar_draw(dpy, drag_client, &current_theme);
                            }
                        }
                    } else if (ev.xbutton.x_root <= SNAP_THRESHOLD) {
                        // Left edge -> Left half (50%)
                        if (drag_client->is_maximized) {
                            drag_client->is_maximized = 0;
                        }
                        drag_client->mode = CLIENT_TILED;
                        drag_client->snap_state = SNAP_LEFT;
                        reflow_all(dpy, clients, &cfg);
                    } else if (ev.xbutton.x_root >= screen_w - SNAP_THRESHOLD) {
                        // Right edge -> Right half (50%)
                        if (drag_client->is_maximized) {
                            drag_client->is_maximized = 0;
                        }
                        drag_client->mode = CLIENT_TILED;
                        drag_client->snap_state = SNAP_RIGHT;
                        reflow_all(dpy, clients, &cfg);
                    } else {
                        // Dropped in free space -> stays floating
                        drag_client->snap_state = SNAP_NONE;
                    }
                    drag_client = NULL;
                }
                break;
            }

            case Expose: {
                if (ev.xexpose.window == context_menu.win) {
                    menu_render(dpy, &context_menu);
                } else if (ev.xexpose.window == dock.win) {
                    dock_render(dpy, &dock, screen_w, screen_h);
                } else if (ev.xexpose.window == rofi_launcher.win) {
                    rofi_launcher_render(dpy, &rofi_launcher, screen_w, screen_h);
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
                if (ev.xproperty.window == root && ev.xproperty.atom == net_muzu_reload_wallpaper) {
                    wallpaper_init(dpy, screen_w, screen_h);
                    break;
                }
                if (ev.xproperty.atom == XA_WM_NAME || ev.xproperty.atom == net_wm_name) {
                    Client *c = client_find_by_any(clients, ev.xproperty.window);
                    if (c) {
                        client_get_title(dpy, c->win, c->title, sizeof(c->title));
                        if (!c->icon_surface) {
                            c->icon_surface = client_get_icon(dpy, c->win, c->title, 32);
                        }
                        if (c->frame) {
                            titlebar_draw(dpy, c, &current_theme);
                        }
                        dock_update_running(&dock, clients);
                        dock_render(dpy, &dock, screen_w, screen_h);
                    }
                }
                break;
            }
        }
    }

        time_t now = time(NULL);
        static time_t last_inactivity_check = 0;
        if (now != last_inactivity_check) {
            last_inactivity_check = now;
            for (Client *curr = clients; curr; curr = curr->next) {
                if (curr != focused_client && !curr->is_minimized && !curr->is_hovered) {
                    if (curr->last_unfocus_time > 0 && (now - curr->last_unfocus_time >= 20)) {
                        if (!curr->is_ghost) {
                            client_set_opacity(dpy, curr->win, 0xA6AAAAAA);
                            curr->is_ghost = 1;
                        }
                    }
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
    menu_cleanup(&context_menu);
    dock_cleanup(&dock);
    rofi_launcher_cleanup(&rofi_launcher);

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
