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

    // Scroll up beyond top clamps to 0
    launchpad_scroll(&lp, -300, 768);
    assert(lp.scroll_y == 0);

    printf("test_calendar passed!\n");
    return 0;
}
