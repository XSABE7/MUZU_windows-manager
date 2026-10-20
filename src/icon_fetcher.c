#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <cairo/cairo.h>
#include "icon_fetcher.h"

#define MAX_THEMES 8
static char active_theme_chain[MAX_THEMES][64];
static int active_theme_count = 0;

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

static const char *app_subdirs[] = {
    "apps/48",
    "apps/scalable",
    "apps/64",
    "apps/32",
    "apps/24",
    "apps/22",
    "apps/16",
    "apps/128",
    "apps/256",
    "apps",
    "48x48/apps",
    "scalable/apps",
    "64x64/apps",
    "32x32/apps",
    "24x24/apps",
    "22x22/apps",
    "16x16/apps",
    "128x128/apps",
    "256x256/apps",
    NULL
};

static void add_theme_to_chain(const char *name) {
    if (!name || name[0] == '\0' || active_theme_count >= MAX_THEMES) return;
    for (int i = 0; i < active_theme_count; i++) {
        if (strcmp(active_theme_chain[i], name) == 0) return;
    }
    snprintf(active_theme_chain[active_theme_count], sizeof(active_theme_chain[0]), "%s", name);
    active_theme_count++;
}

static void parse_index_theme(const char *theme_path) {
    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/index.theme", theme_path);
    FILE *f = fopen(index_path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Inherits=", 9) == 0) {
            char *p = line + 9;
            char *comma;
            while ((comma = strchr(p, ',')) != NULL) {
                *comma = '\0';
                // Trim trailing newline/spaces
                char *end = comma - 1;
                while (end >= p && isspace((unsigned char)*end)) *end-- = '\0';
                while (*p && isspace((unsigned char)*p)) p++;
                if (*p) add_theme_to_chain(p);
                p = comma + 1;
            }
            // Last entry
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            while (*p && isspace((unsigned char)*p)) p++;
            size_t l = strlen(p);
            while (l > 0 && isspace((unsigned char)p[l - 1])) p[--l] = '\0';
            if (*p) add_theme_to_chain(p);
            break;
        }
    }
    fclose(f);
}

void icon_theme_set(const char *theme_name) {
    active_theme_count = 0;
    if (!theme_name || theme_name[0] == '\0') {
        theme_name = "hicolor";
    }

    add_theme_to_chain(theme_name);

    // Look for index.theme in standard bases
    const char *test_override = getenv("MUZUWM_TEST_ICON_DIR");
    const char *home = getenv("HOME");
    const char *bases[8];
    int bcount = 0;
    if (test_override && test_override[0]) bases[bcount++] = test_override;
    if (home) {
        bases[bcount++] = "~/.local/share/icons";
        bases[bcount++] = "~/.icons";
    }
    bases[bcount++] = "/usr/share/icons";
    bases[bcount] = NULL;

    // Iteratively resolve index.theme for all themes in active_theme_chain
    for (int idx = 0; idx < active_theme_count && active_theme_count < MAX_THEMES; idx++) {
        const char *cur_theme = active_theme_chain[idx];
        char test_dir[512];
        for (int i = 0; bases[i] != NULL; i++) {
            if (bases[i][0] == '~' && home) {
                snprintf(test_dir, sizeof(test_dir), "%.256s%.128s/%.64s", home, bases[i] + 1, cur_theme);
            } else {
                snprintf(test_dir, sizeof(test_dir), "%.400s/%.64s", bases[i], cur_theme);
            }

            if (access(test_dir, R_OK) == 0) {
                parse_index_theme(test_dir);
                break;
            }
        }
    }

    add_theme_to_chain("hicolor");
    add_theme_to_chain("gnome");
    add_theme_to_chain("Adwaita");
}

int icon_find_png(const char *icon_name, char *out_path, size_t maxlen) {
    if (!icon_name || icon_name[0] == '\0' || !out_path || maxlen == 0) {
        return 0;
    }

    // 1. Direct absolute path check
    if (icon_name[0] == '/') {
        if (access(icon_name, R_OK) == 0 && strstr(icon_name, ".png")) {
            snprintf(out_path, maxlen, "%s", icon_name);
            return 1;
        }

        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s.png", icon_name);
        if (access(tmp, R_OK) == 0) {
            snprintf(out_path, maxlen, "%s", tmp);
            return 1;
        }
        return 0;
    }

    // Strip extension like .png, .svg, .xpm if present
    char clean_name[128];
    snprintf(clean_name, sizeof(clean_name), "%s", icon_name);

    char *ext = strrchr(clean_name, '.');
    if (ext && (strcmp(ext, ".png") == 0 || strcmp(ext, ".svg") == 0 || strcmp(ext, ".xpm") == 0)) {
        *ext = '\0';
    }

    // 2. Search standard system directories
    char test_path[512];
    for (int i = 0; search_dirs[i] != NULL; i++) {
        snprintf(test_path, sizeof(test_path), "%s/%s.png", search_dirs[i], clean_name);
        if (access(test_path, R_OK) == 0) {
            snprintf(out_path, maxlen, "%s", test_path);
            return 1;
        }
    }

    // 3. Search user local icon directory (~/.local/share/icons)
    const char *home = getenv("HOME");
    if (home) {
        snprintf(test_path, sizeof(test_path), "%s/.local/share/icons/hicolor/48x48/apps/%s.png", home, clean_name);
        if (access(test_path, R_OK) == 0) {
            snprintf(out_path, maxlen, "%s", test_path);
            return 1;
        }
        snprintf(test_path, sizeof(test_path), "%s/.local/share/icons/%s.png", home, clean_name);
        if (access(test_path, R_OK) == 0) {
            snprintf(out_path, maxlen, "%s", test_path);
            return 1;
        }
    }

    return 0;
}

int icon_find_themed_png(const char *icon_name, char *out_path, size_t maxlen) {
    if (!icon_name || icon_name[0] == '\0' || !out_path || maxlen == 0) {
        return 0;
    }

    // Direct path check
    if (icon_name[0] == '/') {
        return icon_find_png(icon_name, out_path, maxlen);
    }

    char clean_name[128];
    snprintf(clean_name, sizeof(clean_name), "%s", icon_name);
    char *ext = strrchr(clean_name, '.');
    if (ext && (strcmp(ext, ".png") == 0 || strcmp(ext, ".svg") == 0 || strcmp(ext, ".xpm") == 0)) {
        *ext = '\0';
    }

    if (active_theme_count == 0) {
        icon_theme_set("hicolor");
    }

    const char *test_override = getenv("MUZUWM_TEST_ICON_DIR");
    const char *home = getenv("HOME");
    const char *bases[8];
    int bcount = 0;
    if (test_override && test_override[0]) bases[bcount++] = test_override;
    if (home) {
        bases[bcount++] = "~/.local/share/icons";
        bases[bcount++] = "~/.icons";
    }
    bases[bcount++] = "/usr/share/icons";
    bases[bcount] = NULL;

    char test_path[1024];

    for (int t = 0; t < active_theme_count; t++) {
        const char *theme = active_theme_chain[t];

        for (int b = 0; bases[b] != NULL; b++) {
            char base_theme_dir[400];
            if (bases[b][0] == '~' && home) {
                snprintf(base_theme_dir, sizeof(base_theme_dir), "%.256s%.64s/%.64s", home, bases[b] + 1, theme);
            } else {
                snprintf(base_theme_dir, sizeof(base_theme_dir), "%.256s/%.64s", bases[b], theme);
            }

            if (access(base_theme_dir, R_OK) != 0) continue;

            for (int s = 0; app_subdirs[s] != NULL; s++) {
                snprintf(test_path, sizeof(test_path), "%.400s/%.32s/%.128s.png", base_theme_dir, app_subdirs[s], clean_name);
                if (access(test_path, R_OK) == 0) {
                    snprintf(out_path, maxlen, "%s", test_path);
                    return 1;
                }
            }
        }
    }

    // Fallback search
    return icon_find_png(clean_name, out_path, maxlen);
}

cairo_surface_t *icon_load_surface(const char *icon_name) {
    char path[512];
    if (!icon_find_themed_png(icon_name, path, sizeof(path))) {
        return NULL;
    }

    cairo_surface_t *surf = cairo_image_surface_create_from_png(path);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surf);
        return NULL;
    }

    return surf;
}

cairo_surface_t *icon_create_from_net_wm_icon(const unsigned long *data, unsigned long nitems, int target_size) {
    if (!data || nitems < 2 || target_size <= 0) return NULL;

    unsigned long best_offset = 0;
    int best_w = 0;
    int best_h = 0;
    int best_diff = 1000000;
    unsigned long offset = 0;

    while (offset + 2 <= nitems) {
        unsigned long w = data[offset];
        unsigned long h = data[offset + 1];

        // Sanity check
        if (w == 0 || h == 0 || w > 1024 || h > 1024) break;
        unsigned long icon_len = w * h;
        if (offset + 2 + icon_len > nitems) break;

        int diff = abs((int)w - target_size) + abs((int)h - target_size);
        if (diff < best_diff) {
            best_diff = diff;
            best_offset = offset;
            best_w = (int)w;
            best_h = (int)h;
        }

        offset += 2 + icon_len;
    }

    if (best_w <= 0 || best_h <= 0) return NULL;

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, best_w, best_h);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surf);
        return NULL;
    }

    uint32_t *dst = (uint32_t *)cairo_image_surface_get_data(surf);
    int stride_pixels = cairo_image_surface_get_stride(surf) / 4;
    for (int y = 0; y < best_h; y++) {
        for (int x = 0; x < best_w; x++) {
            uint32_t pixel = (uint32_t)data[best_offset + 2 + y * best_w + x];
            uint32_t a = (pixel >> 24) & 0xff;
            if (a == 0) {
                dst[y * stride_pixels + x] = 0;
            } else if (a == 255) {
                dst[y * stride_pixels + x] = pixel;
            } else {
                uint32_t r = (((pixel >> 16) & 0xff) * a + 127) / 255;
                uint32_t g = (((pixel >> 8) & 0xff) * a + 127) / 255;
                uint32_t b = ((pixel & 0xff) * a + 127) / 255;
                dst[y * stride_pixels + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
    cairo_surface_mark_dirty(surf);
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
