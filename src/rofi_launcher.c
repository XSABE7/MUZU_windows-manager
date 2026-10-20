#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <cairo/cairo-xlib.h>
#include <math.h>

#include "rofi_launcher.h"
#include "icon_fetcher.h"

static void safe_copy(char *dst, const char *src, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t i = 0;
    while (i + 1 < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void rofi_sanitize_exec(const char *raw, char *clean, size_t max_len) {
    if (!raw || !clean || max_len == 0) return;
    size_t out_idx = 0;
    size_t in_len = strlen(raw);

    for (size_t i = 0; i < in_len && out_idx + 1 < max_len; i++) {
        if (raw[i] == '%' && i + 1 < in_len) {
            char code = raw[i + 1];
            if (code == 'f' || code == 'F' || code == 'u' || code == 'U' ||
                code == 'd' || code == 'D' || code == 'n' || code == 'N' ||
                code == 'i' || code == 'c' || code == 'k' || code == 'v' || code == 'm') {
                i++; // Skip both '%' and field code
                continue;
            }
        }
        clean[out_idx++] = raw[i];
    }
    clean[out_idx] = '\0';

    // Strip trailing whitespace
    while (out_idx > 0 && isspace((unsigned char)clean[out_idx - 1])) {
        clean[--out_idx] = '\0';
    }
}

static void parse_desktop_file(const char *path, RofiApp *app) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    int in_desktop_entry = 0;
    int no_display = 0;
    app->name[0] = '\0';
    app->exec[0] = '\0';
    app->comment[0] = '\0';
    app->icon_name[0] = '\0';

    while (fgets(line, sizeof(line), f)) {
        // Strip newline
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '[') {
            if (strcmp(line, "[Desktop Entry]") == 0) {
                in_desktop_entry = 1;
            } else {
                in_desktop_entry = 0;
            }
            continue;
        }

        if (!in_desktop_entry) continue;

        if (strncmp(line, "Type=", 5) == 0) {
            if (strcmp(line + 5, "Application") != 0) {
                fclose(f);
                app->name[0] = '\0';
                return;
            }
        } else if (strncmp(line, "NoDisplay=", 10) == 0) {
            if (strcasecmp(line + 10, "true") == 0) {
                no_display = 1;
            }
        } else if (strncmp(line, "Name=", 5) == 0 && app->name[0] == '\0') {
            safe_copy(app->name, line + 5, sizeof(app->name));
        } else if (strncmp(line, "Exec=", 5) == 0 && app->exec[0] == '\0') {
            safe_copy(app->exec, line + 5, sizeof(app->exec));
        } else if (strncmp(line, "Icon=", 5) == 0 && app->icon_name[0] == '\0') {
            safe_copy(app->icon_name, line + 5, sizeof(app->icon_name));
        } else if (strncmp(line, "Comment=", 8) == 0 && app->comment[0] == '\0') {
            safe_copy(app->comment, line + 8, sizeof(app->comment));
        }
    }
    fclose(f);

    if (no_display || app->name[0] == '\0' || app->exec[0] == '\0') {
        app->name[0] = '\0';
    }
}

static void scan_directory(const char *dir_path, RofiLauncher *rofi) {
    DIR *d = opendir(dir_path);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (rofi->app_count >= ROFI_MAX_APPS) break;
        if (entry->d_type == DT_DIR) continue;

        size_t len = strlen(entry->d_name);
        if (len < 9 || strcmp(entry->d_name + len - 8, ".desktop") != 0) continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        RofiApp app = {0};
        parse_desktop_file(full_path, &app);

        if (app.name[0] != '\0' && app.exec[0] != '\0') {
            // Check for duplicates
            int duplicate = 0;
            for (int i = 0; i < rofi->app_count; i++) {
                if (strcasecmp(rofi->apps[i].name, app.name) == 0) {
                    duplicate = 1;
                    break;
                }
            }
            if (!duplicate) {
                rofi->apps[rofi->app_count] = app;
                rofi->apps[rofi->app_count].icon_surface = NULL;
                rofi->app_count++;
            }
        }
    }
    closedir(d);
}

static int compare_apps(const void *a, const void *b) {
    const RofiApp *app_a = (const RofiApp *)a;
    const RofiApp *app_b = (const RofiApp *)b;
    return strcasecmp(app_a->name, app_b->name);
}

void rofi_launcher_scan_apps(RofiLauncher *rofi) {
    if (!rofi) return;
    rofi->app_count = 0;

    char user_app_dir[512];
    const char *home = getenv("HOME");
    if (home) {
        snprintf(user_app_dir, sizeof(user_app_dir), "%s/.local/share/applications", home);
        scan_directory(user_app_dir, rofi);
    }
    scan_directory("/usr/share/applications", rofi);

    // Sort alphabetically
    if (rofi->app_count > 1) {
        qsort(rofi->apps, rofi->app_count, sizeof(RofiApp), compare_apps);
    }
}

void rofi_launcher_filter(RofiLauncher *rofi) {
    if (!rofi) return;
    rofi->match_count = 0;

    if (rofi->query[0] == '\0') {
        for (int i = 0; i < rofi->app_count && i < ROFI_MAX_APPS; i++) {
            rofi->matches[rofi->match_count++] = i;
        }
    } else {
        // Prefix matches first
        for (int i = 0; i < rofi->app_count && rofi->match_count < ROFI_MAX_APPS; i++) {
            if (strncasecmp(rofi->apps[i].name, rofi->query, rofi->query_len) == 0) {
                rofi->matches[rofi->match_count++] = i;
            }
        }
        // Substring matches in name
        for (int i = 0; i < rofi->app_count && rofi->match_count < ROFI_MAX_APPS; i++) {
            if (strncasecmp(rofi->apps[i].name, rofi->query, rofi->query_len) != 0 &&
                strcasestr(rofi->apps[i].name, rofi->query) != NULL) {
                rofi->matches[rofi->match_count++] = i;
            }
        }
        // Substring matches in exec/comment
        for (int i = 0; i < rofi->app_count && rofi->match_count < ROFI_MAX_APPS; i++) {
            if (strcasestr(rofi->apps[i].name, rofi->query) == NULL &&
                (strcasestr(rofi->apps[i].exec, rofi->query) != NULL ||
                 strcasestr(rofi->apps[i].comment, rofi->query) != NULL)) {
                rofi->matches[rofi->match_count++] = i;
            }
        }
    }

    rofi->selected_idx = 0;
    rofi->scroll_offset = 0;
}

void rofi_launcher_set_query(RofiLauncher *rofi, const char *q) {
    if (!rofi) return;
    if (!q) {
        rofi->query[0] = '\0';
        rofi->query_len = 0;
    } else {
        snprintf(rofi->query, sizeof(rofi->query), "%s", q);
        rofi->query_len = (int)strlen(rofi->query);
    }
    rofi_launcher_filter(rofi);
}

void rofi_launcher_query_append(RofiLauncher *rofi, char c) {
    if (!rofi) return;
    if (rofi->query_len + 1 < (int)sizeof(rofi->query)) {
        rofi->query[rofi->query_len++] = c;
        rofi->query[rofi->query_len] = '\0';
        rofi_launcher_filter(rofi);
    }
}

void rofi_launcher_query_backspace(RofiLauncher *rofi) {
    if (!rofi) return;
    if (rofi->query_len > 0) {
        rofi->query[--rofi->query_len] = '\0';
        rofi_launcher_filter(rofi);
    }
}

void rofi_launcher_select_next(RofiLauncher *rofi) {
    if (!rofi || rofi->match_count == 0) return;
    rofi->selected_idx++;
    if (rofi->selected_idx >= rofi->match_count) {
        rofi->selected_idx = 0; // Wrap around to top
    }

    // Adjust scroll
    if (rofi->selected_idx < rofi->scroll_offset) {
        rofi->scroll_offset = rofi->selected_idx;
    } else if (rofi->selected_idx >= rofi->scroll_offset + ROFI_VISIBLE_ROWS) {
        rofi->scroll_offset = rofi->selected_idx - ROFI_VISIBLE_ROWS + 1;
    }
}

void rofi_launcher_select_prev(RofiLauncher *rofi) {
    if (!rofi || rofi->match_count == 0) return;
    rofi->selected_idx--;
    if (rofi->selected_idx < 0) {
        rofi->selected_idx = rofi->match_count - 1; // Wrap around to bottom
    }

    // Adjust scroll
    if (rofi->selected_idx < rofi->scroll_offset) {
        rofi->scroll_offset = rofi->selected_idx;
    } else if (rofi->selected_idx >= rofi->scroll_offset + ROFI_VISIBLE_ROWS) {
        rofi->scroll_offset = rofi->selected_idx - ROFI_VISIBLE_ROWS + 1;
    }
}

void rofi_launcher_execute_selected(Display *dpy, RofiLauncher *rofi) {
    if (!rofi || rofi->selected_idx < 0 || rofi->selected_idx >= rofi->match_count) return;

    int app_idx = rofi->matches[rofi->selected_idx];
    char clean_cmd[512];
    rofi_sanitize_exec(rofi->apps[app_idx].exec, clean_cmd, sizeof(clean_cmd));

    if (clean_cmd[0] != '\0') {
        pid_t pid = fork();
        if (pid == 0) {
            setsid();
            execl("/bin/sh", "sh", "-c", clean_cmd, NULL);
            _exit(1);
        }
    }

    if (dpy) {
        rofi_launcher_toggle(dpy, rofi);
    }
}

void rofi_launcher_init(Display *dpy, RofiLauncher *rofi, int screen_w, int screen_h, const GtkTheme *theme) {
    if (!rofi) return;
    memset(rofi, 0, sizeof(RofiLauncher));

    rofi->width = ROFI_WIDTH;
    rofi->height = ROFI_TOTAL_HEIGHT;
    rofi->target_x = (screen_w - rofi->width) / 2;
    rofi->target_y = (screen_h - rofi->height) / 2;
    rofi->x = rofi->target_x;
    rofi->y = rofi->target_y;
    rofi->anim_start_y = rofi->target_y + 24;
    rofi->theme = theme;
    rofi->is_visible = 0;
    rofi->is_animating = 0;

    rofi_launcher_scan_apps(rofi);
    rofi_launcher_filter(rofi);

    if (dpy) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);

        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.background_pixel = 0x121214;
        attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                           PointerMotionMask | StructureNotifyMask | FocusChangeMask;

        rofi->win = XCreateWindow(dpy, root,
                                 rofi->x, rofi->y,
                                 rofi->width, rofi->height,
                                 0, CopyFromParent, InputOutput, CopyFromParent,
                                 CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);

        Atom net_wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom net_wm_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
        XChangeProperty(dpy, rofi->win, net_wm_type, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&net_wm_dialog, 1);

        rofi->surface = cairo_xlib_surface_create(dpy, rofi->win,
                                                  DefaultVisual(dpy, screen),
                                                  rofi->width, rofi->height);
    }
}

void rofi_launcher_cleanup(RofiLauncher *rofi) {
    if (!rofi) return;
    for (int i = 0; i < rofi->app_count; i++) {
        if (rofi->apps[i].icon_surface) {
            cairo_surface_finish(rofi->apps[i].icon_surface);
            cairo_surface_destroy(rofi->apps[i].icon_surface);
            rofi->apps[i].icon_surface = NULL;
        }
    }
    if (rofi->surface) {
        cairo_surface_finish(rofi->surface);
        cairo_surface_destroy(rofi->surface);
        rofi->surface = NULL;
    }
}

static void draw_rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void rofi_launcher_render(Display *dpy, RofiLauncher *rofi, int screen_w, int screen_h) {
    (void)screen_w;
    (void)screen_h;
    if (!dpy || !rofi || !rofi->surface) return;

    cairo_t *cr = cairo_create(rofi->surface);
    if (!cr) return;

    // Background: Deep dark #121214 with subtle 1px border #323238
    cairo_set_source_rgb(cr, 0.07, 0.07, 0.08); // #121214
    cairo_paint(cr);

    // Subtle 1px inner border
    cairo_set_source_rgba(cr, 0.22, 0.22, 0.24, 0.9);
    cairo_set_line_width(cr, 1.5);
    draw_rounded_rect(cr, 0.75, 0.75, rofi->width - 1.5, rofi->height - 1.5, 12.0);
    cairo_stroke(cr);

    // --- Search Header Box ---
    double search_x = 16.0;
    double search_y = 14.0;
    double search_w = rofi->width - 32.0;
    double search_h = 40.0;

    cairo_set_source_rgba(cr, 0.14, 0.14, 0.16, 0.95);
    draw_rounded_rect(cr, search_x, search_y, search_w, search_h, 8.0);
    cairo_fill(cr);

    // Search Box 1px outline
    cairo_set_source_rgba(cr, 0.28, 0.28, 0.32, 0.8);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rect(cr, search_x + 0.5, search_y + 0.5, search_w - 1.0, search_h - 1.0, 8.0);
    cairo_stroke(cr);

    // Prompt Indicator: "> " or icon
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 15.0);
    cairo_set_source_rgba(cr, 0.45, 0.65, 0.95, 1.0); // Accent blue
    cairo_move_to(cr, search_x + 14.0, search_y + 25.0);
    cairo_show_text(cr, "> ");

    // Query text or placeholder
    if (rofi->query_len > 0) {
        cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
        cairo_move_to(cr, search_x + 36.0, search_y + 25.0);
        cairo_show_text(cr, rofi->query);

        // Blinking / solid cursor
        cairo_text_extents_t ext;
        cairo_text_extents(cr, rofi->query, &ext);
        cairo_set_source_rgba(cr, 0.45, 0.65, 0.95, 0.9);
        cairo_rectangle(cr, search_x + 38.0 + ext.x_advance, search_y + 11.0, 2.0, 18.0);
        cairo_fill(cr);
    } else {
        cairo_set_source_rgba(cr, 0.45, 0.45, 0.48, 0.8);
        cairo_move_to(cr, search_x + 36.0, search_y + 25.0);
        cairo_show_text(cr, "Type to search applications...");
    }

    // --- Applications List ---
    double list_y = ROFI_HEADER_HEIGHT + 6.0;
    int start_idx = rofi->scroll_offset;
    int end_idx = start_idx + ROFI_VISIBLE_ROWS;
    if (end_idx > rofi->match_count) end_idx = rofi->match_count;

    for (int i = start_idx; i < end_idx; i++) {
        int row = i - start_idx;
        double item_x = 16.0;
        double item_y = list_y + (row * ROFI_ROW_HEIGHT);
        double item_w = rofi->width - 32.0;
        double item_h = ROFI_ROW_HEIGHT - 6.0;
        int is_selected = (i == rofi->selected_idx);

        if (is_selected) {
            // Selected accent pill
            cairo_set_source_rgba(cr, 0.18, 0.32, 0.52, 0.6); // Deep translucent blue
            draw_rounded_rect(cr, item_x, item_y, item_w, item_h, 8.0);
            cairo_fill(cr);

            cairo_set_source_rgba(cr, 0.35, 0.55, 0.85, 0.8); // Crisp outline
            cairo_set_line_width(cr, 1.0);
            draw_rounded_rect(cr, item_x + 0.5, item_y + 0.5, item_w - 1.0, item_h - 1.0, 8.0);
            cairo_stroke(cr);
        }

        int app_idx = rofi->matches[i];
        RofiApp *app = &rofi->apps[app_idx];

        // Ensure icon is loaded
        if (!app->icon_surface && app->icon_name[0] != '\0') {
            app->icon_surface = icon_load_surface(app->icon_name);
        }

        // Draw icon (32x32)
        double icon_x = item_x + 12.0;
        double icon_y = item_y + (item_h - 32.0) / 2.0;

        if (app->icon_surface) {
            cairo_save(cr);
            int surf_w = cairo_image_surface_get_width(app->icon_surface);
            int surf_h = cairo_image_surface_get_height(app->icon_surface);
            double sx = 32.0 / (double)surf_w;
            double sy = 32.0 / (double)surf_h;
            cairo_translate(cr, icon_x, icon_y);
            cairo_scale(cr, sx, sy);
            cairo_set_source_surface(cr, app->icon_surface, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);
        } else {
            // Fallback letter badge
            cairo_set_source_rgba(cr, 0.25, 0.25, 0.28, 0.9);
            draw_rounded_rect(cr, icon_x, icon_y, 32.0, 32.0, 6.0);
            cairo_fill(cr);

            cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 14.0);
            char initial[2] = { (char)toupper((unsigned char)app->name[0]), '\0' };
            cairo_text_extents_t ext;
            cairo_text_extents(cr, initial, &ext);
            cairo_move_to(cr, icon_x + (32.0 - ext.width) / 2.0 - ext.x_bearing,
                              icon_y + (32.0 - ext.height) / 2.0 - ext.y_bearing);
            cairo_show_text(cr, initial);
        }

        // Application Title
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 13.5);
        if (is_selected) {
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        } else {
            cairo_set_source_rgb(cr, 0.88, 0.88, 0.90);
        }
        cairo_move_to(cr, icon_x + 42.0, item_y + 20.0);
        cairo_show_text(cr, app->name);

        // Application Comment / Subtitle
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0);
        cairo_set_source_rgba(cr, 0.55, 0.55, 0.58, 0.9);
        cairo_move_to(cr, icon_x + 42.0, item_y + 36.0);
        const char *sub = app->comment[0] != '\0' ? app->comment : app->exec;
        cairo_show_text(cr, sub);
    }

    cairo_destroy(cr);
    cairo_surface_flush(rofi->surface);
    if (dpy && rofi->win && rofi->is_visible) {
        XRaiseWindow(dpy, rofi->win);
    }
    if (dpy) XFlush(dpy);
}

void rofi_launcher_toggle(Display *dpy, RofiLauncher *rofi) {
    if (!rofi) return;

    if (!rofi->is_visible) {
        rofi->is_visible = 1;
        rofi_launcher_set_query(rofi, "");

        if (dpy && rofi->win) {
            // Smooth ease-out slide up animation: y_start -> y_final
            rofi->anim_start_y = rofi->target_y + 24;
            rofi->y = rofi->anim_start_y;
            XMoveWindow(dpy, rofi->win, rofi->x, rofi->y);
            XMapRaised(dpy, rofi->win);
            XRaiseWindow(dpy, rofi->win);
            XSetInputFocus(dpy, rofi->win, RevertToPointerRoot, CurrentTime);

            // 6-step ease-out cubic animation (~80ms)
            const int frames = 6;
            for (int f = 1; f <= frames; f++) {
                double t = (double)f / (double)frames;
                double ease = 1.0 - pow(1.0 - t, 3.0); // Ease-out cubic
                int cur_y = (int)(rofi->anim_start_y + (rofi->target_y - rofi->anim_start_y) * ease);
                XMoveWindow(dpy, rofi->win, rofi->x, cur_y);
                XRaiseWindow(dpy, rofi->win);
                rofi_launcher_render(dpy, rofi, 0, 0);
                XFlush(dpy);
                usleep(12000); // ~12ms per frame
            }
            rofi->y = rofi->target_y;
            XMoveWindow(dpy, rofi->win, rofi->x, rofi->y);
            XRaiseWindow(dpy, rofi->win);
            rofi_launcher_render(dpy, rofi, 0, 0);
        }
    } else {
        rofi->is_visible = 0;
        if (rofi->is_pressing && dpy) {
            XUngrabPointer(dpy, CurrentTime);
        }
        rofi->is_pressing = 0;
        rofi->is_dragging_out = 0;
        if (dpy && rofi->win) {
            XUnmapWindow(dpy, rofi->win);
            XFlush(dpy);
        }
    }
}

void rofi_launcher_handle_key(Display *dpy, RofiLauncher *rofi, XKeyEvent *ev) {
    if (!rofi || !ev) return;

    KeySym ks = XLookupKeysym(ev, 0);

    if (ks == XK_Escape) {
        rofi_launcher_toggle(dpy, rofi);
        return;
    }

    if (ks == XK_Return || ks == XK_KP_Enter) {
        rofi_launcher_execute_selected(dpy, rofi);
        return;
    }

    if (ks == XK_Down || (ks == XK_n && (ev->state & ControlMask))) {
        rofi_launcher_select_next(rofi);
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        return;
    }

    if (ks == XK_Up || (ks == XK_p && (ev->state & ControlMask))) {
        rofi_launcher_select_prev(rofi);
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        return;
    }

    if (ks == XK_Page_Down) {
        for (int i = 0; i < ROFI_VISIBLE_ROWS; i++) rofi_launcher_select_next(rofi);
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        return;
    }

    if (ks == XK_Page_Up) {
        for (int i = 0; i < ROFI_VISIBLE_ROWS; i++) rofi_launcher_select_prev(rofi);
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        return;
    }

    if (ks == XK_BackSpace) {
        rofi_launcher_query_backspace(rofi);
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        return;
    }

    // Normal typing
    char buf[16];
    int count = XLookupString(ev, buf, sizeof(buf) - 1, NULL, NULL);
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            if (isprint((unsigned char)buf[i])) {
                rofi_launcher_query_append(rofi, buf[i]);
            }
        }
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
    }
}

void rofi_launcher_handle_click(Display *dpy, RofiLauncher *rofi, unsigned int button, int x, int y) {
    if (!rofi) return;

    if (button != Button1) return;

    // Check if clicked outside window bounds
    if (x < 0 || x >= rofi->width || y < 0 || y >= rofi->height) {
        rofi_launcher_toggle(dpy, rofi);
        return;
    }

    // Check if clicked an item row
    double list_y = ROFI_HEADER_HEIGHT + 6.0;
    if (y >= list_y) {
        int clicked_row = (int)((y - list_y) / ROFI_ROW_HEIGHT);
        int target_idx = rofi->scroll_offset + clicked_row;
        if (target_idx >= 0 && target_idx < rofi->match_count) {
            rofi->selected_idx = target_idx;
            rofi_launcher_execute_selected(dpy, rofi);
        }
    }
}

void rofi_launcher_handle_press(Display *dpy, RofiLauncher *rofi, unsigned int button, int root_x, int root_y) {
    if (!rofi || !rofi->is_visible) return;

    if (button != Button1) return;

    int win_x = root_x - rofi->x;
    int win_y = root_y - rofi->y;

    // Check if clicked outside window bounds
    if (win_x < 0 || win_x >= rofi->width || win_y < 0 || win_y >= rofi->height) {
        rofi_launcher_toggle(dpy, rofi);
        return;
    }

    // Check if clicked an item row
    double list_y = ROFI_HEADER_HEIGHT + 6.0;
    if (win_y >= list_y) {
        int clicked_row = (int)((win_y - list_y) / ROFI_ROW_HEIGHT);
        int target_idx = rofi->scroll_offset + clicked_row;
        if (target_idx >= 0 && target_idx < rofi->match_count) {
            rofi->selected_idx = target_idx;
            rofi->is_pressing = 1;
            rofi->is_dragging_out = 0;
            rofi->drag_start_root_x = root_x;
            rofi->drag_start_root_y = root_y;
            rofi->drag_app_idx = rofi->matches[target_idx];

            if (dpy) {
                int screen = DefaultScreen(dpy);
                Window root = RootWindow(dpy, screen);
                XGrabPointer(dpy, root, False,
                             ButtonReleaseMask | PointerMotionMask,
                             GrabModeAsync, GrabModeAsync,
                             None, None, CurrentTime);
                rofi_launcher_render(dpy, rofi, 0, 0);
            }
        }
    }
}

void rofi_launcher_handle_motion(Display *dpy, RofiLauncher *rofi, int x, int y) {
    if (!rofi || !rofi->is_visible) return;

    if (rofi->is_pressing) {
        int root_x = rofi->x + x;
        int root_y = rofi->y + y;
        int dx = root_x - rofi->drag_start_root_x;
        int dy = root_y - rofi->drag_start_root_y;
        if (dx * dx + dy * dy >= 64) {
            rofi->is_dragging_out = 1;
        }
    }

    if (rofi->is_dragging_out) {
        return;
    }

    double list_y = ROFI_HEADER_HEIGHT + 6.0;
    if (y >= list_y) {
        int hovered_row = (int)((y - list_y) / ROFI_ROW_HEIGHT);
        int target_idx = rofi->scroll_offset + hovered_row;
        if (target_idx >= 0 && target_idx < rofi->match_count && target_idx != rofi->selected_idx) {
            rofi->selected_idx = target_idx;
            if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        }
    }
}
