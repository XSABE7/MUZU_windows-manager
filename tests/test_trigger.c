#include <assert.h>
#include <stdio.h>
#include "trigger.h"

int main(void) {
    TriggerState state = {0};
    // 1st press
    assert(trigger_check_super(&state, 1000) == 0);
    // 2nd press within 220ms -> triggered!
    assert(trigger_check_super(&state, 1220) == 1);
    // Subsequent press after 400ms -> not triggered
    assert(trigger_check_super(&state, 1700) == 0);

    // Hot corner at (0,0) with 8px threshold
    assert(trigger_check_hot_corner(0, 0, 8) == 1);
    assert(trigger_check_hot_corner(5, 5, 8) == 1);
    assert(trigger_check_hot_corner(20, 20, 8) == 0);

    printf("test_trigger passed!\n");
    return 0;
}
