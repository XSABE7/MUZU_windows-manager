#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alttab.h"
#include "wm.h"

int main(void) {
    AltTab at = {0};
    alttab_init(NULL, &at, NULL);

    // Test 1: Empty window list handling (Review Focus 1)
    alttab_populate(NULL, &at, NULL);
    assert(at.app_count == 0);
    assert(alttab_get_selected_window(&at) == None);

    // Test 2: Populate multiple clients from different apps
    // App 1: Terminal (3 windows)
    Client c1 = {.win = 101, .next = NULL};
    strncpy(c1.title, "Terminal 1", sizeof(c1.title) - 1);
    Client c2 = {.win = 102, .next = NULL};
    strncpy(c2.title, "Terminal 2", sizeof(c2.title) - 1);
    Client c3 = {.win = 103, .next = NULL};
    strncpy(c3.title, "Terminal 3", sizeof(c3.title) - 1);

    // App 2: Browser (1 window)
    Client c4 = {.win = 201, .next = NULL};
    strncpy(c4.title, "Mozilla Firefox", sizeof(c4.title) - 1);

    // App 3: Editor (12 windows - Review Focus 2: dots capping test)
    Client editor_clients[12];
    for (int i = 0; i < 12; i++) {
        editor_clients[i].win = 300 + i;
        snprintf(editor_clients[i].title, sizeof(editor_clients[i].title), "VS Code %d", i + 1);
        editor_clients[i].next = (i < 11) ? &editor_clients[i + 1] : NULL;
    }

    // Link all together: c1 -> c2 -> c3 -> c4 -> editor_clients[0]
    c1.next = &c2;
    c2.next = &c3;
    c3.next = &c4;
    c4.next = &editor_clients[0];

    alttab_populate(NULL, &at, &c1);

    assert(at.app_count == 3);
    assert(at.apps[0].win_count == 3);
    assert(at.apps[1].win_count == 1);
    assert(at.apps[2].win_count == 12);

    // Window retrieval
    assert(at.selected_idx == 0);
    assert(alttab_get_selected_window(&at) == 101);

    // Test 3: Navigation and wrapping
    alttab_select_next(&at);
    assert(at.selected_idx == 1);
    assert(alttab_get_selected_window(&at) == 201);

    alttab_select_next(&at);
    assert(at.selected_idx == 2);
    assert(alttab_get_selected_window(&at) == 300);

    // Wrap around forward
    alttab_select_next(&at);
    assert(at.selected_idx == 0);
    assert(alttab_get_selected_window(&at) == 101);

    // Wrap around backward
    alttab_select_prev(&at);
    assert(at.selected_idx == 2);
    alttab_select_prev(&at);
    assert(at.selected_idx == 1);

    // Test 4: Geometry calculation
    alttab_calculate_geometry(&at, 1920, 1080);
    assert(at.width >= 3 * ALTTAB_ITEM_WIDTH + ALTTAB_PADDING_X * 2);
    assert(at.height >= ALTTAB_ITEM_HEIGHT + ALTTAB_PADDING_Y * 2);
    assert(at.x == (1920 - at.width) / 2);
    assert(at.y == (1080 - at.height) / 2);

    alttab_cleanup(&at);
    printf("test_alttab passed!\n");
    return 0;
}
