#include <assert.h>
#include <stdio.h>
#include "wm.h"
#include "dock.h"

int main(void) {
    LayoutConfig cfg = { .screen_width = 1920, .screen_height = 1080, .master_count = 1 };
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

    // Test 5: Dock 4-edge positioning geometry calculation
    Dock dock = {0};
    dock_calculate_geometry(&dock, DOCK_BOTTOM, 0, 1920, 1080);
    assert(dock.pos == DOCK_BOTTOM);
    assert(dock.y > 1000);
    assert(dock.x == (1920 - dock.width) / 2);

    dock_calculate_geometry(&dock, DOCK_TOP, 0, 1920, 1080);
    assert(dock.pos == DOCK_TOP);
    assert(dock.y == 6);

    dock_calculate_geometry(&dock, DOCK_LEFT, 0, 1920, 1080);
    assert(dock.pos == DOCK_LEFT);
    assert(dock.x == 6);
    assert(dock.y == (1080 - dock.height) / 2);

    dock_calculate_geometry(&dock, DOCK_RIGHT, 0, 1920, 1080);
    assert(dock.pos == DOCK_RIGHT);
    assert(dock.x > 1800);

    // Test 6 (Reviewer Fix): Buffer overflow resistance with > 128 clients
    Client *many_head = NULL;
    for (int i = 1; i <= 150; i++) {
        client_add(&many_head, 1000 + i, CLIENT_TILED);
    }
    // Must safely clamp to buffer capacity without crashing
    layout_calculate(&cfg, many_head, 150);
    assert(client_find(many_head, 1001)->width > 0);

    printf("test_layout passed!\n");
    return 0;
}
