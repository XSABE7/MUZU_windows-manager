#include <stdlib.h>
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
    c->is_csd = 0;
    c->is_minimized = 0;
    c->is_maximized = 0;
    c->saved_x = 0;
    c->saved_y = 0;
    c->saved_width = 100;
    c->saved_height = 100;
    c->saved_mode = mode;
    c->title[0] = '\0';
    c->titlebar_surface = NULL;
    c->icon_surface = NULL;
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

int client_count_tiled(Client *head) {
    int count = 0;
    for (Client *curr = head; curr; curr = curr->next) {
        if (curr->mode == CLIENT_TILED && !curr->is_minimized) count++;
    }
    return count;
}

void layout_calculate(const LayoutConfig *cfg, Client *head, int count) {
    (void)count;
    Client *tiled[128];
    int idx = 0;
    for (Client *curr = head; curr && idx < 128; curr = curr->next) {
        if (curr->mode == CLIENT_TILED && !curr->is_minimized) {
            tiled[idx++] = curr;
        }
    }
    if (idx == 0) return;
    int tiled_count = idx; // Clamp strictly to buffer capacity

    // Reverse so first added is master
    for (int i = 0; i < idx / 2; i++) {
        Client *tmp = tiled[i];
        tiled[i] = tiled[idx - 1 - i];
        tiled[idx - 1 - i] = tmp;
    }

    if (tiled_count == 1) {
        tiled[0]->x = 0;
        tiled[0]->y = 0;
        tiled[0]->width = cfg->screen_width;
        tiled[0]->height = cfg->screen_height;
    } else if (tiled_count == 2) {
        int half_w = cfg->screen_width / 2;
        tiled[0]->x = 0;
        tiled[0]->y = 0;
        tiled[0]->width = half_w;
        tiled[0]->height = cfg->screen_height;

        tiled[1]->x = half_w;
        tiled[1]->y = 0;
        tiled[1]->width = cfg->screen_width - half_w;
        tiled[1]->height = cfg->screen_height;
    } else {
        int half_w = cfg->screen_width / 2;
        tiled[0]->x = 0;
        tiled[0]->y = 0;
        tiled[0]->width = half_w;
        tiled[0]->height = cfg->screen_height;

        int stack_count = tiled_count - 1;
        int stack_h = cfg->screen_height / stack_count;
        if (stack_h < 34) stack_h = 34;

        for (int i = 0; i < stack_count; i++) {
            Client *c = tiled[i + 1];
            c->x = half_w;
            c->y = i * stack_h;
            c->width = cfg->screen_width - half_w;
            if (i == stack_count - 1) {
                c->height = cfg->screen_height - c->y;
            } else {
                c->height = stack_h;
            }
            if (c->height < 34) c->height = 34;
        }
    }
}
