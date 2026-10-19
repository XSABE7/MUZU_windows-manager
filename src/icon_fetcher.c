#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include "icon_fetcher.h"

static const char *search_dirs[] = {
    "/usr/share/pixmaps",
    "/usr/share/icons/hicolor/48x48/apps",
    "/usr/share/icons/hicolor/64x64/apps",
    "/usr/share/icons/hicolor/32x32/apps",
    "/usr/share/icons/hicolor/128x128/apps",
    "/usr/share/icons/hicolor/256x256/apps",
    "/usr/share/icons/gnome/48x48/apps",
    "/usr/share/icons/gnome/32x32/apps",
    "/usr/share/icons/gnome/24x24/apps",
    "/usr/share/icons/gnome/22x22/apps",
    "/usr/share/icons/Adwaita/48x48/apps",
    "/usr/share/icons/Adwaita/32x32/apps",
    NULL
};

int icon_find_png(const char *icon_name, char *out_path, size_t maxlen) {
    if (!icon_name || icon_name[0] == '\0' || !out_path || maxlen == 0) {
        return 0;
    }

    // 1. Direct absolute path check
    if (icon_name[0] == '/') {
        if (access(icon_name, R_OK) == 0 && strstr(icon_name, ".png")) {
            strncpy(out_path, icon_name, maxlen - 1);
            out_path[maxlen - 1] = '\0';
            return 1;
        }

        // Try appending .png if extension was omitted
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s.png", icon_name);
        if (access(tmp, R_OK) == 0) {
            strncpy(out_path, tmp, maxlen - 1);
            out_path[maxlen - 1] = '\0';
            return 1;
        }
        return 0;
    }

    // Strip extension like .png, .svg, .xpm if present
    char clean_name[128];
    strncpy(clean_name, icon_name, sizeof(clean_name) - 1);
    clean_name[sizeof(clean_name) - 1] = '\0';

    char *ext = strrchr(clean_name, '.');
    if (ext && (strcmp(ext, ".png") == 0 || strcmp(ext, ".svg") == 0 || strcmp(ext, ".xpm") == 0)) {
        *ext = '\0';
    }

    // 2. Search standard system directories
    char test_path[512];
    for (int i = 0; search_dirs[i] != NULL; i++) {
        snprintf(test_path, sizeof(test_path), "%s/%s.png", search_dirs[i], clean_name);
        if (access(test_path, R_OK) == 0) {
            strncpy(out_path, test_path, maxlen - 1);
            out_path[maxlen - 1] = '\0';
            return 1;
        }
    }

    // 3. Search user local icon directory (~/.local/share/icons)
    const char *home = getenv("HOME");
    if (home) {
        snprintf(test_path, sizeof(test_path), "%s/.local/share/icons/hicolor/48x48/apps/%s.png", home, clean_name);
        if (access(test_path, R_OK) == 0) {
            strncpy(out_path, test_path, maxlen - 1);
            out_path[maxlen - 1] = '\0';
            return 1;
        }
        snprintf(test_path, sizeof(test_path), "%s/.local/share/icons/%s.png", home, clean_name);
        if (access(test_path, R_OK) == 0) {
            strncpy(out_path, test_path, maxlen - 1);
            out_path[maxlen - 1] = '\0';
            return 1;
        }
    }

    return 0;
}

cairo_surface_t *icon_load_surface(const char *icon_name) {
    char path[512];
    if (!icon_find_png(icon_name, path, sizeof(path))) {
        return NULL;
    }

    cairo_surface_t *surf = cairo_image_surface_create_from_png(path);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surf);
        return NULL;
    }

    return surf;
}

void icon_render(cairo_t *cr, cairo_surface_t *icon, const char *fallback_name, int x, int y, int size) {
    if (!cr || size <= 0) return;

    if (icon) {
        int w = cairo_image_surface_get_width(icon);
        int h = cairo_image_surface_get_height(icon);
        if (w > 0 && h > 0) {
            cairo_save(cr);
            cairo_translate(cr, x, y);
            cairo_scale(cr, (double)size / w, (double)size / h);
            cairo_set_source_surface(cr, icon, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);
            return;
        }
    }

    // Procedural Fallback Icon: Modern dark slate badge with accent letter
    cairo_save(cr);

    // Box background
    cairo_set_source_rgba(cr, 0.20, 0.25, 0.35, 0.90);
    cairo_rectangle(cr, x, y, size, size);
    cairo_fill(cr);

    // Subtle border
    cairo_set_source_rgba(cr, 0.35, 0.45, 0.60, 0.70);
    cairo_set_line_width(cr, 1.2);
    cairo_rectangle(cr, x + 0.5, y + 0.5, size - 1, size - 1);
    cairo_stroke(cr);

    // Initial letter
    char initial = 'A';
    if (fallback_name && fallback_name[0] != '\0') {
        initial = (char)toupper((unsigned char)fallback_name[0]);
    }
    char text[2] = { initial, '\0' };

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, size * 0.55);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);

    cairo_set_source_rgb(cr, 0.92, 0.94, 0.98);
    cairo_move_to(cr, x + (size - ext.width) / 2.0 - ext.x_bearing,
                      y + (size - ext.height) / 2.0 - ext.y_bearing);
    cairo_show_text(cr, text);

    cairo_restore(cr);
}
