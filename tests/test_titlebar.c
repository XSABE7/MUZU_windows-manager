#include <assert.h>
#include <stdio.h>
#include "wm.h"
#include "titlebar.h"

int main(void) {
    Client c = {0};
    c.width = 800;
    c.height = 600;
    LayoutConfig cfg = { .screen_width = 1920, .screen_height = 1080 };

    // Test 1: Normal resizable client allows maximize
    c.can_fullscreen = 1;
    TitlebarAction act = titlebar_get_action_for_client(46, c.width, &c);
    assert(act == TITLEBAR_CLICK_MAXIMIZE);

    cfg.top_inset = 30;
    cfg.bottom_inset = 60;
    client_toggle_maximize(NULL, &c, &cfg);
    assert(c.is_maximized == 1);
    assert(c.y == 30);
    assert(c.height == (1080 - 30 - 60));

    // Test 2: Fixed-size / dialog window disables maximize - Review Focus #2
    Client dialog = {0};
    dialog.width = 400;
    dialog.height = 300;
    dialog.can_fullscreen = 0; // fixed size / dialog

    TitlebarAction d_act = titlebar_get_action_for_client(46, dialog.width, &dialog);
    assert(d_act == TITLEBAR_CLICK_NONE);

    client_toggle_maximize(NULL, &dialog, &cfg);
    assert(dialog.is_maximized == 0); // maximize strictly inhibited

    printf("test_titlebar passed!\n");
    return 0;
}
