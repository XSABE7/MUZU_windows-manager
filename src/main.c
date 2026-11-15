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
#include "menu.h"
#include "alttab.h"

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

static void launch_settings_app(void) {
    char exe_path[1024];
    char local_py[1100] = {0};
    char local_bin[1100] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        char *slash = strrchr(exe_path, '/');
        if (slash) {
            *slash = '\0';
            snprintf(local_py, sizeof(local_py), "%s/muzuwm_settings.py", exe_path);
            snprintf(local_bin, sizeof(local_bin), "%s/muzuwm-settings", exe_path);
        }
    }

    const char *candidates[] = {
        local_py[0] ? local_py : NULL,
        local_bin[0] ? local_bin : NULL,
        "./muzuwm_settings.py",
        "/usr/local/bin/muzuwm-settings",
        "/usr/bin/muzuwm-settings",
        NULL
    };

    char settings_cmd[1200] = {0};
    for (int i = 0; i < 5; i++) {
        if (candidates[i] && access(candidates[i], R_OK) == 0) {
            snprintf(settings_cmd, sizeof(settings_cmd), "%s", candidates[i]);
            break;
        }
    }

    pid_t pid = fork();
    if (pid != 0) return;
    setsid();

    if (settings_cmd[0] == '\0') {
        execlp("sh", "sh", "-c",
               "if command -v muzuwm-settings >/dev/null 2>&1; then "
               "  exec wezterm start --always-new-process -- muzuwm-settings || exec x-terminal-emulator -e muzuwm-settings; "
               "fi",
               (char *)NULL);
        _exit(127);
    }

    int is_exec = (access(settings_cmd, X_OK) == 0);
    if (is_exec) {
        execlp("wezterm", "wezterm", "start", "--always-new-process", "--class", "muzuwm-settings", "--", settings_cmd, (char *)NULL);
        execlp("alacritty", "alacritty", "-e", settings_cmd, (char *)NULL);
        execlp("xterm", "xterm", "-T", "MUZUwm Settings", "-e", settings_cmd, (char *)NULL);
        execlp("x-terminal-emulator", "x-terminal-emulator", "-e", settings_cmd, (char *)NULL);
    } else {
        execlp("wezterm", "wezterm", "start", "--always-new-process", "--class", "muzuwm-settings", "--", "python3", settings_cmd, (char *)NULL);
        execlp("alacritty", "alacritty", "-e", "python3", settings_cmd, (char *)NULL);
        execlp("xterm", "xterm", "-T", "MUZUwm Settings", "-e", "python3", settings_cmd, (char *)NULL);
        execlp("x-terminal-emulator", "x-terminal-emulator", "-e", "python3", settings_cmd, (char *)NULL);
    }
    _exit(127);
}

static void spawn_terminal(void) {
    config_spawn_command(
        "if command -v wezterm >/dev/null 2>&1; then "
        "  exec wezterm start --always-new-process; "
        "elif command -v alacritty >/dev/null 2>&1; then "
        "  exec alacritty; "
        "elif command -v xterm >/dev/null 2>&1; then "
        "  exec xterm; "
        "else "
        "  exec x-terminal-emulator; "
        "fi");
}

static void desktop_menu_callback(int action_id, void *userdata) {
    (void)userdata;
    switch (action_id) {
        case DESKTOP_ACTION_TERMINAL:
            spawn_terminal();
            break;
        case DESKTOP_ACTION_INTERNET:
            config_spawn_command("x-www-browser || firefox || google-chrome || chromium");
            break;
        case DESKTOP_ACTION_APPLICATIONS:
            config_spawn_command(
                "if command -v mzshell >/dev/null 2>&1; then exec mzshell; "
                "elif command -v wezterm >/dev/null 2>&1; then exec wezterm start --always-new-process; "
                "elif command -v alacritty >/dev/null 2>&1; then exec alacritty; "
                "elif command -v xterm >/dev/null 2>&1; then exec xterm; "
                "else exec x-terminal-emulator; fi");
            break;
        case DESKTOP_ACTION_SETTINGS:
            launch_settings_app();
            break;
        default:
            break;
    }
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

static void activate_client(Display *d, Client *target_c, Client *clients, LayoutConfig *cfg) {
    if (!d || !target_c) return;

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

static void client_call_back_slot(Display *d, Client *clients, LayoutConfig *cfg, int slot) {
    if (!d || !clients || slot <= 0) return;
    Client *target_c = client_find_by_slot(clients, slot);
    activate_client(d, target_c, clients, cfg);
}

static void grab_alttab_keys(Display *d, Window r, int enable) {
    if (!d || r == None) return;
    KeyCode tab_kc = XKeysymToKeycode(d, XK_Tab);
    if (tab_kc == 0) return;
    const unsigned int locks[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (int l = 0; l < 4; l++) {
        if (enable) {
            XGrabKey(d, tab_kc, Mod1Mask | locks[l], r, True, GrabModeAsync, GrabModeAsync);
            XGrabKey(d, tab_kc, Mod1Mask | ShiftMask | locks[l], r, True, GrabModeAsync, GrabModeAsync);
        } else {
            XUngrabKey(d, tab_kc, Mod1Mask | locks[l], r);
            XUngrabKey(d, tab_kc, Mod1Mask | ShiftMask | locks[l], r);
        }
    }
}

static void update_ewmh_client_list(Display *d, Client *clients) {
    if (!d || root == None) return;
    Atom net_client_list = XInternAtom(d, "_NET_CLIENT_LIST", False);
    int count = 0;
    for (Client *c = clients; c; c = c->next) count++;
    if (count == 0) {
        XDeleteProperty(d, root, net_client_list);
        return;
    }
    Window *wins = malloc((size_t)count * sizeof(Window));
    if (!wins) return;
    int i = 0;
    for (Client *c = clients; c; c = c->next) {
        wins[i++] = c->win;
    }
    XChangeProperty(d, root, net_client_list, XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)wins, count);
    free(wins);
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
                                    1 /* borderless */,
                                    &init_x, &init_y, &win_w, &win_h);

    client_add(clients, w, CLIENT_FLOATING);
    Client *c = client_find(*clients, w);
    if (!c) return;

    c->frame = None;
    c->is_csd = is_csd;
    c->workspace = cfg->current_workspace;
    c->x = init_x;
    c->y = init_y;
    c->width = win_w;
    c->height = win_h;
    c->can_fullscreen = can_fs;
    client_get_title(d, w, c->title, sizeof(c->title));

    XAddToSaveSet(d, w);
    XSelectInput(d, w, PropertyChangeMask | StructureNotifyMask | FocusChangeMask | EnterWindowMask);
    XSetWindowBorderWidth(d, w, 0);

    const unsigned int mouse_locks[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (int l = 0; l < 4; l++) {
        XGrabButton(d, Button1, Mod4Mask | mouse_locks[l], w, False,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(d, Button3, Mod4Mask | mouse_locks[l], w, False,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, None);
    }

    XMapWindow(d, w);
    XMoveResizeWindow(d, w, c->x, c->y, c->width, c->height);

    set_focused_client(d, c, *clients);
    update_ewmh_client_list(d, *clients);
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
    update_ewmh_client_list(d, *clients);

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

    // EWMH Supported Atoms & WM Identity (_NET_SUPPORTING_WM_CHECK)
    Atom net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    Atom net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    Atom net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    Atom net_wm_strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
    Atom net_wm_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    Atom net_workarea = XInternAtom(dpy, "_NET_WORKAREA", False);
    Atom net_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    Atom muzu_reload_settings = XInternAtom(dpy, "_MUZU_RELOAD_SETTINGS", False);

    Atom supported[] = { net_supported, net_supporting_wm_check, net_client_list,
                         net_wm_strut, net_wm_strut_partial,
                         net_workarea, net_active, net_wm_name };
    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)supported, sizeof(supported) / sizeof(Atom));

    Window wm_check_win = XCreateSimpleWindow(dpy, root, -1, -1, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, wm_check_win, net_supporting_wm_check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&wm_check_win, 1);
    XChangeProperty(dpy, wm_check_win, net_wm_name, utf8_string, 8,
                    PropModeReplace, (unsigned char *)"MUZUwm", 6);
    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&wm_check_win, 1);
    XChangeProperty(dpy, root, net_wm_name, utf8_string, 8,
                    PropModeReplace, (unsigned char *)"MUZUwm", 6);

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
    int drag_resize = 0;
    int client_start_w = 0, client_start_h = 0;

    StrutList *strut_list = NULL;
    SnapPreview snap_preview;
    snap_preview_init(dpy, root, &snap_preview);

    Menu desktop_menu;
    menu_init(dpy, &desktop_menu, NULL, desktop_menu_callback, NULL);
    menu_add_item(&desktop_menu, "Terminal", DESKTOP_ACTION_TERMINAL);
    menu_add_item(&desktop_menu, "Internet", DESKTOP_ACTION_INTERNET);
    menu_add_item(&desktop_menu, "App", DESKTOP_ACTION_APPLICATIONS);
    menu_add_separator(&desktop_menu);
    menu_add_item(&desktop_menu, "Settings", DESKTOP_ACTION_SETTINGS);

    AltTab alttab = {0};
    alttab_init(dpy, &alttab, NULL);

    ShortcutConfig sc;
    config_rules_ensure_files();
    cfg.master_count = config_get_master_count();
    int alttab_enabled = config_is_alttab_enabled();
    shortcuts_load(&sc);
    shortcuts_grab_keys(dpy, root, &sc);
    grab_alttab_keys(dpy, root, alttab_enabled);

    // Grab Super + 1..0 for calling back window slots 1-10 and Super + X, M, Z for window actions
    const KeySym action_keys[] = { XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9, XK_0,
                                   XK_x, XK_X, XK_m, XK_M, XK_z, XK_Z };
    const unsigned int num_locks[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (size_t k = 0; k < sizeof(action_keys) / sizeof(action_keys[0]); k++) {
        KeyCode kc = XKeysymToKeycode(dpy, action_keys[k]);
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
            if (children[i] == wm_check_win || children[i] == desktop_menu.win ||
                children[i] == snap_preview.win ||
                (alttab.win != None && children[i] == alttab.win)) {
                continue;
            }
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
                if (alttab.is_visible) {
                    KeySym sym = XLookupKeysym(&ev.xkey, 0);
                    if (sym == XK_Escape) {
                        XUngrabKeyboard(dpy, CurrentTime);
                        alttab_hide(dpy, &alttab);
                        break;
                    }
                    if (sym == XK_Tab) {
                        if (ev.xkey.state & ShiftMask) alttab_select_prev(&alttab);
                        else alttab_select_next(&alttab);
                        alttab_render(dpy, &alttab);
                        break;
                    }
                    if (sym == XK_Left) {
                        alttab_select_prev(&alttab);
                        alttab_render(dpy, &alttab);
                        break;
                    }
                    if (sym == XK_Right) {
                        alttab_select_next(&alttab);
                        alttab_render(dpy, &alttab);
                        break;
                    }
                    if (sym == XK_Return || sym == XK_KP_Enter) {
                        XUngrabKeyboard(dpy, CurrentTime);
                        Window sel_win = alttab_get_selected_window(&alttab);
                        alttab_hide(dpy, &alttab);
                        if (sel_win != None) {
                            Client *target_c = client_find_by_any(clients, sel_win);
                            activate_client(dpy, target_c, clients, &cfg);
                        }
                        break;
                    }
                }

                if (desktop_menu.is_visible) {
                    KeySym sym = XLookupKeysym(&ev.xkey, 0);
                    if (sym == XK_Escape) {
                        menu_hide(dpy, &desktop_menu);
                        XUngrabPointer(dpy, CurrentTime);
                        break;
                    }
                }

                KeySym sym = XLookupKeysym(&ev.xkey, 0);
                unsigned int mods = ev.xkey.state & (Mod1Mask | Mod4Mask | ControlMask | ShiftMask);

                if (alttab_enabled && sym == XK_Tab && (ev.xkey.state & Mod1Mask)) {
                    alttab_populate(dpy, &alttab, clients);
                    if (alttab.app_count > 0) {
                        alttab_show(dpy, &alttab, screen_w, screen_h);
                        if (alttab.app_count > 1) {
                            if (ev.xkey.state & ShiftMask) alttab_select_prev(&alttab);
                            else alttab_select_next(&alttab);
                        }
                        alttab_render(dpy, &alttab);
                        XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
                        XRaiseWindow(dpy, alttab.win);
                    }
                    break;
                }

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

                    // Super + X / M / Z on focused window
                    Client *focused_win = NULL;
                    for (Client *c = clients; c; c = c->next) {
                        if (c->is_focused && !c->is_minimized) { focused_win = c; break; }
                    }

                    if (sym == XK_x || sym == XK_X) {
                        if (focused_win) {
                            client_close(dpy, focused_win->win);
                            break;
                        }
                    } else if (sym == XK_m || sym == XK_M) {
                        if (focused_win && focused_win->can_fullscreen) {
                            client_toggle_maximize(dpy, focused_win, &cfg);
                            if (focused_win->frame != None) titlebar_draw(dpy, focused_win, NULL);
                            reflow_all(dpy, clients, &cfg);
                            break;
                        }
                    } else if (sym == XK_z || sym == XK_Z) {
                        if (focused_win) {
                            focused_win->is_minimized = 1;
                            Window unmap_target = focused_win->frame != None ? focused_win->frame : focused_win->win;
                            XUnmapWindow(dpy, unmap_target);
                            if (focused_win->frame != None && focused_win->win != None) {
                                XUnmapWindow(dpy, focused_win->win);
                            }
                            focused_win->is_focused = 0;
                            Client *next_focus = NULL;
                            for (Client *curr = clients; curr; curr = curr->next) {
                                if (!curr->is_minimized && curr->workspace == cfg.current_workspace) {
                                    next_focus = curr;
                                    break;
                                }
                            }
                            set_focused_client(dpy, next_focus, clients);
                            reflow_all(dpy, clients, &cfg);
                            break;
                        }
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
                        case SHORTCUT_ACT_MINIMIZE:
                            if (focused) {
                                focused->is_minimized = 1;
                                Window unmap_target = focused->frame != None ? focused->frame : focused->win;
                                XUnmapWindow(dpy, unmap_target);
                                if (focused->frame != None && focused->win != None) {
                                    XUnmapWindow(dpy, focused->win);
                                }
                                focused->is_focused = 0;
                                Client *next_focus = NULL;
                                for (Client *curr = clients; curr; curr = curr->next) {
                                    if (!curr->is_minimized && curr->workspace == cfg.current_workspace) {
                                        next_focus = curr;
                                        break;
                                    }
                                }
                                set_focused_client(dpy, next_focus, clients);
                                reflow_all(dpy, clients, &cfg);
                            }
                            break;
                        case SHORTCUT_ACT_LAUNCHER:
                            config_spawn_command("if command -v mzshell >/dev/null 2>&1; then exec mzshell; fi");
                            break;
                        case SHORTCUT_ACT_EXEC:
                            if (strcmp(b->exec_cmd, "wezterm") == 0) {
                                spawn_terminal();
                            } else {
                                config_spawn_command(b->exec_cmd);
                            }
                            break;
                        default:
                            break;
                    }
                }
                break;
            }

            case KeyRelease: {
                KeySym rsym = XLookupKeysym(&ev.xkey, 0);
                if (alttab.is_visible && (rsym == XK_Alt_L || rsym == XK_Alt_R ||
                                          rsym == XK_Meta_L || rsym == XK_Meta_R)) {
                    XUngrabKeyboard(dpy, CurrentTime);
                    Window sel_win = alttab_get_selected_window(&alttab);
                    alttab_hide(dpy, &alttab);
                    if (sel_win != None) {
                        Client *target_c = client_find_by_any(clients, sel_win);
                        activate_client(dpy, target_c, clients, &cfg);
                    }
                }
                break;
            }

            case MapRequest: {
                Window w = ev.xmaprequest.window;
                if (w == desktop_menu.win || w == snap_preview.win ||
                    (alttab.win != None && w == alttab.win)) {
                    XMapWindow(dpy, w);
                    break;
                }
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
                if (w == desktop_menu.win || w == snap_preview.win ||
                    (alttab.win != None && w == alttab.win)) {
                    break;
                }
                Client *c = client_find_by_any(clients, w);
                if (c) {
                    if (ev.type == UnmapNotify && c->is_minimized) {
                        break;
                    }
                    client_teardown(dpy, &clients, w, &cfg, &drag_client, &snap_preview);
                    if (alttab.is_visible) {
                        alttab_populate(dpy, &alttab, clients);
                        if (alttab.app_count == 0) {
                            XUngrabKeyboard(dpy, CurrentTime);
                            alttab_hide(dpy, &alttab);
                        } else {
                            alttab_render(dpy, &alttab);
                        }
                    }
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
                if (w == root && atom == muzu_reload_settings) {
                    cfg.master_count = config_get_master_count();
                    int new_alttab = config_is_alttab_enabled();
                    if (new_alttab != alttab_enabled) {
                        alttab_enabled = new_alttab;
                        grab_alttab_keys(dpy, root, alttab_enabled);
                        if (!alttab_enabled && alttab.is_visible) {
                            XUngrabKeyboard(dpy, CurrentTime);
                            alttab_hide(dpy, &alttab);
                        }
                    }
                    reflow_all(dpy, clients, &cfg);
                } else if (atom == net_wm_strut || atom == net_wm_strut_partial) {
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
                if (desktop_menu.is_visible) {
                    if (ev.xbutton.window == desktop_menu.win) {
                        XUngrabPointer(dpy, CurrentTime);
                        menu_handle_click(dpy, &desktop_menu, ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
                        break;
                    } else {
                        menu_hide(dpy, &desktop_menu);
                        XUngrabPointer(dpy, CurrentTime);
                        if (ev.xbutton.button == Button3) {
                            menu_show(dpy, &desktop_menu, ev.xbutton.x_root, ev.xbutton.y_root, screen_w, screen_h);
                            XGrabPointer(dpy, root, True, ButtonPressMask | PointerMotionMask,
                                         GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
                            break;
                        }
                    }
                }

                if (ev.xbutton.window == root && ev.xbutton.button == Button3) {
                    menu_show(dpy, &desktop_menu, ev.xbutton.x_root, ev.xbutton.y_root, screen_w, screen_h);
                    XGrabPointer(dpy, root, True, ButtonPressMask | PointerMotionMask,
                                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
                    break;
                }

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
                        if (ev.xbutton.state & Mod4Mask) {
                            if (ev.xbutton.button == Button1) {
                                drag_client = c;
                                drag_resize = 0;
                                drag_start_x = ev.xbutton.x_root;
                                drag_start_y = ev.xbutton.y_root;
                                client_start_x = c->x;
                                client_start_y = c->y;
                                XGrabPointer(dpy, root, False,
                                             ButtonReleaseMask | PointerMotionMask,
                                             GrabModeAsync, GrabModeAsync,
                                             None, None, CurrentTime);
                            } else if (ev.xbutton.button == Button3) {
                                drag_client = c;
                                drag_resize = 1;
                                drag_start_x = ev.xbutton.x_root;
                                drag_start_y = ev.xbutton.y_root;
                                client_start_w = c->width;
                                client_start_h = c->height;
                                XGrabPointer(dpy, root, False,
                                             ButtonReleaseMask | PointerMotionMask,
                                             GrabModeAsync, GrabModeAsync,
                                             None, None, CurrentTime);
                            }
                        } else {
                            XAllowEvents(dpy, ReplayPointer, CurrentTime);
                        }
                    }
                }
                break;
            }

            case MotionNotify: {
                if (desktop_menu.is_visible) {
                    if (ev.xmotion.window == desktop_menu.win) {
                        menu_handle_motion(dpy, &desktop_menu, ev.xmotion.x, ev.xmotion.y);
                    } else if (desktop_menu.hover_idx != -1) {
                        desktop_menu.hover_idx = -1;
                        menu_render(dpy, &desktop_menu);
                    }
                    break;
                }

                if (drag_client) {
                    int dx = ev.xmotion.x_root - drag_start_x;
                    int dy = ev.xmotion.y_root - drag_start_y;
                    if (drag_resize) {
                        int nw = client_start_w + dx;
                        int nh = client_start_h + dy;
                        if (nw < 100) nw = 100;
                        if (nh < 60) nh = 60;
                        drag_client->width = nw;
                        drag_client->height = nh;
                        Window target = drag_client->frame != None ? drag_client->frame : drag_client->win;
                        XResizeWindow(dpy, target, drag_client->width, drag_client->height);
                    } else if (abs(dx) > 4 || abs(dy) > 4) {
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
                if (desktop_menu.is_visible) {
                    break;
                }

                if (drag_client) {
                    XUngrabPointer(dpy, CurrentTime);
                    if (drag_resize) {
                        drag_resize = 0;
                    } else {
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
                    }
                    drag_client = NULL;
                }
                break;
            }

            case Expose: {
                if (alttab.is_visible && ev.xexpose.window == alttab.win) {
                    alttab_render(dpy, &alttab);
                    break;
                }
                if (desktop_menu.is_visible && ev.xexpose.window == desktop_menu.win) {
                    menu_render(dpy, &desktop_menu);
                    break;
                }

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
    Window alttab_win = alttab.win;
    alttab_cleanup(&alttab);
    if (alttab_win != None) {
        XDestroyWindow(dpy, alttab_win);
    }
    menu_cleanup(&desktop_menu);
    if (desktop_menu.win != None) {
        XDestroyWindow(dpy, desktop_menu.win);
        desktop_menu.win = None;
    }
    snap_preview_destroy(dpy, &snap_preview);
    while (clients) {
        client_teardown(dpy, &clients, clients->win, &cfg, NULL, NULL);
    }
    if (wm_check_win != None) {
        XDestroyWindow(dpy, wm_check_win);
        wm_check_win = None;
    }
    XCloseDisplay(dpy);
    return 0;
}
