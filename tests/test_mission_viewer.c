#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mission_viewer.h"
#include "wm.h"

int main(void) {
    MissionViewer mv = {0};
    mission_viewer_init(NULL, &mv, NULL);

    int screen_w = 1920;
    int screen_h = 1080;

    // Test 1: Single window layout (Review Focus 3)
    Client c1 = {.win = 101, .workspace = 1, .next = NULL};
    strncpy(c1.title, "Single Window", sizeof(c1.title) - 1);

    mission_viewer_populate(NULL, &mv, &c1, 1);
    assert(mv.slot_count == 1);
    mission_viewer_calculate_layout(&mv, screen_w, screen_h);

    assert(mv.slots[0].w >= 300);
    assert(mv.slots[0].h >= 200);
    assert(mv.slots[0].x >= 0 && mv.slots[0].x + mv.slots[0].w <= screen_w);
    assert(mv.slots[0].y >= 0 && mv.slots[0].y + mv.slots[0].h <= screen_h);

    // Test 1b: Workspace 0 isolation (Finding 5)
    Client ws0_a = {.win = 11, .workspace = 0, .next = NULL};
    Client ws0_b = {.win = 12, .workspace = 0, .next = NULL};
    Client ws1_c = {.win = 13, .workspace = 1, .next = NULL};
    ws0_a.next = &ws0_b;
    ws0_b.next = &ws1_c;
    mission_viewer_populate(NULL, &mv, &ws0_a, 0);
    assert(mv.slot_count == 2);
    assert(mv.slots[0].win == 11);
    assert(mv.slots[1].win == 12);

    // Test 2: 4 windows (2x2 grid)
    Client clients[4];
    for (int i = 0; i < 4; i++) {
        clients[i].win = 200 + i;
        clients[i].workspace = 1;
        snprintf(clients[i].title, sizeof(clients[i].title), "Window %d", i + 1);
        clients[i].next = (i < 3) ? &clients[i + 1] : NULL;
    }
    mission_viewer_populate(NULL, &mv, &clients[0], 1);
    assert(mv.slot_count == 4);
    mission_viewer_calculate_layout(&mv, screen_w, screen_h);

    for (int i = 0; i < 4; i++) {
        assert(mv.slots[i].w > 0);
        assert(mv.slots[i].h > 0);
        assert(mv.slots[i].x >= 0 && mv.slots[i].x + mv.slots[i].w <= screen_w);
        assert(mv.slots[i].y >= 0 && mv.slots[i].y + mv.slots[i].h <= screen_h);
    }
    // Verify no overlap between slot 0 and slot 1
    assert(mv.slots[0].x + mv.slots[0].w <= mv.slots[1].x || mv.slots[0].y + mv.slots[0].h <= mv.slots[1].y);

    // Test 3: 7 windows layout
    Client clients7[7];
    for (int i = 0; i < 7; i++) {
        clients7[i].win = 300 + i;
        clients7[i].workspace = 1;
        snprintf(clients7[i].title, sizeof(clients7[i].title), "Win %d", i + 1);
        clients7[i].next = (i < 6) ? &clients7[i + 1] : NULL;
    }
    mission_viewer_populate(NULL, &mv, &clients7[0], 1);
    assert(mv.slot_count == 7);
    mission_viewer_calculate_layout(&mv, screen_w, screen_h);

    for (int i = 0; i < 7; i++) {
        assert(mv.slots[i].w > 0);
        assert(mv.slots[i].h > 0);
        assert(mv.slots[i].x >= 0 && mv.slots[i].x + mv.slots[i].w <= screen_w);
        assert(mv.slots[i].y >= 0 && mv.slots[i].y + mv.slots[i].h <= screen_h);
    }

    // Test 4: Hit detection
    int center_x = mv.slots[2].x + mv.slots[2].w / 2;
    int center_y = mv.slots[2].y + mv.slots[2].h / 2;
    assert(mission_viewer_get_slot_at(&mv, center_x, center_y) == 2);
    assert(mission_viewer_get_slot_at(&mv, -10, -10) == -1);

    // Test 5: Navigation and selection
    mv.selected_idx = 0;
    assert(mission_viewer_get_selected(&mv) == &clients7[0]);

    mission_viewer_select_next(&mv);
    assert(mv.selected_idx == 1);
    assert(mission_viewer_get_selected(&mv) == &clients7[1]);

    mission_viewer_select_prev(&mv);
    assert(mv.selected_idx == 0);

    // Wrap around backward
    mission_viewer_select_prev(&mv);
    assert(mv.selected_idx == 6);
    assert(mission_viewer_get_selected(&mv) == &clients7[6]);

    mission_viewer_cleanup(&mv);
    printf("test_mission_viewer passed!\n");
    return 0;
}
