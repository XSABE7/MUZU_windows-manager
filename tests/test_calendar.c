#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "launchpad.h"

int main(void) {
    assert(days_in_month(2026, 2) == 28);
    assert(days_in_month(2024, 2) == 29); // Leap year
    assert(days_in_month(2026, 10) == 31);

    // Test first weekday of October 2026: Oct 1, 2026 is Thursday (weekday 4 where Sunday=0)
    assert(first_weekday_of_month(2026, 10) == 4);

    // Test Launchpad vertical scrolling
    Launchpad lp = {0};
    lp.app_count = 60; // 20 rows of 3 apps
    launchpad_scroll(&lp, 60, 768);
    assert(lp.scroll_y == 60);

    launchpad_scroll(&lp, 120, 768);
    assert(lp.scroll_y == 180);

    // Test Launchpad vertical scrolling on 800x600 (height 600, 2 columns)
    Launchpad lp600 = {0};
    lp600.app_count = 60; // 30 rows of 2 apps
    launchpad_scroll(&lp600, 60, 600);
    assert(lp600.scroll_y == 60);
    launchpad_scroll(&lp600, 120, 600);
    assert(lp600.scroll_y == 180);
    launchpad_scroll(&lp600, -300, 600);
    assert(lp600.scroll_y == 0);

    // Test Launchpad app scanning and cleanup
    launchpad_scan_apps(&lp);
    // On Linux with desktop files, app_count should be >= 0 and each app should be clean
    for (int i = 0; i < lp.app_count; i++) {
        assert(lp.apps[i].name[0] != '\0');
        assert(lp.apps[i].exec[0] != '\0');
    }
    launchpad_cleanup(&lp);
    assert(lp.app_count == 0);

    // Test Launchpad safe dismissal and explicit close button
    Launchpad test_lp = {0};
    test_lp.is_visible = 1;

    // 1. Click on empty background / margin -> MUST NOT close
    launchpad_handle_click(NULL, &test_lp, Button1, 10, 10);
    assert(test_lp.is_visible == 1);

    // 2. Click on calendar area -> MUST NOT close
    launchpad_handle_click(NULL, &test_lp, Button1, 700, 200);
    assert(test_lp.is_visible == 1);

    // 3. Click on explicit close button ('✕' at top right: screen_w - 40, 40) -> MUST close
    launchpad_handle_click(NULL, &test_lp, Button1, 1000 - 40, 40);
    assert(test_lp.is_visible == 0);

    // 4. Non-Button1 clicks (Button3 right click, Button2 middle click) must NOT close
    test_lp.is_visible = 1;
    launchpad_handle_click(NULL, &test_lp, Button3, 1000 - 40, 40);
    assert(test_lp.is_visible == 1);

    launchpad_handle_click(NULL, &test_lp, Button2, 1000 - 40, 40);
    assert(test_lp.is_visible == 1);

    printf("test_calendar passed!\n");
    return 0;
}
