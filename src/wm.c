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
    c->next = *head;
    *head = c;
}

void client_remove(Client **head, Window win) {
    Client **curr = head;
    while (*curr) {
        if ((*curr)->win == win) {
            Client *to_free = *curr;
            *curr = (*curr)->next;
            free(to_free);
            return;
        }
        curr = &(*curr)->next;
    }
}

Client *client_find(Client *head, Window win) {
    for (Client *curr = head; curr; curr = curr->next) {
        if (curr->win == win) return curr;
    }
    return NULL;
}

int client_count_tiled(Client *head) {
    int count = 0;
    for (Client *curr = head; curr; curr = curr->next) {
        if (curr->mode == CLIENT_TILED) count++;
    }
    return count;
}

void layout_calculate(const LayoutConfig *cfg, Client *head, int count) {
    (void)count; // Calculate directly from tiled clients list
    int tiled_count = client_count_tiled(head);
    if (tiled_count == 0) return;

    // Collect tiled clients in insertion order or list order
    // To preserve consistent layout, traverse list
    // Note: client_add pushes to head, so reversing or indexing:
    // Let's index tiled clients into a temporary array
    Client *tiled[128];
    int idx = 0;
    for (Client *curr = head; curr && idx < 128; curr = curr->next) {
        if (curr->mode == CLIENT_TILED) {
            tiled[idx++] = curr;
        }
    }
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
        for (int i = 0; i < stack_count; i++) {
            Client *c = tiled[i + 1];
            c->x = half_w;
            c->y = i * stack_h;
            c->width = cfg->screen_width - half_w;
            if (i == stack_count - 1) {
                c->height = cfg->screen_height - c->y; // Remainder
            } else {
                c->height = stack_h;
            }
        }
    }
}
