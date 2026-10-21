#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "wallpaper.h"

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

static void expand_tilde(const char *in, char *out, size_t max_len) {
    if (!in || !out || max_len == 0) return;
    if (in[0] == '~') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(out, max_len, "%s%s", home, in + 1);
            return;
        }
    }
    safe_copy(out, in, max_len);
}

static int parse_fehbg(const char *home, char *path_out, size_t max_len) {
    char fehbg[512];
    snprintf(fehbg, sizeof(fehbg), "%s/.fehbg", home);
    FILE *f = fopen(fehbg, "r");
    if (!f) return 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "feh ") != NULL || strstr(line, "--bg-") != NULL) {
            char *quote_start = strpbrk(line, "'\"");
            if (quote_start) {
                char quote_char = *quote_start;
                char *quote_end = strchr(quote_start + 1, quote_char);
                if (quote_end) {
                    *quote_end = '\0';
                    char expanded[512];
                    expand_tilde(quote_start + 1, expanded, sizeof(expanded));
                    if (expanded[0] != '\0' && access(expanded, R_OK) == 0) {
                        fclose(f);
                        safe_copy(path_out, expanded, max_len);
                        return 1;
                    }
                }
            }
        }
    }
    fclose(f);
    return 0;
}

static int parse_nitrogen(const char *home, char *path_out, size_t max_len) {
    char nit_cfg[512];
    snprintf(nit_cfg, sizeof(nit_cfg), "%s/.config/nitrogen/bg-saved.cfg", home);
    FILE *f = fopen(nit_cfg, "r");
    if (!f) return 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (strncmp(p, "file=", 5) == 0) {
            char *val = p + 5;
            line[strcspn(line, "\r\n")] = '\0';
            char expanded[512];
            expand_tilde(val, expanded, sizeof(expanded));
            if (expanded[0] != '\0' && access(expanded, R_OK) == 0) {
                fclose(f);
                safe_copy(path_out, expanded, max_len);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

int wallpaper_get_configured_path(char *path_out, size_t max_len) {
    if (!path_out || max_len == 0) return 0;
    path_out[0] = '\0';

    const char *home = getenv("HOME");
    if (!home) return 0;

    // 1. Check MUZUwm custom wallpaper: ~/.config/muzuwm/wallpaper.png, .jpg, .jpeg
    const char *exts[] = { ".png", ".jpg", ".jpeg" };
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        char muzuwm_wall[512];
        snprintf(muzuwm_wall, sizeof(muzuwm_wall), "%s/.config/muzuwm/wallpaper%s", home, exts[i]);
        if (access(muzuwm_wall, R_OK) == 0) {
            safe_copy(path_out, muzuwm_wall, max_len);
            return 1;
        }
    }

    // 2. Check Waypaper configuration: ~/.config/waypaper/config.ini
    char waypaper_cfg[512];
    snprintf(waypaper_cfg, sizeof(waypaper_cfg), "%s/.config/waypaper/config.ini", home);

    FILE *f = fopen(waypaper_cfg, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            char *p = line;
            while (*p && isspace((unsigned char)*p)) p++;

            if (strncmp(p, "wallpaper", 9) == 0) {
                p += 9;
                while (*p && (isspace((unsigned char)*p) || *p == '=')) p++;

                // Strip trailing newline / whitespace / quotes
                char val[512];
                safe_copy(val, p, sizeof(val));
                size_t len = strlen(val);
                while (len > 0 && (isspace((unsigned char)val[len - 1]) || val[len - 1] == '"' || val[len - 1] == '\'')) {
                    val[--len] = '\0';
                }
                char *start = val;
                if (*start == '"' || *start == '\'') start++;

                char expanded[512];
                expand_tilde(start, expanded, sizeof(expanded));

                if (expanded[0] != '\0' && access(expanded, R_OK) == 0) {
                    fclose(f);
                    safe_copy(path_out, expanded, max_len);
                    return 1;
                }
            }
        }
        fclose(f);
    }

    // 3. Check upstream feh configuration: ~/.fehbg
    if (parse_fehbg(home, path_out, max_len)) {
        return 1;
    }

    // 4. Check upstream nitrogen configuration: ~/.config/nitrogen/bg-saved.cfg
    if (parse_nitrogen(home, path_out, max_len)) {
        return 1;
    }

    // 5. Check standard user wallpaper fallbacks: ~/.wallpaper.* or ~/.config/wallpaper.*
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        char fallback_wall[512];
        snprintf(fallback_wall, sizeof(fallback_wall), "%s/.wallpaper%s", home, exts[i]);
        if (access(fallback_wall, R_OK) == 0) {
            safe_copy(path_out, fallback_wall, max_len);
            return 1;
        }
        snprintf(fallback_wall, sizeof(fallback_wall), "%s/.config/wallpaper%s", home, exts[i]);
        if (access(fallback_wall, R_OK) == 0) {
            safe_copy(path_out, fallback_wall, max_len);
            return 1;
        }
    }

    return 0;
}

int wallpaper_set_root(Display *dpy, int screen_w, int screen_h, const char *image_path) {
    if (!dpy) return 1; // Success mock for testing

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    Pixmap pmap = XCreatePixmap(dpy, root, screen_w, screen_h, DefaultDepth(dpy, screen));
    if (pmap == None) return 0;

    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, pmap, DefaultVisual(dpy, screen), screen_w, screen_h);
    if (!surf) {
        XFreePixmap(dpy, pmap);
        return 0;
    }

    cairo_t *cr = cairo_create(surf);
    if (!cr) {
        cairo_surface_destroy(surf);
        XFreePixmap(dpy, pmap);
        return 0;
    }

    int image_rendered = 0;
    if (image_path && image_path[0] != '\0') {
        int img_w = 0, img_h = 0, channels = 0;
        unsigned char *pixels = stbi_load(image_path, &img_w, &img_h, &channels, 4);
        if (pixels && img_w > 0 && img_h > 0) {
            cairo_surface_t *img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, img_w, img_h);
            if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
                uint32_t *dst = (uint32_t *)cairo_image_surface_get_data(img);
                int stride = cairo_image_surface_get_stride(img) / 4;
                for (int y = 0; y < img_h; y++) {
                    for (int x = 0; x < img_w; x++) {
                        int idx = (y * img_w + x) * 4;
                        uint32_t r = pixels[idx + 0];
                        uint32_t g = pixels[idx + 1];
                        uint32_t b = pixels[idx + 2];
                        uint32_t a = pixels[idx + 3];
                        dst[y * stride + x] = (a << 24) | (r << 16) | (g << 8) | b;
                    }
                }
                cairo_surface_mark_dirty(img);

                double sx = (double)screen_w / (double)img_w;
                double sy = (double)screen_h / (double)img_h;
                double scale = (sx > sy) ? sx : sy; // Aspect fill
                double dw = (double)img_w * scale;
                double dh = (double)img_h * scale;
                double dx = ((double)screen_w - dw) / 2.0;
                double dy = ((double)screen_h - dh) / 2.0;

                cairo_save(cr);
                cairo_translate(cr, dx, dy);
                cairo_scale(cr, scale, scale);
                cairo_set_source_surface(cr, img, 0, 0);
                cairo_paint(cr);
                cairo_restore(cr);
                image_rendered = 1;
            }
            cairo_surface_destroy(img);
            stbi_image_free(pixels);
        }
    }

    if (!image_rendered) {
        // Fallback: Sleek deep dark gradient matching dark mode (#0e0f14 to #181922)
        cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, screen_w, screen_h);
        cairo_pattern_add_color_stop_rgb(pat, 0.0, 0.055, 0.059, 0.078);
        cairo_pattern_add_color_stop_rgb(pat, 1.0, 0.094, 0.098, 0.133);
        cairo_set_source(cr, pat);
        cairo_paint(cr);
        cairo_pattern_destroy(pat);
    }

    cairo_destroy(cr);
    cairo_surface_finish(surf);
    cairo_surface_destroy(surf);

    // Set _XROOTPMAP_ID and ESETROOT_PMAP_ID atoms on RootWindow
    Atom prop_root = XInternAtom(dpy, "_XROOTPMAP_ID", False);
    Atom prop_esetroot = XInternAtom(dpy, "ESETROOT_PMAP_ID", False);

    // Free previous root pixmap to avoid X server VRAM accumulation on wallpaper reloads
    Atom actual_type;
    int actual_format;
    unsigned long nitems = 0, bytes_after = 0;
    unsigned char *prop_data = NULL;
    if (XGetWindowProperty(dpy, root, prop_root, 0, 1, False, XA_PIXMAP,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop_data) == Success) {
        if (prop_data && actual_type == XA_PIXMAP && actual_format == 32 && nitems == 1) {
            Pixmap old_pmap = *(Pixmap *)prop_data;
            if (old_pmap != None && old_pmap != pmap) {
                XKillClient(dpy, old_pmap);
            }
        }
        if (prop_data) XFree(prop_data);
    }

    XChangeProperty(dpy, root, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pmap, 1);
    XChangeProperty(dpy, root, prop_esetroot, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&pmap, 1);

    // Apply to root window background
    XSetWindowBackgroundPixmap(dpy, root, pmap);
    XClearWindow(dpy, root);
    XFlush(dpy);

    return 1;
}

void wallpaper_init(Display *dpy, int screen_w, int screen_h) {
    char path[512] = {0};
    int found = wallpaper_get_configured_path(path, sizeof(path));
    wallpaper_set_root(dpy, screen_w, screen_h, found ? path : NULL);
}
