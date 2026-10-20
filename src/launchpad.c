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
#include "icon_fetcher.h"

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

int first_weekday_of_month(int year, int month) {
    struct tm t;
    memset(&t, 0, sizeof(struct tm));
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = 1;
    mktime(&t);
    return t.tm_wday; // 0=Sunday, 1=Monday...
}

static void utf8_safe_copy(char *dest, const char *src, size_t max_bytes) {
    if (!dest || !src || max_bytes == 0) return;
    size_t i = 0;
    while (src[i] != '\0' && i < max_bytes - 1) {
        dest[i] = src[i];
        i++;
    }
    while (i > 0 && ((unsigned char)dest[i - 1] & 0xC0) == 0x80) {
        i--;
    }
    if (i > 0 && ((unsigned char)dest[i - 1] & 0x80)) {
        i--;
    }
    dest[i] = '\0';
}

void launchpad_scroll(Launchpad *lp, int delta_y, int screen_h) {
    if (!lp) return;
    int cols = (screen_h <= 600) ? 2 : 3;
    int rows = (lp->app_count + cols - 1) / cols;
    int content_h = rows * 60;
    int visible_h = screen_h - 110;
    int max_scroll = content_h - visible_h;
    if (max_scroll < 0) max_scroll = 0;

    lp->scroll_y += delta_y;
    if (lp->scroll_y < 0) lp->scroll_y = 0;
    if (lp->scroll_y > max_scroll) lp->scroll_y = max_scroll;
}

void launchpad_cleanup(Launchpad *lp) {
    if (!lp) return;
    for (int i = 0; i < lp->app_count; i++) {
        if (lp->apps[i].icon_surface) {
            cairo_surface_destroy(lp->apps[i].icon_surface);
            lp->apps[i].icon_surface = NULL;
        }
    }
    lp->app_count = 0;
    if (lp->surface) {
        cairo_surface_finish(lp->surface);
        cairo_surface_destroy(lp->surface);
        lp->surface = NULL;
    }
}

void launchpad_scan_apps(Launchpad *lp) {
    if (!lp) return;
    launchpad_cleanup(lp);

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
            char icon[128] = {0};
            int is_app = 0;
            int no_display = 0;

            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "Type=Application", 16) == 0) is_app = 1;
                if (strncmp(line, "NoDisplay=true", 14) == 0 || strncmp(line, "Hidden=true", 11) == 0) {
                    no_display = 1;
                }
                if (strncmp(line, "Name=", 5) == 0 && name[0] == '\0') {
                    snprintf(name, sizeof(name), "%.63s", line + 5);
                    name[strcspn(name, "\r\n")] = '\0';
                }
                if (strncmp(line, "Exec=", 5) == 0 && exec[0] == '\0') {
                    snprintf(exec, sizeof(exec), "%.127s", line + 5);
                    exec[strcspn(exec, "\r\n")] = '\0';
                    char *pct = strchr(exec, '%');
                    if (pct) *pct = '\0';
                }
                if (strncmp(line, "Icon=", 5) == 0 && icon[0] == '\0') {
                    snprintf(icon, sizeof(icon), "%.127s", line + 5);
                    icon[strcspn(icon, "\r\n")] = '\0';
                }
            }
            fclose(f);

            if (is_app && !no_display && name[0] != '\0' && exec[0] != '\0') {
                snprintf(lp->apps[lp->app_count].name, sizeof(lp->apps[lp->app_count].name), "%s", name);
                snprintf(lp->apps[lp->app_count].exec, sizeof(lp->apps[lp->app_count].exec), "%s", exec);
                snprintf(lp->apps[lp->app_count].icon, sizeof(lp->apps[lp->app_count].icon), "%s", icon);
                lp->apps[lp->app_count].icon_surface = icon_load_surface(icon);
                lp->app_count++;
            }
        }
    }
    closedir(dir);
}

void launchpad_init(Display *dpy, Launchpad *lp, int screen_w, int screen_h, const GtkTheme *theme) {
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

    lp->surface = NULL;
    lp->is_visible = 0;
    lp->search_query[0] = '\0';
    lp->current_month_offset = 0;
    lp->scroll_y = 0;
    lp->theme = theme;
    easter_egg_init(&lp->egg);

    if (theme && theme->icon_theme_name[0]) {
        icon_theme_set(theme->icon_theme_name);
    }

    launchpad_scan_apps(lp);
}

void launchpad_render(Display *dpy, Launchpad *lp, int screen_w, int screen_h) {
    if (!lp->is_visible) return;

    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);

    if (!lp->surface) {
        lp->surface = cairo_xlib_surface_create(dpy, lp->win, visual, screen_w, screen_h);
    } else {
        cairo_xlib_surface_set_size(lp->surface, screen_w, screen_h);
    }
    cairo_t *cr = cairo_create(lp->surface);

    // Background overlay from GTK Theme
    if (lp->theme) {
        cairo_set_source_rgba(cr, lp->theme->bg_surface.r, lp->theme->bg_surface.g, lp->theme->bg_surface.b, lp->theme->bg_surface.a);
    } else {
        cairo_set_source_rgba(cr, 0.07, 0.09, 0.13, 0.96);
    }
    cairo_paint(cr);

    const char *font_fam = (lp->theme && lp->theme->font_family[0]) ? lp->theme->font_family : "sans-serif";

    // Optimized layout for 800x600, 1366x768, and modern displays
    int is_compact = (screen_w < 1000 || screen_h <= 600);
    int cols = is_compact ? 2 : 3;
    int mid_x = is_compact ? (int)(screen_w * 0.52) : (int)(screen_w * 0.54);

    // LEFT: Search Input (tuned height and padding)
    cairo_set_source_rgba(cr, 0.18, 0.22, 0.28, 0.90);
    cairo_rectangle(cr, 40, 32, mid_x - 60, 40);
    cairo_fill(cr);

    cairo_select_font_face(cr, font_fam, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, is_compact ? 12.0 : 13.0);
    cairo_set_source_rgb(cr, 0.95, 0.96, 0.98);
    cairo_move_to(cr, 56, 57);
    if (strlen(lp->search_query) > 0) {
        cairo_show_text(cr, lp->search_query);
    } else {
        cairo_set_source_rgb(cr, 0.55, 0.60, 0.68);
        cairo_show_text(cr, is_compact ? "Search apps..." : "Search applications (Use mouse wheel to scroll)...");
    }

    // LEFT: App Grid with Cairo Scissor Clipping for Smooth Scrolling
    int start_y = 88;
    int grid_h = screen_h - start_y - 12;
    int col_w = (mid_x - 60) / cols;
    int displayed = 0;

    cairo_save(cr);
    cairo_rectangle(cr, 30, start_y, mid_x - 45, grid_h);
    cairo_clip(cr);

    for (int i = 0; i < lp->app_count; i++) {
        if (strlen(lp->search_query) == 0 || strcasestr(lp->apps[i].name, lp->search_query)) {
            int gx = 40 + (displayed % cols) * col_w;
            int gy = start_y - lp->scroll_y + (displayed / cols) * 60;

            if (gy + 60 >= start_y && gy <= screen_h) {
                // App Icon (Real PNG surface with procedural fallback)
                icon_render(cr, lp->apps[i].icon_surface, lp->apps[i].name, gx, gy, 38);

                // App Name with UTF-8 safe copy
                cairo_set_source_rgb(cr, 0.90, 0.92, 0.96);
                cairo_set_font_size(cr, 11.0);
                cairo_move_to(cr, gx + 46, gy + 24);
                char trunc_name[28];
                utf8_safe_copy(trunc_name, lp->apps[i].name, sizeof(trunc_name));
                cairo_show_text(cr, trunc_name);
            }
            displayed++;
        }
    }
    cairo_restore(cr);

    // Scrollbar indicator on left panel
    int rows = (displayed + cols - 1) / cols;
    int total_content_h = rows * 60;
    if (total_content_h > grid_h) {
        int bar_x = mid_x - 22;
        int bar_w = 4;
        cairo_set_source_rgba(cr, 0.25, 0.30, 0.40, 0.4);
        cairo_rectangle(cr, bar_x, start_y, bar_w, grid_h);
        cairo_fill(cr);

        double ratio = (double)grid_h / total_content_h;
        int thumb_h = (int)(grid_h * ratio);
        if (thumb_h < 24) thumb_h = 24;
        int thumb_y = start_y + (int)((double)lp->scroll_y / (total_content_h - grid_h) * (grid_h - thumb_h));

        cairo_set_source_rgba(cr, 0.45, 0.65, 0.95, 0.8);
        cairo_rectangle(cr, bar_x, thumb_y, bar_w, thumb_h);
        cairo_fill(cr);
    }

    // RIGHT PANEL (Clock, Date, Calendar)
    int right_x = mid_x + 32;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

    char date_str[64];
    strftime(date_str, sizeof(date_str), "%A, %d %B %Y", tm_info);

    // Digital Clock
    cairo_select_font_face(cr, font_fam, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 44.0);
    cairo_set_source_rgb(cr, 0.98, 0.99, 1.0);
    cairo_move_to(cr, right_x, 75);
    cairo_show_text(cr, time_str);

    cairo_select_font_face(cr, font_fam, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgb(cr, 0.65, 0.70, 0.78);
    cairo_move_to(cr, right_x + 2, 102);
    cairo_show_text(cr, date_str);

    // Monthly Calendar
    int year = tm_info->tm_year + 1900;
    int month = tm_info->tm_mon + 1;
    int total_days = days_in_month(year, month);
    int start_col = (first_weekday_of_month(year, month) + 6) % 7;

    int cal_x = right_x;
    int cal_y = 128;
    cairo_set_source_rgba(cr, 0.14, 0.17, 0.23, 0.85);
    cairo_rectangle(cr, cal_x, cal_y, 320, 220);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.40, 0.70, 0.95);
    cairo_set_font_size(cr, 11.5);
    cairo_move_to(cr, cal_x + 18, cal_y + 26);
    cairo_show_text(cr, "Mo   Tu   We   Th   Fr   Sa   Su");

    for (int day = 1; day <= total_days; day++) {
        int cell_idx = start_col + (day - 1);
        int cell_x = cal_x + 18 + (cell_idx % 7) * 40;
        int cell_y = cal_y + 58 + (cell_idx / 7) * 28;

        if (day == tm_info->tm_mday) {
            if (lp->theme) {
                cairo_set_source_rgb(cr, lp->theme->accent_color.r, lp->theme->accent_color.g, lp->theme->accent_color.b);
            } else {
                cairo_set_source_rgb(cr, 0.20, 0.55, 0.95);
            }
            cairo_arc(cr, cell_x + 8, cell_y - 4, 11, 0, 2 * 3.14159);
            cairo_fill(cr);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        } else {
            cairo_set_source_rgb(cr, 0.80, 0.85, 0.90);
        }

        char day_num[16];
        snprintf(day_num, sizeof(day_num), "%2d", day);
        cairo_move_to(cr, cell_x, cell_y);
        cairo_show_text(cr, day_num);
    }

    // RIGHT: "Will She Know" Button (Easter Egg)
    if (lp->egg.triggered) {
        int btn_x = screen_w - 290;
        int btn_y = screen_h - 76;

        cairo_set_source_rgba(cr, 0.88, 0.20, 0.35, 0.95);
        cairo_rectangle(cr, btn_x, btn_y, 250, 44);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 1.0, 0.8, 0.8);
        cairo_set_line_width(cr, 2.0);
        cairo_rectangle(cr, btn_x, btn_y, 250, 44);
        cairo_stroke(cr);

        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12.5);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, btn_x + 16, btn_y + 27);
        cairo_show_text(cr, "🎸 Will She Know (เธอจะรู้บ้างไหม)");
    }

    // Top-right Explicit Close Button ('✕')
    int close_btn_x = screen_w - 56;
    int close_btn_y = 24;
    int close_btn_size = 32;

    cairo_set_source_rgba(cr, 0.22, 0.26, 0.33, 0.85);
    cairo_arc(cr, close_btn_x + close_btn_size / 2.0, close_btn_y + close_btn_size / 2.0, close_btn_size / 2.0, 0, 2 * 3.14159);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.45, 0.50, 0.60, 0.90);
    cairo_set_line_width(cr, 1.5);
    cairo_arc(cr, close_btn_x + close_btn_size / 2.0, close_btn_y + close_btn_size / 2.0, close_btn_size / 2.0, 0, 2 * 3.14159);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.90, 0.92, 0.95);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, close_btn_x + 10, close_btn_y + 10);
    cairo_line_to(cr, close_btn_x + 22, close_btn_y + 22);
    cairo_move_to(cr, close_btn_x + 22, close_btn_y + 10);
    cairo_line_to(cr, close_btn_x + 10, close_btn_y + 22);
    cairo_stroke(cr);

    cairo_destroy(cr);
    cairo_surface_flush(lp->surface);
    if (dpy) XFlush(dpy);
}

void launchpad_toggle(Display *dpy, Launchpad *lp) {
    if (!lp) return;
    if (lp->is_visible) {
        if (dpy) {
            XUngrabKeyboard(dpy, CurrentTime);
            XUnmapWindow(dpy, lp->win);
        }
        lp->is_visible = 0;
        easter_egg_init(&lp->egg);
    } else {
        if (dpy) {
            XMapRaised(dpy, lp->win);
            XGrabKeyboard(dpy, lp->win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        }
        lp->is_visible = 1;
        lp->search_query[0] = '\0';
        lp->scroll_y = 0; // Reset scroll position when opening
        if (dpy) {
            int screen = DefaultScreen(dpy);
            launchpad_render(dpy, lp, DisplayWidth(dpy, screen), DisplayHeight(dpy, screen));
        }
    }
}

void launchpad_handle_key(Display *dpy, Launchpad *lp, XKeyEvent *ev) {
    if (!lp->is_visible) return;

    int screen = DefaultScreen(dpy);
    int screen_h = DisplayHeight(dpy, screen);

    KeySym sym = XLookupKeysym(ev, 0);
    if (sym == XK_Escape) {
        launchpad_toggle(dpy, lp);
        return;
    }

    // Scroll up/down with PageUp/PageDown or Up/Down arrows
    if (sym == XK_Page_Up || sym == XK_Up) {
        launchpad_scroll(lp, -60, screen_h);
        launchpad_render(dpy, lp, DisplayWidth(dpy, screen), screen_h);
        return;
    } else if (sym == XK_Page_Down || sym == XK_Down) {
        launchpad_scroll(lp, 60, screen_h);
        launchpad_render(dpy, lp, DisplayWidth(dpy, screen), screen_h);
        return;
    }

    char buf[16] = {0};
    int len = XLookupString(ev, buf, sizeof(buf) - 1, NULL, NULL);
    if (len > 0) {
        easter_egg_feed_key(&lp->egg, buf[0]);

        if (sym == XK_BackSpace) {
            int cur_len = strlen(lp->search_query);
            if (cur_len > 0) {
                lp->search_query[cur_len - 1] = '\0';
                lp->scroll_y = 0;
            }
        } else if (sym >= 32 && sym <= 126) {
            int cur_len = strlen(lp->search_query);
            if (cur_len < (int)sizeof(lp->search_query) - 1) {
                lp->search_query[cur_len] = buf[0];
                lp->search_query[cur_len + 1] = '\0';
                lp->scroll_y = 0; // Reset scroll when filtering
            }
        }
    }

    launchpad_render(dpy, lp, DisplayWidth(dpy, screen), screen_h);
}

void launchpad_handle_click(Display *dpy, Launchpad *lp, unsigned int button, int x, int y) {
    if (!lp || !lp->is_visible) return;
    int screen_w = 1000;
    int screen_h = 700;
    if (dpy) {
        int screen = DefaultScreen(dpy);
        screen_w = DisplayWidth(dpy, screen);
        screen_h = DisplayHeight(dpy, screen);
    }

    // Mouse wheel scrolling: Button4 = Scroll Up, Button5 = Scroll Down
    if (button == Button4) {
        launchpad_scroll(lp, -60, screen_h);
        if (dpy) launchpad_render(dpy, lp, screen_w, screen_h);
        return;
    } else if (button == Button5) {
        launchpad_scroll(lp, 60, screen_h);
        if (dpy) launchpad_render(dpy, lp, screen_w, screen_h);
        return;
    }

    // Only primary button (Button1) activates buttons and applications
    if (button != Button1) return;

    // Check if clicked top-right explicit close button ('✕')
    int close_btn_x = screen_w - 56;
    int close_btn_y = 24;
    int close_btn_size = 32;
    if (x >= close_btn_x - 6 && x <= close_btn_x + close_btn_size + 6 &&
        y >= close_btn_y - 6 && y <= close_btn_y + close_btn_size + 6) {
        launchpad_toggle(dpy, lp);
        return;
    }

    int is_compact = (screen_w < 1000 || screen_h <= 600);
    int cols = is_compact ? 2 : 3;
    int mid_x = is_compact ? (int)(screen_w * 0.52) : (int)(screen_w * 0.54);

    // Check if clicked inside search box -> ignore click dismiss
    if (x >= 40 && x <= mid_x - 20 && y >= 32 && y <= 72) {
        return;
    }

    // Check if clicked "Will She Know" button
    if (lp->egg.triggered) {
        int btn_x = screen_w - 290;
        int btn_y = screen_h - 76;
        if (x >= btn_x && x <= btn_x + 250 && y >= btn_y && y <= btn_y + 44) {
            easter_egg_launch_music();
            launchpad_toggle(dpy, lp);
            return;
        }
    }

    // Check if clicked an app in the grid
    int start_y = 88;
    int col_w = (mid_x - 60) / cols;
    int displayed = 0;

    for (int i = 0; i < lp->app_count; i++) {
        if (strlen(lp->search_query) == 0 || strcasestr(lp->apps[i].name, lp->search_query)) {
            int gx = 40 + (displayed % cols) * col_w;
            int gy = start_y - lp->scroll_y + (displayed / cols) * 60;

            if (x >= gx && x <= gx + col_w && y >= gy && y <= gy + 50) {
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

    // Safe dismissal: Clicks on background, margins, or calendar do NOT close launchpad
}
