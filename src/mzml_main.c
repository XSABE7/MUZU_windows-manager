#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include "monitors.h"
#include "mzml.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MZML_WIN_W 600
#define MZML_WIN_H 430
#define MZML_VISIBLE_ROWS 8
#define MZML_ROW_H 40
#define MZML_SEARCH_H 48
#define MZML_PAD 14

static volatile sig_atomic_t running = 1;
static int sig_pipe[2] = { -1, -1 };

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

static void draw_rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    if (r * 2.0 > w) r = w / 2.0;
    if (r * 2.0 > h) r = h / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

static void utf8_pop_char(char *buf) {
    size_t len = strlen(buf);
    while (len > 0) {
        unsigned char c = (unsigned char)buf[len - 1];
        buf[--len] = '\0';
        if ((c & 0xC0) != 0x80) break;
    }
}

static void spawn_detached_cmd(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        signal(SIGCHLD, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
}

typedef struct {
    Display *dpy;
    Window win;
    cairo_surface_t *surface;
    int width;
    int height;
    MzmlAppList apps;
    int *filtered;
    int filtered_count;
    int selected;
    int scroll;
    int hover_row;
    char query[128];
} MzmlUi;

static void ui_refilter(MzmlUi *ui) {
    ui->filtered_count = mzml_filter_apps(&ui->apps, ui->query, ui->filtered,
                                          ui->apps.count > 0 ? ui->apps.count : 1);
    ui->selected = 0;
    ui->scroll = 0;
    ui->hover_row = -1;
}

static void ui_clamp_selection(MzmlUi *ui) {
    if (ui->filtered_count <= 0) {
        ui->selected = 0;
        ui->scroll = 0;
        return;
    }
    if (ui->selected < 0) ui->selected = 0;
    if (ui->selected >= ui->filtered_count) ui->selected = ui->filtered_count - 1;
    if (ui->selected < ui->scroll) ui->scroll = ui->selected;
    if (ui->selected >= ui->scroll + MZML_VISIBLE_ROWS) {
        ui->scroll = ui->selected - MZML_VISIBLE_ROWS + 1;
    }
    if (ui->scroll < 0) ui->scroll = 0;
}

static void ui_render(MzmlUi *ui) {
    if (!ui || !ui->surface) return;
    cairo_t *cr = cairo_create(ui->surface);
    if (!cr) return;

    // Background
    cairo_set_source_rgb(cr, 0.086, 0.086, 0.102);
    cairo_paint(cr);

    // Accent border (#89b4fa)
    cairo_set_source_rgba(cr, 0.537, 0.706, 0.980, 0.75);
    cairo_set_line_width(cr, 1.5);
    draw_rounded_rect(cr, 1.0, 1.0, ui->width - 2.0, ui->height - 2.0, 10.0);
    cairo_stroke(cr);

    // Header & search box
    double sx = MZML_PAD;
    double sy = MZML_PAD;
    double sw = ui->width - MZML_PAD * 2.0;

    cairo_set_source_rgb(cr, 0.125, 0.129, 0.165);
    draw_rounded_rect(cr, sx, sy, sw, MZML_SEARCH_H, 8.0);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.537, 0.706, 0.980, 0.55);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rect(cr, sx + 0.5, sy + 0.5, sw - 1.0, MZML_SEARCH_H - 1.0, 8.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.537, 0.706, 0.980);
    cairo_move_to(cr, sx + 12.0, sy + 28.0);
    cairo_show_text(cr, "mzml >");

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14.0);
    if (ui->query[0] == '\0') {
        cairo_set_source_rgba(cr, 0.65, 0.68, 0.75, 0.55);
        cairo_move_to(cr, sx + 74.0, sy + 29.0);
        cairo_show_text(cr, "Search applications or type command...");
    } else {
        cairo_set_source_rgb(cr, 0.94, 0.95, 0.98);
        cairo_move_to(cr, sx + 74.0, sy + 29.0);
        cairo_show_text(cr, ui->query);
    }

    // List area
    double list_y = sy + MZML_SEARCH_H + 10.0;
    if (ui->filtered_count == 0) {
        cairo_set_source_rgba(cr, 0.78, 0.80, 0.86, 0.75);
        cairo_set_font_size(cr, 13.0);
        cairo_move_to(cr, sx + 12.0, list_y + 28.0);
        if (ui->query[0] != '\0') {
            char hint[192];
            snprintf(hint, sizeof(hint), "Press Enter to run shell command: %.96s", ui->query);
            cairo_show_text(cr, hint);
        } else {
            cairo_show_text(cr, "No desktop applications found. Type a command and press Enter.");
        }
    } else {
        int end = ui->scroll + MZML_VISIBLE_ROWS;
        if (end > ui->filtered_count) end = ui->filtered_count;
        for (int i = ui->scroll; i < end; i++) {
            int row = i - ui->scroll;
            double ry = list_y + row * MZML_ROW_H;
            int is_sel = (i == ui->selected);
            int is_hov = (row == ui->hover_row);

            if (is_sel) {
                cairo_set_source_rgba(cr, 0.537, 0.706, 0.980, 0.28);
                draw_rounded_rect(cr, sx, ry + 2.0, sw, MZML_ROW_H - 4.0, 6.0);
                cairo_fill(cr);
            } else if (is_hov) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
                draw_rounded_rect(cr, sx, ry + 2.0, sw, MZML_ROW_H - 4.0, 6.0);
                cairo_fill(cr);
            }

            const MzmlAppEntry *app = &ui->apps.entries[ui->filtered[i]];

            cairo_save(cr);
            cairo_rectangle(cr, sx + 10.0, ry, sw - 20.0, MZML_ROW_H);
            cairo_clip(cr);

            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                                   is_sel ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 13.0);
            cairo_set_source_rgb(cr, is_sel ? 1.0 : 0.90, is_sel ? 1.0 : 0.92, is_sel ? 1.0 : 0.96);
            cairo_move_to(cr, sx + 14.0, ry + 18.0);
            cairo_show_text(cr, app->name);

            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 10.5);
            cairo_set_source_rgba(cr, 0.65, 0.68, 0.76, 0.75);
            cairo_move_to(cr, sx + 14.0, ry + 32.0);
            cairo_show_text(cr, app->comment[0] ? app->comment : app->exec_clean);

            cairo_restore(cr);
        }
    }

    // Footer status
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.5);
    cairo_set_source_rgba(cr, 0.60, 0.63, 0.70, 0.70);
    char footer[96];
    snprintf(footer, sizeof(footer), "%d / %d apps  |  Enter: launch  |  Shift+Enter: run raw  |  Esc: close",
             ui->filtered_count, ui->apps.count);
    cairo_move_to(cr, sx + 6.0, ui->height - 12.0);
    cairo_show_text(cr, footer);

    cairo_destroy(cr);
    cairo_surface_flush(ui->surface);
    XFlush(ui->dpy);
}

static int row_at_y(const MzmlUi *ui, int y) {
    double list_y = MZML_PAD + MZML_SEARCH_H + 10.0;
    if (y < list_y) return -1;
    int row = (int)((y - list_y) / MZML_ROW_H);
    if (row < 0 || row >= MZML_VISIBLE_ROWS) return -1;
    if (ui->scroll + row >= ui->filtered_count) return -1;
    return row;
}

static void launch_selected_or_query(const MzmlUi *ui, int force_raw) {
    if (!force_raw && ui->filtered_count > 0 &&
        ui->selected >= 0 && ui->selected < ui->filtered_count) {
        const MzmlAppEntry *app = &ui->apps.entries[ui->filtered[ui->selected]];
        spawn_detached_cmd(app->exec_clean);
    } else if (ui->query[0] != '\0') {
        spawn_detached_cmd(ui->query);
    }
}

int main(void) {
    if (pipe2(sig_pipe, O_NONBLOCK | O_CLOEXEC) != 0) {
        sig_pipe[0] = sig_pipe[1] = -1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "mzml: cannot open display\n");
        if (sig_pipe[0] >= 0) close(sig_pipe[0]);
        if (sig_pipe[1] >= 0) close(sig_pipe[1]);
        return 1;
    }
    int x11_fd = ConnectionNumber(dpy);
    int fd_flags = fcntl(x11_fd, F_GETFD);
    if (fd_flags >= 0) {
        fcntl(x11_fd, F_SETFD, fd_flags | FD_CLOEXEC);
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int sw = DisplayWidth(dpy, screen);
    int sh = DisplayHeight(dpy, screen);

    Monitor mons[MAX_MONITORS];
    int mon_count = monitors_query_xrandr(dpy, root, mons, MAX_MONITORS);
    int mx = 0, my = 0, mw = sw, mh = sh;
    if (mon_count > 0) {
        Window rr = None, cr = None;
        int rx = 0, ry = 0, wx = 0, wy = 0;
        unsigned int mask = 0;
        int m = 0;
        if (XQueryPointer(dpy, root, &rr, &cr, &rx, &ry, &wx, &wy, &mask)) {
            m = monitor_find_for_point(mons, mon_count, rx, ry);
        }
        mx = mons[m].x;
        my = mons[m].y;
        mw = mons[m].width;
        mh = mons[m].height;
    }

    MzmlUi ui;
    memset(&ui, 0, sizeof(ui));
    ui.dpy = dpy;
    ui.width = (mw > 40 && MZML_WIN_W > mw - 20) ? (mw - 20) : MZML_WIN_W;
    ui.height = (mh > 40 && MZML_WIN_H > mh - 20) ? (mh - 20) : MZML_WIN_H;
    ui.hover_row = -1;

    mzml_scan_default_dirs(&ui.apps);
    int alloc_n = ui.apps.count > 0 ? ui.apps.count : 1;
    ui.filtered = calloc((size_t)alloc_n, sizeof(int));
    if (!ui.filtered) {
        mzml_list_free(&ui.apps);
        XCloseDisplay(dpy);
        if (sig_pipe[0] >= 0) close(sig_pipe[0]);
        if (sig_pipe[1] >= 0) close(sig_pipe[1]);
        return 1;
    }
    ui_refilter(&ui);

    int pos_x = mx + (mw - ui.width) / 2;
    int pos_y = my + (mh - ui.height) / 2;

    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = 0x16161a;
    attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | PointerMotionMask;

    ui.win = XCreateWindow(dpy, root, pos_x, pos_y, (unsigned int)ui.width, (unsigned int)ui.height,
                           0, CopyFromParent, InputOutput, CopyFromParent,
                           CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);

    Atom net_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom net_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(dpy, ui.win, net_type, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&net_dialog, 1);

    ui.surface = cairo_xlib_surface_create(dpy, ui.win, DefaultVisual(dpy, screen),
                                           ui.width, ui.height);
    XMapRaised(dpy, ui.win);
    XSync(dpy, False);

    for (int i = 0; i < 20; i++) {
        if (XGrabKeyboard(dpy, ui.win, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) {
            break;
        }
        struct timespec ts = { 0, 10000000L };
        nanosleep(&ts, NULL);
    }
    XGrabPointer(dpy, ui.win, False, ButtonPressMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    ui_render(&ui);

    XEvent ev;
    while (running) {
        while (running && XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);
            switch (ev.type) {
                case Expose:
                    ui_render(&ui);
                    break;
                case MotionNotify: {
                    int row = row_at_y(&ui, ev.xmotion.y);
                    if (ev.xmotion.x < 0 || ev.xmotion.x >= ui.width) row = -1;
                    if (row != ui.hover_row) {
                        ui.hover_row = row;
                        ui_render(&ui);
                    }
                    break;
                }
                case ButtonPress: {
                    if (ev.xbutton.x < 0 || ev.xbutton.x >= ui.width ||
                        ev.xbutton.y < 0 || ev.xbutton.y >= ui.height) {
                        running = 0;
                        break;
                    }
                    if (ev.xbutton.button == Button4) {
                        if (ui.selected > 0) {
                            ui.selected--;
                            ui_clamp_selection(&ui);
                            ui_render(&ui);
                        }
                    } else if (ev.xbutton.button == Button5) {
                        if (ui.selected + 1 < ui.filtered_count) {
                            ui.selected++;
                            ui_clamp_selection(&ui);
                            ui_render(&ui);
                        }
                    } else if (ev.xbutton.button == Button1) {
                        int row = row_at_y(&ui, ev.xbutton.y);
                        if (row >= 0) {
                            ui.selected = ui.scroll + row;
                            launch_selected_or_query(&ui, 0);
                            running = 0;
                        }
                    }
                    break;
                }
                case KeyPress: {
                    char buf[32] = {0};
                    KeySym sym = NoSymbol;
                    int n = XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &sym, NULL);

                    if (sym == XK_Escape) {
                        running = 0;
                        break;
                    }
                    if (sym == XK_Return || sym == XK_KP_Enter) {
                        launch_selected_or_query(&ui, (ev.xkey.state & ShiftMask) != 0);
                        running = 0;
                        break;
                    }
                    if (sym == XK_Up) {
                        if (ui.selected > 0) {
                            ui.selected--;
                            ui_clamp_selection(&ui);
                            ui_render(&ui);
                        }
                        break;
                    }
                    if (sym == XK_Down) {
                        if (ui.selected + 1 < ui.filtered_count) {
                            ui.selected++;
                            ui_clamp_selection(&ui);
                            ui_render(&ui);
                        }
                        break;
                    }
                    if (sym == XK_Page_Up) {
                        ui.selected -= MZML_VISIBLE_ROWS;
                        ui_clamp_selection(&ui);
                        ui_render(&ui);
                        break;
                    }
                    if (sym == XK_Page_Down) {
                        ui.selected += MZML_VISIBLE_ROWS;
                        ui_clamp_selection(&ui);
                        ui_render(&ui);
                        break;
                    }
                    if (sym == XK_BackSpace) {
                        utf8_pop_char(ui.query);
                        ui_refilter(&ui);
                        ui_render(&ui);
                        break;
                    }
                    if ((ev.xkey.state & ControlMask) && (sym == XK_u || sym == XK_U)) {
                        ui.query[0] = '\0';
                        ui_refilter(&ui);
                        ui_render(&ui);
                        break;
                    }
                    if (n > 0 && (unsigned char)buf[0] >= 0x20 && (unsigned char)buf[0] != 0x7f) {
                        size_t cur = strlen(ui.query);
                        if (cur + (size_t)n < sizeof(ui.query)) {
                            memcpy(ui.query + cur, buf, (size_t)n);
                            ui.query[cur + (size_t)n] = '\0';
                            ui_refilter(&ui);
                            ui_render(&ui);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
        if (!running) break;
        XFlush(dpy);

        struct pollfd pfds[2];
        nfds_t nfds = 1;
        pfds[0].fd = x11_fd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        if (sig_pipe[0] >= 0) {
            pfds[1].fd = sig_pipe[0];
            pfds[1].events = POLLIN;
            pfds[1].revents = 0;
            nfds = 2;
        }
        int pr = poll(pfds, nfds, -1);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (nfds == 2 && (pfds[1].revents & POLLIN)) {
            char drain[32];
            while (read(sig_pipe[0], drain, sizeof(drain)) > 0) {}
            break;
        }
        if (pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) break;
    }

    XUngrabPointer(dpy, CurrentTime);
    XUngrabKeyboard(dpy, CurrentTime);
    if (ui.surface) {
        cairo_surface_finish(ui.surface);
        cairo_surface_destroy(ui.surface);
    }
    if (ui.win != None) {
        XDestroyWindow(dpy, ui.win);
    }
    free(ui.filtered);
    mzml_list_free(&ui.apps);
    XSync(dpy, True);
    XCloseDisplay(dpy);
    if (sig_pipe[0] >= 0) close(sig_pipe[0]);
    if (sig_pipe[1] >= 0) close(sig_pipe[1]);
    return 0;
}
