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

    // Test 3: Separator hit detection must return -1 (Review Focus 5)
    Dock dock_sep = {0};
    dock_sep.size = 64;
    dock_sep.magnification_enabled = 1;
    dock_sep.pos = DOCK_BOTTOM;
    dock_sep.width = 600;
    dock_sep.height = 64;
    dock_add_app(&dock_sep, "Term", "terminal", "term");
    dock_add_app(&dock_sep, "Web", "firefox", "web");
    dock_add_app(&dock_sep, "Trash", "__TRASH__", "trash");

    int icon_sz = 40;
    int m = 8;
    int reg_cnt = 2;
    int l_len = 3 * (icon_sz + m) + 18;
    int sx = (dock_sep.width - l_len) / 2;
    int dpos = sx + m / 2 + reg_cnt * (icon_sz + m) + 9;

    dock_sep.mouse_pos = (double)dpos;
    dock_sep.is_hovered = 1;
    assert(dock_get_item_at(&dock_sep, dpos, dock_sep.height / 2) == -1);
    assert(dock_get_item_at(&dock_sep, dpos + 1, dock_sep.height / 2) == -1);
    assert(dock_get_item_at(&dock_sep, dpos - 1, dock_sep.height / 2) == -1);

    // Test 4: Minimized client right-click populates app actions (Finding 4)
    DockItem min_item = {0};
    strncpy(min_item.name, "MinimizedWindow", sizeof(min_item.name) - 1);
    menu_clear(&menu);
    dock_menu_populate_app(&menu, &min_item, 1);
    assert(menu_has_action(&menu, DOCK_ACTION_LAUNCH_APP));
    assert(menu_has_action(&menu, DOCK_ACTION_CLOSE_APP));
    assert(!menu_has_action(&menu, DOCK_ACTION_TOGGLE_MAGNIFICATION));

    printf("test_dock_menu passed!\n");
    return 0;
}
