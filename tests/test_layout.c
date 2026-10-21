#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wm.h"
#include "dock.h"
#include "titlebar.h"

static void free_clients(Client **head) {
    if (!head) return;
    while (*head) {
        client_remove(head, (*head)->win);
    }
}

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
    free_clients(&head);

    // Test 5: Dock 3-edge positioning geometry calculation (Top disabled, falls back to Bottom)
    Dock dock = {0};
    dock_calculate_geometry(&dock, DOCK_BOTTOM, 0, 1920, 1080);
    assert(dock.pos == DOCK_BOTTOM);
    assert(dock.y > 1000);
    assert(dock.x == (1920 - dock.width) / 2);

    dock_calculate_geometry(&dock, DOCK_TOP, 0, 1920, 1080);
    assert(dock.pos == DOCK_BOTTOM);
    assert(dock.y > 1000);

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
    free_clients(&many_head);

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
    free_clients(&head_800);

    // Dock positioning on 800x600
    Dock dock_800 = {0};
    dock_calculate_geometry(&dock_800, DOCK_BOTTOM, 0, 800, 600);
    assert(dock_800.pos == DOCK_BOTTOM);
    assert(dock_800.y > 530);
    assert(dock_800.x == (800 - dock_800.width) / 2);

    dock_calculate_geometry(&dock_800, DOCK_TOP, 0, 800, 600);
    assert(dock_800.pos == DOCK_BOTTOM);
    assert(dock_800.y > 530);

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

    // Test 9 (Reviewer CRIT-1 Fix): client_find_by_any and client_remove with None
    Client *csd_list = NULL;
    client_add(&csd_list, 999, CLIENT_TILED);
    csd_list->frame = None; // CSD window has no frame
    csd_list->is_csd = 1;

    // Searching for None must return NULL, NOT the CSD client!
    assert(client_find_by_any(csd_list, None) == NULL);
    assert(client_find(csd_list, None) == NULL);

    // client_remove with None must not remove the CSD client!
    client_remove(&csd_list, None);
    assert(client_find(csd_list, 999) != NULL);

    client_remove(&csd_list, 999);
    assert(csd_list == NULL);

    // Test 10: macOS Titlebar Traffic Light Controls & Maximize/Restore
    assert(titlebar_get_action(14, 500) == TITLEBAR_CLICK_CLOSE);     // Red dot (x ~ 14)
    assert(titlebar_get_action(30, 500) == TITLEBAR_CLICK_MINIMIZE);  // Yellow dot (x ~ 30)
    assert(titlebar_get_action(46, 500) == TITLEBAR_CLICK_MAXIMIZE);  // Green dot (x ~ 46)
    assert(titlebar_get_action(100, 500) == TITLEBAR_CLICK_DRAG);     // Titlebar body

    // Test maximize toggle
    Client test_client = {0};
    test_client.x = 100;
    test_client.y = 80;
    test_client.width = 600;
    test_client.height = 400;
    test_client.mode = CLIENT_FLOATING;

    // Toggle maximize -> expands to screen dimensions
    client_toggle_maximize(NULL, &test_client, &cfg);
    assert(test_client.is_maximized == 1);
    assert(test_client.x == 0 && test_client.y == 0);
    assert(test_client.width == 1920 && test_client.height == 1080);

    // Toggle again -> restores pre-maximize dimensions
    client_toggle_maximize(NULL, &test_client, &cfg);
    assert(test_client.is_maximized == 0);
    assert(test_client.x == 100 && test_client.y == 80);
    assert(test_client.width == 600 && test_client.height == 400);
    assert(test_client.mode == CLIENT_FLOATING);

    // Test 11: Window Minimization & Dock Integration
    {
        Dock min_dock = {0};
        min_dock.size = 48;
        min_dock.item_count = 4;
        dock_calculate_geometry(&min_dock, DOCK_BOTTOM, 0, 1000, 700);

        Client c_min = {0};
        c_min.win = 888;
        snprintf(c_min.title, sizeof(c_min.title), "TextEditor");

        assert(dock_add_minimized(&min_dock, &c_min) == 1);
        assert(min_dock.minimized_count == 1);
        assert(min_dock.minimized[0].client == &c_min);

        // Duplicate add should return 1 but not increase count
        assert(dock_add_minimized(&min_dock, &c_min) == 1);
        assert(min_dock.minimized_count == 1);

        // Capacity test: fill up to DOCK_MAX_MINIMIZED
        Client dummy_clients[16];
        for (int i = 1; i < 16; i++) {
            dummy_clients[i] = (Client){0};
            dummy_clients[i].win = 900 + i;
            assert(dock_add_minimized(&min_dock, &dummy_clients[i]) == 1);
        }
        assert(min_dock.minimized_count == 16);

        // 17th add should fail and return 0
        Client overflow_c = {0};
        overflow_c.win = 999;
        assert(dock_add_minimized(&min_dock, &overflow_c) == 0);
        assert(min_dock.minimized_count == 16);

        // Remove dummy clients to restore test state
        for (int i = 1; i < 16; i++) {
            dock_remove_minimized(&min_dock, &dummy_clients[i]);
        }
        assert(min_dock.minimized_count == 1);

        // Check hit testing on minimized item
        int icon_size = 32;
        int margin = 8;
        int launcher_w = min_dock.item_count * (icon_size + margin);
        int total_w = launcher_w + 16 + min_dock.minimized_count * (icon_size + margin);
        int start_x = (min_dock.width - total_w) / 2;
        int min_x = start_x + launcher_w + 16;
        int min_y = (min_dock.height - icon_size) / 2;

        Client *hit_c = dock_get_minimized_at(&min_dock, min_x + 10, min_y + 10);
        assert(hit_c == &c_min);

        // Outside hit test should return NULL
        assert(dock_get_minimized_at(&min_dock, 0, 0) == NULL);

        // Minimized window excluded from tiled reflow
        Client t1 = {0};
        Client t2 = {0};
        t1.mode = CLIENT_TILED;
        t2.mode = CLIENT_TILED;
        t1.next = &t2;
        t1.win = 101;
        t2.win = 102;

        // Both visible -> 50:50
        layout_calculate(&cfg, &t1, 0);
        assert(t1.width == cfg.screen_width / 2);
        assert(t2.width == cfg.screen_width - cfg.screen_width / 2);

        // Minimize t1 -> t2 gets full screen
        t1.is_minimized = 1;
        layout_calculate(&cfg, &t1, 0);
        assert(t2.width == cfg.screen_width);
        assert(t2.height == cfg.screen_height);

        // Unminimize t1 -> restores 50:50
        t1.is_minimized = 0;
        layout_calculate(&cfg, &t1, 0);
        assert(t1.width == cfg.screen_width / 2);

        dock_remove_minimized(&min_dock, &c_min);
        assert(min_dock.minimized_count == 0);
        assert(dock_get_minimized_at(&min_dock, min_x + 10, min_y + 10) == NULL);
    }

    // Test 12: macOS Dock UI, Magnification & Dynamic Pinning
    {
        char test_home[] = "/tmp/test_muzuwm_layout_XXXXXX";
        mkdtemp(test_home);
        setenv("HOME", test_home, 1);

        Dock d = {0};
        dock_init(NULL, &d, DOCK_BOTTOM, 1920, 1080, NULL);
        assert(d.pos == DOCK_BOTTOM);
        assert(d.item_count == 2); // Clean default: Trash + Apps (white 3-dots at right corner)
        assert(strcmp(d.items[0].name, "Trash") == 0);
        assert(strcmp(d.items[1].name, "Apps") == 0);
        assert(d.width < 120); // Tight fit around icons, no 260px empty gap

        // Add dynamic apps: Apps stays at right corner (last index), Trash right beside it
        int web_idx = dock_add_app(&d, "Web", "x-www-browser", "web-browser");
        int files_idx = dock_add_app(&d, "Files", "nautilus", "system-file-manager");
        assert(d.item_count == 4);
        assert(web_idx == 0);
        assert(files_idx == 1);
        assert(strcmp(d.items[2].name, "Trash") == 0);
        assert(strcmp(d.items[3].name, "Apps") == 0);

        // Test left-click drag reordering of pinned apps
        int new_idx = dock_reorder_item(&d, 0, 1);
        assert(new_idx == 1);
        assert(strcmp(d.items[0].name, "Files") == 0);
        assert(strcmp(d.items[1].name, "Web") == 0);
        assert(strcmp(d.items[3].name, "Apps") == 0); // Apps stays at right corner
        // Reorder back
        dock_reorder_item(&d, 1, 0);
        assert(strcmp(d.items[0].name, "Web") == 0);
        assert(strcmp(d.items[1].name, "Files") == 0);

        // Test configurable Apps 3-dots corner (Left corner vs Right corner only)
        dock_set_apps_corner(&d, 1);
        assert(d.apps_at_left == 1);
        assert(strcmp(d.items[0].name, "Apps") == 0); // Moved to Left corner!
        assert(strcmp(d.items[d.item_count - 1].name, "Trash") == 0);
        dock_set_apps_corner(&d, 0);
        assert(d.apps_at_left == 0);
        assert(strcmp(d.items[d.item_count - 1].name, "Apps") == 0); // Back at Right corner!

        // Test macOS magnification scale calculation
        d.magnification_enabled = 1;
        d.is_hovered = 1;
        d.mouse_pos = 100.0;
        // At exact mouse position, scale is peak ~1.45
        double peak_scale = dock_get_magnification_scale(&d, 100);
        assert(peak_scale >= 1.44 && peak_scale <= 1.46);
        // Far away from mouse position, scale is 1.0
        double flat_scale = dock_get_magnification_scale(&d, 500);
        assert(flat_scale == 1.0);

        dock_calculate_geometry(&d, DOCK_BOTTOM, 0, 1920, 1080);
        assert(d.width > d.height); // Horizontal

        dock_cycle_position(NULL, &d, 1920, 1080);
        assert(d.pos == DOCK_LEFT);
        assert(d.height > d.width); // Vertical

        // Top disabled, cycles directly to Right
        dock_cycle_position(NULL, &d, 1920, 1080);
        assert(d.pos == DOCK_RIGHT);
        assert(d.height > d.width); // Vertical

        // Cycle 3: Right -> Bottom
        dock_cycle_position(NULL, &d, 1920, 1080);
        assert(d.pos == DOCK_BOTTOM);
        assert(d.width > d.height); // Horizontal

        // Test running indicator updates
        Client app1 = {0};
        snprintf(app1.title, sizeof(app1.title), "Mozilla Firefox Web");
        dock_update_running(&d, &app1);
        assert(d.items[0].is_running == 1); // Web running
        assert(d.items[1].is_running == 0); // Files not running
        assert(d.items[2].is_running == 0); // Trash never marked running
        assert(d.items[3].is_running == 0); // Apps launcher never marked running

        // Remove dynamic app (Files at index 1)
        assert(dock_remove_app(&d, 1) == 1);
        assert(d.item_count == 3);
        // Cannot remove Trash (index 1) or Apps launcher (index 2)
        assert(dock_remove_app(&d, 1) == 0);
        assert(dock_remove_app(&d, 2) == 0);
        assert(d.item_count == 3);

        // Test Auto-Hide toggle and behavior
        // 1. Auto-hide disabled -> cursor moving away does NOT hide dock
        dock_set_autohide(&d, 0, 1920, 1080, NULL);
        assert(d.autohide_enabled == 0);
        assert(d.is_hidden == 0);
        dock_handle_motion(NULL, &d, 100, 100, 1920, 1080); // Far away
        assert(d.is_hidden == 0); // Stays visible!

        // 2. Auto-hide enabled -> cursor moving away hides dock
        dock_set_autohide(&d, 1, 1920, 1080, NULL);
        assert(d.autohide_enabled == 1);
        assert(d.is_hidden == 1);
        dock_handle_motion(NULL, &d, 960, 1075, 1920, 1080); // Trigger zone
        assert(d.is_hidden == 0); // Slid up
        dock_handle_motion(NULL, &d, 960, 500, 1920, 1080); // Outside trigger zone
        assert(d.is_hidden == 1); // Auto-hidden

        // 3. Settings persistence round-trip
        dock_set_autohide(&d, 0, 1920, 1080, NULL);
        Dock d2 = {0};
        dock_load_settings(&d2);
        assert(d2.autohide_enabled == 0);
        assert(d2.is_hidden == 0);

        dock_cleanup(&d);
    }

    // Test 13: Dock Calculate Nearest Edge for Drag-and-Snap
    {
        // Bottom: near (960, 1000) on 1920x1080 screen
        assert(dock_calculate_nearest_edge(960, 1000, 1920, 1080) == DOCK_BOTTOM);

        // Top edge disabled: near (960, 50) snaps to nearest valid edge (Left or Right, not Top)
        assert(dock_calculate_nearest_edge(960, 50, 1920, 1080) != DOCK_TOP);

        // Left: near (50, 540) on 1920x1080 screen
        assert(dock_calculate_nearest_edge(50, 540, 1920, 1080) == DOCK_LEFT);

        // Right: near (1880, 540) on 1920x1080 screen
        assert(dock_calculate_nearest_edge(1880, 540, 1920, 1080) == DOCK_RIGHT);
    }

    // Test 14: Window Snap to Left/Right Edge (Single Window & Multi-Window)
    {
        Client *snap_head = NULL;
        client_add(&snap_head, 501, CLIENT_TILED);

        // Single window default (SNAP_NONE) -> Fullscreen auto-maximize
        layout_calculate(&cfg, snap_head, 1);
        assert(snap_head->x == 0 && snap_head->y == 0);
        assert(snap_head->width == 1920 && snap_head->height == 1080);

        // Single window snapped to LEFT edge -> 50% left half
        snap_head->snap_state = SNAP_LEFT;
        layout_calculate(&cfg, snap_head, 1);
        assert(snap_head->x == 0 && snap_head->y == 0);
        assert(snap_head->width == 960 && snap_head->height == 1080);

        // Single window snapped to RIGHT edge -> 50% right half
        snap_head->snap_state = SNAP_RIGHT;
        layout_calculate(&cfg, snap_head, 1);
        assert(snap_head->x == 960 && snap_head->y == 0);
        assert(snap_head->width == 960 && snap_head->height == 1080);

        // Single window snapped to TOP edge -> 100% full screen
        snap_head->snap_state = SNAP_TOP;
        layout_calculate(&cfg, snap_head, 1);
        assert(snap_head->x == 0 && snap_head->y == 0);
        assert(snap_head->width == 1920 && snap_head->height == 1080);

        // Add second window where window 1 is snapped right and window 2 is snapped left
        client_add(&snap_head, 502, CLIENT_TILED);
        Client *w_first = client_find(snap_head, 501);
        Client *w_second = client_find(snap_head, 502);
        w_first->snap_state = SNAP_RIGHT;
        w_second->snap_state = SNAP_LEFT;

        layout_calculate(&cfg, snap_head, 2);
        assert(w_second->x == 0 && w_second->width == 960);
        assert(w_first->x == 960 && w_first->width == 960);

        // Drag-to-unmaximize behavior
        w_first->is_maximized = 1;
        w_first->saved_width = 800;
        w_first->saved_height = 600;
        handle_titlebar_drag(NULL, w_first, 200, 100, 1920, 1080);
        assert(w_first->is_maximized == 0);
        assert(w_first->width == 800 && w_first->height == 600);
        assert(w_first->x == 200 && w_first->y == 100);

        client_remove(&snap_head, 501);
        client_remove(&snap_head, 502);
    }

    // Test 15: Pre-rendered Liquid Glass Titlebar Surface
    {
        cairo_surface_t *cache = titlebar_pre_render(NULL, 640, 1, NULL);
        assert(cache != NULL);
        assert(cairo_surface_status(cache) == CAIRO_STATUS_SUCCESS);
        assert(cairo_image_surface_get_width(cache) == 640);
        assert(cairo_image_surface_get_height(cache) == TITLEBAR_HEIGHT);
        cairo_surface_destroy(cache);
    }

    // Test 16: Squircle Dock Initialization and Dimensions
    {
        Dock test_dock;
        dock_init(NULL, &test_dock, DOCK_BOTTOM, 1920, 1080, NULL);
        assert(test_dock.width > 0 && test_dock.height > 0);
        dock_cleanup(&test_dock);
    }

    // Test 17: Auto-hide Dock Render Bypass (Zero Overhead)
    {
        Dock auto_dock;
        dock_init(NULL, &auto_dock, DOCK_BOTTOM, 1920, 1080, NULL);
        auto_dock.autohide_enabled = 1;
        auto_dock.is_hidden = 1;
        dock_render(NULL, &auto_dock, 1920, 1080);
        assert(auto_dock.is_hidden == 1);
        dock_cleanup(&auto_dock);
    }

    // Test 18: Special Items Prominent Separator Math & Tooltip Functions
    {
        Dock sep_dock = {0};
        sep_dock.size = 48;
        sep_dock.apps_at_left = 0;
        dock_add_app(&sep_dock, "Trash", "__TRASH__", "user-trash");
        dock_add_app(&sep_dock, "Apps", "__ROFI__", "view-app-grid");
        dock_add_app(&sep_dock, "Firefox", "firefox", "firefox");
        dock_add_app(&sep_dock, "Terminal", "xterm", "utilities-terminal");
        assert(sep_dock.item_count == 4);

        // Apps is at right (apps_at_left == 0)
        // Items 0 and 1 are regular apps: Firefox, Terminal
        // Item 2 is Trash, Item 3 is Apps
        int c0 = dock_get_item_center_coord(&sep_dock, 0, 100, 32, 8);
        int c1 = dock_get_item_center_coord(&sep_dock, 1, 100, 32, 8);
        int c2 = dock_get_item_center_coord(&sep_dock, 2, 100, 32, 8);
        int c3 = dock_get_item_center_coord(&sep_dock, 3, 100, 32, 8);

        // Gap between index 0 and 1 is regular margin (32 + 8 = 40)
        assert(c1 - c0 == 40);
        // Gap between index 1 (last regular app) and index 2 (Trash) includes 18px separator gap (40 + 18 = 58)
        assert(c2 - c1 == 58);
        // Gap between index 2 (Trash) and index 3 (Apps) is regular margin (40)
        assert(c3 - c2 == 40);

        // Minimized item coordinates
        int mc0 = dock_get_minimized_center_coord(&sep_dock, 0, 500, 32, 8);
        assert(mc0 == 500 + 4 + 16);

        // Tooltip hide safety with NULL display
        dock_hide_tooltip(NULL, &sep_dock);
        dock_render_tooltip(NULL, &sep_dock, 1920, 1080);

        dock_cleanup(&sep_dock);
    }

    // Test dock top position removal & edge cycling
    {
        Dock d;
        memset(&d, 0, sizeof(d));
        d.size = 48;
        d.pos = DOCK_BOTTOM;

        dock_cycle_position(NULL, &d, 1920, 1080);
        assert(d.pos == DOCK_LEFT);
        dock_cycle_position(NULL, &d, 1920, 1080);
        assert(d.pos == DOCK_RIGHT);
        dock_cycle_position(NULL, &d, 1920, 1080);
        assert(d.pos == DOCK_BOTTOM);

        DockPosition nearest = dock_calculate_nearest_edge(960, 10, 1920, 1080);
        assert(nearest != DOCK_TOP);

        d.pos = DOCK_TOP;
        dock_calculate_geometry(&d, d.pos, 0, 1920, 1080);
        assert(d.pos == DOCK_BOTTOM);

        // Test dock EWMH helper setup
        dock_setup_ewmh(NULL, 0);
    }

    printf("test_layout passed!\n");
    return 0;
}
