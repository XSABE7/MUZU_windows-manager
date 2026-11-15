#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wm.h"
#include "titlebar.h"

static void free_clients(Client **head) {
    if (!head) return;
    while (*head) {
        client_remove(head, (*head)->win);
    }
}

int main(void) {
    LayoutConfig cfg = {
        .screen_width = 1920,
        .screen_height = 1080,
        .master_count = 1,
        .top_inset = 0,
        .bottom_inset = 0,
        .current_workspace = 0
    };
    Client *head = NULL;

    // Test 1: Single window auto-maximize
    client_add(&head, 101, CLIENT_TILED);
    layout_calculate(&cfg, head, 1);
    assert(head->width == 1920);
    assert(head->height == 1080);
    assert(head->x == 0 && head->y == 0);

    // Test 2: Two windows 50:50 split
    client_add(&head, 102, CLIENT_TILED);
    layout_calculate(&cfg, head, 2);
    Client *first = client_find(head, 101);
    Client *second = client_find(head, 102);
    assert(first->width == 960);
    assert(second->width == 960);
    assert(first->x == 0 && second->x == 960);

    // Test 3: Three windows (Master 50% left, Stack 2x 50% right split)
    client_add(&head, 103, CLIENT_TILED);
    layout_calculate(&cfg, head, 3);
    Client *third = client_find(head, 103);
    assert(first->width == 960 && first->height == 1080);
    assert(second->width == 960 && second->height == 540);
    assert(third->width == 960 && third->height == 540);
    assert(third->y == 540);

    // Test 4: Drag-to-Float state transition and reflow
    Client *c = client_find(head, 101);
    assert(c->mode == CLIENT_TILED);
    c->mode = CLIENT_FLOATING;
    layout_calculate(&cfg, head, 2);
    assert(client_find(head, 102)->width == 960);
    assert(client_find(head, 103)->width == 960);

    Client *c3 = client_find(head, 103);
    c3->mode = CLIENT_FLOATING;
    layout_calculate(&cfg, head, 1);
    assert(client_find(head, 102)->width == 1920);

    // Test 5: Dynamic insets applied to layout
    cfg.top_inset = 32;
    cfg.bottom_inset = 48;
    client_find(head, 102)->mode = CLIENT_TILED;
    layout_calculate(&cfg, head, 1);
    Client *c2 = client_find(head, 102);
    assert(c2->y == 32);
    assert(c2->height == (1080 - 32 - 48));

    free_clients(&head);

    // Test 6: Slot assignment 1..10 for Super 1-0 recall
    Client *slot_head = NULL;
    client_add(&slot_head, 201, CLIENT_FLOATING);
    assert(client_find(slot_head, 201)->slot == 1);
    client_add(&slot_head, 202, CLIENT_FLOATING);
    assert(client_find(slot_head, 202)->slot == 2);
    assert(client_find_by_slot(slot_head, 1)->win == 201);
    assert(client_find_by_slot(slot_head, 2)->win == 202);

    // Slot reuse on window close
    client_remove(&slot_head, 201);
    client_add(&slot_head, 203, CLIENT_FLOATING);
    assert(client_find(slot_head, 203)->slot == 1); // slot 1 reused

    free_clients(&slot_head);

    printf("test_layout passed!\n");
    return 0;
}
