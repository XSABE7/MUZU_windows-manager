#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include "wm.h"
#include "titlebar.h"
#include "client_props.h"
#include "ewmh.h"
#include "monitors.h"
#include "workspace.h"
#include "config_rules.h"
#include "struts.h"
#include "snap_preview.h"
#include "menu.h"
#include "alttab.h"

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t wm_conflict_detected = 0;
static int sig_pipe[2] = { -1, -1 };
static Display *dpy = NULL;
static Window root = None;
static Monitor monitors[MAX_MONITORS];
static int monitor_count = 0;
static int total_screen_w = 0;
static int total_screen_h = 0;
static int num_workspaces = DEFAULT_WORKSPACE_COUNT;
static int focus_follows_mouse_enabled = 1;
static int desktop_menu_enabled = 1;
static int current_border_width = 2;
static unsigned long focus_border_pixel = 0;
static unsigned long unfocus_border_pixel = 0;
static int cad_pending = 0;
static Time cad_first_time = 0;
static DragState drag = {0};
static SnapPreview snap_preview;

static void sig_handler(int sig) {
    (void)sig;
    running = 0;
    if (sig_pipe[1] >= 0) {
        int saved = errno;
        ssize_t r = write(sig_pipe[1], "x", 1);
        (void)r;
        errno = saved;
    }
}

static void cancel_active_drag(Display *d) {
    if (!drag.client) return;
    wm_drag_reset(&drag);
    if (d) {
        XUngrabPointer(d, CurrentTime);
        snap_preview_hide(d, &snap_preview);
    }
}

static void sigchld_handler(int sig) {
    (void)sig;
    int saved = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = saved;
}

static int startup_xerror_handler(Display *d, XErrorEvent *ee) {
    (void)d;
    if (ee && ee->error_code == BadAccess) {
        wm_conflict_detected = 1;
    }
    return 0;
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
    wm_find_readable_path(candidates, sizeof(candidates) / sizeof(candidates[0]),
                          settings_cmd, sizeof(settings_cmd));

    pid_t pid = fork();
    if (pid != 0) return;
    setsid();
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);

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

static unsigned long alloc_hex_pixel(Display *d, const char *hex, unsigned long fallback_px) {
    if (!d) return fallback_px;
    unsigned short r16 = 0, g16 = 0, b16 = 0;
    if (!config_parse_hex_color(hex, &r16, &g16, &b16)) {
        return fallback_px;
    }
    XColor xc;
    memset(&xc, 0, sizeof(xc));
    xc.red = r16;
    xc.green = g16;
    xc.blue = b16;
    xc.flags = DoRed | DoGreen | DoBlue;
    Colormap cmap = DefaultColormap(d, DefaultScreen(d));
    if (XAllocColor(d, cmap, &xc)) {
        return xc.pixel;
    }
    return fallback_px;
}

static void apply_client_border(Display *d, const Client *c, int bw) {
    if (!d || !c || c->win == None) return;
    Window target = (c->frame != None) ? c->frame : c->win;
    if (c->is_fullscreen) {
        XSetWindowBorderWidth(d, target, 0);
        return;
    }
    int clamped = (bw > 0) ? bw : 0;
    XSetWindowBorderWidth(d, target, (unsigned int)clamped);
    if (clamped > 0) {
        XSetWindowBorder(d, target, c->is_focused ? focus_border_pixel : unfocus_border_pixel);
    }
}

static void load_appearance_settings(Display *d, LayoutConfig *cfg) {
    focus_follows_mouse_enabled = config_is_focus_follows_mouse_enabled();
    desktop_menu_enabled = config_is_desktop_menu_enabled();
    current_border_width = config_get_border_width();
    if (cfg) cfg->border_width = current_border_width;

    if (d) {
        int scr = DefaultScreen(d);
        char focus_hex[32], unfocus_hex[32];
        config_get_focus_border_color(focus_hex, sizeof(focus_hex));
        config_get_unfocus_border_color(unfocus_hex, sizeof(unfocus_hex));
        focus_border_pixel = alloc_hex_pixel(d, focus_hex, WhitePixel(d, scr));
        unfocus_border_pixel = alloc_hex_pixel(d, unfocus_hex, BlackPixel(d, scr));
    }
}

static void find_local_mzml(char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';
    char exe_path[1024];
    char local_mzml[1100] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        char *slash = strrchr(exe_path, '/');
        if (slash) {
            *slash = '\0';
            snprintf(local_mzml, sizeof(local_mzml), "%s/mzml", exe_path);
        }
    }
    const char *candidates[] = {
        local_mzml[0] ? local_mzml : NULL,
        "./mzml",
        "/usr/local/bin/mzml",
        "/usr/bin/mzml",
        NULL
    };
    for (size_t i = 0; candidates[i] != NULL || i < 4; i++) {
        if (candidates[i] && access(candidates[i], X_OK) == 0) {
            snprintf(out, out_len, "%s", candidates[i]);
            return;
        }
    }
}

static void spawn_mzml_fallback(void) {
    char mzml_path[1100] = {0};
    find_local_mzml(mzml_path, sizeof(mzml_path));
    if (mzml_path[0] != '\0') {
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "exec \"%s\"", mzml_path);
        config_spawn_command(cmd);
    } else {
        config_spawn_command("if command -v mzml >/dev/null 2>&1; then exec mzml; fi");
    }
}

static void spawn_launcher(void) {
    char mzml_path[1100] = {0};
    find_local_mzml(mzml_path, sizeof(mzml_path));
    if (mzml_path[0] != '\0') {
        char cmd[1400];
        snprintf(cmd, sizeof(cmd),
                 "if command -v mzshell >/dev/null 2>&1; then exec mzshell; "
                 "else exec \"%s\"; fi", mzml_path);
        config_spawn_command(cmd);
    } else {
        config_spawn_command(
            "if command -v mzshell >/dev/null 2>&1; then exec mzshell; "
            "elif command -v mzml >/dev/null 2>&1; then exec mzml; "
            "elif command -v rofi >/dev/null 2>&1; then exec rofi -show drun; fi");
    }
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
            spawn_launcher();
            break;
        case DESKTOP_ACTION_MZML:
            spawn_mzml_fallback();
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
        if (was_focused != curr->is_focused || curr == c) {
            apply_client_border(d, curr, current_border_width);
            if (curr->frame != None) {
                titlebar_draw(d, curr, NULL);
            }
        }
    }
    Window active_win = ewmh_resolve_active_window(c);
    if (c && active_win != None) {
        XSetInputFocus(d, c->win, RevertToPointerRoot, CurrentTime);
    } else {
        XSetInputFocus(d, root, RevertToPointerRoot, CurrentTime);
    }
    ewmh_set_active_window(d, root, active_win);
}

static void dismiss_desktop_menu(Display *d, Menu *menu, Client *clients, int current_ws) {
    if (!d || !menu) return;
    menu_hide(d, menu);
    XUngrabPointer(d, CurrentTime);
    Client *refocus = NULL;
    for (Client *curr = clients; curr; curr = curr->next) {
        if (curr->is_focused && !curr->is_minimized && curr->workspace == current_ws) {
            refocus = curr;
            break;
        }
    }
    set_focused_client(d, refocus, clients);
}

static void refresh_struts_and_workarea(Display *d, StrutList *strut_list, LayoutConfig *cfg) {
    if (!d || !cfg) return;
    if (monitor_count > 0) {
        monitor_apply_struts(monitors, monitor_count, strut_list, total_screen_w, total_screen_h);
        int prim = 0;
        for (int i = 0; i < monitor_count; i++) {
            if (monitors[i].primary) { prim = i; break; }
        }
        int saved_bw = cfg->border_width;
        monitor_to_layout_config(&monitors[prim], cfg->master_count, cfg->current_workspace, cfg);
        cfg->border_width = saved_bw;
        int wa_x = monitors[prim].x + monitors[prim].left_inset;
        int wa_y = monitors[prim].y + monitors[prim].top_inset;
        int wa_w = monitors[prim].width - monitors[prim].left_inset - monitors[prim].right_inset;
        int wa_h = monitors[prim].height - monitors[prim].top_inset - monitors[prim].bottom_inset;
        strut_apply_workarea_rect_desktops(d, root, wa_x, wa_y, wa_w, wa_h, num_workspaces);
    } else {
        int l = 0, r = 0, t = 0, b = 0;
        strut_calculate_insets(strut_list, total_screen_w, total_screen_h, &l, &r, &t, &b);
        cfg->top_inset = t;
        cfg->bottom_inset = b;
        strut_apply_workarea_rect_desktops(d, root, l, t,
                                           total_screen_w - l - r,
                                           total_screen_h - t - b,
                                           num_workspaces);
    }
}

static int get_pointer_monitor(Display *d, int *out_px, int *out_py) {
    int rx = 0, ry = 0, wx = 0, wy = 0;
    Window rr = None, cr = None;
    unsigned int mask = 0;
    if (d && root != None && XQueryPointer(d, root, &rr, &cr, &rx, &ry, &wx, &wy, &mask)) {
        if (out_px) *out_px = rx;
        if (out_py) *out_py = ry;
        return monitor_find_for_point(monitors, monitor_count, rx, ry);
    }
    if (out_px) *out_px = 0;
    if (out_py) *out_py = 0;
    return 0;
}

static void monitor_layout_for_client(const Client *c, const LayoutConfig *base_cfg, LayoutConfig *out_cfg) {
    if (!out_cfg || !base_cfg) return;
    if (c && monitor_count > 0) {
        int m = monitor_find_for_rect(monitors, monitor_count, c->x, c->y, c->width, c->height);
        monitor_to_layout_config(&monitors[m], base_cfg->master_count, base_cfg->current_workspace, out_cfg);
        out_cfg->border_width = base_cfg->border_width;
    } else {
        *out_cfg = *base_cfg;
    }
}

static void reflow_all(Display *d, Client *clients, LayoutConfig *cfg) {
    if (!d || !cfg) return;
    if (monitor_count <= 0) {
        int count = client_count_tiled(clients);
        layout_calculate(cfg, clients, count);
    } else {
        for (int m = 0; m < monitor_count; m++) {
            LayoutConfig mon_cfg;
            monitor_to_layout_config(&monitors[m], cfg->master_count, cfg->current_workspace, &mon_cfg);
            mon_cfg.border_width = cfg->border_width;
            Client *tiled[128];
            int idx = 0;
            for (Client *curr = clients; curr && idx < 128; curr = curr->next) {
                if (curr->mode == CLIENT_TILED && !curr->is_minimized && !curr->is_fullscreen &&
                    curr->workspace == cfg->current_workspace &&
                    monitor_find_for_rect(monitors, monitor_count, curr->x, curr->y, curr->width, curr->height) == m) {
                    tiled[idx++] = curr;
                }
            }
            if (idx > 0) {
                for (int i = 0; i < idx / 2; i++) {
                    Client *tmp = tiled[i];
                    tiled[i] = tiled[idx - 1 - i];
                    tiled[idx - 1 - i] = tmp;
                }
                layout_calculate_clients(&mon_cfg, tiled, idx);
            }
        }
    }

    for (Client *c = clients; c; c = c->next) {
        if (c->is_minimized || c->is_fullscreen || c->workspace != cfg->current_workspace) continue;
        if (c->mode == CLIENT_TILED || c->is_maximized) {
            if (c->is_maximized) {
                LayoutConfig mon_cfg;
                monitor_layout_for_client(c, cfg, &mon_cfg);
                int bw = (mon_cfg.border_width > 0) ? mon_cfg.border_width : 0;
                int mw = mon_cfg.screen_width - mon_cfg.left_inset - mon_cfg.right_inset - 2 * bw;
                int mh = mon_cfg.screen_height - mon_cfg.top_inset - mon_cfg.bottom_inset - 2 * bw;
                if (mw < 10) mw = 10;
                if (mh < 10) mh = 10;
                c->x = mon_cfg.mon_x + mon_cfg.left_inset;
                c->y = mon_cfg.mon_y + mon_cfg.top_inset;
                c->width = mw;
                c->height = mh;
            }
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

static void raise_client_with_transients(Display *d, Client *target_c, Client *clients) {
    if (!d || !target_c) return;
    Window target = target_c->frame != None ? target_c->frame : target_c->win;
    XRaiseWindow(d, target);

    for (Client *c = clients; c; c = c->next) {
        if (c == target_c || c->is_minimized || c->workspace != target_c->workspace) continue;
        if (c->transient_for == target_c->win) {
            Window child_w = c->frame != None ? c->frame : c->win;
            XRaiseWindow(d, child_w);
            for (Client *gc = clients; gc; gc = gc->next) {
                if (gc == c || gc == target_c || gc->is_minimized || gc->workspace != target_c->workspace) continue;
                if (gc->transient_for == c->win) {
                    Window gc_w = gc->frame != None ? gc->frame : gc->win;
                    XRaiseWindow(d, gc_w);
                }
            }
        }
    }
}

static void activate_client(Display *d, Client *target_c, Client *clients, LayoutConfig *cfg) {
    if (!d || !target_c) return;

    int switched = 0;
    if (target_c->workspace != cfg->current_workspace) {
        switched = workspace_switch(d, root, clients, cfg, target_c->workspace, num_workspaces, NULL);
    }

    if (target_c->is_minimized) {
        target_c->is_minimized = 0;
        Window target = target_c->frame != None ? target_c->frame : target_c->win;
        XMapWindow(d, target);
        if (target_c->frame != None && target_c->win != None) {
            XMapWindow(d, target_c->win);
        }
        ewmh_sync_client_state(d, target_c->win, target_c->is_fullscreen,
                               target_c->is_maximized, target_c->is_minimized);
        raise_client_with_transients(d, target_c, clients);
        set_focused_client(d, target_c, clients);
        reflow_all(d, clients, cfg);
    } else {
        raise_client_with_transients(d, target_c, clients);
        set_focused_client(d, target_c, clients);
        if (switched) reflow_all(d, clients, cfg);
    }
}

static void switch_to_workspace(Display *d, Client *clients, LayoutConfig *cfg, int target_ws) {
    if (drag.client && drag.client->workspace != target_ws) {
        cancel_active_drag(d);
    }
    Client *next_focus = NULL;
    if (workspace_switch(d, root, clients, cfg, target_ws, num_workspaces, &next_focus)) {
        set_focused_client(d, next_focus, clients);
        if (next_focus) raise_client_with_transients(d, next_focus, clients);
        reflow_all(d, clients, cfg);
    }
}

static void move_client_to_workspace(Display *d, Client *c, Client *clients, LayoutConfig *cfg, int target_ws) {
    if (!c) return;
    if (wm_drag_should_cancel(&drag, c) && target_ws != cfg->current_workspace) {
        cancel_active_drag(d);
    }
    Client *next_focus = NULL;
    if (workspace_move_client(d, c, clients, cfg, target_ws, num_workspaces, &next_focus)) {
        if (c->workspace != cfg->current_workspace) {
            set_focused_client(d, next_focus, clients);
        } else {
            set_focused_client(d, c, clients);
            raise_client_with_transients(d, c, clients);
        }
        reflow_all(d, clients, cfg);
    }
}

static void minimize_client(Display *d, Client *c, Client *clients, LayoutConfig *cfg) {
    if (!d || !c || c->is_minimized) return;
    if (wm_drag_should_cancel(&drag, c)) {
        cancel_active_drag(d);
    }
    c->is_minimized = 1;
    Window unmap_target = c->frame != None ? c->frame : c->win;
    XUnmapWindow(d, unmap_target);
    if (c->frame != None && c->win != None) {
        XUnmapWindow(d, c->win);
    }
    ewmh_sync_client_state(d, c->win, c->is_fullscreen, c->is_maximized, c->is_minimized);
    if (c->is_focused) {
        c->is_focused = 0;
        Client *next_focus = NULL;
        for (Client *curr = clients; curr; curr = curr->next) {
            if (!curr->is_minimized && curr->workspace == cfg->current_workspace) {
                next_focus = curr;
                break;
            }
        }
        set_focused_client(d, next_focus, clients);
    }
    reflow_all(d, clients, cfg);
}

static void client_call_back_slot(Display *d, Client *clients, LayoutConfig *cfg, int slot) {
    if (!d || !clients || slot <= 0) return;
    Client *target_c = client_find_by_slot(clients, slot);
    activate_client(d, target_c, clients, cfg);
}

static void grab_alttab_keys(Display *d, Window r, int enable) {
    if (!d || r == None) return;
    const KeySym keys[2] = { XK_Tab, XK_grave };
    const unsigned int locks[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (int k = 0; k < 2; k++) {
        KeyCode kc = XKeysymToKeycode(d, keys[k]);
        if (kc == 0) continue;
        for (int l = 0; l < 4; l++) {
            if (enable) {
                XGrabKey(d, kc, Mod1Mask | locks[l], r, True, GrabModeAsync, GrabModeAsync);
                XGrabKey(d, kc, Mod1Mask | ShiftMask | locks[l], r, True, GrabModeAsync, GrabModeAsync);
            } else {
                XUngrabKey(d, kc, Mod1Mask | locks[l], r);
                XUngrabKey(d, kc, Mod1Mask | ShiftMask | locks[l], r);
            }
        }
    }
}

static void start_alttab_switcher(Display *d, AltTab *at, Client *clients,
                                  int current_ws, int same_app_only, int reverse) {
    if (!d || !at) return;
    Client *focused_win = NULL;
    for (Client *c = clients; c; c = c->next) {
        if (c->is_focused && !c->is_minimized && c->workspace == current_ws) {
            focused_win = c;
            break;
        }
    }
    alttab_populate_workspace(d, at, clients, current_ws);
    if (at->app_count <= 0) return;

    if (focused_win) {
        alttab_select_window(at, focused_win->win);
    }

    int m = get_pointer_monitor(d, NULL, NULL);
    int mw = (monitor_count > 0) ? monitors[m].width : total_screen_w;
    int mh = (monitor_count > 0) ? monitors[m].height : total_screen_h;
    alttab_show(d, at, mw, mh);
    if (monitor_count > 0 && at->win != None) {
        XMoveWindow(d, at->win, monitors[m].x + at->x, monitors[m].y + at->y);
    }

    if (same_app_only) {
        if (reverse) alttab_select_prev_win(at);
        else alttab_select_next_win(at);
    } else if (at->app_count > 1) {
        if (reverse) alttab_select_prev(at);
        else alttab_select_next(at);
    }

    alttab_render(d, at);
    if (XGrabKeyboard(d, root, True, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess) {
        alttab_hide(d, at);
        return;
    }
    XRaiseWindow(d, at->win);
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

    WindowType wtype = client_get_window_type(d, w);
    Window trans = client_get_transient_for(d, w);

    if (!client_should_manage_as_normal(wtype)) {
        XMapWindow(d, w);
        if (wtype == WIN_TYPE_DESKTOP) {
            XLowerWindow(d, w);
        } else {
            XRaiseWindow(d, w);
        }
        return;
    }

    int is_csd = client_is_csd(d, w);
    int can_fs = client_supports_fullscreen(d, w);
    if (wtype == WIN_TYPE_DIALOG || wtype == WIN_TYPE_UTILITY || wtype == WIN_TYPE_SPLASH) {
        can_fs = 0;
    }

    Client *parent = (trans != None) ? client_find(*clients, trans) : NULL;

    int mon_idx = 0;
    if (parent && monitor_count > 0) {
        mon_idx = monitor_find_for_rect(monitors, monitor_count, parent->x, parent->y, parent->width, parent->height);
    } else if (monitor_count > 0) {
        mon_idx = get_pointer_monitor(d, NULL, NULL);
    }
    LayoutConfig mon_cfg = *cfg;
    if (monitor_count > 0) {
        monitor_to_layout_config(&monitors[mon_idx], cfg->master_count, cfg->current_workspace, &mon_cfg);
        mon_cfg.border_width = cfg->border_width;
    }

    int init_x = mon_cfg.mon_x + mon_cfg.left_inset;
    int init_y = mon_cfg.mon_y + mon_cfg.top_inset;
    int win_w = 600, win_h = 400;
    int bw = (mon_cfg.border_width > 0) ? mon_cfg.border_width : 0;
    int avail_w = mon_cfg.screen_width - mon_cfg.left_inset - mon_cfg.right_inset - 2 * bw;
    int avail_h = mon_cfg.screen_height - mon_cfg.top_inset - mon_cfg.bottom_inset - 2 * bw;
    if (avail_w < 10) avail_w = 10;
    if (avail_h < 10) avail_h = 10;

    if (parent && ((wa.x == 0 && wa.y == 0) || wtype == WIN_TYPE_DIALOG)) {
        client_center_over_parent(parent->x, parent->y, parent->width, parent->height,
                                  mon_cfg.mon_x + mon_cfg.left_inset, mon_cfg.mon_y + mon_cfg.top_inset,
                                  avail_w, avail_h,
                                  wa.width, wa.height,
                                  &init_x, &init_y, &win_w, &win_h);
    } else {
        int rel_x = (wa.x > 0 || wa.y > 0) ? (wa.x - mon_cfg.mon_x - mon_cfg.left_inset) : 0;
        int rel_y = (wa.x > 0 || wa.y > 0) ? (wa.y - mon_cfg.mon_y) : 0;
        client_resolve_initial_geometry(avail_w, mon_cfg.screen_height,
                                        mon_cfg.top_inset, mon_cfg.bottom_inset + 2 * bw,
                                        rel_x, rel_y, wa.width, wa.height,
                                        1 /* borderless */,
                                        &init_x, &init_y, &win_w, &win_h);
        init_x += mon_cfg.mon_x + mon_cfg.left_inset;
        init_y += mon_cfg.mon_y;
    }

    client_add(clients, w, CLIENT_FLOATING);
    Client *c = client_find(*clients, w);
    if (!c) return;

    c->frame = None;
    c->is_csd = is_csd;
    c->workspace = parent ? parent->workspace : cfg->current_workspace;
    c->transient_for = trans;
    c->x = init_x;
    c->y = init_y;
    c->width = win_w;
    c->height = win_h;
    c->can_fullscreen = can_fs;
    client_get_title(d, w, c->title, sizeof(c->title));

    XAddToSaveSet(d, w);
    XSelectInput(d, w, PropertyChangeMask | StructureNotifyMask | FocusChangeMask | EnterWindowMask);
    apply_client_border(d, c, cfg->border_width);

    const unsigned int mouse_locks[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (int l = 0; l < 4; l++) {
        XGrabButton(d, Button1, mouse_locks[l], w, False,
                    ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
        XGrabButton(d, Button3, mouse_locks[l], w, False,
                    ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
        XGrabButton(d, Button1, Mod4Mask | mouse_locks[l], w, False,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(d, Button3, Mod4Mask | mouse_locks[l], w, False,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, None);
    }

    workspace_set_client_desktop(d, w, c->workspace);
    if (c->workspace != cfg->current_workspace) {
        c->ws_hidden = 1;
        XMoveResizeWindow(d, w, c->x, c->y, c->width, c->height);
    } else {
        XMapWindow(d, w);
        if (ewmh_window_has_fullscreen_state(d, w)) {
            ewmh_apply_fullscreen(d, c, &mon_cfg, 1);
        } else {
            XMoveResizeWindow(d, w, c->x, c->y, c->width, c->height);
        }
        set_focused_client(d, c, *clients);
    }
    update_ewmh_client_list(d, *clients);
}

static void client_teardown(Display *d, Client **clients, Window w, LayoutConfig *cfg,
                            DragState *drag, SnapPreview *preview) {
    if (!d || !clients || w == None) return;
    Client *c = client_find_by_any(*clients, w);
    if (!c) return;

    if (drag && drag->client == c) {
        drag->client = NULL;
        drag->is_resize = 0;
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
    if (pipe2(sig_pipe, O_NONBLOCK | O_CLOEXEC) != 0) {
        sig_pipe[0] = sig_pipe[1] = -1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Do NOT set SA_RESTART so poll() wakes on termination signals
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    signal(SIGPIPE, SIG_IGN);

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "muzuwm: cannot open display\n");
        return 1;
    }
    int x11_fd = ConnectionNumber(dpy);
    int fd_flags = fcntl(x11_fd, F_GETFD);
    if (fd_flags >= 0) {
        fcntl(x11_fd, F_SETFD, fd_flags | FD_CLOEXEC);
    }

    int screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    total_screen_w = DisplayWidth(dpy, screen);
    total_screen_h = DisplayHeight(dpy, screen);

    wm_conflict_detected = 0;
    XSetErrorHandler(startup_xerror_handler);
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask |
                            StructureNotifyMask | ButtonPressMask | KeyPressMask | PropertyChangeMask);
    XSync(dpy, False);
    if (wm_conflict_detected) {
        fprintf(stderr, "muzuwm: another window manager is already running\n");
        XCloseDisplay(dpy);
        return 1;
    }

    int rr_event_base = 0, rr_error_base = 0;
    int has_xrandr = XRRQueryExtension(dpy, &rr_event_base, &rr_error_base);
    if (has_xrandr) {
        XRRSelectInput(dpy, root, RRScreenChangeNotifyMask);
    }
    monitor_count = monitors_query_xrandr(dpy, root, monitors, MAX_MONITORS);

    XSetErrorHandler(x_error_handler);

    // EWMH Supported Atoms & WM Identity (_NET_SUPPORTING_WM_CHECK)
    Atom net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    Atom net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    Atom net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    Atom net_wm_strut = XInternAtom(dpy, "_NET_WM_STRUT", False);
    Atom net_wm_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    Atom net_workarea = XInternAtom(dpy, "_NET_WORKAREA", False);
    Atom net_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_type_desktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    Atom net_wm_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    Atom net_wm_type_notify = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    Atom net_wm_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    Atom net_wm_type_splash = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    Atom net_wm_type_utility = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    Atom net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom net_wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    Atom net_wm_state_max_horz = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom net_wm_state_max_vert = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom net_wm_state_hidden = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
    Atom net_close_window = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
    Atom net_num_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    Atom net_cur_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    Atom net_wm_desktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    Atom muzu_reload_settings = XInternAtom(dpy, "_MUZU_RELOAD_SETTINGS", False);

    Atom supported[] = { net_supported, net_supporting_wm_check, net_client_list,
                         net_wm_strut, net_wm_strut_partial,
                         net_workarea, net_active, net_wm_name,
                         net_wm_window_type, net_wm_type_desktop, net_wm_type_dock,
                         net_wm_type_notify, net_wm_type_dialog, net_wm_type_splash,
                         net_wm_type_utility,
                         net_wm_state, net_wm_state_fullscreen,
                         net_wm_state_max_horz, net_wm_state_max_vert,
                         net_wm_state_hidden, net_close_window,
                         net_num_desktops, net_cur_desktop, net_wm_desktop };
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
        .mon_x = monitors[0].x,
        .mon_y = monitors[0].y,
        .screen_width = monitors[0].width > 0 ? monitors[0].width : total_screen_w,
        .screen_height = monitors[0].height > 0 ? monitors[0].height : total_screen_h,
        .master_count = 1,
        .left_inset = 0,
        .right_inset = 0,
        .top_inset = 0,
        .bottom_inset = 0,
        .current_workspace = 0
    };

    Client *clients = NULL;
    memset(&drag, 0, sizeof(drag));

    StrutList *strut_list = NULL;
    snap_preview_init(dpy, root, &snap_preview);

    Menu desktop_menu;
    menu_init(dpy, &desktop_menu, NULL, desktop_menu_callback, NULL);
    menu_add_item(&desktop_menu, "Terminal", DESKTOP_ACTION_TERMINAL);
    menu_add_item(&desktop_menu, "Internet", DESKTOP_ACTION_INTERNET);
    menu_add_item(&desktop_menu, "App", DESKTOP_ACTION_APPLICATIONS);
    menu_add_item(&desktop_menu, "Fallback Launcher (mzml)", DESKTOP_ACTION_MZML);
    menu_add_separator(&desktop_menu);
    menu_add_item(&desktop_menu, "Settings", DESKTOP_ACTION_SETTINGS);

    AltTab alttab = {0};
    alttab_init(dpy, &alttab, NULL);

    ShortcutConfig sc;
    config_rules_ensure_files();
    cfg.master_count = config_get_master_count();
    load_appearance_settings(dpy, &cfg);
    num_workspaces = config_get_workspace_count();
    workspace_publish_ewmh(dpy, root, num_workspaces, cfg.current_workspace);
    int alttab_enabled = config_is_alttab_enabled();
    shortcuts_load(&sc);
    shortcuts_grab_keys(dpy, root, &sc);
    grab_alttab_keys(dpy, root, alttab_enabled);

    // Hardcoded emergency safety hatch: Ctrl+Alt + 2x Delete force-opens desktop_menu
    KeyCode cad_kc = XKeysymToKeycode(dpy, XK_Delete);
    if (cad_kc != 0) {
        const unsigned int cad_locks[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
        for (int l = 0; l < 4; l++) {
            XGrabKey(dpy, cad_kc, ControlMask | Mod1Mask | cad_locks[l], root, True,
                     GrabModeSync, GrabModeAsync);
        }
    }

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
    refresh_struts_and_workarea(dpy, strut_list, &cfg);

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
        refresh_struts_and_workarea(dpy, strut_list, &cfg);
        reflow_all(dpy, clients, &cfg);
    }

    config_run_autostart();

    XEvent ev;
    while (running) {
        while (running && XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);

            switch (ev.type) {
            case KeyPress: {
                KeySym raw_sym = XLookupKeysym(&ev.xkey, 0);
                unsigned int raw_mods = ev.xkey.state & (Mod1Mask | Mod4Mask | ControlMask | ShiftMask);

                // Hardcoded emergency trigger: Ctrl+Alt + 2x Delete force-opens desktop_menu
                if ((raw_mods & (ControlMask | Mod1Mask)) == (ControlMask | Mod1Mask) &&
                    !(raw_mods & Mod4Mask) && raw_sym == XK_Delete) {
                    CadAction cad_act = wm_handle_cad_keypress(
                        ev.xkey.time, ev.xkey.send_event, &cad_pending, &cad_first_time, 500);
                    if (cad_act == CAD_ACTION_TRIGGER_MENU) {
                        XAllowEvents(dpy, AsyncKeyboard, ev.xkey.time);
                        cancel_active_drag(dpy);
                        if (alttab.is_visible) {
                            XUngrabKeyboard(dpy, CurrentTime);
                            alttab_hide(dpy, &alttab);
                        }
                        if (desktop_menu.is_visible) {
                            menu_hide(dpy, &desktop_menu);
                            XUngrabPointer(dpy, CurrentTime);
                        }
                        int px = 0, py = 0;
                        int m = get_pointer_monitor(dpy, &px, &py);
                        if (px == 0 && py == 0) {
                            px = (monitor_count > 0) ? (monitors[m].x + monitors[m].width / 2) : (total_screen_w / 2);
                            py = (monitor_count > 0) ? (monitors[m].y + monitors[m].height / 2) : (total_screen_h / 2);
                        }
                        int mx_end = (monitor_count > 0) ? (monitors[m].x + monitors[m].width) : total_screen_w;
                        int my_end = (monitor_count > 0) ? (monitors[m].y + monitors[m].height) : total_screen_h;
                        menu_show(dpy, &desktop_menu, px, py, mx_end, my_end);
                        if (XGrabPointer(dpy, root, True, ButtonPressMask | PointerMotionMask,
                                         GrabModeAsync, GrabModeAsync, None, None, CurrentTime) != GrabSuccess) {
                            dismiss_desktop_menu(dpy, &desktop_menu, clients, cfg.current_workspace);
                        }
                    } else if (cad_act == CAD_ACTION_REPLAY_FIRST) {
                        XAllowEvents(dpy, ReplayKeyboard, ev.xkey.time);
                    }
                    break;
                }

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
                    if (sym == XK_grave || sym == XK_asciitilde) {
                        if (ev.xkey.state & ShiftMask) alttab_select_prev_win(&alttab);
                        else alttab_select_next_win(&alttab);
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
                        dismiss_desktop_menu(dpy, &desktop_menu, clients, cfg.current_workspace);
                        break;
                    }
                }

                KeySym sym = XLookupKeysym(&ev.xkey, 0);
                unsigned int mods = ev.xkey.state & (Mod1Mask | Mod4Mask | ControlMask | ShiftMask);

                if (alttab_enabled && (ev.xkey.state & Mod1Mask) &&
                    (sym == XK_Tab || sym == XK_grave || sym == XK_asciitilde)) {
                    if (desktop_menu.is_visible) {
                        dismiss_desktop_menu(dpy, &desktop_menu, clients, cfg.current_workspace);
                    }
                    int same_app = (sym == XK_grave || sym == XK_asciitilde);
                    start_alttab_switcher(dpy, &alttab, clients, cfg.current_workspace,
                                          same_app, (ev.xkey.state & ShiftMask) != 0);
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
                        if (c->is_focused && !c->is_minimized && c->workspace == cfg.current_workspace) { focused_win = c; break; }
                    }

                    if (sym == XK_x || sym == XK_X) {
                        if (focused_win) {
                            client_close(dpy, focused_win->win);
                            break;
                        }
                    } else if (sym == XK_m || sym == XK_M) {
                        if (focused_win && focused_win->can_fullscreen && !focused_win->is_fullscreen) {
                            LayoutConfig mon_cfg;
                            monitor_layout_for_client(focused_win, &cfg, &mon_cfg);
                            client_toggle_maximize(dpy, focused_win, &mon_cfg);
                            ewmh_sync_client_state(dpy, focused_win->win, focused_win->is_fullscreen,
                                                   focused_win->is_maximized, focused_win->is_minimized);
                            if (focused_win->frame != None) titlebar_draw(dpy, focused_win, NULL);
                            reflow_all(dpy, clients, &cfg);
                            break;
                        }
                    } else if (sym == XK_z || sym == XK_Z) {
                        if (focused_win) {
                            minimize_client(dpy, focused_win, clients, &cfg);
                            break;
                        }
                    }
                }

                const ShortcutBinding *b = shortcuts_match_event(&sc, ev.xkey.state, sym);
                Client *focused = NULL;
                for (Client *c = clients; c; c = c->next) {
                    if (c->is_focused && !c->is_minimized && c->workspace == cfg.current_workspace) { focused = c; break; }
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
                            if (focused && focused->can_fullscreen && !focused->is_fullscreen) {
                                LayoutConfig mon_cfg;
                                monitor_layout_for_client(focused, &cfg, &mon_cfg);
                                client_toggle_maximize(dpy, focused, &mon_cfg);
                                ewmh_sync_client_state(dpy, focused->win, focused->is_fullscreen,
                                                       focused->is_maximized, focused->is_minimized);
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
                                minimize_client(dpy, focused, clients, &cfg);
                            }
                            break;
                        case SHORTCUT_ACT_WS_SWITCH:
                            switch_to_workspace(dpy, clients, &cfg, b->target_ws);
                            break;
                        case SHORTCUT_ACT_WS_MOVE:
                            if (focused) {
                                move_client_to_workspace(dpy, focused, clients, &cfg, b->target_ws);
                            }
                            break;
                        case SHORTCUT_ACT_WS_NEXT:
                            switch_to_workspace(dpy, clients, &cfg,
                                                (cfg.current_workspace + 1) % num_workspaces);
                            break;
                        case SHORTCUT_ACT_WS_PREV:
                            switch_to_workspace(dpy, clients, &cfg,
                                                (cfg.current_workspace + num_workspaces - 1) % num_workspaces);
                            break;
                        case SHORTCUT_ACT_LAUNCHER:
                            spawn_launcher();
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
                    refresh_struts_and_workarea(dpy, strut_list, &cfg);
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
                if (ewmh_allows_client_configure(c)) {
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
                } else if (c && (c->is_fullscreen || c->is_maximized)) {
                    int ch = (c->frame != None && !c->is_fullscreen) ? (c->height - TITLEBAR_HEIGHT) : c->height;
                    if (ch < 10) ch = 10;
                    XEvent ce;
                    memset(&ce, 0, sizeof(ce));
                    ce.type = ConfigureNotify;
                    ce.xconfigure.display = dpy;
                    ce.xconfigure.event = c->win;
                    ce.xconfigure.window = c->win;
                    ce.xconfigure.x = (c->frame != None) ? 0 : c->x;
                    ce.xconfigure.y = (c->frame != None && !c->is_fullscreen) ? TITLEBAR_HEIGHT : (c->frame != None ? 0 : c->y);
                    ce.xconfigure.width = c->width;
                    ce.xconfigure.height = ch;
                    ce.xconfigure.border_width = 0;
                    ce.xconfigure.above = None;
                    ce.xconfigure.override_redirect = False;
                    XSendEvent(dpy, c->win, False, StructureNotifyMask, &ce);
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
                    if (ev.type == UnmapNotify && (c->is_minimized || c->ws_hidden || c->workspace != cfg.current_workspace)) {
                        break;
                    }
                    client_teardown(dpy, &clients, w, &cfg, &drag, &snap_preview);
                    if (alttab.is_visible) {
                        alttab_populate_workspace(dpy, &alttab, clients, cfg.current_workspace);
                        if (alttab.app_count == 0) {
                            XUngrabKeyboard(dpy, CurrentTime);
                            alttab_hide(dpy, &alttab);
                        } else {
                            if (alttab.selected_idx >= alttab.app_count) {
                                alttab.selected_idx = alttab.app_count - 1;
                            }
                            alttab_render(dpy, &alttab);
                        }
                    }
                } else {
                    // Check if strut window unmapped/crashed - Review Focus #1
                    strut_list_remove(&strut_list, w);
                    refresh_struts_and_workarea(dpy, strut_list, &cfg);
                    reflow_all(dpy, clients, &cfg);
                }
                break;
            }

            case PropertyNotify: {
                Atom atom = ev.xproperty.atom;
                Window w = ev.xproperty.window;
                if (w == root && atom == muzu_reload_settings) {
                    cfg.master_count = config_get_master_count();
                    int new_ws_count = config_get_workspace_count();
                    if (new_ws_count != num_workspaces) {
                        num_workspaces = new_ws_count;
                        for (Client *curr = clients; curr; curr = curr->next) {
                            if (curr->workspace >= num_workspaces) {
                                curr->workspace = num_workspaces - 1;
                                workspace_set_client_desktop(dpy, curr->win, curr->workspace);
                                if (curr->workspace == cfg.current_workspace && curr->ws_hidden) {
                                    curr->ws_hidden = 0;
                                    if (!curr->is_minimized) {
                                        if (curr->frame != None) {
                                            XMapWindow(dpy, curr->frame);
                                            if (curr->win != None) XMapWindow(dpy, curr->win);
                                        } else if (curr->win != None) {
                                            XMapWindow(dpy, curr->win);
                                        }
                                    }
                                }
                            }
                        }
                        if (cfg.current_workspace >= num_workspaces) {
                            switch_to_workspace(dpy, clients, &cfg, num_workspaces - 1);
                        } else {
                            workspace_publish_ewmh(dpy, root, num_workspaces, cfg.current_workspace);
                        }
                        refresh_struts_and_workarea(dpy, strut_list, &cfg);
                    }
                    int new_alttab = config_is_alttab_enabled();
                    if (new_alttab != alttab_enabled) {
                        alttab_enabled = new_alttab;
                        grab_alttab_keys(dpy, root, alttab_enabled);
                        if (!alttab_enabled && alttab.is_visible) {
                            XUngrabKeyboard(dpy, CurrentTime);
                            alttab_hide(dpy, &alttab);
                        }
                    }
                    load_appearance_settings(dpy, &cfg);
                    for (Client *curr = clients; curr; curr = curr->next) {
                        apply_client_border(dpy, curr, cfg.border_width);
                    }
                    reflow_all(dpy, clients, &cfg);
                } else if (atom == net_wm_strut || atom == net_wm_strut_partial) {
                    StrutPartial sp;
                    if (strut_read_window_property(dpy, w, &sp)) {
                        strut_list_add_or_update(&strut_list, w, &sp);
                    } else {
                        strut_list_remove(&strut_list, w);
                    }
                    refresh_struts_and_workarea(dpy, strut_list, &cfg);
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
                if (alttab.is_visible) {
                    XUngrabKeyboard(dpy, CurrentTime);
                    alttab_hide(dpy, &alttab);
                }

                if (desktop_menu.is_visible) {
                    if (ev.xbutton.window == desktop_menu.win) {
                        menu_handle_click(dpy, &desktop_menu, ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
                        dismiss_desktop_menu(dpy, &desktop_menu, clients, cfg.current_workspace);
                        break;
                    } else {
                        dismiss_desktop_menu(dpy, &desktop_menu, clients, cfg.current_workspace);
                        if (ev.xbutton.button == Button3 && desktop_menu_enabled) {
                            int m = monitor_find_for_point(monitors, monitor_count, ev.xbutton.x_root, ev.xbutton.y_root);
                            int mx_end = (monitor_count > 0) ? (monitors[m].x + monitors[m].width) : total_screen_w;
                            int my_end = (monitor_count > 0) ? (monitors[m].y + monitors[m].height) : total_screen_h;
                            menu_show(dpy, &desktop_menu, ev.xbutton.x_root, ev.xbutton.y_root, mx_end, my_end);
                            if (XGrabPointer(dpy, root, True, ButtonPressMask | PointerMotionMask,
                                             GrabModeAsync, GrabModeAsync, None, None, CurrentTime) != GrabSuccess) {
                                dismiss_desktop_menu(dpy, &desktop_menu, clients, cfg.current_workspace);
                            }
                        }
                        break;
                    }
                }

                if (ev.xbutton.window == root && ev.xbutton.button == Button3) {
                    if (!desktop_menu_enabled) {
                        break;
                    }
                    int m = monitor_find_for_point(monitors, monitor_count, ev.xbutton.x_root, ev.xbutton.y_root);
                    int mx_end = (monitor_count > 0) ? (monitors[m].x + monitors[m].width) : total_screen_w;
                    int my_end = (monitor_count > 0) ? (monitors[m].y + monitors[m].height) : total_screen_h;
                    menu_show(dpy, &desktop_menu, ev.xbutton.x_root, ev.xbutton.y_root, mx_end, my_end);
                    if (XGrabPointer(dpy, root, True, ButtonPressMask | PointerMotionMask,
                                     GrabModeAsync, GrabModeAsync, None, None, CurrentTime) != GrabSuccess) {
                        menu_hide(dpy, &desktop_menu);
                    }
                    break;
                }

                Client *c = client_find_by_any(clients, ev.xbutton.window);
                if (c) {
                    set_focused_client(dpy, c, clients);
                    raise_client_with_transients(dpy, c, clients);

                    if (c->frame != None && ev.xbutton.window == c->frame) {
                        TitlebarAction act = titlebar_get_action_for_client(ev.xbutton.x, c->width, c);
                        if (act == TITLEBAR_CLICK_CLOSE) {
                            client_close(dpy, c->win);
                        } else if (act == TITLEBAR_CLICK_MAXIMIZE) {
                            if (!c->is_fullscreen) {
                                LayoutConfig mon_cfg;
                                monitor_layout_for_client(c, &cfg, &mon_cfg);
                                client_toggle_maximize(dpy, c, &mon_cfg);
                                ewmh_sync_client_state(dpy, c->win, c->is_fullscreen,
                                                       c->is_maximized, c->is_minimized);
                                titlebar_draw(dpy, c, NULL);
                                reflow_all(dpy, clients, &cfg);
                            }
                        } else if (act == TITLEBAR_CLICK_MINIMIZE) {
                            minimize_client(dpy, c, clients, &cfg);
                        } else if (ev.xbutton.button == Button1 && ewmh_can_drag_client(c)) {
                            int gs = XGrabPointer(dpy, root, False,
                                                  ButtonReleaseMask | PointerMotionMask,
                                                  GrabModeAsync, GrabModeAsync,
                                                  None, None, CurrentTime);
                            wm_drag_commit_if_grabbed(&drag, c, 0, ev.xbutton.x_root, ev.xbutton.y_root, gs);
                        }
                    } else if (ev.xbutton.window == c->win) {
                        if ((ev.xbutton.state & Mod4Mask) && ewmh_can_drag_client(c)) {
                            if (ev.xbutton.button == Button1) {
                                int gs = XGrabPointer(dpy, root, False,
                                                      ButtonReleaseMask | PointerMotionMask,
                                                      GrabModeAsync, GrabModeAsync,
                                                      None, None, CurrentTime);
                                wm_drag_commit_if_grabbed(&drag, c, 0, ev.xbutton.x_root, ev.xbutton.y_root, gs);
                            } else if (ev.xbutton.button == Button3 && c->can_fullscreen) {
                                int gs = XGrabPointer(dpy, root, False,
                                                      ButtonReleaseMask | PointerMotionMask,
                                                      GrabModeAsync, GrabModeAsync,
                                                      None, None, CurrentTime);
                                wm_drag_commit_if_grabbed(&drag, c, 1, ev.xbutton.x_root, ev.xbutton.y_root, gs);
                            }
                        } else {
                            XAllowEvents(dpy, ReplayPointer, ev.xbutton.time);
                        }
                    }
                }
                break;
            }

            case EnterNotify: {
                Client *c = client_find_by_any(clients, ev.xcrossing.window);
                if (wm_should_focus_on_enter(focus_follows_mouse_enabled,
                                             ev.xcrossing.mode,
                                             ev.xcrossing.detail,
                                             drag.client != NULL,
                                             desktop_menu.is_visible,
                                             alttab.is_visible,
                                             c,
                                             cfg.current_workspace)) {
                    set_focused_client(dpy, c, clients);
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

                if (drag.client) {
                    int dx = ev.xmotion.x_root - drag.start_x;
                    int dy = ev.xmotion.y_root - drag.start_y;
                    if (drag.is_resize) {
                        if (wm_drag_prepare_resize(drag.client)) {
                            ewmh_sync_client_state(dpy, drag.client->win, drag.client->is_fullscreen,
                                                   drag.client->is_maximized, drag.client->is_minimized);
                            if (drag.client->frame != None) titlebar_draw(dpy, drag.client, NULL);
                            reflow_all(dpy, clients, &cfg);
                        }
                        int nw = drag.orig_w + dx;
                        int nh = drag.orig_h + dy;
                        if (nw < 100) nw = 100;
                        if (nh < 60) nh = 60;
                        drag.client->width = nw;
                        drag.client->height = nh;
                        if (drag.client->frame != None) {
                            int ch = nh - TITLEBAR_HEIGHT;
                            if (ch < 10) ch = 10;
                            XResizeWindow(dpy, drag.client->frame, drag.client->width, drag.client->height);
                            XResizeWindow(dpy, drag.client->win, drag.client->width, ch);
                            titlebar_draw(dpy, drag.client, NULL);
                        } else {
                            XResizeWindow(dpy, drag.client->win, drag.client->width, drag.client->height);
                        }
                    } else if (abs(dx) > 4 || abs(dy) > 4) {
                        if (ewmh_begin_move_clears_maximized(drag.client)) {
                            ewmh_sync_client_state(dpy, drag.client->win, drag.client->is_fullscreen,
                                                   drag.client->is_maximized, drag.client->is_minimized);
                            if (drag.client->frame != None) titlebar_draw(dpy, drag.client, NULL);
                        }
                        if (drag.client->mode == CLIENT_TILED) {
                            drag.client->mode = CLIENT_FLOATING;
                            drag.client->snap_state = SNAP_NONE;
                            reflow_all(dpy, clients, &cfg);
                        }
                        drag.client->x = drag.orig_x + dx;
                        drag.client->y = drag.orig_y + dy;
                        Window target = drag.client->frame != None ? drag.client->frame : drag.client->win;
                        XMoveWindow(dpy, target, drag.client->x, drag.client->y);

                        // Review Focus #2: Suppress snap preview on fixed-size windows
                        if (drag.client->can_fullscreen) {
                            int m = monitor_find_for_point(monitors, monitor_count, ev.xmotion.x_root, ev.xmotion.y_root);
                            Monitor mon = (monitor_count > 0) ? monitors[m]
                                : (Monitor){ 0, 0, total_screen_w, total_screen_h, cfg.left_inset, cfg.right_inset, cfg.top_inset, cfg.bottom_inset, 1 };
                            SnapState snap = snap_preview_detect_zone_in_monitor(
                                ev.xmotion.x_root, ev.xmotion.y_root,
                                mon.x, mon.y, mon.width, mon.height,
                                mon.top_inset, SNAP_THRESHOLD);
                            snap_preview_update_in_monitor(
                                dpy, &snap_preview, snap,
                                mon.x, mon.y, mon.width, mon.height,
                                mon.left_inset, mon.right_inset, mon.top_inset, mon.bottom_inset);
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

                if (drag.client) {
                    XUngrabPointer(dpy, CurrentTime);
                    if (drag.is_resize) {
                        drag.is_resize = 0;
                    } else {
                        SnapState snap = snap_preview.current_snap;
                        snap_preview_hide(dpy, &snap_preview);

                        if (drag.client->can_fullscreen) {
                            int m = monitor_find_for_point(monitors, monitor_count, ev.xbutton.x_root, ev.xbutton.y_root);
                            LayoutConfig mon_cfg = cfg;
                            if (monitor_count > 0) {
                                monitor_to_layout_config(&monitors[m], cfg.master_count, cfg.current_workspace, &mon_cfg);
                                mon_cfg.border_width = cfg.border_width;
                            }
                            if (snap == SNAP_TOP) {
                                client_toggle_maximize(dpy, drag.client, &mon_cfg);
                                ewmh_sync_client_state(dpy, drag.client->win, drag.client->is_fullscreen,
                                                       drag.client->is_maximized, drag.client->is_minimized);
                                if (drag.client->frame != None) titlebar_draw(dpy, drag.client, NULL);
                                reflow_all(dpy, clients, &cfg);
                            } else if (snap == SNAP_LEFT || snap == SNAP_RIGHT) {
                                if (monitor_count > 0) {
                                    drag.client->x = monitors[m].x + monitors[m].left_inset;
                                    drag.client->y = monitors[m].y + monitors[m].top_inset;
                                    drag.client->width = 100;
                                    drag.client->height = 100;
                                }
                                drag.client->mode = CLIENT_TILED;
                                drag.client->snap_state = snap;
                                reflow_all(dpy, clients, &cfg);
                            }
                        }
                    }
                    drag.client = NULL;
                    drag.is_resize = 0;
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

            case ClientMessage: {
                XClientMessageEvent *cm = &ev.xclient;
                if (cm->message_type == net_active) {
                    Client *c = client_find_by_any(clients, cm->window);
                    if (c) {
                        activate_client(dpy, c, clients, &cfg);
                    }
                } else if (cm->message_type == net_wm_state) {
                    Client *c = client_find_by_any(clients, cm->window);
                    if (c) {
                        EwmhStateFlags flags = ewmh_parse_state_atoms(
                            (Atom)cm->data.l[1], (Atom)cm->data.l[2],
                            net_wm_state_fullscreen, net_wm_state_max_horz,
                            net_wm_state_max_vert, net_wm_state_hidden);
                        long action = cm->data.l[0];
                        if (flags.has_fullscreen) {
                            int want = ewmh_resolve_state_action(action, c->is_fullscreen);
                            if (want != c->is_fullscreen) {
                                if (want && wm_drag_should_cancel(&drag, c)) {
                                    cancel_active_drag(dpy);
                                }
                                LayoutConfig mon_cfg;
                                monitor_layout_for_client(c, &cfg, &mon_cfg);
                                ewmh_apply_fullscreen(dpy, c, &mon_cfg, want);
                                if (!want) {
                                    apply_client_border(dpy, c, cfg.border_width);
                                    reflow_all(dpy, clients, &cfg);
                                }
                            }
                        }
                        if ((flags.has_max_horz || flags.has_max_vert) &&
                            c->can_fullscreen && !c->is_fullscreen) {
                            int want = ewmh_resolve_state_action(action, c->is_maximized);
                            if (want != c->is_maximized) {
                                LayoutConfig mon_cfg;
                                monitor_layout_for_client(c, &cfg, &mon_cfg);
                                client_toggle_maximize(dpy, c, &mon_cfg);
                                ewmh_sync_client_state(dpy, c->win, c->is_fullscreen,
                                                       c->is_maximized, c->is_minimized);
                                if (c->frame != None) titlebar_draw(dpy, c, NULL);
                                reflow_all(dpy, clients, &cfg);
                            }
                        }
                        if (flags.has_hidden) {
                            int want = ewmh_resolve_state_action(action, c->is_minimized);
                            if (want != c->is_minimized) {
                                if (want) {
                                    minimize_client(dpy, c, clients, &cfg);
                                } else {
                                    activate_client(dpy, c, clients, &cfg);
                                }
                            }
                        }
                    }
                } else if (cm->message_type == net_close_window) {
                    Client *c = client_find_by_any(clients, cm->window);
                    if (c) {
                        client_close(dpy, c->win);
                    }
                } else if (cm->message_type == net_cur_desktop) {
                    int target_ws = ewmh_parse_desktop_index(cm->data.l[0], num_workspaces);
                    if (target_ws >= 0) {
                        switch_to_workspace(dpy, clients, &cfg, target_ws);
                    }
                } else if (cm->message_type == net_wm_desktop) {
                    int target_ws = ewmh_parse_desktop_index(cm->data.l[0], num_workspaces);
                    if (target_ws >= 0) {
                        Client *c = client_find_by_any(clients, cm->window);
                        if (c) {
                            move_client_to_workspace(dpy, c, clients, &cfg, target_ws);
                        }
                    }
                }
                break;
            }

            default:
                if (has_xrandr && ev.type == rr_event_base + RRScreenChangeNotify) {
                    XRRUpdateConfiguration(&ev);
                    total_screen_w = DisplayWidth(dpy, screen);
                    total_screen_h = DisplayHeight(dpy, screen);
                    monitor_count = monitors_query_xrandr(dpy, root, monitors, MAX_MONITORS);
                    refresh_struts_and_workarea(dpy, strut_list, &cfg);
                    reflow_all(dpy, clients, &cfg);
                }
                break;
        }
        }
        if (!running) break;
        XFlush(dpy);
        int wr = wm_wait_x11_or_signal(x11_fd, sig_pipe[0]);
        if (wr < 0) break;
        if (wr > 0 && sig_pipe[0] >= 0) {
            char drain[32];
            while (read(sig_pipe[0], drain, sizeof(drain)) > 0) {}
        }
    }

    // Teardown
    if (drag.client || desktop_menu.is_visible) {
        XUngrabPointer(dpy, CurrentTime);
    }
    if (alttab.is_visible) {
        XUngrabKeyboard(dpy, CurrentTime);
    }
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
    while (strut_list) {
        strut_list_remove(&strut_list, strut_list->win);
    }
    if (wm_check_win != None) {
        XDestroyWindow(dpy, wm_check_win);
        wm_check_win = None;
    }
    XSync(dpy, True);
    XCloseDisplay(dpy);
    cairo_debug_reset_static_data();
    if (sig_pipe[0] >= 0) close(sig_pipe[0]);
    if (sig_pipe[1] >= 0) close(sig_pipe[1]);
    return 0;
}
