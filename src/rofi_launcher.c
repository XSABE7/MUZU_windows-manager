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

static void get_system_lang_code(char *lang_out, size_t max_len) {
    if (!lang_out || max_len == 0) return;
    lang_out[0] = '\0';
    const char *env = getenv("LC_ALL");
    if (!env || !env[0]) env = getenv("LC_MESSAGES");
    if (!env || !env[0]) env = getenv("LANG");
    if (!env || !env[0] || strcmp(env, "C") == 0 || strcmp(env, "POSIX") == 0) return;

    size_t i = 0;
    while (i + 1 < max_len && env[i] != '\0' && env[i] != '_' && env[i] != '.' && env[i] != '@') {
        lang_out[i] = env[i];
        i++;
    }
    lang_out[i] = '\0';
}

static void parse_desktop_file(const char *path, RofiApp *app) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char lang[16] = {0};
    char name_loc_key[32] = {0};
    char comment_loc_key[32] = {0};
    get_system_lang_code(lang, sizeof(lang));
    if (lang[0] != '\0' && strcmp(lang, "en") != 0) {
        snprintf(name_loc_key, sizeof(name_loc_key), "Name[%s]=", lang);
        snprintf(comment_loc_key, sizeof(comment_loc_key), "Comment[%s]=", lang);
    }
    size_t name_loc_len = strlen(name_loc_key);
    size_t comment_loc_len = strlen(comment_loc_key);

    char line[512];
    int in_desktop_entry = 0;
    int no_display = 0;
    int has_loc_name = 0;
    int has_loc_comment = 0;
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
        } else if (name_loc_len > 0 && strncmp(line, name_loc_key, name_loc_len) == 0 && !has_loc_name) {
            safe_copy(app->name, line + name_loc_len, sizeof(app->name));
            has_loc_name = 1;
        } else if (strncmp(line, "Name=", 5) == 0 && app->name[0] == '\0') {
            safe_copy(app->name, line + 5, sizeof(app->name));
        } else if (strncmp(line, "Exec=", 5) == 0 && app->exec[0] == '\0') {
            safe_copy(app->exec, line + 5, sizeof(app->exec));
        } else if (strncmp(line, "Icon=", 5) == 0 && app->icon_name[0] == '\0') {
            safe_copy(app->icon_name, line + 5, sizeof(app->icon_name));
        } else if (comment_loc_len > 0 && strncmp(line, comment_loc_key, comment_loc_len) == 0 && !has_loc_comment) {
            safe_copy(app->comment, line + comment_loc_len, sizeof(app->comment));
            has_loc_comment = 1;
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

    for (int i = 0; i < rofi->app_count; i++) {
        if (rofi->apps[i].icon_surface) {
            cairo_surface_finish(rofi->apps[i].icon_surface);
            cairo_surface_destroy(rofi->apps[i].icon_surface);
            rofi->apps[i].icon_surface = NULL;
        }
    }
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

void rofi_launcher_scroll(RofiLauncher *rofi, int direction) {
    if (!rofi || rofi->match_count <= 0 || direction == 0) return;

    int max_scroll = rofi->match_count - ROFI_VISIBLE_ROWS;
    if (max_scroll < 0) max_scroll = 0;

    if (direction > 0) {
        if (rofi->scroll_offset < max_scroll) {
            rofi->scroll_offset++;
        }
        if (rofi->selected_idx < rofi->match_count - 1) {
            rofi->selected_idx++;
        }
    } else {
        if (rofi->scroll_offset > 0) {
            rofi->scroll_offset--;
        }
        if (rofi->selected_idx > 0) {
            rofi->selected_idx--;
        }
    }

    if (rofi->selected_idx < rofi->scroll_offset) {
        rofi->selected_idx = rofi->scroll_offset;
    } else if (rofi->selected_idx >= rofi->scroll_offset + ROFI_VISIBLE_ROWS) {
        rofi->selected_idx = rofi->scroll_offset + ROFI_VISIBLE_ROWS - 1;
        if (rofi->selected_idx >= rofi->match_count) {
            rofi->selected_idx = rofi->match_count - 1;
        }
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

static int rofi_parse_hex_color(const char *val, double *r, double *g, double *b) {
    if (!val) return 0;
    const char *hash = strchr(val, '#');
    if (!hash) return 0;
    hash++; // Skip '#'
    unsigned int hex = 0;
    int len = 0;
    while (isxdigit((unsigned char)hash[len])) len++;
    if (len >= 6) {
        if (sscanf(hash, "%6x", &hex) == 1) {
            *r = ((hex >> 16) & 0xFF) / 255.0;
            *g = ((hex >> 8) & 0xFF) / 255.0;
            *b = (hex & 0xFF) / 255.0;
            return 1;
        }
    } else if (len >= 3) {
        if (sscanf(hash, "%3x", &hex) == 1) {
            unsigned int r_int = (hex >> 8) & 0xF;
            unsigned int g_int = (hex >> 4) & 0xF;
            unsigned int b_int = hex & 0xF;
            *r = (r_int | (r_int << 4)) / 255.0;
            *g = (g_int | (g_int << 4)) / 255.0;
            *b = (b_int | (b_int << 4)) / 255.0;
            return 1;
        }
    }
    return 0;
}

static void parse_rasi_file(RofiLauncher *rofi, const char *path, const char *base_dir, int depth) {
    if (!rofi || !path || depth > 5) return;
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // Strip comments // or /*
        char *c_slash = strstr(line, "//");
        if (c_slash) *c_slash = '\0';
        char *c_block = strstr(line, "/*");
        if (c_block) *c_block = '\0';
        line[strcspn(line, "\r\n")] = '\0';

        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;

        // Check @import "filename"
        if (strncmp(p, "@import", 7) == 0) {
            char *quote_start = strpbrk(p + 7, "'\"");
            if (quote_start) {
                char q = *quote_start;
                char *quote_end = strchr(quote_start + 1, q);
                if (quote_end) {
                    *quote_end = '\0';
                    char imp_name[256];
                    safe_copy(imp_name, quote_start + 1, sizeof(imp_name));
                    if (!strchr(imp_name, '.')) {
                        strncat(imp_name, ".rasi", sizeof(imp_name) - strlen(imp_name) - 1);
                    }
                    char resolved[512];
                    if (imp_name[0] == '/' || imp_name[0] == '~') {
                        if (imp_name[0] == '~') {
                            const char *home = getenv("HOME");
                            snprintf(resolved, sizeof(resolved), "%s%s", home ? home : "", imp_name + 1);
                        } else {
                            safe_copy(resolved, imp_name, sizeof(resolved));
                        }
                    } else if (base_dir && base_dir[0] != '\0') {
                        snprintf(resolved, sizeof(resolved), "%s/%s", base_dir, imp_name);
                    } else {
                        safe_copy(resolved, imp_name, sizeof(resolved));
                    }
                    parse_rasi_file(rofi, resolved, base_dir, depth + 1);
                }
            }
            continue;
        }

        // Look for property : value
        char *colon = strchr(p, ':');
        if (!colon) continue;
        *colon = '\0';
        char *key = p;
        char *val = colon + 1;

        // Trim key
        while (*key && isspace((unsigned char)*key)) key++;
        char *end_k = key + strlen(key) - 1;
        while (end_k > key && isspace((unsigned char)*end_k)) *end_k-- = '\0';

        // Trim val
        while (*val && isspace((unsigned char)*val)) val++;
        char *end_v = val + strlen(val) - 1;
        while (end_v > val && (*end_v == ';' || isspace((unsigned char)*end_v))) *end_v-- = '\0';

        if (strcmp(key, "bg") == 0 || strcmp(key, "background") == 0 || strcmp(key, "background-color") == 0) {
            if (rofi_parse_hex_color(val, &rofi->bg_r, &rofi->bg_g, &rofi->bg_b)) {
                rofi->has_custom_colors = 1;
            }
        } else if (strcmp(key, "fg") == 0 || strcmp(key, "foreground") == 0 || strcmp(key, "text-color") == 0) {
            if (rofi_parse_hex_color(val, &rofi->fg_r, &rofi->fg_g, &rofi->fg_b)) {
                rofi->has_custom_colors = 1;
            }
        } else if (strcmp(key, "accent") == 0 || strcmp(key, "primary") == 0 || strcmp(key, "highlight") == 0 || strcmp(key, "border-color") == 0) {
            if (rofi_parse_hex_color(val, &rofi->accent_r, &rofi->accent_g, &rofi->accent_b)) {
                rofi->has_custom_colors = 1;
            }
        } else if (strcmp(key, "font") == 0) {
            char *q_start = strpbrk(val, "'\"");
            char font_buf[128];
            if (q_start) {
                char q = *q_start;
                char *q_end = strchr(q_start + 1, q);
                if (q_end) *q_end = '\0';
                safe_copy(font_buf, q_start + 1, sizeof(font_buf));
            } else {
                safe_copy(font_buf, val, sizeof(font_buf));
            }
            char *last_space = strrchr(font_buf, ' ');
            if (last_space && isdigit((unsigned char)*(last_space + 1))) {
                *last_space = '\0';
            }
            safe_copy(rofi->font_name, font_buf, sizeof(rofi->font_name));
        } else if (strcmp(key, "display-drun") == 0 || strcmp(key, "prompt") == 0) {
            char *q_start = strpbrk(val, "'\"");
            if (q_start) {
                char q = *q_start;
                char *q_end = strchr(q_start + 1, q);
                if (q_end) *q_end = '\0';
                safe_copy(rofi->prompt_text, q_start + 1, sizeof(rofi->prompt_text));
            } else {
                safe_copy(rofi->prompt_text, val, sizeof(rofi->prompt_text));
            }
        }
    }
    fclose(f);
}

const char *rofi_launcher_get_config_path(const RofiLauncher *rofi) {
    if (rofi && rofi->config_path[0] != '\0') return rofi->config_path;

    static char found_path[512] = {0};
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    const char *candidates[] = {
        "%s/.config/muzuwm/config.rasi",
        "%s/.config/muzuwm/rofi.rasi",
        "%s/.config/rofi/config.rasi",
        "%s/.config/i3/rofi/config.rasi",
        "/etc/rofi/config.rasi"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        char path[512];
        if (candidates[i][0] == '%') {
            snprintf(path, sizeof(path), candidates[i], home);
        } else {
            safe_copy(path, candidates[i], sizeof(path));
        }
        if (access(path, R_OK) == 0) {
            safe_copy(found_path, path, sizeof(found_path));
            return found_path;
        }
    }

    snprintf(found_path, sizeof(found_path), "%s/.config/muzuwm/config.rasi", home);
    return found_path;
}

void rofi_launcher_load_config(RofiLauncher *rofi) {
    if (!rofi) return;
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    const char *cfg_path = rofi_launcher_get_config_path(rofi);
    if (cfg_path && access(cfg_path, R_OK) == 0) {
        safe_copy(rofi->config_path, cfg_path, sizeof(rofi->config_path));
        char base_dir[512];
        safe_copy(base_dir, cfg_path, sizeof(base_dir));
        char *slash = strrchr(base_dir, '/');
        if (slash) *slash = '\0';
        else base_dir[0] = '\0';

        parse_rasi_file(rofi, cfg_path, base_dir, 0);
    }

    // Supplementary check for colors.rasi if needed
    if (!rofi->has_custom_colors) {
        const char *color_candidates[] = {
            "%s/.config/muzuwm/colors.rasi",
            "%s/.config/rofi/colors.rasi",
            "%s/.config/i3/rofi/colors.rasi"
        };
        for (size_t i = 0; i < sizeof(color_candidates) / sizeof(color_candidates[0]); i++) {
            char cpath[512];
            snprintf(cpath, sizeof(cpath), color_candidates[i], home);
            if (access(cpath, R_OK) == 0) {
                char base_dir[512];
                safe_copy(base_dir, cpath, sizeof(base_dir));
                char *slash = strrchr(base_dir, '/');
                if (slash) *slash = '\0';
                else base_dir[0] = '\0';
                parse_rasi_file(rofi, cpath, base_dir, 0);
                if (rofi->has_custom_colors) break;
            }
        }
    }

    const char *ext_env = getenv("MUZU_EXTERNAL_ROFI");
    if (ext_env && (strcmp(ext_env, "1") == 0 || strcmp(ext_env, "true") == 0)) {
        rofi->use_external_rofi = 1;
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

    rofi_launcher_load_config(rofi);
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

    const char *font_fam = rofi->font_name[0] ? rofi->font_name : "Sans";
    const char *prompt_str = rofi->prompt_text[0] ? rofi->prompt_text : "> ";

    // Background: Deep dark or custom from rasi
    if (rofi->has_custom_colors) {
        cairo_set_source_rgb(cr, rofi->bg_r, rofi->bg_g, rofi->bg_b);
        cairo_paint(cr);

        cairo_set_source_rgba(cr, rofi->accent_r, rofi->accent_g, rofi->accent_b, 0.45);
        cairo_set_line_width(cr, 1.5);
        draw_rounded_rect(cr, 0.75, 0.75, rofi->width - 1.5, rofi->height - 1.5, 12.0);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgb(cr, 0.07, 0.07, 0.08); // #121214
        cairo_paint(cr);

        // Subtle 1px inner border
        cairo_set_source_rgba(cr, 0.22, 0.22, 0.24, 0.9);
        cairo_set_line_width(cr, 1.5);
        draw_rounded_rect(cr, 0.75, 0.75, rofi->width - 1.5, rofi->height - 1.5, 12.0);
        cairo_stroke(cr);
    }

    // --- Search Header Box ---
    double search_x = 16.0;
    double search_y = 14.0;
    double search_w = rofi->width - 32.0;
    double search_h = 40.0;

    if (rofi->has_custom_colors) {
        double sb_r = rofi->bg_r + 0.06; if (sb_r > 1.0) sb_r = 1.0;
        double sb_g = rofi->bg_g + 0.06; if (sb_g > 1.0) sb_g = 1.0;
        double sb_b = rofi->bg_b + 0.06; if (sb_b > 1.0) sb_b = 1.0;
        cairo_set_source_rgba(cr, sb_r, sb_g, sb_b, 0.95);
        draw_rounded_rect(cr, search_x, search_y, search_w, search_h, 8.0);
        cairo_fill(cr);

        cairo_set_source_rgba(cr, rofi->accent_r, rofi->accent_g, rofi->accent_b, 0.35);
        cairo_set_line_width(cr, 1.0);
        draw_rounded_rect(cr, search_x + 0.5, search_y + 0.5, search_w - 1.0, search_h - 1.0, 8.0);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgba(cr, 0.14, 0.14, 0.16, 0.95);
        draw_rounded_rect(cr, search_x, search_y, search_w, search_h, 8.0);
        cairo_fill(cr);

        // Search Box 1px outline
        cairo_set_source_rgba(cr, 0.28, 0.28, 0.32, 0.8);
        cairo_set_line_width(cr, 1.0);
        draw_rounded_rect(cr, search_x + 0.5, search_y + 0.5, search_w - 1.0, search_h - 1.0, 8.0);
        cairo_stroke(cr);
    }

    // Prompt Indicator: "> " or custom prompt
    cairo_select_font_face(cr, font_fam, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 15.0);
    if (rofi->has_custom_colors) {
        cairo_set_source_rgba(cr, rofi->accent_r, rofi->accent_g, rofi->accent_b, 1.0);
    } else {
        cairo_set_source_rgba(cr, 0.45, 0.65, 0.95, 1.0); // Accent blue
    }
    cairo_text_extents_t pext;
    cairo_text_extents(cr, prompt_str, &pext);
    cairo_move_to(cr, search_x + 14.0, search_y + 25.0);
    cairo_show_text(cr, prompt_str);

    double query_start_x = search_x + 18.0 + pext.x_advance;

    // Query text or placeholder
    if (rofi->query_len > 0) {
        if (rofi->has_custom_colors) {
            cairo_set_source_rgb(cr, rofi->fg_r, rofi->fg_g, rofi->fg_b);
        } else {
            cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
        }
        cairo_move_to(cr, query_start_x, search_y + 25.0);
        cairo_show_text(cr, rofi->query);

        // Blinking / solid cursor
        cairo_text_extents_t ext;
        cairo_text_extents(cr, rofi->query, &ext);
        if (rofi->has_custom_colors) {
            cairo_set_source_rgba(cr, rofi->accent_r, rofi->accent_g, rofi->accent_b, 0.9);
        } else {
            cairo_set_source_rgba(cr, 0.45, 0.65, 0.95, 0.9);
        }
        cairo_rectangle(cr, query_start_x + 2.0 + ext.x_advance, search_y + 11.0, 2.0, 18.0);
        cairo_fill(cr);
    } else {
        cairo_set_source_rgba(cr, 0.45, 0.45, 0.48, 0.8);
        cairo_move_to(cr, query_start_x, search_y + 25.0);
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
            if (rofi->has_custom_colors) {
                cairo_set_source_rgba(cr, rofi->accent_r, rofi->accent_g, rofi->accent_b, 0.28);
                draw_rounded_rect(cr, item_x, item_y, item_w, item_h, 8.0);
                cairo_fill(cr);

                cairo_set_source_rgba(cr, rofi->accent_r, rofi->accent_g, rofi->accent_b, 0.85);
                cairo_set_line_width(cr, 1.0);
                draw_rounded_rect(cr, item_x + 0.5, item_y + 0.5, item_w - 1.0, item_h - 1.0, 8.0);
                cairo_stroke(cr);
            } else {
                cairo_set_source_rgba(cr, 0.18, 0.32, 0.52, 0.6); // Deep translucent blue
                draw_rounded_rect(cr, item_x, item_y, item_w, item_h, 8.0);
                cairo_fill(cr);

                cairo_set_source_rgba(cr, 0.35, 0.55, 0.85, 0.8); // Crisp outline
                cairo_set_line_width(cr, 1.0);
                draw_rounded_rect(cr, item_x + 0.5, item_y + 0.5, item_w - 1.0, item_h - 1.0, 8.0);
                cairo_stroke(cr);
            }
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
            cairo_select_font_face(cr, font_fam, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 14.0);
            char initial[2] = { (char)toupper((unsigned char)app->name[0]), '\0' };
            cairo_text_extents_t ext;
            cairo_text_extents(cr, initial, &ext);
            cairo_move_to(cr, icon_x + (32.0 - ext.width) / 2.0 - ext.x_bearing,
                              icon_y + (32.0 - ext.height) / 2.0 - ext.y_bearing);
            cairo_show_text(cr, initial);
        }

        // Application Title
        cairo_select_font_face(cr, font_fam, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 13.5);
        if (is_selected) {
            if (rofi->has_custom_colors) {
                cairo_set_source_rgb(cr, rofi->fg_r, rofi->fg_g, rofi->fg_b);
            } else {
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            }
        } else {
            if (rofi->has_custom_colors) {
                cairo_set_source_rgba(cr, rofi->fg_r, rofi->fg_g, rofi->fg_b, 0.88);
            } else {
                cairo_set_source_rgb(cr, 0.88, 0.88, 0.90);
            }
        }
        cairo_move_to(cr, icon_x + 42.0, item_y + 20.0);
        cairo_show_text(cr, app->name);

        // Application Comment / Subtitle
        cairo_select_font_face(cr, font_fam, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0);
        if (rofi->has_custom_colors) {
            cairo_set_source_rgba(cr, rofi->fg_r, rofi->fg_g, rofi->fg_b, 0.55);
        } else {
            cairo_set_source_rgba(cr, 0.55, 0.55, 0.58, 0.9);
        }
        cairo_move_to(cr, icon_x + 42.0, item_y + 36.0);
        const char *sub = app->comment[0] != '\0' ? app->comment : app->exec;
        cairo_show_text(cr, sub);
    }

    // Subtle scrollbar indicator when list exceeds visible rows
    if (rofi->match_count > ROFI_VISIBLE_ROWS) {
        double track_h = (double)(ROFI_VISIBLE_ROWS * ROFI_ROW_HEIGHT) - 8.0;
        double thumb_h = track_h * ((double)ROFI_VISIBLE_ROWS / (double)rofi->match_count);
        if (thumb_h < 24.0) thumb_h = 24.0;
        double max_scroll = (double)(rofi->match_count - ROFI_VISIBLE_ROWS);
        double scroll_ratio = max_scroll > 0.0 ? ((double)rofi->scroll_offset / max_scroll) : 0.0;
        if (scroll_ratio < 0.0) scroll_ratio = 0.0;
        if (scroll_ratio > 1.0) scroll_ratio = 1.0;
        double thumb_y = list_y + scroll_ratio * (track_h - thumb_h);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.22);
        draw_rounded_rect(cr, rofi->width - 8.0, thumb_y, 4.0, thumb_h, 2.0);
        cairo_fill(cr);
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
        if (rofi->use_external_rofi) {
            pid_t pid = fork();
            if (pid == 0) {
                setsid();
                if (rofi->config_path[0] != '\0') {
                    execlp("rofi", "rofi", "-show", "drun", "-config", rofi->config_path, NULL);
                } else {
                    execlp("rofi", "rofi", "-show", "drun", NULL);
                }
                _exit(1);
            }
            return;
        }

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

    if (button == Button4) {
        rofi_launcher_scroll(rofi, -1);
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        return;
    }
    if (button == Button5) {
        rofi_launcher_scroll(rofi, 1);
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        return;
    }

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

    if (button == Button4) {
        rofi_launcher_scroll(rofi, -1);
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        return;
    }
    if (button == Button5) {
        rofi_launcher_scroll(rofi, 1);
        if (dpy) rofi_launcher_render(dpy, rofi, 0, 0);
        return;
    }

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
