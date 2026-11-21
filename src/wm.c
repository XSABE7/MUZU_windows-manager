#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include "wm.h"

void client_add(Client **head, Window win, ClientMode mode) {
    Client *c = malloc(sizeof(Client));
    if (!c) return;
    c->win = win;
    c->frame = None;
    c->x = 0;
    c->y = 0;
    c->width = 100;
    c->height = 100;
    c->mode = mode;
    c->snap_state = SNAP_NONE;
    c->is_csd = 0;
    c->is_minimized = 0;
    c->is_maximized = 0;
    c->is_fullscreen = 0;
    c->saved_x = 0;
    c->saved_y = 0;
    c->saved_width = 100;
    c->saved_height = 100;
    c->saved_mode = mode;
    c->saved_fs_x = 0;
    c->saved_fs_y = 0;
    c->saved_fs_w = 100;
    c->saved_fs_h = 100;
    c->saved_fs_mode = CLIENT_FLOATING;
    c->saved_fs_snap = SNAP_NONE;
    c->saved_fs_maximized = 0;
    c->title[0] = '\0';
    c->titlebar_surface = NULL;
    c->titlebar_cache = NULL;
    c->titlebar_cache_w = 0;
    c->titlebar_cache_focused = 0;
    c->icon_surface = NULL;
    c->last_unfocus_time = time(NULL);
    c->is_focused = 0;
    c->can_fullscreen = 1;
    c->workspace = 0;
    c->ws_hidden = 0;
    c->transient_for = None;

    int used_slots[16] = {0};
    for (Client *curr = *head; curr; curr = curr->next) {
        if (curr->slot >= 1 && curr->slot <= 10) {
            used_slots[curr->slot] = 1;
        }
    }
    c->slot = 1;
    for (int s = 1; s <= 10; s++) {
        if (!used_slots[s]) {
            c->slot = s;
            break;
        }
    }

    c->next = *head;
    *head = c;
}

void client_remove(Client **head, Window win) {
    if (win == None) return;
    Client **curr = head;
    while (*curr) {
        if ((*curr)->win == win || ((*curr)->frame != None && (*curr)->frame == win)) {
            Client *to_free = *curr;
            *curr = (*curr)->next;
            if (to_free->titlebar_surface) {
                cairo_surface_finish(to_free->titlebar_surface);
                cairo_surface_destroy(to_free->titlebar_surface);
                to_free->titlebar_surface = NULL;
            }
            if (to_free->titlebar_cache) {
                cairo_surface_destroy(to_free->titlebar_cache);
                to_free->titlebar_cache = NULL;
            }
            if (to_free->icon_surface) {
                cairo_surface_finish(to_free->icon_surface);
                cairo_surface_destroy(to_free->icon_surface);
                to_free->icon_surface = NULL;
            }
            free(to_free);
            return;
        }
        curr = &(*curr)->next;
    }
}

Client *client_find(Client *head, Window win) {
    if (win == None) return NULL;
    for (Client *curr = head; curr; curr = curr->next) {
        if (curr->win == win) return curr;
    }
    return NULL;
}

Client *client_find_by_any(Client *head, Window win) {
    if (win == None) return NULL;
    for (Client *curr = head; curr; curr = curr->next) {
        if (curr->win == win || (curr->frame != None && curr->frame == win)) return curr;
    }
    return NULL;
}

Client *client_find_by_slot(Client *head, int slot) {
    if (slot <= 0) return NULL;
    for (Client *curr = head; curr; curr = curr->next) {
        if (curr->slot == slot) return curr;
    }
    int idx = 1;
    for (Client *curr = head; curr; curr = curr->next, idx++) {
        if (idx == slot) return curr;
    }
    return NULL;
}

int client_count_tiled(Client *head) {
    int count = 0;
    for (Client *curr = head; curr; curr = curr->next) {
        if (curr->mode == CLIENT_TILED && !curr->is_minimized && !curr->is_fullscreen) count++;
    }
    return count;
}

void layout_calculate_clients(const LayoutConfig *cfg, Client **tiled, int tiled_count) {
    if (!cfg || !tiled || tiled_count <= 0) return;

    int base_x = cfg->mon_x + cfg->left_inset;
    int top_y = cfg->mon_y + cfg->top_inset;
    int avail_w = cfg->screen_width - cfg->left_inset - cfg->right_inset;
    int avail_h = cfg->screen_height - cfg->top_inset - cfg->bottom_inset;
    if (avail_w < 34) avail_w = 34;
    if (avail_h < 34) avail_h = 34;

    if (tiled_count == 1) {
        if (tiled[0]->snap_state == SNAP_LEFT) {
            int half_w = avail_w / 2;
            tiled[0]->x = base_x;
            tiled[0]->y = top_y;
            tiled[0]->width = half_w;
            tiled[0]->height = avail_h;
        } else if (tiled[0]->snap_state == SNAP_RIGHT) {
            int half_w = avail_w / 2;
            tiled[0]->x = base_x + half_w;
            tiled[0]->y = top_y;
            tiled[0]->width = avail_w - half_w;
            tiled[0]->height = avail_h;
        } else {
            tiled[0]->x = base_x;
            tiled[0]->y = top_y;
            tiled[0]->width = avail_w;
            tiled[0]->height = avail_h;
        }
    } else if (tiled_count == 2) {
        int half_w = avail_w / 2;
        if ((tiled[1]->snap_state == SNAP_LEFT && tiled[0]->snap_state != SNAP_LEFT) ||
            (tiled[0]->snap_state == SNAP_RIGHT && tiled[1]->snap_state != SNAP_RIGHT)) {
            Client *tmp = tiled[0];
            tiled[0] = tiled[1];
            tiled[1] = tmp;
        }

        tiled[0]->x = base_x;
        tiled[0]->y = top_y;
        tiled[0]->width = half_w;
        tiled[0]->height = avail_h;

        tiled[1]->x = base_x + half_w;
        tiled[1]->y = top_y;
        tiled[1]->width = avail_w - half_w;
        tiled[1]->height = avail_h;
    } else {
        int half_w = avail_w / 2;
        for (int i = 1; i < tiled_count; i++) {
            if (tiled[i]->snap_state == SNAP_LEFT && tiled[0]->snap_state != SNAP_LEFT) {
                Client *tmp = tiled[0];
                tiled[0] = tiled[i];
                tiled[i] = tmp;
                break;
            }
        }

        tiled[0]->x = base_x;
        tiled[0]->y = top_y;
        tiled[0]->width = half_w;
        tiled[0]->height = avail_h;

        int stack_count = tiled_count - 1;
        int stack_h = avail_h / stack_count;
        if (stack_h < 34) stack_h = 34;

        for (int i = 0; i < stack_count; i++) {
            Client *c = tiled[i + 1];
            c->x = base_x + half_w;
            c->y = top_y + i * stack_h;
            c->width = avail_w - half_w;
            if (i == stack_count - 1) {
                c->height = (top_y + avail_h) - c->y;
            } else {
                c->height = stack_h;
            }
            if (c->height < 34) c->height = 34;
        }
    }

    int bw = (cfg->border_width > 0) ? cfg->border_width : 0;
    if (bw > 0) {
        for (int i = 0; i < tiled_count; i++) {
            tiled[i]->width -= 2 * bw;
            tiled[i]->height -= 2 * bw;
            if (tiled[i]->width < 10) tiled[i]->width = 10;
            if (tiled[i]->height < 10) tiled[i]->height = 10;
        }
    }
}

void layout_calculate(const LayoutConfig *cfg, Client *head, int count) {
    (void)count;
    if (!cfg) return;
    Client *tiled[128];
    int idx = 0;
    for (Client *curr = head; curr && idx < 128; curr = curr->next) {
        if (curr->mode == CLIENT_TILED && !curr->is_minimized && !curr->is_fullscreen &&
            curr->workspace == cfg->current_workspace) {
            tiled[idx++] = curr;
        }
    }
    if (idx == 0) return;

    // Reverse so first added is master
    for (int i = 0; i < idx / 2; i++) {
        Client *tmp = tiled[i];
        tiled[i] = tiled[idx - 1 - i];
        tiled[idx - 1 - i] = tmp;
    }

    layout_calculate_clients(cfg, tiled, idx);
}

int wm_find_readable_path(const char *const *candidates, size_t count, char *out, size_t out_len) {
    if (!candidates || !out || out_len == 0) return 0;
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        if (candidates[i] && candidates[i][0] != '\0' && access(candidates[i], R_OK) == 0) {
            snprintf(out, out_len, "%s", candidates[i]);
            return 1;
        }
    }
    return 0;
}

int wm_wait_x11_or_signal(int x11_fd, int sig_fd) {
    struct pollfd pfds[2];
    nfds_t nfds = 1;
    pfds[0].fd = x11_fd;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;
    if (sig_fd >= 0) {
        pfds[1].fd = sig_fd;
        pfds[1].events = POLLIN;
        pfds[1].revents = 0;
        nfds = 2;
    }

    int pr = poll(pfds, nfds, -1);
    if (pr < 0) {
        return (errno == EINTR) ? 1 : -1;
    }
    if (nfds == 2 && (pfds[1].revents & POLLIN)) {
        return 1;
    }
    if (pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return -1;
    }
    return 0;
}

int wm_drag_commit_if_grabbed(DragState *drag, Client *c, int is_resize,
                              int start_x, int start_y, int grab_status) {
    if (!drag || !c || grab_status != GrabSuccess) return 0;
    drag->client = c;
    drag->is_resize = is_resize ? 1 : 0;
    drag->start_x = start_x;
    drag->start_y = start_y;
    drag->orig_x = c->x;
    drag->orig_y = c->y;
    drag->orig_w = c->width;
    drag->orig_h = c->height;
    return 1;
}

int wm_drag_prepare_resize(Client *c) {
    if (!c) return 0;
    int changed = 0;
    if (c->is_maximized) {
        c->is_maximized = 0;
        changed = 1;
    }
    if (c->mode == CLIENT_TILED || c->snap_state != SNAP_NONE) {
        c->mode = CLIENT_FLOATING;
        c->snap_state = SNAP_NONE;
        changed = 1;
    }
    return changed;
}

int wm_drag_should_cancel(const DragState *drag, const Client *c) {
    return (drag && c && drag->client == c) ? 1 : 0;
}

void wm_drag_reset(DragState *drag) {
    if (!drag) return;
    drag->client = NULL;
    drag->is_resize = 0;
}

int wm_should_focus_on_enter(int ffm_enabled, int enter_mode, int enter_detail,
                             int has_drag, int menu_visible, int alttab_visible,
                             const Client *c, int current_ws) {
    if (!ffm_enabled) return 0;
    if (enter_mode != NotifyNormal || enter_detail == NotifyInferior) return 0;
    if (has_drag || menu_visible || alttab_visible) return 0;
    if (!c || c->is_minimized || c->ws_hidden) return 0;
    if (c->workspace != current_ws || c->is_focused) return 0;
    return 1;
}

int wm_is_double_tap_within(Time prev_time, Time curr_time, int has_prev, unsigned long window_ms) {
    if (!has_prev) return 0;
    uint32_t delta = (uint32_t)curr_time - (uint32_t)prev_time;
    return ((unsigned long)delta <= window_ms) ? 1 : 0;
}
