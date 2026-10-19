#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "launchpad.h"

int main(void) {
    assert(days_in_month(2026, 2) == 28);
    assert(days_in_month(2024, 2) == 29); // Leap year
    assert(days_in_month(2026, 10) == 31);
    printf("test_calendar passed!\n");
    return 0;
}
