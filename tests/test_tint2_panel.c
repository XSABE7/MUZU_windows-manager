#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "tint2_panel.h"
#include "wm.h"

int main(void) {
    Tint2Panel panel = {0};
    tint2_panel_init(NULL, &panel, 1920, NULL);

    assert(panel.width == 1920);
    assert(panel.height == TINT2_PANEL_HEIGHT);
    assert(panel.current_workspace == 0);

    // Test 1: Config file generation
    const char *cfg_path = tint2_panel_get_config_path();
    assert(cfg_path != NULL);
    assert(strstr(cfg_path, "tint2rc") != NULL);
    FILE *f = fopen(cfg_path, "r");
    assert(f != NULL);
    fclose(f);

    // Test 2: Clock with seconds (HH:MM:SS) and Date formatting
    time_t sample_time = 1760962809; // Fixed timestamp
    tint2_panel_update_time(&panel, sample_time);
    assert(strlen(panel.time_str) == 8);
    assert(panel.time_str[2] == ':' && panel.time_str[5] == ':');
    assert(strlen(panel.date_str) > 6);
    printf("Formatted Panel Clock: '%s' | Date: '%s'\n", panel.time_str, panel.date_str);

    // Test 3: Workspaces manager tracking
    Client *clients = NULL;
    client_add(&clients, 1001, CLIENT_TILED);
    clients->workspace = 0;
    client_add(&clients, 1002, CLIENT_TILED);
    clients->workspace = 2;

    tint2_panel_update_workspaces(&panel, clients, 2, "Terminal");
    assert(panel.current_workspace == 2);
    assert(panel.workspace_has_windows[0] == 1);
    assert(panel.workspace_has_windows[1] == 0);
    assert(panel.workspace_has_windows[2] == 1);
    assert(panel.workspace_has_windows[3] == 0);
    assert(strcmp(panel.active_title, "Terminal") == 0);

    // Test 4: Hit testing for Workspaces, Clock, and Power Options button
    assert(tint2_panel_get_action_at(&panel, 20, 14) == TINT2_ACTION_WORKSPACE_1);
    assert(tint2_panel_get_action_at(&panel, 60, 14) == TINT2_ACTION_WORKSPACE_2);
    assert(tint2_panel_get_action_at(&panel, 100, 14) == TINT2_ACTION_WORKSPACE_3);
    assert(tint2_panel_get_action_at(&panel, 140, 14) == TINT2_ACTION_WORKSPACE_4);
    assert(tint2_panel_get_action_at(&panel, 1920 / 2, 14) == TINT2_ACTION_CLOCK);
    assert(tint2_panel_get_action_at(&panel, 1920 - 40, 14) == TINT2_ACTION_POWER);

    // Test 5: Hover state changes
    int changed = tint2_panel_handle_motion(NULL, &panel, 1920 - 40, 14);
    assert(changed == 1);
    assert(panel.hover_power == 1);

    changed = tint2_panel_handle_motion(NULL, &panel, 60, 14);
    assert(changed == 1);
    assert(panel.hover_power == 0);
    assert(panel.hover_workspace == 1);

    // Test 6: Per-workspace tiling with top_inset = TINT2_PANEL_HEIGHT
    LayoutConfig lcfg = {
        .screen_width = 1920,
        .screen_height = 1080,
        .master_count = 1,
        .top_inset = TINT2_PANEL_HEIGHT,
        .current_workspace = 0
    };
    layout_calculate(&lcfg, clients, 0);
    Client *c1 = client_find(clients, 1001);
    assert(c1->y == TINT2_PANEL_HEIGHT);
    assert(c1->height == 1080 - TINT2_PANEL_HEIGHT);
    assert(c1->width == 1920);

    while (clients) {
        client_remove(&clients, clients->win);
    }
    tint2_panel_cleanup(&panel);
    printf("test_tint2_panel passed!\n");
    return 0;
}
