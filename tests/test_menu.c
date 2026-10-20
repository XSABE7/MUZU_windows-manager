#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"

static int last_action = -1;
static void test_callback(int action_id, void *userdata) {
    (void)userdata;
    last_action = action_id;
}

int main(void) {
    Menu menu = {0};
    menu_init(NULL, &menu, NULL, test_callback, NULL);

    // Test 1: Add items
    menu_add_item(&menu, "Option 1", 101);
    menu_add_item(&menu, "Option 2", 102);
    menu_add_separator(&menu);
    menu_add_item(&menu, "Option 3", 103);

    assert(menu.item_count == 4);
    assert(strcmp(menu.items[0].label, "Option 1") == 0);
    assert(menu.items[2].is_separator == 1);

    // Test 2: Geometry calculation
    menu_calculate_geometry(&menu, 100, 200, 1920, 1080);
    assert(menu.x == 100);
    assert(menu.y == 200);
    assert(menu.width >= 160);
    assert(menu.height > 0);

    // Test 3: Screen clamping at bottom-right edge
    menu_calculate_geometry(&menu, 1900, 1050, 1920, 1080);
    assert(menu.x + menu.width <= 1920);
    assert(menu.y + menu.height <= 1080);

    // Test 4: Hit testing (window-relative coordinates)
    menu_calculate_geometry(&menu, 100, 100, 1920, 1080);
    int hit = menu_get_item_at(&menu, 10, MENU_PADDING_Y + 10);
    assert(hit == 0);

    // Separator hit should return -1
    int sep_y = MENU_PADDING_Y + 2 * MENU_ROW_HEIGHT + MENU_SEP_HEIGHT / 2;
    int sep_hit = menu_get_item_at(&menu, 10, sep_y);
    assert(sep_hit == -1);

    // Test 5: Action dispatch
    menu_select_item(&menu, 0);
    assert(last_action == 101);
    assert(menu.is_visible == 0); // Dismisses on select

    // Test 6: Dock settings menu positioned above bottom dock (dock_y = 1012)
    int dock_y = 1012;
    menu_calculate_geometry(&menu, 960, 1040, 1920, dock_y - 2);
    assert(menu.y + menu.height < dock_y);

    menu_cleanup(&menu);
    printf("test_menu passed!\n");
    return 0;
}
