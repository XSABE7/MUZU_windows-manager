#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "launchpad.h"

int days_in_month(int year, int month) {
    if (month == 2) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            return 29;
        }
        return 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }
    return 31;
}

void launchpad_scan_apps(Launchpad *lp) {
    lp->app_count = 0;
    DIR *dir = opendir("/usr/share/applications");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && lp->app_count < 128) {
        if (strstr(entry->d_name, ".desktop")) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "/usr/share/applications/%s", entry->d_name);
            FILE *f = fopen(filepath, "r");
            if (!f) continue;

            char line[256];
            char name[64] = {0};
            char exec[128] = {0};
            int is_app = 0;

            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "Type=Application", 16) == 0) is_app = 1;
                if (strncmp(line, "Name=", 5) == 0 && name[0] == '\0') {
                    snprintf(name, sizeof(name), "%.63s", line + 5);
                    name[strcspn(name, "\r\n")] = '\0';
                }
                if (strncmp(line, "Exec=", 5) == 0 && exec[0] == '\0') {
                    snprintf(exec, sizeof(exec), "%.127s", line + 5);
                    exec[strcspn(exec, "\r\n")] = '\0';
                    // Strip %u, %F, etc.
                    char *pct = strchr(exec, '%');
                    if (pct) *pct = '\0';
                }
            }
            fclose(f);

            if (is_app && name[0] != '\0' && exec[0] != '\0') {
                snprintf(lp->apps[lp->app_count].name, sizeof(lp->apps[lp->app_count].name), "%s", name);
                snprintf(lp->apps[lp->app_count].exec, sizeof(lp->apps[lp->app_count].exec), "%s", exec);
                lp->app_count++;
            }
        }
    }
    closedir(dir);
}

void launchpad_init(Display *dpy, Launchpad *lp, int screen_w, int screen_h) {
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = BlackPixel(dpy, screen);
    attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

    lp->win = XCreateWindow(dpy, root, 0, 0, screen_w, screen_h, 0,
                            DefaultDepth(dpy, screen), InputOutput,
                            DefaultVisual(dpy, screen),
                            CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);

    lp->is_visible = 0;
    lp->search_query[0] = '\0';
    lp->current_month_offset = 0;
    easter_egg_init(&lp->egg);

    launchpad_scan_apps(lp);
}

void launchpad_render(Display *dpy, Launchpad *lp, int screen_w, int screen_h) {
    if (!lp->is_visible) return;

    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);

    cairo_surface_t *surface = cairo_xlib_surface_create(dpy, lp->win, visual, screen_w, screen_h);
    cairo_t *cr = cairo_create(surface);

    // Background overlay
    cairo_set_source_rgba(cr, 0.07, 0.09, 0.13, 0.96);
    cairo_paint(cr);

    int mid_x = screen_w * 0.55;

    // LEFT: Search Input
    cairo_set_source_rgba(cr, 0.18, 0.22, 0.28, 0.90);
    cairo_rectangle(cr, 60, 60, mid_x - 100, 44);
    cairo_fill(cr);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14.0);
    cairo_set_source_rgb(cr, 0.95, 0.96, 0.98);
    cairo_move_to(cr, 76, 88);
    if (strlen(lp->search_query) > 0) {
        cairo_show_text(cr, lp->search_query);
    } else {
        cairo_set_source_rgb(cr, 0.55, 0.60, 0.68);
        cairo_show_text(cr, "Search applications...");
    }

    // LEFT: App Grid
    int start_y = 130;
    int col_w = (mid_x - 100) / 3;
    int displayed = 0;

    for (int i = 0; i < lp->app_count && displayed < 24; i++) {
        if (strlen(lp->search_query) == 0 || strcasestr(lp->apps[i].name, lp->search_query)) {
            int gx = 60 + (displayed % 3) * col_w;
            int gy = start_y + (displayed / 3) * 70;

            // App Icon Box
            cairo_set_source_rgba(cr, 0.22, 0.28, 0.38, 0.85);
            cairo_rectangle(cr, gx, gy, 42, 42);
            cairo_fill(cr);

            // App Name
            cairo_set_source_rgb(cr, 0.90, 0.92, 0.96);
            cairo_set_font_size(cr, 11.0);
            cairo_move_to(cr, gx + 50, gy + 25);
            char trunc_name[32];
            snprintf(trunc_name, sizeof(trunc_name), "%s", lp->apps[i].name);
            cairo_show_text(cr, trunc_name);

            displayed++;
        }
    }

    // RIGHT: Digital Clock
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

    char date_str[64];
    strftime(date_str, sizeof(date_str), "%A, %d %B %Y", tm_info);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 48.0);
    cairo_set_source_rgb(cr, 0.98, 0.99, 1.0);
    cairo_move_to(cr, mid_x + 40, 110);
    cairo_show_text(cr, time_str);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14.0);
    cairo_set_source_rgb(cr, 0.65, 0.70, 0.78);
    cairo_move_to(cr, mid_x + 44, 140);
    cairo_show_text(cr, date_str);

    // RIGHT: Monthly Calendar
    int year = tm_info->tm_year + 1900;
    int month = tm_info->tm_mon + 1;
    int total_days = days_in_month(year, month);

    int cal_x = mid_x + 40;
    int cal_y = 190;
    cairo_set_source_rgba(cr, 0.14, 0.17, 0.23, 0.85);
    cairo_rectangle(cr, cal_x, cal_y, 320, 240);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.40, 0.70, 0.95);
    cairo_set_font_size(cr, 12.0);
    cairo_move_to(cr, cal_x + 20, cal_y + 30);
    cairo_show_text(cr, "Mo   Tu   We   Th   Fr   Sa   Su");

    for (int day = 1; day <= total_days; day++) {
        int cell_x = cal_x + 20 + ((day - 1) % 7) * 40;
        int cell_y = cal_y + 65 + ((day - 1) / 7) * 30;

        if (day == tm_info->tm_mday) {
            cairo_set_source_rgb(cr, 0.20, 0.55, 0.95);
            cairo_arc(cr, cell_x + 8, cell_y - 4, 12, 0, 2 * 3.14159);
            cairo_fill(cr);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        } else {
            cairo_set_source_rgb(cr, 0.80, 0.85, 0.90);
        }

        char day_num[8];
        snprintf(day_num, sizeof(day_num), "%2d", day);
        cairo_move_to(cr, cell_x, cell_y);
        cairo_show_text(cr, day_num);
    }

    // RIGHT: "Will She Know" Button (only when easter egg is triggered)
    if (lp->egg.triggered) {
        int btn_x = screen_w - 280;
        int btn_y = screen_h - 100;

        // Glowing button background
        cairo_set_source_rgba(cr, 0.88, 0.20, 0.35, 0.95);
        cairo_rectangle(cr, btn_x, btn_y, 240, 48);
        cairo_fill(cr);

        // Border
        cairo_set_source_rgb(cr, 1.0, 0.8, 0.8);
        cairo_set_line_width(cr, 2.0);
        cairo_rectangle(cr, btn_x, btn_y, 240, 48);
        cairo_stroke(cr);

        // Text
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 13.0);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, btn_x + 18, btn_y + 30);
        cairo_show_text(cr, "🎸 Will She Know (เธอจะรู้บ้างไหม)");
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    XFlush(dpy);
}

void launchpad_toggle(Display *dpy, Launchpad *lp) {
    if (lp->is_visible) {
        XUnmapWindow(dpy, lp->win);
        lp->is_visible = 0;
    } else {
        XMapRaised(dpy, lp->win);
        lp->is_visible = 1;
        lp->search_query[0] = '\0';
        int screen = DefaultScreen(dpy);
        launchpad_render(dpy, lp, DisplayWidth(dpy, screen), DisplayHeight(dpy, screen));
    }
}

void launchpad_handle_key(Display *dpy, Launchpad *lp, XKeyEvent *ev) {
    if (!lp->is_visible) return;

    KeySym sym = XLookupKeysym(ev, 0);
    if (sym == XK_Escape) {
        launchpad_toggle(dpy, lp);
        return;
    }

    char buf[16] = {0};
    int len = XLookupString(ev, buf, sizeof(buf) - 1, NULL, NULL);
    if (len > 0) {
        // Feed into Easter Egg buffer
        easter_egg_feed_key(&lp->egg, buf[0]);

        if (sym == XK_BackSpace) {
            int cur_len = strlen(lp->search_query);
            if (cur_len > 0) lp->search_query[cur_len - 1] = '\0';
        } else if (sym >= 32 && sym <= 126) {
            int cur_len = strlen(lp->search_query);
            if (cur_len < (int)sizeof(lp->search_query) - 1) {
                lp->search_query[cur_len] = buf[0];
                lp->search_query[cur_len + 1] = '\0';
            }
        }
    }

    int screen = DefaultScreen(dpy);
    launchpad_render(dpy, lp, DisplayWidth(dpy, screen), DisplayHeight(dpy, screen));
}

void launchpad_handle_click(Display *dpy, Launchpad *lp, int x, int y) {
    if (!lp->is_visible) return;
    int screen = DefaultScreen(dpy);
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);

    // Check if clicked "Will She Know" button
    if (lp->egg.triggered) {
        int btn_x = screen_w - 280;
        int btn_y = screen_h - 100;
        if (x >= btn_x && x <= btn_x + 240 && y >= btn_y && y <= btn_y + 48) {
            easter_egg_launch_music();
            launchpad_toggle(dpy, lp);
            return;
        }
    }

    // Check if clicked an app in the grid
    int mid_x = screen_w * 0.55;
    int start_y = 130;
    int col_w = (mid_x - 100) / 3;
    int displayed = 0;

    for (int i = 0; i < lp->app_count && displayed < 24; i++) {
        if (strlen(lp->search_query) == 0 || strcasestr(lp->apps[i].name, lp->search_query)) {
            int gx = 60 + (displayed % 3) * col_w;
            int gy = start_y + (displayed / 3) * 70;

            if (x >= gx && x <= gx + col_w && y >= gy && y <= gy + 50) {
                // Launch app
                pid_t pid = fork();
                if (pid == 0) {
                    setsid();
                    execl("/bin/sh", "sh", "-c", lp->apps[i].exec, (char *)NULL);
                    _exit(1);
                }
                launchpad_toggle(dpy, lp);
                return;
            }
            displayed++;
        }
    }

    // Click outside closes launchpad
    launchpad_toggle(dpy, lp);
}
