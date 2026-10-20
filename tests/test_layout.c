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

    // Test 7: 800x600 Compact Screen Layout & Dock
    LayoutConfig cfg_800 = { .screen_width = 800, .screen_height = 600, .master_count = 1 };
    Client *head_800 = NULL;

    // Single window auto-maximize on 800x600
    client_add(&head_800, 201, CLIENT_TILED);
    layout_calculate(&cfg_800, head_800, 1);
    assert(head_800->width == 800);
    assert(head_800->height == 600);
    assert(head_800->x == 0 && head_800->y == 0);

    // Two windows 50:50 split on 800x600
    client_add(&head_800, 202, CLIENT_TILED);
    layout_calculate(&cfg_800, head_800, 2);
    Client *w1 = client_find(head_800, 201);
    Client *w2 = client_find(head_800, 202);
    assert(w1->width == 400 && w1->height == 600);
    assert(w2->width == 400 && w2->height == 600);
    assert(w1->x == 0 && w2->x == 400);

    // Three windows (Master 400x600 left, Stack 2x 400x300 right)
    client_add(&head_800, 203, CLIENT_TILED);
    layout_calculate(&cfg_800, head_800, 3);
    Client *w3 = client_find(head_800, 203);
    assert(w1->width == 400 && w1->height == 600);
    assert(w2->width == 400 && w2->height == 300);
    assert(w3->width == 400 && w3->height == 300);
    assert(w3->y == 300);

    // Dock positioning on 800x600
    Dock dock_800 = {0};
    dock_calculate_geometry(&dock_800, DOCK_BOTTOM, 0, 800, 600);
    assert(dock_800.pos == DOCK_BOTTOM);
    assert(dock_800.y > 530);
    assert(dock_800.x == (800 - dock_800.width) / 2);

    dock_calculate_geometry(&dock_800, DOCK_TOP, 0, 800, 600);
    assert(dock_800.pos == DOCK_TOP);
    assert(dock_800.y == 6);

    dock_calculate_geometry(&dock_800, DOCK_LEFT, 0, 800, 600);
    assert(dock_800.pos == DOCK_LEFT);
    assert(dock_800.x == 6);
    assert(dock_800.y == (600 - dock_800.height) / 2);

    dock_calculate_geometry(&dock_800, DOCK_RIGHT, 0, 800, 600);
    assert(dock_800.pos == DOCK_RIGHT);
    assert(dock_800.x > 700);

    // Test 8: Dock items and click bounds
    Dock test_dock = {0};
    test_dock.size = 48;
    test_dock.item_count = 4;
    test_dock.hover_item = -1;
    dock_calculate_geometry(&test_dock, DOCK_BOTTOM, 0, 1000, 700);

    // Coordinate outside dock items should return -1
    assert(dock_get_item_at(&test_dock, 2, 24) == -1);

    // Center of items 0..3 should return 0..3
    int icon_size = 32;
    int margin = 8;
    int start_x = (test_dock.width - (4 * (icon_size + margin))) / 2;
    assert(dock_get_item_at(&test_dock, start_x + 16, 24) == 0);
    assert(dock_get_item_at(&test_dock, start_x + (icon_size + margin) + 16, 24) == 1);
    assert(dock_get_item_at(&test_dock, start_x + 2 * (icon_size + margin) + 16, 24) == 2);
    assert(dock_get_item_at(&test_dock, start_x + 3 * (icon_size + margin) + 16, 24) == 3);

    printf("test_layout passed!\n");
    return 0;
}
