#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <X11/Xatom.h>
#include "dock.h"
#include "wm.h"
#include "icon_fetcher.h"
#include "rofi_launcher.h"
#include "client_props.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

static void draw_rounded_rectangle(cairo_t *cr, double x, double y, double w, double h, double r) {
    if (r * 2.0 > w) r = w / 2.0;
    if (r * 2.0 > h) r = h / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

static int dock_get_base_icon_size(const Dock *dock) {
    if (!dock) return 32;
    if (dock->size <= 36) return 24;
    if (dock->size >= 64) return 40;
    return 32;
}

static int is_apps_item(const DockItem *it) {
    if (!it) return 0;
    return (strcmp(it->exec_cmd, "__ROFI__") == 0 || strcmp(it->exec_cmd, "__LAUNCHPAD__") == 0);
}

static int is_trash_item(const DockItem *it) {
    if (!it) return 0;
    return (strcmp(it->exec_cmd, "__TRASH__") == 0);
}

static int is_special_item(const DockItem *it) {
    return is_apps_item(it) || is_trash_item(it);
}

static void draw_apps_three_dots_icon(cairo_t *cr, int x, int y, int size) {
    cairo_save(cr);
    // Subtle dark glass squircle button frame
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
    draw_rounded_rectangle(cr, x + 1.0, y + 1.0, size - 2.0, size - 2.0, size * 0.26);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.28);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rectangle(cr, x + 1.0, y + 1.0, size - 2.0, size - 2.0, size * 0.26);
    cairo_stroke(cr);

    // Three pure white dots (•••)
    double cx = x + size / 2.0;
    double cy = y + size / 2.0;
    double r = size * 0.088;
    if (r < 2.2) r = 2.2;
    double step = size * 0.24;

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.98);
    cairo_arc(cr, cx - step, cy, r, 0, 2.0 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, cx, cy, r, 0, 2.0 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, cx + step, cy, r, 0, 2.0 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
}

static void draw_trash_bin_icon(cairo_t *cr, int x, int y, int size) {
    cairo_save(cr);
    // Subtle frosted dark metallic squircle frame
    cairo_set_source_rgba(cr, 0.20, 0.23, 0.29, 0.78);
    draw_rounded_rectangle(cr, x + 1.0, y + 1.0, size - 2.0, size - 2.0, size * 0.24);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.22);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rectangle(cr, x + 1.0, y + 1.0, size - 2.0, size - 2.0, size * 0.24);
    cairo_stroke(cr);

    double cx = x + size / 2.0;
    double top_y = y + size * 0.27;
    double bot_y = y + size * 0.79;
    double top_w = size * 0.46;
    double bot_w = size * 0.36;

    // Trash lid handle
    cairo_set_source_rgba(cr, 0.92, 0.95, 1.0, 0.95);
    cairo_set_line_width(cr, size * 0.055);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    draw_rounded_rectangle(cr, cx - size * 0.11, y + size * 0.17, size * 0.22, size * 0.09, 2.0);
    cairo_stroke(cr);

    // Lid bar
    cairo_move_to(cr, cx - top_w * 0.60, top_y);
    cairo_line_to(cr, cx + top_w * 0.60, top_y);
    cairo_stroke(cr);

    // Trash bin body (slightly tapered trapezoid)
    cairo_new_path(cr);
    cairo_move_to(cr, cx - top_w / 2.0, top_y + size * 0.04);
    cairo_line_to(cr, cx - bot_w / 2.0, bot_y);
    cairo_line_to(cr, cx + bot_w / 2.0, bot_y);
    cairo_line_to(cr, cx + top_w / 2.0, top_y + size * 0.04);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.85, 0.89, 0.96, 0.22);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.94, 0.96, 1.0, 0.92);
    cairo_stroke(cr);

    // Vertical ribs inside trash can
    cairo_set_line_width(cr, size * 0.042);
    double rib_top = top_y + size * 0.13;
    double rib_bot = bot_y - size * 0.08;
    cairo_move_to(cr, cx - size * 0.09, rib_top);
    cairo_line_to(cr, cx - size * 0.07, rib_bot);
    cairo_move_to(cr, cx, rib_top);
    cairo_line_to(cr, cx, rib_bot);
    cairo_move_to(cr, cx + size * 0.09, rib_top);
    cairo_line_to(cr, cx + size * 0.07, rib_bot);
    cairo_stroke(cr);

    cairo_restore(cr);
}

static int dock_get_special_separator_offset(const Dock *dock, int index) {
    if (!dock || dock->item_count <= 0) return 0;

    int has_apps = 0, has_trash = 0, reg_count = 0;
    for (int i = 0; i < dock->item_count; i++) {
        if (is_apps_item(&dock->items[i])) has_apps = 1;
        else if (is_trash_item(&dock->items[i])) has_trash = 1;
        else reg_count++;
    }

    if (reg_count == 0 || (!has_apps && !has_trash)) return 0;

    const int SEP_GAP = 18;
    if (!dock->apps_at_left) {
        if (index >= reg_count) return SEP_GAP;
        return 0;
    } else {
        if (index == 0) return 0;
        int off = SEP_GAP;
        if (has_trash && index == dock->item_count - 1) {
            off += SEP_GAP;
        }
        return off;
    }
}

static int dock_get_total_separator_gap(const Dock *dock) {
    if (!dock || dock->item_count <= 0) return 0;

    int has_apps = 0, has_trash = 0, reg_count = 0;
    for (int i = 0; i < dock->item_count; i++) {
        if (is_apps_item(&dock->items[i])) has_apps = 1;
        else if (is_trash_item(&dock->items[i])) has_trash = 1;
        else reg_count++;
    }

    if (reg_count == 0 || (!has_apps && !has_trash)) return 0;

    const int SEP_GAP = 18;
    if (!dock->apps_at_left) {
        return SEP_GAP;
    } else {
        return has_trash ? (SEP_GAP * 2) : SEP_GAP;
    }
}

int dock_get_item_center_coord(const Dock *dock, int index, int start_pos, int icon_size, int margin) {
    int sep_off = dock_get_special_separator_offset(dock, index);
    return start_pos + margin / 2 + index * (icon_size + margin) + sep_off + icon_size / 2;
}

int dock_get_minimized_center_coord(const Dock *dock, int index, int min_start_pos, int icon_size, int margin) {
    (void)dock;
    return min_start_pos + margin / 2 + index * (icon_size + margin) + icon_size / 2;
}

static void draw_prominent_divider(cairo_t *cr, double pos, int is_vertical, int dock_thickness, double accent_r, double accent_g, double accent_b) {
    cairo_save(cr);
    double line_len = 22.0;
    if (line_len > dock_thickness - 16.0) line_len = dock_thickness - 16.0;

    // Glowing vibrant halo
    cairo_set_source_rgba(cr, accent_r, accent_g, accent_b, 0.45);
    cairo_set_line_width(cr, 3.6);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    if (is_vertical) {
        cairo_move_to(cr, (dock_thickness - line_len) / 2.0, pos);
        cairo_line_to(cr, (dock_thickness + line_len) / 2.0, pos);
    } else {
        cairo_move_to(cr, pos, (dock_thickness - line_len) / 2.0);
        cairo_line_to(cr, pos, (dock_thickness + line_len) / 2.0);
    }
    cairo_stroke(cr);

    // Crisp high-luminance core
    cairo_set_source_rgba(cr, accent_r * 0.3 + 0.7, accent_g * 0.3 + 0.7, accent_b * 0.3 + 0.7, 0.98);
    cairo_set_line_width(cr, 1.8);
    if (is_vertical) {
        cairo_move_to(cr, (dock_thickness - (line_len - 2.0)) / 2.0, pos);
        cairo_line_to(cr, (dock_thickness + (line_len - 2.0)) / 2.0, pos);
    } else {
        cairo_move_to(cr, pos, (dock_thickness - (line_len - 2.0)) / 2.0);
        cairo_line_to(cr, pos, (dock_thickness + (line_len - 2.0)) / 2.0);
    }
    cairo_stroke(cr);
    cairo_restore(cr);
}

void dock_calculate_geometry(Dock *dock, DockPosition pos, int is_hidden, int screen_w, int screen_h) {
    if (!dock) return;
    if (dock->size <= 0) dock->size = 48;
    if (pos == DOCK_TOP) pos = DOCK_BOTTOM;
    dock->pos = pos;
    dock->is_hidden = is_hidden;

    int icon_size = dock_get_base_icon_size(dock);
    int margin = 8;
    int count = dock->item_count > 0 ? dock->item_count : 1;
    int min_count = dock->minimized_count;
    int sep_gap = dock_get_total_separator_gap(dock);
    // Tight fit: no 260px minimum width, dedicated gap for special items separator and minimized separator
    int content_len = count * (icon_size + margin) + sep_gap +
                      (min_count > 0 ? 18 + min_count * (icon_size + margin) : 0) + 12;

    switch (pos) {
        case DOCK_BOTTOM:
        case DOCK_TOP:
            dock->width = content_len < (int)(screen_w * 0.85) ? content_len : (int)(screen_w * 0.85);
            dock->height = dock->size;
            dock->x = (screen_w - dock->width) / 2;
            dock->y = is_hidden ? screen_h - 4 : screen_h - dock->height - 6;
            break;
        case DOCK_LEFT:
            dock->width = dock->size;
            dock->height = content_len < (int)(screen_h * 0.85) ? content_len : (int)(screen_h * 0.85);
            dock->x = is_hidden ? -dock->width + 4 : 6;
            dock->y = (screen_h - dock->height) / 2;
            break;
        case DOCK_RIGHT:
            dock->width = dock->size;
            dock->height = content_len < (int)(screen_h * 0.85) ? content_len : (int)(screen_h * 0.85);
            dock->x = is_hidden ? screen_w - 4 : screen_w - dock->width - 6;
            dock->y = (screen_h - dock->height) / 2;
            break;
    }
}

double dock_get_magnification_scale(const Dock *dock, int item_center) {
    if (!dock || !dock->magnification_enabled || !dock->is_hovered || dock->mouse_pos < 0) {
        return 1.0;
    }
    double dist = fabs(dock->mouse_pos - (double)item_center);
    const double R = 75.0; // Wave influence radius
    if (dist >= R) return 1.0;
    double cosine = cos((M_PI * dist) / (2.0 * R));
    return 1.0 + 0.45 * (cosine * cosine); // Scales smoothly from 1.0 to 1.45
}

int dock_get_item_at(const Dock *dock, int x, int y) {
    if (!dock || dock->item_count <= 0) return -1;
    int icon_size = dock_get_base_icon_size(dock);
    int margin = 8;
    int is_vertical = (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT);
    int launcher_len = dock->item_count * (icon_size + margin) + dock_get_total_separator_gap(dock);
    int total_len = launcher_len + (dock->minimized_count > 0 ? 18 + dock->minimized_count * (icon_size + margin) : 0);

    if (is_vertical) {
        int start_y = (dock->height - total_len) / 2;
        if (start_y < 4) start_y = 4;
        int center_x = dock->width / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int center_y = dock_get_item_center_coord(dock, i, start_y, icon_size, margin);
            double scale = dock_get_magnification_scale(dock, center_y);
            double half_w = (icon_size * scale) / 2.0 + 3.0;
            double half_h = (icon_size * scale) / 2.0 + 3.0;
            if (x >= center_x - half_w && x <= center_x + half_w &&
                y >= center_y - half_h && y <= center_y + half_h) {
                return i;
            }
        }
        return -1;
    } else {
        int start_x = (dock->width - total_len) / 2;
        if (start_x < 4) start_x = 4;
        int center_y = dock->height / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int center_x = dock_get_item_center_coord(dock, i, start_x, icon_size, margin);
            double scale = dock_get_magnification_scale(dock, center_x);
            double half_w = (icon_size * scale) / 2.0 + 3.0;
            double half_h = (icon_size * scale) / 2.0 + 3.0;
            if (x >= center_x - half_w && x <= center_x + half_w &&
                y >= center_y - half_h && y <= center_y + half_h) {
                return i;
            }
        }
        return -1;
    }
}

int dock_add_minimized(Dock *dock, Client *c) {
    if (!dock || !c || dock->minimized_count >= DOCK_MAX_MINIMIZED) return 0;
    for (int i = 0; i < dock->minimized_count; i++) {
        if (dock->minimized[i].client == c) return 1;
    }
    int idx = dock->minimized_count;
    dock->minimized[idx].client = c;
    const char *src = c->title[0] ? c->title : "Window";
    safe_copy(dock->minimized[idx].title, src, sizeof(dock->minimized[idx].title));

    if (c->icon_surface) {
        dock->minimized[idx].icon_surface = c->icon_surface;
    } else {
        c->icon_surface = icon_load_surface(c->title);
        dock->minimized[idx].icon_surface = c->icon_surface;
    }
    dock->minimized_count++;
    {
        int icon_size = dock_get_base_icon_size(dock);
        int margin = 8;
        int count = dock->item_count > 0 ? dock->item_count : 1;
        int sep_gap = dock_get_total_separator_gap(dock);
        int content_len = count * (icon_size + margin) + sep_gap + 18 + dock->minimized_count * (icon_size + margin) + 12;
        if (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT) {
            dock->height = content_len;
        } else {
            dock->width = content_len;
        }
    }
    return 1;
}

void dock_remove_minimized(Dock *dock, Client *c) {
    if (!dock || !c || dock->minimized_count <= 0) return;
    int found = -1;
    for (int i = 0; i < dock->minimized_count; i++) {
        if (dock->minimized[i].client == c) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        dock->minimized[found].icon_surface = NULL;
        for (int i = found; i < dock->minimized_count - 1; i++) {
            dock->minimized[i] = dock->minimized[i + 1];
        }
        dock->minimized_count--;
        memset(&dock->minimized[dock->minimized_count], 0, sizeof(DockMinimizedItem));
        int icon_size = dock_get_base_icon_size(dock);
        int margin = 8;
        int count = dock->item_count > 0 ? dock->item_count : 1;
        int sep_gap = dock_get_total_separator_gap(dock);
        int content_len = count * (icon_size + margin) + sep_gap +
                          (dock->minimized_count > 0 ? 18 + dock->minimized_count * (icon_size + margin) : 0) + 12;
        if (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT) {
            dock->height = content_len;
        } else {
            dock->width = content_len;
        }
    }
}

Client *dock_get_minimized_at(const Dock *dock, int x, int y) {
    if (!dock || dock->minimized_count <= 0) return NULL;
    int icon_size = dock_get_base_icon_size(dock);
    int margin = 8;
    int is_vertical = (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT);
    int launcher_len = dock->item_count * (icon_size + margin) + dock_get_total_separator_gap(dock);
    int total_len = launcher_len + 18 + dock->minimized_count * (icon_size + margin);

    if (is_vertical) {
        int start_y = (dock->height - total_len) / 2;
        if (start_y < 4) start_y = 4;
        int min_start_y = start_y + launcher_len + 18;
        int center_x = dock->width / 2;

        for (int i = 0; i < dock->minimized_count; i++) {
            int center_y = dock_get_minimized_center_coord(dock, i, min_start_y, icon_size, margin);
            double scale = dock_get_magnification_scale(dock, center_y);
            double half_w = (icon_size * scale) / 2.0 + 3.0;
            double half_h = (icon_size * scale) / 2.0 + 3.0;
            if (x >= center_x - half_w && x <= center_x + half_w &&
                y >= center_y - half_h && y <= center_y + half_h) {
                return dock->minimized[i].client;
            }
        }
        return NULL;
    } else {
        int start_x = (dock->width - total_len) / 2;
        if (start_x < 4) start_x = 4;
        int min_start_x = start_x + launcher_len + 18;
        int center_y = dock->height / 2;

        for (int i = 0; i < dock->minimized_count; i++) {
            int center_x = dock_get_minimized_center_coord(dock, i, min_start_x, icon_size, margin);
            double scale = dock_get_magnification_scale(dock, center_x);
            double half_w = (icon_size * scale) / 2.0 + 3.0;
            double half_h = (icon_size * scale) / 2.0 + 3.0;
            if (x >= center_x - half_w && x <= center_x + half_w &&
                y >= center_y - half_h && y <= center_y + half_h) {
                return dock->minimized[i].client;
            }
        }
        return NULL;
    }
}

void dock_enforce_special_items(Dock *dock) {
    if (!dock || dock->item_count <= 1) return;

    DockItem apps_item = {0};
    int has_apps = 0;
    DockItem trash_item = {0};
    int has_trash = 0;
    DockItem regular[DOCK_MAX_ITEMS];
    int reg_count = 0;

    for (int i = 0; i < dock->item_count; i++) {
        if (is_apps_item(&dock->items[i]) && !has_apps) {
            apps_item = dock->items[i];
            has_apps = 1;
        } else if (is_trash_item(&dock->items[i]) && !has_trash) {
            trash_item = dock->items[i];
            has_trash = 1;
        } else {
            regular[reg_count++] = dock->items[i];
        }
    }

    int out = 0;
    if (dock->apps_at_left) {
        // Strictly enforce Apps (3-dots) at Left Corner (index 0)
        if (has_apps) {
            dock->items[out++] = apps_item;
        }
        for (int i = 0; i < reg_count; i++) {
            dock->items[out++] = regular[i];
        }
        if (has_trash) {
            dock->items[out++] = trash_item;
        }
    } else {
        // Default: Strictly enforce Apps (3-dots) at Right Corner (last index), with Trash right beside it
        for (int i = 0; i < reg_count; i++) {
            dock->items[out++] = regular[i];
        }
        if (has_trash) {
            dock->items[out++] = trash_item;
        }
        if (has_apps) {
            dock->items[out++] = apps_item;
        }
    }
    dock->item_count = out;
}

int dock_reorder_item(Dock *dock, int from_idx, int to_idx) {
    if (!dock || from_idx < 0 || from_idx >= dock->item_count ||
        to_idx < 0 || to_idx >= dock->item_count || from_idx == to_idx) {
        return from_idx;
    }
    // Special items (Apps 3-dots and Trash) have fixed corner positions and cannot be displaced
    if (is_special_item(&dock->items[from_idx]) || is_special_item(&dock->items[to_idx])) {
        return from_idx;
    }

    DockItem moving = dock->items[from_idx];
    if (from_idx < to_idx) {
        for (int i = from_idx; i < to_idx; i++) {
            dock->items[i] = dock->items[i + 1];
        }
    } else {
        for (int i = from_idx; i > to_idx; i--) {
            dock->items[i] = dock->items[i - 1];
        }
    }
    dock->items[to_idx] = moving;
    dock_enforce_special_items(dock);
    dock_save_pinned_apps(dock);
    return to_idx;
}

void dock_set_apps_corner(Dock *dock, int at_left) {
    if (!dock) return;
    dock->apps_at_left = (at_left != 0) ? 1 : 0;
    dock_enforce_special_items(dock);
    dock_save_settings(dock);
}

void dock_empty_trash(void) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c",
              "gio trash --empty 2>/dev/null || rm -rf \"$HOME/.local/share/Trash/files/\"* \"$HOME/.local/share/Trash/info/\"* 2>/dev/null",
              (char *)NULL);
        _exit(0);
    }
}

int dock_add_app(Dock *dock, const char *name, const char *exec, const char *icon_name) {
    if (!dock || !name || !exec || dock->item_count >= DOCK_MAX_ITEMS) return -1;

    for (int i = 0; i < dock->item_count; i++) {
        if (strcmp(dock->items[i].exec_cmd, exec) == 0) return i;
    }

    int idx = dock->item_count++;
    safe_copy(dock->items[idx].name, name, sizeof(dock->items[idx].name));
    safe_copy(dock->items[idx].exec_cmd, exec, sizeof(dock->items[idx].exec_cmd));
    safe_copy(dock->items[idx].icon_name, icon_name ? icon_name : "", sizeof(dock->items[idx].icon_name));
    dock->items[idx].is_running = 0;
    dock->items[idx].icon_surface = icon_load_surface(dock->items[idx].icon_name);
    if (!dock->items[idx].icon_surface) {
        dock->items[idx].icon_surface = icon_load_surface("application-x-executable");
    }

    dock_enforce_special_items(dock);
    for (int i = 0; i < dock->item_count; i++) {
        if (strcmp(dock->items[i].exec_cmd, exec) == 0) return i;
    }
    return idx;
}

int dock_remove_app(Dock *dock, int index) {
    if (!dock || index < 0 || index >= dock->item_count) return 0;
    if (is_special_item(&dock->items[index])) return 0; // Never remove Apps launcher or Trash

    if (dock->items[index].icon_surface) {
        cairo_surface_finish(dock->items[index].icon_surface);
        cairo_surface_destroy(dock->items[index].icon_surface);
        dock->items[index].icon_surface = NULL;
    }

    for (int i = index; i < dock->item_count - 1; i++) {
        dock->items[i] = dock->items[i + 1];
    }
    dock->item_count--;
    dock_save_pinned_apps(dock);
    return 1;
}

void dock_save_pinned_apps(const Dock *dock) {
    const char *home = getenv("HOME");
    if (!home) return;
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.config/muzuwm", home);
    mkdir(dir, 0755);
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/muzuwm/dock_apps.ini", home);

    FILE *f = fopen(path, "w");
    if (!f) return;

    for (int i = 0; i < dock->item_count; i++) {
        if (is_special_item(&dock->items[i])) continue;
        fprintf(f, "%s|%s|%s\n", dock->items[i].name, dock->items[i].exec_cmd, dock->items[i].icon_name);
    }
    fclose(f);
}

void dock_load_pinned_apps(Dock *dock) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/muzuwm/dock_apps.ini", home);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *name = line;
        char *exec = strchr(name, '|');
        if (!exec) continue;
        *exec++ = '\0';
        char *icon = strchr(exec, '|');
        if (!icon) continue;
        *icon++ = '\0';

        dock_add_app(dock, name, exec, icon);
    }
    fclose(f);
}

void dock_save_settings(const Dock *dock) {
    if (!dock) return;
    const char *home = getenv("HOME");
    if (!home) return;
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.config/muzuwm", home);
    mkdir(dir, 0755);
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/muzuwm/dock_settings.ini", home);

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "autohide=%d\n", dock->autohide_enabled);
    fprintf(f, "magnification=%d\n", dock->magnification_enabled);
    fprintf(f, "position=%d\n", (int)dock->pos);
    fprintf(f, "size=%d\n", dock->size);
    fprintf(f, "apps_corner=%s\n", dock->apps_at_left ? "left" : "right");
    if (dock->top_offset > 0) {
        fprintf(f, "top_offset=%d\n", dock->top_offset);
    }
    fclose(f);
}

void dock_load_settings(Dock *dock) {
    if (!dock) return;
    const char *home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/muzuwm/dock_settings.ini", home);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        int val = 0;
        char str_val[64] = {0};
        if (sscanf(line, "autohide=%d", &val) == 1) {
            dock->autohide_enabled = (val != 0);
            dock->is_hidden = dock->autohide_enabled ? 1 : 0;
        } else if (sscanf(line, "magnification=%d", &val) == 1) {
            dock->magnification_enabled = (val != 0);
        } else if (sscanf(line, "position=%d", &val) == 1) {
            if (val >= 0 && val <= 3) dock->pos = (val == DOCK_TOP) ? DOCK_BOTTOM : (DockPosition)val;
        } else if (sscanf(line, "size=%d", &val) == 1) {
            if (val >= 24 && val <= 128) dock->size = val;
        } else if (sscanf(line, "apps_corner=%63s", str_val) == 1) {
            // Strictly enforce only left corner or right corner
            if (strcasecmp(str_val, "left") == 0 || strcmp(str_val, "1") == 0) {
                dock->apps_at_left = 1;
            } else {
                dock->apps_at_left = 0;
            }
        } else if (sscanf(line, "apps_at_left=%d", &val) == 1) {
            dock->apps_at_left = (val != 0) ? 1 : 0;
        } else if (sscanf(line, "top_offset=%d", &val) == 1) {
            if (val > 0 && val < 500) dock->top_offset = val;
        }
    }
    fclose(f);
    dock_enforce_special_items(dock);
}

void dock_set_autohide(Dock *dock, int enabled, int screen_w, int screen_h, Display *dpy) {
    if (!dock) return;
    dock->autohide_enabled = enabled ? 1 : 0;
    dock->is_hidden = dock->autohide_enabled ? 1 : 0;
    dock_calculate_geometry(dock, dock->pos, dock->is_hidden, screen_w, screen_h);
    if (dpy && dock->win) {
        XMoveWindow(dpy, dock->win, dock->x, dock->y);
        if (!dock->is_hidden) {
            XRaiseWindow(dpy, dock->win);
        }
    }
    dock_render(dpy, dock, screen_w, screen_h);
    dock_save_settings(dock);
}

void dock_set_magnification(Dock *dock, int enabled) {
    if (!dock) return;
    dock->magnification_enabled = enabled ? 1 : 0;
    dock_save_settings(dock);
}

void dock_init(Display *dpy, Dock *dock, DockPosition pos, int screen_w, int screen_h, const GtkTheme *theme) {
    if (!dock) return;
    dock->size = 48;
    dock->autohide_enabled = 0; // Default: Always visible dock (can be toggled)
    dock->is_hidden = 0;
    dock->is_dragging = 0;
    dock->drag_offset_x = 0;
    dock->drag_offset_y = 0;
    dock->hover_state = 0;
    dock->hover_item = -1;
    dock->hover_minimized = -1;
    dock->magnification_enabled = 1;
    dock->mouse_pos = -1.0;
    dock->is_hovered = 0;
    dock->apps_at_left = 0; // Default: 3-dots Apps button at Right Corner
    dock->press_item = -1;
    dock->press_x = 0;
    dock->press_y = 0;
    dock->is_reordering = 0;
    dock->surface = NULL;
    dock->tooltip_win = None;
    dock->tooltip_surface = NULL;
    dock->theme = theme;
    dock->item_count = 0;
    dock->minimized_count = 0;
    dock->pos = pos;

    // Built-in items: Trash bin & Apps Launcher (white three dots at right corner by default)
    dock_add_app(dock, "Trash", "__TRASH__", "user-trash");
    dock_add_app(dock, "Apps", "__ROFI__", "view-app-grid");

    // Load user pinned apps and user settings
    dock_load_pinned_apps(dock);
    dock_load_settings(dock);
    dock_enforce_special_items(dock);

    dock_calculate_geometry(dock, dock->pos, dock->is_hidden, screen_w, screen_h);

    if (dpy) {
        int screen = DefaultScreen(dpy);
        Window root = RootWindow(dpy, screen);

        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.background_pixel = BlackPixel(dpy, screen);
        attrs.event_mask = ExposureMask | EnterWindowMask | LeaveWindowMask |
                           ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

        dock->win = XCreateWindow(dpy, root, dock->x, dock->y, dock->width, dock->height, 0,
                                  DefaultDepth(dpy, screen), InputOutput,
                                  DefaultVisual(dpy, screen),
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);

        Atom net_wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom net_wm_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
        XChangeProperty(dpy, dock->win, net_wm_type, XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)&net_wm_dock, 1);

        Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
        XChangeProperty(dpy, dock->win, net_wm_name, XInternAtom(dpy, "UTF8_STRING", False), 8,
                        PropModeReplace, (unsigned char *)"MUZUwm Dock", 11);
        XStoreName(dpy, dock->win, "MUZUwm Dock");

        client_set_opacity(dpy, dock->win, 0xF0AAAAAA);

        XSetWindowAttributes tattrs;
        tattrs.override_redirect = True;
        tattrs.background_pixel = BlackPixel(dpy, screen);
        tattrs.event_mask = ExposureMask;

        dock->tooltip_win = XCreateWindow(dpy, root, 0, 0, 10, 10, 0,
                                          DefaultDepth(dpy, screen), InputOutput,
                                          DefaultVisual(dpy, screen),
                                          CWOverrideRedirect | CWBackPixel | CWEventMask, &tattrs);

        XMapRaised(dpy, dock->win);
        dock_render(dpy, dock, screen_w, screen_h);
    }
}

void dock_render(Display *dpy, Dock *dock, int screen_w, int screen_h) {
    if (!dpy || !dock) return;
    if (dock->autohide_enabled && dock->is_hidden) {
        return; // Zero CPU consumption while hidden
    }
    if (screen_w > 0 && screen_h > 0 && !dock->is_dragging) {
        int old_w = dock->width;
        int old_h = dock->height;
        int old_x = dock->x;
        int old_y = dock->y;
        dock_calculate_geometry(dock, dock->pos, dock->is_hidden, screen_w, screen_h);
        if (dock->win && (dock->width != old_w || dock->height != old_h || dock->x != old_x || dock->y != old_y)) {
            XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
        }
    }
    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);

    if (!dock->surface) {
        dock->surface = cairo_xlib_surface_create(dpy, dock->win, visual, dock->width, dock->height);
    } else {
        cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
    }
    cairo_t *cr = cairo_create(dock->surface);

    // Clear surface completely to remove any ghost artifacts or trails
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    int is_vertical = (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT);

    // Liquid Frosted Smoked Glass Squircle Plate (Rounded Rectangle with 14px continuous curvature)
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, 0, dock->height);
    cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.10, 0.12, 0.16, 0.82);
    cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.06, 0.07, 0.10, 0.72);
    cairo_set_source(cr, pat);
    draw_rounded_rectangle(cr, 1.0, 1.0, dock->width - 2.0, dock->height - 2.0, 14.0);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    // Outer subtle shadow stroke
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.50);
    cairo_set_line_width(cr, 1.5);
    draw_rounded_rectangle(cr, 0.5, 0.5, dock->width - 1.0, dock->height - 1.0, 14.0);
    cairo_stroke(cr);

    // Inner specular refraction rim
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.16);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rectangle(cr, 1.5, 1.5, dock->width - 3.0, dock->height - 3.0, 13.0);
    cairo_stroke(cr);

    // Render launcher icons
    int icon_size = dock_get_base_icon_size(dock);
    int margin = 8;
    double accent_r = dock->theme ? dock->theme->accent_color.r : 0.25;
    double accent_g = dock->theme ? dock->theme->accent_color.g : 0.55;
    double accent_b = dock->theme ? dock->theme->accent_color.b : 0.95;

    double colors[4][3] = {
        {accent_r, accent_g, accent_b},
        {0.20, 0.75, 0.50},
        {0.45, 0.45, 0.90},
        {0.90, 0.60, 0.20}
    };

    int has_apps = 0, has_trash = 0, reg_count = 0;
    for (int i = 0; i < dock->item_count; i++) {
        if (is_apps_item(&dock->items[i])) has_apps = 1;
        else if (is_trash_item(&dock->items[i])) has_trash = 1;
        else reg_count++;
    }

    int launcher_len = dock->item_count * (icon_size + margin) + dock_get_total_separator_gap(dock);
    int total_len = launcher_len + (dock->minimized_count > 0 ? 18 + dock->minimized_count * (icon_size + margin) : 0);

    if (is_vertical) {
        int start_y = (dock->height - total_len) / 2;
        if (start_y < 4) start_y = 4;
        int center_x = dock->width / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int center_y = dock_get_item_center_coord(dock, i, start_y, icon_size, margin);
            double scale = dock_get_magnification_scale(dock, center_y);
            int eff_size = (int)(icon_size * scale);
            int ix = center_x - eff_size / 2;
            int iy = center_y - eff_size / 2;

            // Hover / Reorder highlight
            if (dock->is_reordering && dock->press_item == i) {
                cairo_set_source_rgba(cr, accent_r, accent_g, accent_b, 0.45);
                draw_rounded_rectangle(cr, ix - 4, iy - 4, eff_size + 8, eff_size + 8, 9.0);
                cairo_fill(cr);
            } else if (dock->hover_item == i) {
                cairo_set_source_rgba(cr, 0.25, 0.27, 0.32, 0.60);
                draw_rounded_rectangle(cr, ix - 3, iy - 3, eff_size + 6, eff_size + 6, 8.0);
                cairo_fill(cr);
            }

            if (is_apps_item(&dock->items[i])) {
                draw_apps_three_dots_icon(cr, ix, iy, eff_size);
            } else if (is_trash_item(&dock->items[i])) {
                draw_trash_bin_icon(cr, ix, iy, eff_size);
            } else if (dock->items[i].icon_surface) {
                icon_render(cr, dock->items[i].icon_surface, dock->items[i].name, ix, iy, eff_size);
            } else {
                cairo_set_source_rgb(cr, colors[i % 4][0], colors[i % 4][1], colors[i % 4][2]);
                draw_rounded_rectangle(cr, ix, iy, eff_size, eff_size, 6.0);
                cairo_fill(cr);

                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 14.0 * scale);
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                cairo_move_to(cr, ix + 10 * scale, iy + 22 * scale);
                char letter[2] = { dock->items[i].name[0], '\0' };
                cairo_show_text(cr, letter);
            }

            // Running indicator dot
            if (dock->items[i].is_running) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.90);
                double dot_x = (dock->pos == DOCK_LEFT) ? (ix + eff_size + 4) : (ix - 4);
                double dot_y = center_y;
                cairo_arc(cr, dot_x, dot_y, 2.0, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }

        // Prominent colored divider line separating Apps/Trash from pinned/running applications
        if (!dock->apps_at_left) {
            if (reg_count > 0 && (has_apps || has_trash)) {
                double div_pos = start_y + margin / 2 + reg_count * (icon_size + margin) + 9.0;
                draw_prominent_divider(cr, div_pos, 1, dock->width, accent_r, accent_g, accent_b);
            }
        } else {
            if (has_apps && reg_count > 0) {
                double div1_pos = start_y + margin / 2 + 1 * (icon_size + margin) + 9.0;
                draw_prominent_divider(cr, div1_pos, 1, dock->width, accent_r, accent_g, accent_b);
            }
            if (has_trash && reg_count > 0) {
                double div2_pos = start_y + margin / 2 + (dock->item_count - 1) * (icon_size + margin) + 18.0 + 9.0;
                draw_prominent_divider(cr, div2_pos, 1, dock->width, accent_r, accent_g, accent_b);
            }
        }

        // Separator line and Minimized Windows section (vertical)
        if (dock->minimized_count > 0) {
            int sep_y = start_y + launcher_len + 9;
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, (dock->width - 24) / 2.0, sep_y);
            cairo_line_to(cr, (dock->width + 24) / 2.0, sep_y);
            cairo_stroke(cr);

            int min_start_y = start_y + launcher_len + 18;
            for (int i = 0; i < dock->minimized_count; i++) {
                int center_y = dock_get_minimized_center_coord(dock, i, min_start_y, icon_size, margin);
                double scale = dock_get_magnification_scale(dock, center_y);
                int eff_size = (int)(icon_size * scale);
                int mx = center_x - eff_size / 2;
                int my = center_y - eff_size / 2;

                if (dock->minimized[i].icon_surface) {
                    icon_render(cr, dock->minimized[i].icon_surface, dock->minimized[i].title, mx, my, eff_size);
                } else {
                    cairo_set_source_rgba(cr, 0.35, 0.35, 0.38, 0.85);
                    draw_rounded_rectangle(cr, mx, my, eff_size, eff_size, 6.0);
                    cairo_fill(cr);

                    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_set_font_size(cr, 14.0 * scale);
                    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    char letter[2] = { dock->minimized[i].title[0] ? dock->minimized[i].title[0] : 'W', '\0' };
                    cairo_move_to(cr, mx + 10 * scale, my + 21 * scale);
                    cairo_show_text(cr, letter);
                }
            }
        }
    } else {
        // Horizontal dock
        int start_x = (dock->width - total_len) / 2;
        if (start_x < 4) start_x = 4;
        int center_y = dock->height / 2;

        for (int i = 0; i < dock->item_count; i++) {
            int center_x = dock_get_item_center_coord(dock, i, start_x, icon_size, margin);
            double scale = dock_get_magnification_scale(dock, center_x);
            int eff_size = (int)(icon_size * scale);
            int ix = center_x - eff_size / 2;
            int iy = center_y - eff_size / 2;

            if (dock->is_reordering && dock->press_item == i) {
                cairo_set_source_rgba(cr, accent_r, accent_g, accent_b, 0.45);
                draw_rounded_rectangle(cr, ix - 4, iy - 4, eff_size + 8, eff_size + 8, 9.0);
                cairo_fill(cr);
            } else if (dock->hover_item == i) {
                cairo_set_source_rgba(cr, 0.25, 0.27, 0.32, 0.60);
                draw_rounded_rectangle(cr, ix - 3, iy - 3, eff_size + 6, eff_size + 6, 8.0);
                cairo_fill(cr);
            }

            if (is_apps_item(&dock->items[i])) {
                draw_apps_three_dots_icon(cr, ix, iy, eff_size);
            } else if (is_trash_item(&dock->items[i])) {
                draw_trash_bin_icon(cr, ix, iy, eff_size);
            } else if (dock->items[i].icon_surface) {
                icon_render(cr, dock->items[i].icon_surface, dock->items[i].name, ix, iy, eff_size);
            } else {
                cairo_set_source_rgb(cr, colors[i % 4][0], colors[i % 4][1], colors[i % 4][2]);
                draw_rounded_rectangle(cr, ix, iy, eff_size, eff_size, 6.0);
                cairo_fill(cr);

                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 14.0 * scale);
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                cairo_move_to(cr, ix + 10 * scale, iy + 22 * scale);
                char letter[2] = { dock->items[i].name[0], '\0' };
                cairo_show_text(cr, letter);
            }

            // Running indicator dot
            if (dock->items[i].is_running) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.90);
                double dot_x = center_x;
                double dot_y = (dock->pos == DOCK_TOP) ? (iy - 3) : (iy + eff_size + 4);
                cairo_arc(cr, dot_x, dot_y, 2.0, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }

        // Prominent colored divider line separating Apps/Trash from pinned/running applications
        if (!dock->apps_at_left) {
            if (reg_count > 0 && (has_apps || has_trash)) {
                double div_pos = start_x + margin / 2 + reg_count * (icon_size + margin) + 9.0;
                draw_prominent_divider(cr, div_pos, 0, dock->height, accent_r, accent_g, accent_b);
            }
        } else {
            if (has_apps && reg_count > 0) {
                double div1_pos = start_x + margin / 2 + 1 * (icon_size + margin) + 9.0;
                draw_prominent_divider(cr, div1_pos, 0, dock->height, accent_r, accent_g, accent_b);
            }
            if (has_trash && reg_count > 0) {
                double div2_pos = start_x + margin / 2 + (dock->item_count - 1) * (icon_size + margin) + 18.0 + 9.0;
                draw_prominent_divider(cr, div2_pos, 0, dock->height, accent_r, accent_g, accent_b);
            }
        }

        // Separator line and Minimized Windows section (horizontal)
        if (dock->minimized_count > 0) {
            int sep_x = start_x + launcher_len + 9;
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
            cairo_set_line_width(cr, 1.5);
            cairo_move_to(cr, sep_x, (dock->height - 24) / 2.0);
            cairo_line_to(cr, sep_x, (dock->height + 24) / 2.0);
            cairo_stroke(cr);

            int min_start_x = start_x + launcher_len + 18;
            for (int i = 0; i < dock->minimized_count; i++) {
                int center_x = dock_get_minimized_center_coord(dock, i, min_start_x, icon_size, margin);
                double scale = dock_get_magnification_scale(dock, center_x);
                int eff_size = (int)(icon_size * scale);
                int mx = center_x - eff_size / 2;
                int my = center_y - eff_size / 2;

                if (dock->minimized[i].icon_surface) {
                    icon_render(cr, dock->minimized[i].icon_surface, dock->minimized[i].title, mx, my, eff_size);
                } else {
                    cairo_set_source_rgba(cr, 0.35, 0.35, 0.38, 0.85);
                    draw_rounded_rectangle(cr, mx, my, eff_size, eff_size, 6.0);
                    cairo_fill(cr);

                    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_set_font_size(cr, 14.0 * scale);
                    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    char letter[2] = { dock->minimized[i].title[0] ? dock->minimized[i].title[0] : 'W', '\0' };
                    cairo_move_to(cr, mx + 10 * scale, my + 21 * scale);
                    cairo_show_text(cr, letter);
                }
            }
        }
    }

    cairo_destroy(cr);
    cairo_surface_flush(dock->surface);
    if (dpy) {
        if (!dock->is_hidden) {
            XRaiseWindow(dpy, dock->win);
        }
        dock_render_tooltip(dpy, dock, screen_w, screen_h);
        XFlush(dpy);
    }
}

void dock_hide_tooltip(Display *dpy, Dock *dock) {
    if (!dock) return;
    if (dpy && dock->tooltip_win) {
        XUnmapWindow(dpy, dock->tooltip_win);
        XFlush(dpy);
    }
}

void dock_render_tooltip(Display *dpy, Dock *dock, int screen_w, int screen_h) {
    if (!dpy || !dock || !dock->tooltip_win) return;
    if (dock->is_hidden || dock->is_dragging || !dock->is_hovered || (dock->hover_item < 0 && dock->hover_minimized < 0)) {
        dock_hide_tooltip(dpy, dock);
        return;
    }

    const char *text = NULL;
    int is_minimized_item = 0;
    int target_idx = -1;

    if (dock->hover_item >= 0 && dock->hover_item < dock->item_count) {
        text = dock->items[dock->hover_item].name;
        target_idx = dock->hover_item;
    } else if (dock->hover_minimized >= 0 && dock->hover_minimized < dock->minimized_count) {
        text = dock->minimized[dock->hover_minimized].title;
        target_idx = dock->hover_minimized;
        is_minimized_item = 1;
    }

    if (!text || text[0] == '\0') {
        dock_hide_tooltip(dpy, dock);
        return;
    }

    // Measure text extents
    cairo_surface_t *measure_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *mcr = cairo_create(measure_surf);
    cairo_select_font_face(mcr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(mcr, 11.5);
    cairo_text_extents_t extents;
    cairo_text_extents(mcr, text, &extents);
    cairo_destroy(mcr);
    cairo_surface_destroy(measure_surf);

    int tooltip_w = (int)(extents.width + 22.0);
    if (tooltip_w < 44) tooltip_w = 44;
    int tooltip_h = 24;

    int icon_size = dock_get_base_icon_size(dock);
    int margin = 8;
    int is_vertical = (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT);
    int launcher_len = dock->item_count * (icon_size + margin) + dock_get_total_separator_gap(dock);
    int total_len = launcher_len + (dock->minimized_count > 0 ? 18 + dock->minimized_count * (icon_size + margin) : 0);

    int item_center = 0;
    if (is_vertical) {
        int start_y = (dock->height - total_len) / 2;
        if (start_y < 4) start_y = 4;
        if (is_minimized_item) {
            int min_start_y = start_y + launcher_len + 18;
            item_center = dock_get_minimized_center_coord(dock, target_idx, min_start_y, icon_size, margin);
        } else {
            item_center = dock_get_item_center_coord(dock, target_idx, start_y, icon_size, margin);
        }
    } else {
        int start_x = (dock->width - total_len) / 2;
        if (start_x < 4) start_x = 4;
        if (is_minimized_item) {
            int min_start_x = start_x + launcher_len + 18;
            item_center = dock_get_minimized_center_coord(dock, target_idx, min_start_x, icon_size, margin);
        } else {
            item_center = dock_get_item_center_coord(dock, target_idx, start_x, icon_size, margin);
        }
    }

    int tooltip_x = 0;
    int tooltip_y = 0;

    switch (dock->pos) {
        case DOCK_BOTTOM:
            tooltip_x = dock->x + item_center - tooltip_w / 2;
            tooltip_y = dock->y - tooltip_h - 8;
            break;
        case DOCK_TOP:
            tooltip_x = dock->x + item_center - tooltip_w / 2;
            tooltip_y = dock->y + dock->height + 8;
            break;
        case DOCK_LEFT:
            tooltip_x = dock->x + dock->width + 8;
            tooltip_y = dock->y + item_center - tooltip_h / 2;
            break;
        case DOCK_RIGHT:
            tooltip_x = dock->x - tooltip_w - 8;
            tooltip_y = dock->y + item_center - tooltip_h / 2;
            break;
    }

    if (tooltip_x < 4) tooltip_x = 4;
    if (tooltip_x + tooltip_w > screen_w - 4) tooltip_x = screen_w - tooltip_w - 4;
    if (tooltip_y < 4) tooltip_y = 4;
    if (tooltip_y + tooltip_h > screen_h - 4) tooltip_y = screen_h - tooltip_h - 4;

    XMoveResizeWindow(dpy, dock->tooltip_win, tooltip_x, tooltip_y, tooltip_w, tooltip_h);
    XMapRaised(dpy, dock->tooltip_win);

    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);
    if (!dock->tooltip_surface) {
        dock->tooltip_surface = cairo_xlib_surface_create(dpy, dock->tooltip_win, visual, tooltip_w, tooltip_h);
    } else {
        cairo_xlib_surface_set_size(dock->tooltip_surface, tooltip_w, tooltip_h);
    }

    cairo_t *cr = cairo_create(dock->tooltip_surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Liquid Glass Tooltip Pill (Deep dark frosted background + specular highlight)
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, 0, tooltip_h);
    cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.12, 0.14, 0.18, 0.96);
    cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.07, 0.08, 0.11, 0.92);
    cairo_set_source(cr, pat);
    draw_rounded_rectangle(cr, 1.0, 1.0, tooltip_w - 2.0, tooltip_h - 2.0, 6.0);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    // Subtle dark outer border
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.65);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rectangle(cr, 0.5, 0.5, tooltip_w - 1.0, tooltip_h - 1.0, 6.0);
    cairo_stroke(cr);

    // Specular refraction rim
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.22);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rectangle(cr, 1.5, 1.5, tooltip_w - 3.0, tooltip_h - 3.0, 5.0);
    cairo_stroke(cr);

    // Crisp high-contrast text centered
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.5);
    double tx = (tooltip_w - extents.width) / 2.0 - extents.x_bearing;
    double ty = (tooltip_h - extents.height) / 2.0 - extents.y_bearing;

    // Drop shadow
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.70);
    cairo_move_to(cr, tx, ty + 1.0);
    cairo_show_text(cr, text);

    // Text foreground
    cairo_set_source_rgba(cr, 0.97, 0.98, 1.0, 1.0);
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, text);

    cairo_destroy(cr);
    cairo_surface_flush(dock->tooltip_surface);
    XFlush(dpy);
}

void dock_handle_motion(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h) {
    if (!dock) return;

    if (!dock->autohide_enabled) {
        if (dock->is_hidden) {
            dock->is_hidden = 0;
            dock_calculate_geometry(dock, dock->pos, 0, screen_w, screen_h);
            if (dpy && dock->win) {
                XMoveWindow(dpy, dock->win, dock->x, dock->y);
                XRaiseWindow(dpy, dock->win);
            }
        }
        return;
    }

    int in_trigger = 0;
    if (dock->pos == DOCK_BOTTOM && y >= screen_h - dock->size - 10) in_trigger = 1;
    else if (dock->pos == DOCK_LEFT && x <= dock->size + 10) in_trigger = 1;
    else if (dock->pos == DOCK_RIGHT && x >= screen_w - dock->size - 10) in_trigger = 1;

    if (in_trigger && dock->is_hidden) {
        dock->is_hidden = 0;
        dock_calculate_geometry(dock, dock->pos, 0, screen_w, screen_h);
        if (dpy && dock->win) {
            XMoveWindow(dpy, dock->win, dock->x, dock->y);
            XRaiseWindow(dpy, dock->win);
            dock_render(dpy, dock, screen_w, screen_h);
        }
    } else if (!in_trigger && !dock->is_hidden && !dock->is_dragging && dock->press_item < 0) {
        dock->is_hidden = 1;
        dock->hover_item = -1;
        dock->hover_minimized = -1;
        dock->is_hovered = 0;
        dock->mouse_pos = -1.0;
        dock_hide_tooltip(dpy, dock);
        dock_calculate_geometry(dock, dock->pos, 1, screen_w, screen_h);
        if (dpy && dock->win) {
            XMoveWindow(dpy, dock->win, dock->x, dock->y);
        }
    }
}

void dock_handle_hover(Display *dpy, Dock *dock, int x, int y, int screen_w, int screen_h) {
    if (!dock) return;
    int is_vertical = (dock->pos == DOCK_LEFT || dock->pos == DOCK_RIGHT);
    dock->is_hovered = 1;
    dock->mouse_pos = is_vertical ? (double)y : (double)x;

    if (dpy && dock->win) {
        XRaiseWindow(dpy, dock->win);
    }

    int item = dock_get_item_at(dock, x, y);
    int min_idx = -1;
    if (item < 0 && dock->minimized_count > 0) {
        Client *mc = dock_get_minimized_at(dock, x, y);
        if (mc) {
            for (int i = 0; i < dock->minimized_count; i++) {
                if (dock->minimized[i].client == mc) {
                    min_idx = i;
                    break;
                }
            }
        }
    }

    if (item != dock->hover_item || min_idx != dock->hover_minimized || dock->magnification_enabled) {
        dock->hover_item = item;
        dock->hover_minimized = min_idx;
        dock_render(dpy, dock, screen_w, screen_h);
    }
}

void dock_handle_click(Display *dpy, Dock *dock, RofiLauncher *rofi, int x, int y) {
    if (!dock) return;
    dock_hide_tooltip(dpy, dock);

    int item = dock_get_item_at(dock, x, y);
    if (item < 0 || item >= dock->item_count) return;

    if (is_apps_item(&dock->items[item])) {
        if (rofi) rofi_launcher_toggle(dpy, rofi);
        return;
    }

    if (is_trash_item(&dock->items[item])) {
        pid_t tpid = fork();
        if (tpid == 0) {
            setsid();
            execl("/bin/sh", "sh", "-c",
                  "mkdir -p \"$HOME/.local/share/Trash/files\" \"$HOME/.local/share/Trash/info\" && "
                  "(gio open trash:// 2>/dev/null || xdg-open trash:// 2>/dev/null || xdg-open \"$HOME/.local/share/Trash/files\")",
                  (char *)NULL);
            _exit(1);
        }
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", dock->items[item].exec_cmd, (char *)NULL);
        _exit(1);
    }
}

void dock_set_position(Display *dpy, Dock *dock, DockPosition new_pos, int screen_w, int screen_h) {
    dock_calculate_geometry(dock, new_pos, dock->is_hidden, screen_w, screen_h);
    if (dpy && dock->win) {
        XMoveResizeWindow(dpy, dock->win, dock->x, dock->y, dock->width, dock->height);
        if (!dock->is_hidden) {
            XRaiseWindow(dpy, dock->win);
        }
    }
    if (dock->surface) {
        cairo_xlib_surface_set_size(dock->surface, dock->width, dock->height);
    }
    dock_render(dpy, dock, screen_w, screen_h);
    dock_save_settings(dock);
}

void dock_cycle_position(Display *dpy, Dock *dock, int screen_w, int screen_h) {
    if (!dock) return;
    DockPosition next_pos;
    switch (dock->pos) {
        case DOCK_BOTTOM: next_pos = DOCK_LEFT; break;
        case DOCK_LEFT:   next_pos = DOCK_RIGHT; break;
        case DOCK_RIGHT:  next_pos = DOCK_BOTTOM; break;
        default:          next_pos = DOCK_BOTTOM; break;
    }
    dock_set_position(dpy, dock, next_pos, screen_w, screen_h);
}

DockPosition dock_calculate_nearest_edge(int center_x, int center_y, int screen_w, int screen_h) {
    int dist_bottom = screen_h - center_y;
    int dist_left = center_x;
    int dist_right = screen_w - center_x;

    int min_dist = dist_bottom;
    DockPosition pos = DOCK_BOTTOM;

    if (dist_left < min_dist) {
        min_dist = dist_left;
        pos = DOCK_LEFT;
    }
    if (dist_right < min_dist) {
        min_dist = dist_right;
        pos = DOCK_RIGHT;
    }

    return pos;
}

void dock_handle_drag(Display *dpy, Dock *dock, int root_x, int root_y, int screen_w, int screen_h) {
    if (!dock || !dock->is_dragging) return;
    int nx = root_x - dock->drag_offset_x;
    int ny = root_y - dock->drag_offset_y;

    if (nx + dock->width > screen_w) nx = screen_w - dock->width;
    if (ny + dock->height > screen_h) ny = screen_h - dock->height;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;

    dock->x = nx;
    dock->y = ny;
    if (dpy && dock->win) {
        XMoveWindow(dpy, dock->win, dock->x, dock->y);
    }
}

void dock_end_drag(Display *dpy, Dock *dock, int screen_w, int screen_h) {
    if (!dock) return;
    dock->is_dragging = 0;
    int cx = dock->x + dock->width / 2;
    int cy = dock->y + dock->height / 2;
    DockPosition nearest = dock_calculate_nearest_edge(cx, cy, screen_w, screen_h);
    dock_set_position(dpy, dock, nearest, screen_w, screen_h);
}

void dock_update_running(Dock *dock, const Client *clients) {
    if (!dock) return;
    for (int i = 0; i < dock->item_count; i++) {
        dock->items[i].is_running = 0;
        const char *item_name = dock->items[i].name;

        // Skip special items (Apps launcher & Trash are never marked as running windows)
        if (is_special_item(&dock->items[i])) {
            continue;
        }

        for (const Client *c = clients; c; c = c->next) {
            if (c->title[0] != '\0' && strcasestr(c->title, item_name) != NULL) {
                dock->items[i].is_running = 1;
                break;
            }
        }
    }
}

void dock_cleanup(Dock *dock) {
    if (!dock) return;
    for (int i = 0; i < dock->item_count; i++) {
        if (dock->items[i].icon_surface) {
            cairo_surface_finish(dock->items[i].icon_surface);
            cairo_surface_destroy(dock->items[i].icon_surface);
            dock->items[i].icon_surface = NULL;
        }
    }
    for (int i = 0; i < dock->minimized_count; i++) {
        dock->minimized[i].icon_surface = NULL;
    }
    if (dock->tooltip_surface) {
        cairo_surface_finish(dock->tooltip_surface);
        cairo_surface_destroy(dock->tooltip_surface);
        dock->tooltip_surface = NULL;
    }
    if (dock->surface) {
        cairo_surface_finish(dock->surface);
        cairo_surface_destroy(dock->surface);
        dock->surface = NULL;
    }
}
