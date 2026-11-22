#include <assert.h>
#include <stdio.h>
#include "wm.h"
#include "workspace.h"

static void free_clients(Client **head) {
    while (*head) {
        client_remove(head, (*head)->win);
    }
}

int main(void) {
    // Test 1: Workspace index bounds check
    assert(workspace_clamp_index(-1, 4) == -1);
    assert(workspace_clamp_index(4, 4) == -1);
    assert(workspace_clamp_index(0, 4) == 0);
    assert(workspace_clamp_index(3, 4) == 3);

    // Test 2 (Review Focus #1 & #5): Workspace switching marks inactive clients ws_hidden
    LayoutConfig cfg = {
        .mon_x = 0,
        .mon_y = 0,
        .screen_width = 1920,
        .screen_height = 1080,
        .master_count = 1,
        .current_workspace = 0
    };
    Client *head = NULL;
    client_add(&head, 101, CLIENT_TILED);
    client_add(&head, 102, CLIENT_FLOATING);
    client_add(&head, 103, CLIENT_TILED);

    Client *c1 = client_find(head, 101);
    Client *c2 = client_find(head, 102);
    Client *c3 = client_find(head, 103);
    c1->workspace = 0;
    c2->workspace = 1;
    c2->is_minimized = 1;
    c2->ws_hidden = 1;
    c3->workspace = 1;
    c3->ws_hidden = 1;

    Client *next_focus = NULL;
    assert(workspace_switch(NULL, None, head, &cfg, 1, 4, &next_focus) == 1);
    assert(cfg.current_workspace == 1);
    assert(c1->ws_hidden == 1);
    assert(c3->ws_hidden == 0);
    assert(c2->ws_hidden == 0);
    assert(c2->is_minimized == 1);
    assert(next_focus == c3);

    // Out-of-bounds and no-op same-workspace switches return 0 cleanly
    assert(workspace_switch(NULL, None, head, &cfg, 99, 4, &next_focus) == 0);
    assert(cfg.current_workspace == 1);
    assert(workspace_switch(NULL, None, head, &cfg, -1, 4, &next_focus) == 0);
    assert(cfg.current_workspace == 1);
    assert(workspace_switch(NULL, None, head, &cfg, 1, 4, &next_focus) == 0);

    // Test 3: Moving client to another workspace marks ws_hidden and rejects invalid target
    assert(workspace_move_client(NULL, c3, head, &cfg, 2, 4, &next_focus) == 1);
    assert(c3->workspace == 2);
    assert(c3->ws_hidden == 1);

    assert(workspace_move_client(NULL, c3, head, &cfg, -1, 4, &next_focus) == 0);
    assert(c3->workspace == 2);
    assert(workspace_move_client(NULL, c3, head, &cfg, 4, 4, &next_focus) == 0);
    assert(c3->workspace == 2);

    free_clients(&head);
    printf("test_workspace passed!\n");
    return 0;
}
