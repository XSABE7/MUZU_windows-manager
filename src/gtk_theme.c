#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>
#include "gtk_theme.h"

static void trim(char *s) {
    if (!s) return;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') || (s[0] == '\'' && s[len - 1] == '\''))) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static void update_palette(GtkTheme *theme) {
    if (theme->is_dark) {
        theme->bg_titlebar = (ColorRGBA){0.12, 0.14, 0.18, 1.0};
        theme->fg_titlebar = (ColorRGBA){0.92, 0.94, 0.98, 1.0};
        theme->accent_color = (ColorRGBA){0.20, 0.55, 0.95, 1.0};
        theme->bg_surface = (ColorRGBA){0.07, 0.09, 0.13, 0.96};
        theme->border_color = (ColorRGBA){0.25, 0.30, 0.40, 0.80};
    } else {
        theme->bg_titlebar = (ColorRGBA){0.93, 0.94, 0.96, 1.0};
        theme->fg_titlebar = (ColorRGBA){0.12, 0.14, 0.18, 1.0};
        theme->accent_color = (ColorRGBA){0.15, 0.50, 0.90, 1.0};
        theme->bg_surface = (ColorRGBA){0.95, 0.96, 0.98, 0.96};
        theme->border_color = (ColorRGBA){0.75, 0.78, 0.84, 0.80};
    }
}

static void parse_font_string(const char *font_str, char *out_family, size_t max_family, double *out_size) {
    if (!font_str || !out_family || max_family == 0 || !out_size) return;

    char buf[64];
    size_t len = strlen(font_str);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, font_str, len);
    buf[len] = '\0';
    trim(buf);

    char *last_space = strrchr(buf, ' ');
    if (last_space) {
        char *endptr;
        double size = strtod(last_space + 1, &endptr);
        if (endptr != last_space + 1 && size > 4.0 && size < 72.0) {
            *out_size = size;
            *last_space = '\0';
            trim(buf);
            snprintf(out_family, max_family, "%s", buf);
            return;
        }
    }

    snprintf(out_family, max_family, "%s", buf);
}

void gtk_theme_load_from_file(GtkTheme *theme, const char *filepath) {
    if (!theme || !filepath) return;

    FILE *f = fopen(filepath, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "gtk-theme-name") == 0 && strlen(val) > 0) {
            strncpy(theme->theme_name, val, sizeof(theme->theme_name) - 1);
            theme->theme_name[sizeof(theme->theme_name) - 1] = '\0';
            if (strcasestr(val, "dark")) {
                theme->is_dark = 1;
            } else if (strcasestr(val, "light")) {
                theme->is_dark = 0;
            }
        } else if (strcmp(key, "gtk-icon-theme-name") == 0 && strlen(val) > 0) {
            strncpy(theme->icon_theme_name, val, sizeof(theme->icon_theme_name) - 1);
            theme->icon_theme_name[sizeof(theme->icon_theme_name) - 1] = '\0';
        } else if (strcmp(key, "gtk-cursor-theme-name") == 0 && strlen(val) > 0) {
            strncpy(theme->cursor_theme_name, val, sizeof(theme->cursor_theme_name) - 1);
            theme->cursor_theme_name[sizeof(theme->cursor_theme_name) - 1] = '\0';
        } else if (strcmp(key, "gtk-cursor-theme-size") == 0 && strlen(val) > 0) {
            int sz = atoi(val);
            if (sz > 0) theme->cursor_theme_size = sz;
        } else if (strcmp(key, "gtk-font-name") == 0 && strlen(val) > 0) {
            parse_font_string(val, theme->font_family, sizeof(theme->font_family), &theme->font_size);
        } else if (strcmp(key, "gtk-application-prefer-dark-theme") == 0) {
            theme->is_dark = (atoi(val) == 1 || strcasecmp(val, "true") == 0);
        }
    }
    fclose(f);

    update_palette(theme);
}

void gtk_theme_init(GtkTheme *theme) {
    if (!theme) return;
    memset(theme, 0, sizeof(*theme));

    // Sensible modern defaults
    strncpy(theme->theme_name, "Adwaita-dark", sizeof(theme->theme_name) - 1);
    strncpy(theme->icon_theme_name, "hicolor", sizeof(theme->icon_theme_name) - 1);
    strncpy(theme->cursor_theme_name, "Adwaita", sizeof(theme->cursor_theme_name) - 1);
    theme->cursor_theme_size = 24;
    strncpy(theme->font_family, "Sans", sizeof(theme->font_family) - 1);
    theme->font_size = 11.0;
    theme->is_dark = 1;

    // Auto-detect config from standard locations
    const char *home = getenv("HOME");
    if (home) {
        char path[512];
        snprintf(path, sizeof(path), "%s/.gtkrc-2.0", home);
        if (access(path, R_OK) == 0) {
            gtk_theme_load_from_file(theme, path);
        }

        snprintf(path, sizeof(path), "%s/.config/gtk-3.0/settings.ini", home);
        if (access(path, R_OK) == 0) {
            gtk_theme_load_from_file(theme, path);
        }

        snprintf(path, sizeof(path), "%s/.config/gtk-4.0/settings.ini", home);
        if (access(path, R_OK) == 0) {
            gtk_theme_load_from_file(theme, path);
        }
    }

    update_palette(theme);
}

void gtk_theme_export_env(const GtkTheme *theme) {
    if (!theme) return;
    if (strlen(theme->theme_name) > 0) {
        setenv("GTK_THEME", theme->theme_name, 1);
    }
    if (strlen(theme->cursor_theme_name) > 0) {
        setenv("XCURSOR_THEME", theme->cursor_theme_name, 1);
    }
    if (theme->cursor_theme_size > 0) {
        char sz_buf[16];
        snprintf(sz_buf, sizeof(sz_buf), "%d", theme->cursor_theme_size);
        setenv("XCURSOR_SIZE", sz_buf, 1);
    }
}

void gtk_theme_apply_cursor(Display *dpy, Window root, const GtkTheme *theme) {
    if (!dpy || root == None || !theme) return;

    if (strlen(theme->cursor_theme_name) > 0) {
        XcursorSetTheme(dpy, theme->cursor_theme_name);
    }
    int size = theme->cursor_theme_size > 0 ? theme->cursor_theme_size : 24;
    XcursorSetDefaultSize(dpy, size);

    Cursor cur = XcursorLibraryLoadCursor(dpy, "left_ptr");
    if (cur == None) {
        cur = XCreateFontCursor(dpy, XC_left_ptr);
    }
    if (cur != None) {
        XDefineCursor(dpy, root, cur);
        XFreeCursor(dpy, cur);
        XFlush(dpy);
    }
}
