#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <locale.h>
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
#include "compositor.h"
#include "tint2_panel.h"
#include "config_rules.h"
#include "alttab.h"
#include "mission_viewer.h"
#include "kbd_layout.h"
#include "confirm_dialog.h"

static int running = 1;
static GtkTheme current_theme;

typedef struct {
    Display *dpy;
    Window root;
    Dock *dock;
    RofiLauncher *rofi;
    Client **clients_ptr;
    LayoutConfig *cfg_ptr;
    int screen_w, screen_h;
    int right_clicked_dock_item;
    ConfirmDialog *confirm;
    KbdLayouts *kbd;
    Tint2Panel *panel;
} WmContext;

static void reflow_all(Display *dpy, Client *clients, LayoutConfig *cfg);
static void raise_system_overlays(Display *dpy);

static pid_t client_get_pid(Display *dpy, Window win) {
    if (!dpy || win == None) return 0;
    Atom net_wm_pid = XInternAtom(dpy, "_NET_WM_PID", True);
    if (net_wm_pid == None) return 0;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    pid_t pid = 0;
    if (XGetWindowProperty(dpy, win, net_wm_pid, 0, 1, False, XA_CARDINAL,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems > 0) {
            pid = (pid_t)(*(unsigned long *)prop);
        }
        if (prop) XFree(prop);
    }
    return pid;
}

static int client_matches_dock_item(Display *dpy, const Client *c, const DockItem *item) {
    if (!c || !item) return 0;
    if (c->title[0] != '\0') {
        if (strcasestr(c->title, item->name) != NULL) return 1;
        if (item->exec_cmd[0] != '\0' && strcasestr(c->title, item->exec_cmd) != NULL) return 1;
    }
    if (dpy && c->win != None) {
        XClassHint ch;
        if (XGetClassHint(dpy, c->win, &ch)) {
            int match = 0;
            if (ch.res_name) {
                if (strcasestr(ch.res_name, item->name) ||
                    (item->exec_cmd[0] && strcasestr(ch.res_name, item->exec_cmd))) {
                    match = 1;
                }
            }
            if (ch.res_class) {
                if (strcasestr(ch.res_class, item->name) ||
                    (item->exec_cmd[0] && strcasestr(ch.res_class, item->exec_cmd))) {
                    match = 1;
                }
            }
            if (ch.res_name) XFree(ch.res_name);
            if (ch.res_class) XFree(ch.res_class);
            if (match) return 1;
        }
    }
    return 0;
}

static void execute_power_action(int action_id) {
    const char *cmd = NULL;
    switch (action_id) {
        case POWER_ACTION_SUSPEND:
            cmd = "systemctl suspend || loginctl suspend || pm-suspend";
            break;
        case POWER_ACTION_REBOOT:
            cmd = "systemctl reboot || loginctl reboot || reboot";
            break;
        case POWER_ACTION_POWEROFF:
            cmd = "systemctl poweroff || loginctl poweroff || poweroff";
            break;
        case POWER_ACTION_LOGOUT:
            running = 0;
            return;
        default:
            return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(0);
    }
}

static void handle_menu_action(int action_id, void *userdata) {
    WmContext *ctx = (WmContext *)userdata;
    if (!ctx) return;
    Display *dpy = ctx->dpy;
    Dock *dock = ctx->dock;
    int screen_w = ctx->screen_w;
    int screen_h = ctx->screen_h;

    // Suspend / Reboot / Power Off / Log Out must be confirmed first
    if (power_action_needs_confirm(action_id)) {
        if (ctx->confirm && dpy) {
            confirm_dialog_show(dpy, ctx->confirm, action_id, screen_w, screen_h);
            XGrabKeyboard(dpy, ctx->confirm->win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
            XRaiseWindow(dpy, ctx->confirm->win);
            XFlush(dpy);
        }
        return;
    }

    // Keyboard layout picked from the panel language menu
    if (action_id >= KBD_MENU_ACTION_BASE && action_id < KBD_MENU_ACTION_BASE + KBD_MAX_GROUPS) {
        if (ctx->kbd) {
            kbd_layout_set_group(dpy, ctx->kbd, action_id - KBD_MENU_ACTION_BASE);
            if (ctx->panel) {
                tint2_panel_set_lang(ctx->panel, kbd_layout_current_short(ctx->kbd));
                tint2_panel_render(dpy, ctx->panel);
            }
        }
        return;
    }

    switch (action_id) {
        case DOCK_ACTION_LAUNCH_APP:
            if (ctx->right_clicked_dock_item >= 0 && ctx->right_clicked_dock_item < dock->item_count) {
                const char *cmd = dock->items[ctx->right_clicked_dock_item].exec_cmd;
                pid_t pid = fork();
                if (pid == 0) {
                    setsid();
                    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
                    _exit(1);
                }
            }
            break;

        case DOCK_ACTION_CLOSE_APP:
            if (ctx->right_clicked_dock_item >= 0 && ctx->right_clicked_dock_item < dock->item_count && ctx->clients_ptr) {
                const DockItem *item = &dock->items[ctx->right_clicked_dock_item];
                Client *target_c = NULL;
                for (Client *c = *ctx->clients_ptr; c; c = c->next) {
                    if (client_matches_dock_item(dpy, c, item)) {
                        if (c->is_focused) {
                            target_c = c;
                            break;
                        }
                        if (!target_c) target_c = c;
                    }
                }
                if (target_c) {
                    client_close(dpy, target_c->win);
                }
            }
            break;

        case DOCK_ACTION_KILL_APP:
            if (ctx->right_clicked_dock_item >= 0 && ctx->right_clicked_dock_item < dock->item_count && ctx->clients_ptr) {
                const DockItem *item = &dock->items[ctx->right_clicked_dock_item];
                for (Client *c = *ctx->clients_ptr; c; c = c->next) {
                    if (client_matches_dock_item(dpy, c, item)) {
                        pid_t pid = client_get_pid(dpy, c->win);
                        if (pid > 1) {
                            kill(pid, SIGKILL);
                        }
                        XKillClient(dpy, c->win);
                    }
                }
            }
            break;

        case DOCK_ACTION_CLOSE_ALL_WINDOWS:
            if (ctx->right_clicked_dock_item >= 0 && ctx->right_clicked_dock_item < dock->item_count && ctx->clients_ptr) {
                const DockItem *item = &dock->items[ctx->right_clicked_dock_item];
                for (Client *c = *ctx->clients_ptr; c; c = c->next) {
                    if (client_matches_dock_item(dpy, c, item)) {
                        client_close(dpy, c->win);
                    }
                }
            }
            break;

        case DOCK_ACTION_DETACH_ALL_WINDOWS:
            if (ctx->right_clicked_dock_item >= 0 && ctx->right_clicked_dock_item < dock->item_count && ctx->clients_ptr) {
                const DockItem *item = &dock->items[ctx->right_clicked_dock_item];
                int detached_any = 0;
                int stagger = 0;
                for (Client *c = *ctx->clients_ptr; c; c = c->next) {
                    if (client_matches_dock_item(dpy, c, item)) {
                        if (c->mode == CLIENT_TILED) {
                            c->mode = CLIENT_FLOATING;
                            c->snap_state = SNAP_NONE;
                            c->is_maximized = 0;
                            c->width = (screen_w * 3) / 5;
                            c->height = (screen_h * 3) / 5;
                            c->x = (screen_w - c->width) / 2 + stagger;
                            c->y = (screen_h - c->height) / 2 + stagger;
                            stagger += 30;
                            Window target = c->frame ? c->frame : c->win;
                            XMoveResizeWindow(dpy, target, c->x, c->y, c->width, c->height);
                            if (c->frame) {
                                int ch = c->height - TITLEBAR_HEIGHT;
                                if (ch < 10) ch = 10;
                                XResizeWindow(dpy, c->win, c->width, ch);
                                titlebar_draw(dpy, c, &current_theme);
                            }
                            XRaiseWindow(dpy, target);
                            detached_any = 1;
                        }
                    }
                }
                if (detached_any && ctx->cfg_ptr) {
                    reflow_all(dpy, *ctx->clients_ptr, ctx->cfg_ptr);
                    raise_system_overlays(dpy);
                }
            }
            break;

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
            dock_set_position(dpy, dock, DOCK_BOTTOM, screen_w, screen_h);
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

        case DOCK_ACTION_REMOVE_APP:
            if (ctx->right_clicked_dock_item >= 0 && ctx->right_clicked_dock_item < dock->item_count) {
                dock_remove_app(dock, ctx->right_clicked_dock_item);
                dock_calculate_geometry(dock, dock->pos, dock->is_hidden, screen_w, screen_h);
                if (dock->win) {
                    XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
                    cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
                }
                dock_render(dpy, dock, screen_w, screen_h);
            }
            break;

        case DOCK_ACTION_TOGGLE_APPS_CORNER:
            dock_set_apps_corner(dock, !dock->apps_at_left);
            dock_render(dpy, dock, screen_w, screen_h);
            break;

        case DOCK_ACTION_EMPTY_TRASH:
            dock_empty_trash();
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

        case DESKTOP_ACTION_SETTINGS: {
            pid_t pid = fork();
            if (pid == 0) {
                setsid();
                execlp("muzuwm-settings", "muzuwm-settings", (char *)NULL);
                /* Allow Settings to work from an uninstalled source checkout too. */
                execl("./tools/muzuwm-settings", "muzuwm-settings", (char *)NULL);
                _exit(127);
            }
            break;
        }

        case POWER_ACTION_LOCK: {
            pid_t pid = fork();
            if (pid == 0) {
                setsid();
                execl("/bin/sh", "sh", "-c", "loginctl lock-session || xdg-screensaver lock || i3lock || slock", NULL);
                _exit(0);
            }
            break;
        }

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
static Tint2Panel *g_panel = NULL;
static AltTab *g_alttab = NULL;
static MissionViewer *g_mission_viewer = NULL;
static ConfirmDialog *g_confirm = NULL;

static void raise_system_overlays(Display *dpy) {
    if (!dpy) return;
    if (g_panel && g_panel->win) {
        XRaiseWindow(dpy, g_panel->win);
    }
    if (g_dock && g_dock->win) {
        XRaiseWindow(dpy, g_dock->win);
    }
    if (g_rofi && g_rofi->win && g_rofi->is_visible) {
        XRaiseWindow(dpy, g_rofi->win);
    }
    if (g_menu && g_menu->win && g_menu->is_visible) {
        XRaiseWindow(dpy, g_menu->win);
    }
    if (g_mission_viewer && g_mission_viewer->win && g_mission_viewer->is_visible) {
        XRaiseWindow(dpy, g_mission_viewer->win);
    }
    if (g_alttab && g_alttab->win && g_alttab->is_visible) {
        XRaiseWindow(dpy, g_alttab->win);
    }
    /* A pending destructive action must remain above every other overlay. */
    if (g_confirm && g_confirm->win && g_confirm->is_visible) {
        XRaiseWindow(dpy, g_confirm->win);
    }
}

static void reflow_all(Display *dpy, Client *clients, LayoutConfig *cfg) {
    layout_calculate(cfg, clients, 0);
    for (Client *curr = clients; curr; curr = curr->next) {
        if (curr->is_minimized || curr->workspace != cfg->current_workspace) continue;
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

    if (dpy) {
        Atom net_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
        Window active_w = (c && c->win != None) ? c->win : None;
        XChangeProperty(dpy, DefaultRootWindow(dpy), net_active, XA_WINDOW, 32,
                        PropModeReplace, (unsigned char *)&active_w, 1);
    }

    if (g_panel) {
        if (c && c->title[0] != '\0') {
            snprintf(g_panel->active_title, sizeof(g_panel->active_title), "%.127s", c->title);
        } else {
            g_panel->active_title[0] = '\0';
        }
        tint2_panel_render(dpy, g_panel);
        XFlush(dpy);
    }
}

static void update_panel_state(Display *dpy, Client *clients, LayoutConfig *cfg) {
    if (!g_panel) return;
    tint2_panel_update_workspaces(g_panel, clients, cfg->current_workspace,
                                  focused_client ? focused_client->title : "");
    tint2_panel_render(dpy, g_panel);
    XFlush(dpy);
}

// Tile a window to the left/right half. Windows that cannot go fullscreen ignore this.
static void client_snap(Display *dpy, Client *clients, Client *c, LayoutConfig *cfg, SnapState snap) {
    if (!c || !c->can_fullscreen) return;
    if (c->mode == CLIENT_FLOATING && !c->is_maximized) {
        c->saved_x = c->x;
        c->saved_y = c->y;
        c->saved_width = c->width;
        c->saved_height = c->height;
        c->saved_mode = CLIENT_FLOATING;
    }
    c->is_maximized = 0;
    c->mode = CLIENT_TILED;
    c->snap_state = snap;
    reflow_all(dpy, clients, cfg);
}

// Return a window to floating, restoring its previous floating geometry (or a centered default).
static void client_float(Display *dpy, Client *clients, Client *c, LayoutConfig *cfg) {
    if (!c) return;
    if (c->is_maximized) {
        client_toggle_maximize(dpy, c, cfg);
    }
    int max_h = cfg->screen_height - cfg->top_inset - cfg->bottom_inset;
    int have_saved = (c->saved_width > 0 && c->saved_width < cfg->screen_width &&
                      c->saved_height > 0 && c->saved_height < max_h);
    if (c->mode == CLIENT_TILED || !have_saved) {
        if (have_saved && c->saved_mode == CLIENT_FLOATING) {
            c->x = c->saved_x;
            c->y = c->saved_y;
            c->width = c->saved_width;
            c->height = c->saved_height;
        } else if (c->mode == CLIENT_TILED || c->width >= cfg->screen_width) {
            c->width = cfg->screen_width * 3 / 5;
            c->height = max_h * 3 / 5;
            c->x = (cfg->screen_width - c->width) / 2;
            c->y = cfg->top_inset + (max_h - c->height) / 2;
        }
    }
    c->mode = CLIENT_FLOATING;
    c->snap_state = SNAP_NONE;
    c->is_maximized = 0;
    Window target = c->frame ? c->frame : c->win;
    XMoveResizeWindow(dpy, target, c->x, c->y, c->width, c->height);
    if (c->frame) {
        int ch = c->height - TITLEBAR_HEIGHT;
        if (ch < 10) ch = 10;
        XResizeWindow(dpy, c->win, c->width, ch);
        titlebar_draw(dpy, c, &current_theme);
    }
    XRaiseWindow(dpy, target);
    reflow_all(dpy, clients, cfg);
}

static void switch_workspace(Display *dpy, Client *clients, LayoutConfig *cfg, Dock *dock, int target_ws) {
    if (target_ws < 0 || target_ws >= TINT2_NUM_WORKSPACES) return;
    if (target_ws == cfg->current_workspace) return;

    cfg->current_workspace = target_ws;

    for (Client *curr = clients; curr; curr = curr->next) {
        Window target = curr->frame ? curr->frame : curr->win;
        if (curr->workspace == target_ws) {
            if (!curr->is_minimized) {
                XMapWindow(dpy, target);
                if (curr->frame && curr->win) {
                    XMapWindow(dpy, curr->win);
                }
            }
        } else {
            XUnmapWindow(dpy, target);
        }
    }

    if (focused_client && (focused_client->workspace != target_ws || focused_client->is_minimized)) {
        focused_client = NULL;
    }
    if (!focused_client) {
        for (Client *curr = clients; curr; curr = curr->next) {
            if (curr->workspace == target_ws && !curr->is_minimized) {
                set_focused_client(dpy, curr);
                break;
            }
        }
    }

    reflow_all(dpy, clients, cfg);
    if (dock) {
        dock_update_running(dock, clients);
        dock_render(dpy, dock, cfg->screen_width, cfg->screen_height);
    }
    update_panel_state(dpy, clients, cfg);
    raise_system_overlays(dpy);
    XFlush(dpy);
}

static void move_client_to_workspace(Display *dpy, Client *clients, Client *c, LayoutConfig *cfg, Dock *dock, int target_ws) {
    if (!c || target_ws < 0 || target_ws >= TINT2_NUM_WORKSPACES) return;
    if (c->workspace == target_ws) return;

    c->workspace = target_ws;
    if (target_ws != cfg->current_workspace) {
        Window target = c->frame ? c->frame : c->win;
        XUnmapWindow(dpy, target);
        if (focused_client == c) {
            focused_client = NULL;
            for (Client *curr = clients; curr; curr = curr->next) {
                if (curr->workspace == cfg->current_workspace && !curr->is_minimized) {
                    set_focused_client(dpy, curr);
                    break;
                }
            }
        }
    }

    reflow_all(dpy, clients, cfg);
    if (dock) {
        dock_update_running(dock, clients);
        dock_render(dpy, dock, cfg->screen_width, cfg->screen_height);
    }
    update_panel_state(dpy, clients, cfg);
    raise_system_overlays(dpy);
    XFlush(dpy);
}

static void client_activate_and_unminimize(Display *dpy, Client *c, Client *clients, LayoutConfig *cfg, Dock *dock, int screen_w, int screen_h) {
    if (!c || !dpy || !cfg) return;

    if (c->is_minimized) {
        c->is_minimized = 0;
        c->workspace = cfg->current_workspace;
        Window target = c->frame ? c->frame : c->win;
        XMapWindow(dpy, target);
        if (c->frame && c->win) {
            XMapWindow(dpy, c->win);
        }
        XRaiseWindow(dpy, target);
        if (dock) {
            dock_remove_minimized(dock, c);
            dock_update_running(dock, clients);
            dock_render(dpy, dock, screen_w, screen_h);
        }
    } else {
        if (c->workspace != cfg->current_workspace) {
            switch_workspace(dpy, clients, cfg, dock, c->workspace);
        }
        Window target = c->frame ? c->frame : c->win;
        XRaiseWindow(dpy, target);
    }

    set_focused_client(dpy, c);
    reflow_all(dpy, clients, cfg);
    update_panel_state(dpy, clients, cfg);
    raise_system_overlays(dpy);
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
            if (!o->is_minimized && o->workspace == cfg->current_workspace) {
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
    update_panel_state(dpy, *clients, cfg);
    XFlush(dpy);
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
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

    config_rules_ensure_files();
    LayoutConfig cfg = {
        .screen_width = screen_w,
        .screen_height = screen_h,
        .master_count = config_get_master_count(),
        .top_inset = TINT2_PANEL_HEIGHT,
        .current_workspace = 0
    };

    Client *clients = NULL;
    RofiLauncher rofi_launcher;
    Dock dock = {0};
    Tint2Panel panel = {0};

    // Select root window events early so MapRequest events during init are queued
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask |
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                            KeyPressMask | KeyReleaseMask | PropertyChangeMask);

    // Load GTK theme configuration & export env for GTK applications
    gtk_theme_init(&current_theme);
    gtk_theme_export_env(&current_theme);
    gtk_theme_apply_cursor(dpy, root, &current_theme);

    // Initialize Wallpaper (Zero-Overhead Root Window Pixmap)
    if (cli_wallpaper) {
        wallpaper_set_root(dpy, screen_w, screen_h, cli_wallpaper);
    } else {
        wallpaper_init(dpy, screen_w, screen_h);
    }

    // Initialize UI subsystems
    tint2_panel_init(dpy, &panel, screen_w, &current_theme);
    if (panel.is_top) {
        cfg.top_inset = panel.height;
        cfg.bottom_inset = 0;
    } else {
        cfg.top_inset = 0;
        cfg.bottom_inset = panel.height;
    }
    rofi_launcher_init(dpy, &rofi_launcher, screen_w, screen_h, &current_theme);
    dock_init(dpy, &dock, DOCK_BOTTOM, screen_w, screen_h, &current_theme);

    Menu context_menu = {0};
    WmContext wm_ctx = {
        .dpy = dpy,
        .root = root,
        .dock = &dock,
        .rofi = &rofi_launcher,
        .clients_ptr = &clients,
        .cfg_ptr = &cfg,
        .screen_w = screen_w,
        .screen_h = screen_h,
        .right_clicked_dock_item = -1
    };
    ConfirmDialog confirm_dialog;
    confirm_dialog_init(dpy, &confirm_dialog, &current_theme);
    g_confirm = &confirm_dialog;
    KbdLayouts kbd_layouts;
    char configured_layouts[128];
    if (config_get_setting("keyboard_layouts", configured_layouts, sizeof(configured_layouts))) {
        kbd_layout_apply_layouts(configured_layouts);
    }
    kbd_layout_init(dpy, &kbd_layouts);
    tint2_panel_set_lang(&panel, kbd_layout_current_short(&kbd_layouts));
    tint2_panel_render(dpy, &panel);
    wm_ctx.confirm = &confirm_dialog;
    wm_ctx.kbd = &kbd_layouts;
    wm_ctx.panel = &panel;
    menu_init(dpy, &context_menu, &current_theme, handle_menu_action, &wm_ctx);

    g_panel = &panel;
    g_dock = &dock;
    g_rofi = &rofi_launcher;
    g_menu = &context_menu;

    AltTab alttab = {0};
    alttab_init(dpy, &alttab, &current_theme);
    MissionViewer mission_viewer = {0};
    mission_viewer_init(dpy, &mission_viewer, &current_theme);

    g_alttab = &alttab;
    g_mission_viewer = &mission_viewer;

    // Launch X11 Compositor (Picom with Dual-Kawase Frosted Glass Blur)
    compositor_init(dpy);

    // Load window rules & custom shortcuts, then launch apps listed in auto_allowlist
    ShortcutConfig shortcuts_cfg = {0};
    shortcuts_load(&shortcuts_cfg);
    int super_pending = 0;
    config_run_autostart();

    // Adopt any pre-existing mapped top-level windows on root
    {
        Window root_ret = None, parent_ret = None, *children = NULL;
        unsigned int nchildren = 0;
        if (XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &nchildren) && children) {
            for (int i = (int)nchildren - 1; i >= 0; i--) {
                Window cw = children[i];
                if (cw == rofi_launcher.win || cw == dock.win || cw == context_menu.win ||
                    cw == panel.win || (dock.tooltip_win && cw == dock.tooltip_win) ||
                    (alttab.win && cw == alttab.win) || (mission_viewer.win && cw == mission_viewer.win)) {
                    continue;
                }
                XWindowAttributes wa = {0};
                if (XGetWindowAttributes(dpy, cw, &wa) && !wa.override_redirect && wa.map_state == IsViewable) {
                    XEvent mrev;
                    memset(&mrev, 0, sizeof(mrev));
                    mrev.type = MapRequest;
                    mrev.xmaprequest.display = dpy;
                    mrev.xmaprequest.parent = root;
                    mrev.xmaprequest.window = cw;
                    XPutBackEvent(dpy, &mrev);
                }
            }
            XFree(children);
        }
    }

    // Grab Super key (Mod4Mask)
    KeyCode super_kc = XKeysymToKeycode(dpy, XK_Super_L);
    if (super_kc == 0) super_kc = XKeysymToKeycode(dpy, XK_Super_R);
    if (super_kc != 0) {
        const unsigned int lock_variants[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
        for (int i = 0; i < 4; i++) {
            XGrabKey(dpy, super_kc, lock_variants[i], root, True, GrabModeAsync, GrabModeAsync);
        }
    }
    shortcuts_grab_keys(dpy, root, &shortcuts_cfg);

    // Grab Super+Space for keyboard layout switching
    KeyCode space_kc = XKeysymToKeycode(dpy, XK_space);
    if (space_kc != 0) {
        const unsigned int lock_variants[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
        for (int i = 0; i < 4; i++) {
            XGrabKey(dpy, space_kc, Mod4Mask | lock_variants[i], root, True, GrabModeAsync, GrabModeAsync);
        }
    }

    // Grab Alt+F1 as fallback trigger
    KeyCode alt_f1_kc = XKeysymToKeycode(dpy, XK_F1);
    XGrabKey(dpy, alt_f1_kc, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);

    // Grab Alt+Tab and Alt+Shift+Tab for Alt-Tab Switcher, and Super+Tab for Mission Viewer
    KeyCode tab_kc = XKeysymToKeycode(dpy, XK_Tab);
    if (tab_kc != 0) {
        const unsigned int lock_variants[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
        for (int i = 0; i < 4; i++) {
            XGrabKey(dpy, tab_kc, Mod1Mask | lock_variants[i], root, True, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, tab_kc, Mod1Mask | ShiftMask | lock_variants[i], root, True, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, tab_kc, Mod4Mask | lock_variants[i], root, True, GrabModeAsync, GrabModeAsync);
        }
    }

    // Grab Workspace hotkeys (Alt+1..4 / Super+1..4 to switch, +Shift to move window)
    KeySym ws_syms[4] = { XK_1, XK_2, XK_3, XK_4 };
    for (int i = 0; i < 4; i++) {
        KeyCode kc = XKeysymToKeycode(dpy, ws_syms[i]);
        if (kc != 0) {
            XGrabKey(dpy, kc, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, kc, Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, kc, Mod1Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, kc, Mod4Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
        }
    }

    // EWMH supported atoms setup
    Atom net_muzu_reload_wallpaper = XInternAtom(dpy, "_MUZU_RELOAD_WALLPAPER", False);
    Atom net_muzu_reload_settings = XInternAtom(dpy, "_MUZU_RELOAD_SETTINGS", False);
    Atom net_wm_moveresize = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
    Atom net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    Atom net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    Atom net_wm_icon = XInternAtom(dpy, "_NET_WM_ICON", False);
    Atom net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom net_close_window = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);

    Window wm_check_win = XCreateSimpleWindow(dpy, root, -100, -100, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, wm_check_win, net_supporting_wm_check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&wm_check_win, 1);
    XChangeProperty(dpy, wm_check_win, net_wm_name, utf8_string, 8,
                    PropModeReplace, (const unsigned char *)"MUZUwm", 6);
    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&wm_check_win, 1);

    Atom supported_atoms[] = {
        net_supported,
        net_supporting_wm_check,
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
            if (kbd_layout_handle_event(dpy, &kbd_layouts, &ev)) {
                tint2_panel_set_lang(&panel, kbd_layout_current_short(&kbd_layouts));
                tint2_panel_render(dpy, &panel);
            }
            switch (ev.type) {
            case MapRequest: {
                Window w = ev.xmaprequest.window;
                if (w == rofi_launcher.win || w == dock.win || w == context_menu.win || w == panel.win ||
                    (dock.tooltip_win && w == dock.tooltip_win) ||
                    (alttab.win && w == alttab.win) || (mission_viewer.win && w == mission_viewer.win)) {
                    XMapWindow(dpy, w);
                    break;
                }

                XWindowAttributes wa = {0};
                if (XGetWindowAttributes(dpy, w, &wa) && wa.override_redirect) {
                    XMapWindow(dpy, w);
                    break;
                }

                if (!client_find(clients, w)) {
                    int is_csd = client_is_csd(dpy, w);
                    int can_fs = client_supports_fullscreen(dpy, w);

                    int init_x = 0, init_y = TINT2_PANEL_HEIGHT, win_w = 600, win_h = 400;
                    client_resolve_initial_geometry(screen_w, screen_h,
                                                    cfg.top_inset, cfg.bottom_inset,
                                                    wa.x, wa.y, wa.width, wa.height,
                                                    is_csd,
                                                    &init_x, &init_y, &win_w, &win_h);

                    Window frame = is_csd ? None : titlebar_create(dpy, w, win_w);
                    client_add(&clients, w, CLIENT_FLOATING);
                    Client *c = client_find(clients, w);
                    if (c) {
                        c->frame = frame;
                        c->is_csd = is_csd;
                        c->workspace = cfg.current_workspace;
                        c->x = init_x;
                        c->y = init_y;
                        c->width = win_w;
                        c->height = is_csd ? win_h : (win_h + TITLEBAR_HEIGHT);
                        c->can_fullscreen = can_fs;
                        client_get_title(dpy, w, c->title, sizeof(c->title));
                        if (!c->icon_surface) {
                            c->icon_surface = client_get_icon(dpy, c->win, c->title, 32);
                        }
                        // Only apps listed in fullscreen_allowlist open maximized
                        if (can_fs && config_client_in_fullscreen_allowlist(dpy, w, c->title)) {
                            client_toggle_maximize(dpy, c, &cfg);
                        }
                    }
                    XAddToSaveSet(dpy, w);
                    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask | FocusChangeMask | EnterWindowMask | LeaveWindowMask);
                }
                XMapWindow(dpy, w);
                Client *c = client_find(clients, w);
                if (c) {
                    set_focused_client(dpy, c);
                    if (c->mode == CLIENT_FLOATING) {
                        if (c->frame != None) {
                            int ch = c->height - TITLEBAR_HEIGHT;
                            if (ch < 10) ch = 10;
                            XMoveResizeWindow(dpy, c->frame, c->x, c->y, c->width, c->height);
                            XMoveResizeWindow(dpy, w, 0, TITLEBAR_HEIGHT, c->width, ch);
                            titlebar_draw(dpy, c, &current_theme);
                        } else {
                            XMoveResizeWindow(dpy, w, c->x, c->y, c->width, c->height);
                        }
                    } else {
                        reflow_all(dpy, clients, &cfg);
                    }
                    if (c->frame) {
                        XRaiseWindow(dpy, c->frame);
                    }
                } else {
                    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
                }
                dock_update_running(&dock, clients);
                dock_render(dpy, &dock, screen_w, screen_h);
                update_panel_state(dpy, clients, &cfg);
                raise_system_overlays(dpy);
                break;
            }

            case ConfigureRequest: {
                XConfigureRequestEvent *cre = &ev.xconfigurerequest;
                Client *c = client_find(clients, cre->window);
                if (c) {
                    if (c->mode == CLIENT_FLOATING && !c->is_maximized) {
                        if (cre->value_mask & CWX) c->x = cre->x;
                        if (cre->value_mask & CWY) c->y = cre->y;
                        if (cre->value_mask & CWWidth) c->width = cre->width > 10 ? cre->width : 10;
                        if (cre->value_mask & CWHeight) {
                            int req_h = cre->height > 10 ? cre->height : 10;
                            c->height = c->frame ? (req_h + TITLEBAR_HEIGHT) : req_h;
                        }
                        if (c->frame) {
                            int ch = c->height - TITLEBAR_HEIGHT;
                            if (ch < 10) ch = 10;
                            XMoveResizeWindow(dpy, c->frame, c->x, c->y, c->width, c->height);
                            XMoveResizeWindow(dpy, c->win, 0, TITLEBAR_HEIGHT, c->width, ch);
                            titlebar_draw(dpy, c, &current_theme);
                        } else {
                            XMoveResizeWindow(dpy, c->win, c->x, c->y, c->width, c->height);
                            if ((cre->value_mask & CWStackMode) && cre->detail == Above) {
                                XRaiseWindow(dpy, c->win);
                            }
                        }
                    } else {
                        // Tiled or maximized: enforce WM geometry and send synthetic ConfigureNotify
                        int ch = c->frame ? (c->height - TITLEBAR_HEIGHT) : c->height;
                        if (ch < 10) ch = 10;
                        if (c->frame) {
                            XMoveResizeWindow(dpy, c->win, 0, TITLEBAR_HEIGHT, c->width, ch);
                        } else {
                            XMoveResizeWindow(dpy, c->win, c->x, c->y, c->width, c->height);
                        }
                        XConfigureEvent ce;
                        memset(&ce, 0, sizeof(ce));
                        ce.type = ConfigureNotify;
                        ce.display = dpy;
                        ce.event = c->win;
                        ce.window = c->win;
                        ce.x = c->x;
                        ce.y = c->frame ? (c->y + TITLEBAR_HEIGHT) : c->y;
                        ce.width = c->width;
                        ce.height = ch;
                        ce.border_width = 0;
                        ce.above = None;
                        ce.override_redirect = False;
                        XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
                    }
                } else {
                    XWindowChanges wc;
                    wc.x = cre->x;
                    wc.y = cre->y;
                    wc.width = cre->width;
                    wc.height = cre->height;
                    wc.border_width = cre->border_width;
                    wc.sibling = cre->above;
                    wc.stack_mode = cre->detail;
                    XConfigureWindow(dpy, cre->window, cre->value_mask, &wc);
                }
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
                        if (c->workspace != cfg.current_workspace) {
                            switch_workspace(dpy, clients, &cfg, &dock, c->workspace);
                        }
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
                            update_panel_state(dpy, clients, &cfg);
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
                if (w == rofi_launcher.win || w == dock.win || w == context_menu.win || w == panel.win ||
                    (dock.tooltip_win && w == dock.tooltip_win) ||
                    (alttab.win && w == alttab.win) || (mission_viewer.win && w == mission_viewer.win)) {
                    break;
                }
                Client *c = client_find_by_any(clients, w);
                if (c && (c->is_minimized || c->workspace != cfg.current_workspace)) {
                    break;
                }
                client_teardown(dpy, &clients, w, &cfg, &drag_client, &last_titlebar_click_win, &dock);
                if (alttab.is_visible) {
                    alttab_populate(dpy, &alttab, clients);
                    if (alttab.app_count == 0) {
                        XUngrabKeyboard(dpy, CurrentTime);
                        alttab_hide(dpy, &alttab);
                    } else {
                        alttab_render(dpy, &alttab);
                    }
                }
                if (mission_viewer.is_visible) {
                    mission_viewer_populate(dpy, &mission_viewer, clients, cfg.current_workspace);
                    if (mission_viewer.slot_count == 0) {
                        XUngrabKeyboard(dpy, CurrentTime);
                        mission_viewer_hide(dpy, &mission_viewer);
                    } else {
                        mission_viewer_calculate_layout(&mission_viewer, screen_w, screen_h);
                        mission_viewer_render(dpy, &mission_viewer);
                    }
                }
                break;
            }

            case DestroyNotify: {
                Window w = ev.xdestroywindow.window;
                if (w == rofi_launcher.win || w == dock.win || w == context_menu.win || w == panel.win ||
                    (dock.tooltip_win && w == dock.tooltip_win) ||
                    (alttab.win && w == alttab.win) || (mission_viewer.win && w == mission_viewer.win)) {
                    break;
                }
                client_teardown(dpy, &clients, w, &cfg, &drag_client, &last_titlebar_click_win, &dock);
                if (alttab.is_visible) {
                    alttab_populate(dpy, &alttab, clients);
                    if (alttab.app_count == 0) {
                        XUngrabKeyboard(dpy, CurrentTime);
                        alttab_hide(dpy, &alttab);
                    } else {
                        alttab_render(dpy, &alttab);
                    }
                }
                if (mission_viewer.is_visible) {
                    mission_viewer_populate(dpy, &mission_viewer, clients, cfg.current_workspace);
                    if (mission_viewer.slot_count == 0) {
                        XUngrabKeyboard(dpy, CurrentTime);
                        mission_viewer_hide(dpy, &mission_viewer);
                    } else {
                        mission_viewer_calculate_layout(&mission_viewer, screen_w, screen_h);
                        mission_viewer_render(dpy, &mission_viewer);
                    }
                }
                break;
            }

            case KeyPress: {
                if (confirm_dialog.is_visible) {
                    KeySym sym = XLookupKeysym(&ev.xkey, 0);
                    if (sym == XK_Escape) {
                        XUngrabKeyboard(dpy, CurrentTime);
                        confirm_dialog_hide(dpy, &confirm_dialog);
                    } else if (sym == XK_Return || sym == XK_KP_Enter) {
                        int act = confirm_dialog.action_id;
                        XUngrabKeyboard(dpy, CurrentTime);
                        confirm_dialog_hide(dpy, &confirm_dialog);
                        execute_power_action(act);
                    }
                    super_pending = 0;
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
                        if (ev.xkey.state & ShiftMask) {
                            alttab_select_prev(&alttab);
                        } else {
                            alttab_select_next(&alttab);
                        }
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
                }

                if (mission_viewer.is_visible) {
                    KeySym sym = XLookupKeysym(&ev.xkey, 0);
                    if (sym == XK_Escape) {
                        XUngrabKeyboard(dpy, CurrentTime);
                        mission_viewer_hide(dpy, &mission_viewer);
                        break;
                    }
                    if (sym == XK_Tab || sym == XK_Right || sym == XK_Down) {
                        mission_viewer_select_next(&mission_viewer);
                        mission_viewer_render(dpy, &mission_viewer);
                        break;
                    }
                    if (sym == XK_Left || sym == XK_Up) {
                        mission_viewer_select_prev(&mission_viewer);
                        mission_viewer_render(dpy, &mission_viewer);
                        break;
                    }
                    if (sym == XK_Return || sym == XK_KP_Enter || sym == XK_space) {
                        Client *sel_c = mission_viewer_get_selected(&mission_viewer);
                        XUngrabKeyboard(dpy, CurrentTime);
                        mission_viewer_hide(dpy, &mission_viewer);
                        if (sel_c) {
                            client_activate_and_unminimize(dpy, sel_c, clients, &cfg, &dock, screen_w, screen_h);
                        }
                        break;
                    }
                }

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

                // Alt+Tab Switcher trigger
                if (sym == XK_Tab && (ev.xkey.state & Mod1Mask)) {
                    alttab_populate(dpy, &alttab, clients);
                    if (alttab.app_count > 0) {
                        alttab_show(dpy, &alttab, screen_w, screen_h);
                        if (alttab.app_count > 1) {
                            if (ev.xkey.state & ShiftMask) alttab_select_prev(&alttab);
                            else alttab_select_next(&alttab);
                        }
                        alttab_render(dpy, &alttab);
                        XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
                        raise_system_overlays(dpy);
                    }
                    break;
                }

                // Super+Tab Mission Viewer trigger
                if (sym == XK_Tab && (ev.xkey.state & Mod4Mask)) {
                    if (!mission_viewer.is_visible) {
                        mission_viewer_populate(dpy, &mission_viewer, clients, cfg.current_workspace);
                        if (mission_viewer.slot_count > 0) {
                            mission_viewer_show(dpy, &mission_viewer, screen_w, screen_h);
                            XGrabKeyboard(dpy, mission_viewer.win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
                            raise_system_overlays(dpy);
                        }
                    } else {
                        XUngrabKeyboard(dpy, CurrentTime);
                        mission_viewer_hide(dpy, &mission_viewer);
                    }
                    break;
                }

                // Super+Space cycles keyboard layout (EN <-> TH ...)
                if (sym == XK_space && (ev.xkey.state & Mod4Mask)) {
                    super_pending = 0;
                    kbd_layout_cycle(dpy, &kbd_layouts);
                    tint2_panel_set_lang(&panel, kbd_layout_current_short(&kbd_layouts));
                    tint2_panel_render(dpy, &panel);
                    break;
                }

                if (sym == XK_Super_L || sym == XK_Super_R) {
                    // Tapping Super alone opens the launcher on release (so Super+Arrow does not)
                    super_pending = 1;
                    break;
                }
                super_pending = 0;

                const ShortcutBinding *sb = shortcuts_match_event(&shortcuts_cfg, ev.xkey.state, sym);
                if (sb) {
                    switch (sb->type) {
                    case SHORTCUT_ACT_TILE_LEFT:
                        client_snap(dpy, clients, focused_client, &cfg, SNAP_LEFT);
                        break;
                    case SHORTCUT_ACT_TILE_RIGHT:
                        client_snap(dpy, clients, focused_client, &cfg, SNAP_RIGHT);
                        break;
                    case SHORTCUT_ACT_FULLSCREEN:
                        if (focused_client && focused_client->can_fullscreen) {
                            client_toggle_maximize(dpy, focused_client, &cfg);
                            if (focused_client->frame) {
                                titlebar_draw(dpy, focused_client, &current_theme);
                                XRaiseWindow(dpy, focused_client->frame);
                            }
                            reflow_all(dpy, clients, &cfg);
                        }
                        break;
                    case SHORTCUT_ACT_FLOAT:
                        client_float(dpy, clients, focused_client, &cfg);
                        break;
                    case SHORTCUT_ACT_CLOSE:
                        if (focused_client) client_close(dpy, focused_client->win);
                        break;
                    case SHORTCUT_ACT_LAUNCHER:
                        rofi_launcher_toggle(dpy, &rofi_launcher);
                        break;
                    case SHORTCUT_ACT_EXEC:
                        config_spawn_command(sb->exec_cmd);
                        break;
                    case SHORTCUT_ACT_NONE:
                        break;
                    }
                    update_panel_state(dpy, clients, &cfg);
                    raise_system_overlays(dpy);
                    break;
                }

                if (sym == XK_F1 && (ev.xkey.state & Mod1Mask)) {
                    rofi_launcher_toggle(dpy, &rofi_launcher);
                } else if ((sym >= XK_1 && sym <= XK_4) &&
                           (ev.xkey.state & (Mod1Mask | Mod4Mask))) {
                    int target_ws = (int)(sym - XK_1);
                    if (ev.xkey.state & ShiftMask) {
                        if (focused_client) {
                            move_client_to_workspace(dpy, clients, focused_client, &cfg, &dock, target_ws);
                        }
                    } else {
                        switch_workspace(dpy, clients, &cfg, &dock, target_ws);
                    }
                }
                break;
            }

            case KeyRelease: {
                KeySym rsym = XLookupKeysym(&ev.xkey, 0);
                if (alttab.is_visible && (rsym == XK_Alt_L || rsym == XK_Alt_R)) {
                    XUngrabKeyboard(dpy, CurrentTime);
                    Window sel_win = alttab_get_selected_window(&alttab);
                    alttab_hide(dpy, &alttab);
                    if (sel_win != None) {
                        Client *target_c = client_find_by_any(clients, sel_win);
                        if (target_c) {
                            client_activate_and_unminimize(dpy, target_c, clients, &cfg, &dock, screen_w, screen_h);
                        }
                    }
                    break;
                }
                if ((rsym == XK_Super_L || rsym == XK_Super_R) && super_pending) {
                    super_pending = 0;
                    rofi_launcher_toggle(dpy, &rofi_launcher);
                }
                break;
            }

            case ButtonPress: {
                super_pending = 0;
                if (confirm_dialog.is_visible) {
                    ConfirmButton btn = CONFIRM_BTN_CANCEL;
                    if (ev.xbutton.window == confirm_dialog.win) {
                        if (ev.xbutton.button != Button1) break;
                        btn = confirm_dialog_button_at(&confirm_dialog, ev.xbutton.x, ev.xbutton.y);
                        if (btn == CONFIRM_BTN_NONE) break;
                    }
                    int act = confirm_dialog.action_id;
                    XUngrabKeyboard(dpy, CurrentTime);
                    confirm_dialog_hide(dpy, &confirm_dialog);
                    if (btn == CONFIRM_BTN_OK) execute_power_action(act);
                    break;
                }

                if (mission_viewer.is_visible) {
                    if (ev.xbutton.window == mission_viewer.win) {
                        int slot_idx = mission_viewer_get_slot_at(&mission_viewer, ev.xbutton.x, ev.xbutton.y);
                        XUngrabKeyboard(dpy, CurrentTime);
                        mission_viewer_hide(dpy, &mission_viewer);
                        if (slot_idx >= 0 && slot_idx < mission_viewer.slot_count) {
                            Client *c = mission_viewer.slots[slot_idx].client;
                            if (c) {
                                client_activate_and_unminimize(dpy, c, clients, &cfg, &dock, screen_w, screen_h);
                            }
                        }
                    } else {
                        XUngrabKeyboard(dpy, CurrentTime);
                        mission_viewer_hide(dpy, &mission_viewer);
                    }
                    break;
                }

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

                if (ev.xbutton.window == panel.win) {
                    if (ev.xbutton.button == Button4) {
                        int prev_ws = (cfg.current_workspace - 1 + TINT2_NUM_WORKSPACES) % TINT2_NUM_WORKSPACES;
                        switch_workspace(dpy, clients, &cfg, &dock, prev_ws);
                        break;
                    }
                    if (ev.xbutton.button == Button5) {
                        int next_ws = (cfg.current_workspace + 1) % TINT2_NUM_WORKSPACES;
                        switch_workspace(dpy, clients, &cfg, &dock, next_ws);
                        break;
                    }
                    Tint2PanelAction p_act = tint2_panel_get_action_at(&panel, ev.xbutton.x, ev.xbutton.y);
                    if (p_act >= TINT2_ACTION_WORKSPACE_1 && p_act <= TINT2_ACTION_WORKSPACE_4) {
                        int target_ws = (int)(p_act - TINT2_ACTION_WORKSPACE_1);
                        if (ev.xbutton.button == Button1) {
                            switch_workspace(dpy, clients, &cfg, &dock, target_ws);
                        } else if (ev.xbutton.button == Button3 && focused_client) {
                            move_client_to_workspace(dpy, clients, focused_client, &cfg, &dock, target_ws);
                        }
                    } else if (p_act == TINT2_ACTION_POWER && (ev.xbutton.button == Button1 || ev.xbutton.button == Button3)) {
                        menu_clear(&context_menu);
                        menu_add_item(&context_menu, "Suspend", POWER_ACTION_SUSPEND);
                        menu_add_item(&context_menu, "Restart / Reboot", POWER_ACTION_REBOOT);
                        menu_add_item(&context_menu, "Power Off", POWER_ACTION_POWEROFF);
                        menu_add_separator(&context_menu);
                        menu_add_item(&context_menu, "Lock Screen", POWER_ACTION_LOCK);
                        menu_add_item(&context_menu, "Log Out", POWER_ACTION_LOGOUT);

                        menu_show(dpy, &context_menu, screen_w - 220, TINT2_PANEL_HEIGHT + 4, screen_w, screen_h);
                        raise_system_overlays(dpy);
                    } else if (p_act == TINT2_ACTION_LANG && (ev.xbutton.button == Button1 || ev.xbutton.button == Button3)) {
                        menu_clear(&context_menu);
                        int cur = kbd_layouts.current_group;
                        for (int g = 0; g < kbd_layouts.count; g++) {
                            if (!kbd_layout_is_unique(&kbd_layouts, g)) continue;
                            int is_cur = strcmp(kbd_layouts.codes[g],
                                                kbd_layouts.codes[(cur >= 0 && cur < kbd_layouts.count) ? cur : 0]) == 0;
                            char label[64];
                            snprintf(label, sizeof(label), "%s %s   %s", is_cur ? "\u2713" : "   ",
                                     kbd_layouts.short_names[g], kbd_layout_display_name(kbd_layouts.codes[g]));
                            menu_add_item(&context_menu, label, KBD_MENU_ACTION_BASE + g);
                        }
                        if (context_menu.item_count > 0) {
                            menu_show(dpy, &context_menu, TINT2_LANG_X(panel.width), TINT2_PANEL_HEIGHT + 4, screen_w, screen_h);
                            raise_system_overlays(dpy);
                        }
                    }
                    break;
                }

                if (ev.xbutton.window == dock.win) {
                    if (ev.xbutton.button == Button3) {
                        int clicked_item = dock_get_item_at(&dock, ev.xbutton.x, ev.xbutton.y);
                        Client *min_c = (clicked_item < 0) ? dock_get_minimized_at(&dock, ev.xbutton.x, ev.xbutton.y) : NULL;
                        wm_ctx.right_clicked_dock_item = clicked_item;

                        menu_clear(&context_menu);
                        if (clicked_item >= 0 && clicked_item < dock.item_count) {
                            int matching_win_count = 0;
                            for (Client *c = clients; c; c = c->next) {
                                if (client_matches_dock_item(dpy, c, &dock.items[clicked_item])) {
                                    matching_win_count++;
                                }
                            }
                            dock_menu_populate_app(&context_menu, &dock.items[clicked_item], matching_win_count);
                        } else if (min_c) {
                            DockItem temp_item = {0};
                            snprintf(temp_item.name, sizeof(temp_item.name), "%.63s", min_c->title);
                            dock_menu_populate_app(&context_menu, &temp_item, 1);
                        } else {
                            dock_menu_populate_settings(&context_menu, &dock);
                        }

                        int menu_x = ev.xbutton.x_root;
                        int menu_y = ev.xbutton.y_root;
                        int bound_w = screen_w;
                        int bound_h = screen_h;
                        if (dock.pos == DOCK_BOTTOM && dock.y > 200) {
                            bound_h = dock.y - 2;
                        } else if (dock.pos == DOCK_TOP) {
                            menu_y = dock.y + dock.height + 6;
                        } else if (dock.pos == DOCK_LEFT) {
                            menu_x = dock.x + dock.width + 6;
                        } else if (dock.pos == DOCK_RIGHT && dock.x > 250) {
                            bound_w = dock.x - 2;
                        }

                        menu_show(dpy, &context_menu, menu_x, menu_y, bound_w, bound_h);
                        raise_system_overlays(dpy);
                        break;
                    }
                    if (ev.xbutton.button == Button1) {
                        Client *min_c = dock_get_minimized_at(&dock, ev.xbutton.x, ev.xbutton.y);
                        if (min_c) {
                            client_activate_and_unminimize(dpy, min_c, clients, &cfg, &dock, screen_w, screen_h);
                        } else {
                            int item = dock_get_item_at(&dock, ev.xbutton.x, ev.xbutton.y);
                            if (item >= 0) {
                                dock.press_item = item;
                                dock.press_x = ev.xbutton.x_root;
                                dock.press_y = ev.xbutton.y_root;
                                dock.is_reordering = 0;
                                XGrabPointer(dpy, root, False,
                                             ButtonReleaseMask | PointerMotionMask,
                                             GrabModeAsync, GrabModeAsync,
                                             None, None, CurrentTime);
                            }
                        }
                    }
                    break;
                }

                if (ev.xbutton.window == root && ev.xbutton.button == Button3) {
                    menu_clear(&context_menu);
                    menu_add_item(&context_menu, "Choose Wallpaper (File Browser)...", DESKTOP_ACTION_WALLPAPER_BROWSE);
                    menu_add_item(&context_menu, "Set Wallpaper Path...", DESKTOP_ACTION_WALLPAPER_PATH);
                    menu_add_separator(&context_menu);
                    menu_add_item(&context_menu, "Settings...", DESKTOP_ACTION_SETTINGS);
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
                                            if (!o->is_minimized && o->workspace == cfg.current_workspace) {
                                                set_focused_client(dpy, o);
                                                break;
                                            }
                                        }
                                    }
                                    reflow_all(dpy, clients, &cfg);
                                    dock_render(dpy, &dock, screen_w, screen_h);
                                    update_panel_state(dpy, clients, &cfg);
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
                if (confirm_dialog.is_visible) {
                    if (ev.xmotion.window == confirm_dialog.win) {
                        confirm_dialog_handle_motion(dpy, &confirm_dialog, ev.xmotion.x, ev.xmotion.y);
                    }
                    break;
                }

                if (mission_viewer.is_visible && ev.xmotion.window == mission_viewer.win) {
                    int slot_idx = mission_viewer_get_slot_at(&mission_viewer, ev.xmotion.x, ev.xmotion.y);
                    if (slot_idx != mission_viewer.hover_idx) {
                        mission_viewer.hover_idx = slot_idx;
                        mission_viewer_render(dpy, &mission_viewer);
                    }
                    break;
                }

                if (context_menu.is_visible) {
                    menu_handle_motion(dpy, &context_menu,
                                       ev.xmotion.x_root - context_menu.x,
                                       ev.xmotion.y_root - context_menu.y);
                    raise_system_overlays(dpy);
                    break;
                }

                if (dock.press_item >= 0) {
                    int dx = ev.xmotion.x_root - dock.press_x;
                    int dy = ev.xmotion.y_root - dock.press_y;
                    if (!dock.is_reordering && (dx * dx + dy * dy > 36)) {
                        dock.is_reordering = 1;
                    }
                    int local_x = ev.xmotion.x_root - dock.x;
                    int local_y = ev.xmotion.y_root - dock.y;
                    dock_handle_hover(dpy, &dock, local_x, local_y, screen_w, screen_h);
                    if (dock.is_reordering) {
                        int target_idx = dock_get_item_at(&dock, local_x, local_y);
                        if (target_idx >= 0 && target_idx != dock.press_item) {
                            int new_idx = dock_reorder_item(&dock, dock.press_item, target_idx);
                            if (new_idx != dock.press_item) {
                                dock.press_item = new_idx;
                                dock.hover_item = new_idx;
                                dock_render(dpy, &dock, screen_w, screen_h);
                            }
                        }
                    }
                    break;
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
                    if (abs(dx) > 4 || abs(dy) > 4) {
                        if (drag_client->mode == CLIENT_TILED) {
                            drag_client->mode = CLIENT_FLOATING;
                            drag_client->snap_state = SNAP_NONE;
                            reflow_all(dpy, clients, &cfg);
                        }
                        handle_titlebar_drag(dpy, drag_client, client_start_x + dx, client_start_y + dy, screen_w, screen_h);
                    }
                }

                if (ev.xmotion.window == panel.win) {
                    tint2_panel_handle_motion(dpy, &panel, ev.xmotion.x, ev.xmotion.y);
                }

                if (ev.xmotion.window == dock.win) {
                    dock_handle_hover(dpy, &dock, ev.xmotion.x, ev.xmotion.y, screen_w, screen_h);
                }

                dock_handle_motion(dpy, &dock, ev.xmotion.x_root, ev.xmotion.y_root, screen_w, screen_h);
                if (context_menu.is_visible || rofi_launcher.is_visible) {
                    raise_system_overlays(dpy);
                }
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
                        raise_system_overlays(dpy);
                    }
                    break;
                }

                Client *c = client_find_by_any(clients, ev.xcrossing.window);
                if (c) {
                    // Enforce single-hover exclusivity across all windows
                    for (Client *o = clients; o; o = o->next) {
                        if (o != c && o->is_hovered) {
                            o->is_hovered = 0;
                            if (o != focused_client && o->last_unfocus_time > 0 &&
                                (time(NULL) - o->last_unfocus_time >= 20)) {
                                client_set_opacity(dpy, o->win, 0xA6AAAAAA);
                                o->is_ghost = 1;
                            }
                        }
                    }

                    if (!c->is_hovered) {
                        c->is_hovered = 1;
                        if (c->is_ghost) {
                            client_set_opacity(dpy, c->win, 0xFFFFFFFF);
                        }
                    }
                }
                break;
            }

            case LeaveNotify: {
                if (ev.xcrossing.window == panel.win) {
                    tint2_panel_handle_motion(dpy, &panel, -1, -1);
                    break;
                }

                if (ev.xcrossing.window == dock.win && !rofi_launcher.is_dragging_out && dock.press_item < 0) {
                    dock_hide_tooltip(dpy, &dock);
                    if (dock.hover_item != -1 || dock.hover_minimized != -1 || dock.is_hovered) {
                        dock.hover_item = -1;
                        dock.hover_minimized = -1;
                        dock.is_hovered = 0;
                        dock.mouse_pos = -1.0;
                        if (!dock.is_hidden) {
                            dock_render(dpy, &dock, screen_w, screen_h);
                        }
                    }
                    int in_trigger = 0;
                    int rx = ev.xcrossing.x_root;
                    int ry = ev.xcrossing.y_root;
                    if (dock.pos == DOCK_BOTTOM && ry >= screen_h - dock.size - 10) in_trigger = 1;
                    else if (dock.pos == DOCK_LEFT && rx <= dock.size + 10) in_trigger = 1;
                    else if (dock.pos == DOCK_RIGHT && rx >= screen_w - dock.size - 10) in_trigger = 1;

                    if (!in_trigger && dock.autohide_enabled && !dock.is_hidden && !dock.is_dragging && !context_menu.is_visible) {
                        dock.is_hidden = 1;
                        dock_calculate_geometry(&dock, dock.pos, 1, screen_w, screen_h);
                        XMoveResizeWindow(dpy, dock.win, dock.x, dock.y, dock.width, dock.height);
                    }
                    raise_system_overlays(dpy);
                    break;
                }

                if (ev.xcrossing.detail != NotifyInferior) {
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
                }
                break;
            }

            case ButtonRelease: {
                if (dock.press_item >= 0 && ev.xbutton.button == Button1) {
                    XUngrabPointer(dpy, CurrentTime);
                    int local_x = ev.xbutton.x_root - dock.x;
                    int local_y = ev.xbutton.y_root - dock.y;
                    int was_reordering = dock.is_reordering;
                    int pressed_idx = dock.press_item;
                    dock.press_item = -1;
                    dock.is_reordering = 0;

                    if (was_reordering) {
                        int drop_idx = dock_get_item_at(&dock, local_x, local_y);
                        if (drop_idx >= 0 && drop_idx != pressed_idx &&
                            strcmp(dock.items[drop_idx].exec_cmd, "__TRASH__") == 0) {
                            if (dock_remove_app(&dock, pressed_idx)) {
                                dock_calculate_geometry(&dock, dock.pos, dock.is_hidden, screen_w, screen_h);
                                if (dock.win) {
                                    XMoveResizeWindow(dpy, dock.win, dock.x, dock.y, dock.width, dock.height);
                                    cairo_xlib_surface_set_size(dock.surface, dock.width, dock.height);
                                }
                            }
                        }
                        dock_render(dpy, &dock, screen_w, screen_h);
                    } else {
                        dock_handle_click(dpy, &dock, &rofi_launcher, local_x, local_y);
                    }
                    break;
                }
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
                    if (drag_client->can_fullscreen && ev.xbutton.y_root <= TINT2_PANEL_HEIGHT + SNAP_THRESHOLD) {
                        // Top edge -> Maximize!
                        if (!drag_client->is_maximized) {
                            client_toggle_maximize(dpy, drag_client, &cfg);
                            if (drag_client->frame) {
                                titlebar_draw(dpy, drag_client, &current_theme);
                            }
                        }
                    } else if (drag_client->can_fullscreen && ev.xbutton.x_root <= SNAP_THRESHOLD) {
                        // Left edge -> Left half (50%)
                        if (drag_client->is_maximized) {
                            drag_client->is_maximized = 0;
                        }
                        drag_client->mode = CLIENT_TILED;
                        drag_client->snap_state = SNAP_LEFT;
                        reflow_all(dpy, clients, &cfg);
                    } else if (drag_client->can_fullscreen && ev.xbutton.x_root >= screen_w - SNAP_THRESHOLD) {
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
                if (confirm_dialog.is_visible && ev.xexpose.window == confirm_dialog.win) {
                    confirm_dialog_render(dpy, &confirm_dialog);
                } else if (alttab.is_visible && ev.xexpose.window == alttab.win) {
                    alttab_render(dpy, &alttab);
                } else if (mission_viewer.is_visible && ev.xexpose.window == mission_viewer.win) {
                    mission_viewer_render(dpy, &mission_viewer);
                } else if (ev.xexpose.window == context_menu.win) {
                    menu_render(dpy, &context_menu);
                } else if (ev.xexpose.window == panel.win) {
                    tint2_panel_render(dpy, &panel);
                    raise_system_overlays(dpy);
                } else if (ev.xexpose.window == dock.win) {
                    dock_render(dpy, &dock, screen_w, screen_h);
                    raise_system_overlays(dpy);
                } else if (dock.tooltip_win && ev.xexpose.window == dock.tooltip_win) {
                    dock_render_tooltip(dpy, &dock, screen_w, screen_h);
                } else if (ev.xexpose.window == rofi_launcher.win) {
                    rofi_launcher_render(dpy, &rofi_launcher, screen_w, screen_h);
                    raise_system_overlays(dpy);
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
                if (ev.xproperty.window == root && ev.xproperty.atom == net_muzu_reload_settings) {
                    cfg.master_count = config_get_master_count();
                    dock_load_settings(&dock);
                    dock_calculate_geometry(&dock, dock.pos, dock.is_hidden, screen_w, screen_h);
                    XMoveResizeWindow(dpy, dock.win, dock.x, dock.y, dock.width, dock.height);
                    if (dock.surface) cairo_xlib_surface_set_size(dock.surface, dock.width, dock.height);
                    dock_render(dpy, &dock, screen_w, screen_h);
                    reflow_all(dpy, clients, &cfg);
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
                        update_panel_state(dpy, clients, &cfg);
                        raise_system_overlays(dpy);
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
            tint2_panel_update_time(&panel, now);
            tint2_panel_render(dpy, &panel);
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
    confirm_dialog_cleanup(&confirm_dialog);
    dock_cleanup(&dock);
    if (dock.tooltip_win) {
        XDestroyWindow(dpy, dock.tooltip_win);
        dock.tooltip_win = None;
    }
    tint2_panel_cleanup(&panel);
    rofi_launcher_cleanup(&rofi_launcher);
    alttab_cleanup(&alttab);
    mission_viewer_cleanup(&mission_viewer);
    compositor_cleanup();

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
