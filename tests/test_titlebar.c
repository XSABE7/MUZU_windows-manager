#include <assert.h>
#include <stdio.h>
#include "wm.h"
#include "titlebar.h"

int main(void) {
    Client c = {0};
    c.width = 800;
    c.height = 600;
    LayoutConfig cfg = { .screen_width = 1920, .screen_height = 1080 };

    // Test 1: Button positions for X, M, Z
    c.can_fullscreen = 1;
    // 'X' at x = 14 -> CLOSE
    assert(titlebar_get_action_for_client(14, c.width, &c) == TITLEBAR_CLICK_CLOSE);
    // 'M' at x = 30 -> MAXIMIZE
    assert(titlebar_get_action_for_client(30, c.width, &c) == TITLEBAR_CLICK_MAXIMIZE);
    // 'Z' at x = 46 -> MINIMIZE (make disappear)
    assert(titlebar_get_action_for_client(46, c.width, &c) == TITLEBAR_CLICK_MINIMIZE);

    cfg.top_inset = 30;
    cfg.bottom_inset = 60;
    client_toggle_maximize(NULL, &c, &cfg);
    assert(c.is_maximized == 1);
    assert(c.y == 30);
    assert(c.height == (1080 - 30 - 60));

    // Test 2: Fixed-size / dialog window disables 'M' (Maximize)
    Client dialog = {0};
    dialog.width = 400;
    dialog.height = 300;
    dialog.can_fullscreen = 0; // fixed size / dialog

    // 'X' still closes
    assert(titlebar_get_action_for_client(14, dialog.width, &dialog) == TITLEBAR_CLICK_CLOSE);
    // 'M' is disabled
    assert(titlebar_get_action_for_client(30, dialog.width, &dialog) == TITLEBAR_CLICK_NONE);
    // 'Z' still minimizes/disappears
    assert(titlebar_get_action_for_client(46, dialog.width, &dialog) == TITLEBAR_CLICK_MINIMIZE);

    client_toggle_maximize(NULL, &dialog, &cfg);
    assert(dialog.is_maximized == 0); // maximize strictly inhibited

    // Test 3: Maximize accounts for 2 * border_width
    Client bordered = {0};
    bordered.width = 800;
    bordered.height = 600;
    bordered.can_fullscreen = 1;
    LayoutConfig bcfg = {
        .screen_width = 1920,
        .screen_height = 1080,
        .top_inset = 30,
        .bottom_inset = 60,
        .border_width = 3
    };
    client_toggle_maximize(NULL, &bordered, &bcfg);
    assert(bordered.is_maximized == 1);
    assert(bordered.width == 1914);
    assert(bordered.height == (1080 - 30 - 60 - 6));

    printf("test_titlebar passed!\n");
    return 0;
}
