#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dock.h"
#include "menu.h"

static int menu_has_action(const Menu *menu, int action_id) {
    for (int i = 0; i < menu->item_count; i++) {
        if (menu->items[i].action_id == action_id) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    Menu menu = {0};
    Dock dock = {0};
    dock.magnification_enabled = 1;
    dock.autohide_enabled = 0;
    dock.apps_at_left = 0;
    dock.pos = DOCK_BOTTOM;
    dock.size = 48;

    DockItem app_item = {0};
    strncpy(app_item.name, "TestApp", sizeof(app_item.name) - 1);
    strncpy(app_item.exec_cmd, "testapp", sizeof(app_item.exec_cmd) - 1);

    // Test 1: App menu population has app actions and NO dock settings actions
    menu_clear(&menu);
    dock_menu_populate_app(&menu, &app_item, 2);

    assert(menu_has_action(&menu, DOCK_ACTION_LAUNCH_APP));
    assert(menu_has_action(&menu, DOCK_ACTION_CLOSE_APP));
    assert(menu_has_action(&menu, DOCK_ACTION_KILL_APP));
    assert(menu_has_action(&menu, DOCK_ACTION_CLOSE_ALL_WINDOWS));
    assert(menu_has_action(&menu, DOCK_ACTION_DETACH_ALL_WINDOWS));
    assert(menu_has_action(&menu, DOCK_ACTION_REMOVE_APP));

    // Ensure zero dock settings in app menu
    assert(!menu_has_action(&menu, DOCK_ACTION_TOGGLE_MAGNIFICATION));
    assert(!menu_has_action(&menu, DOCK_ACTION_TOGGLE_AUTOHIDE));
    assert(!menu_has_action(&menu, DOCK_ACTION_POS_BOTTOM));
    assert(!menu_has_action(&menu, DOCK_ACTION_SIZE_MEDIUM));
    assert(!menu_has_action(&menu, DOCK_ACTION_TOGGLE_APPS_CORNER));

    // Test 2: Dock settings menu has dock settings actions and NO app actions
    menu_clear(&menu);
    dock_menu_populate_settings(&menu, &dock);

    assert(menu_has_action(&menu, DOCK_ACTION_TOGGLE_MAGNIFICATION));
    assert(menu_has_action(&menu, DOCK_ACTION_TOGGLE_AUTOHIDE));
    assert(menu_has_action(&menu, DOCK_ACTION_TOGGLE_APPS_CORNER));
    assert(menu_has_action(&menu, DOCK_ACTION_POS_BOTTOM));
    assert(menu_has_action(&menu, DOCK_ACTION_POS_LEFT));
    assert(menu_has_action(&menu, DOCK_ACTION_POS_RIGHT));
    assert(menu_has_action(&menu, DOCK_ACTION_SIZE_SMALL));
    assert(menu_has_action(&menu, DOCK_ACTION_SIZE_MEDIUM));
    assert(menu_has_action(&menu, DOCK_ACTION_SIZE_LARGE));

    // Ensure zero app actions in settings menu
    assert(!menu_has_action(&menu, DOCK_ACTION_LAUNCH_APP));
    assert(!menu_has_action(&menu, DOCK_ACTION_CLOSE_APP));
    assert(!menu_has_action(&menu, DOCK_ACTION_KILL_APP));
    assert(!menu_has_action(&menu, DOCK_ACTION_REMOVE_APP));

    printf("test_dock_menu passed!\n");
    return 0;
}
