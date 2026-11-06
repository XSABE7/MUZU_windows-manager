#include <assert.h>
#include <stdio.h>
#include "struts.h"

int main(void) {
    StrutList *list = NULL;
    int left = 0, right = 0, top = 0, bottom = 0;

    // Test 1: Add top panel (height 32)
    StrutPartial top_panel = { .top = 32, .top_start_x = 0, .top_end_x = 1919 };
    strut_list_add_or_update(&list, 1001, &top_panel);
    strut_calculate_insets(list, 1920, 1080, &left, &right, &top, &bottom);
    assert(top == 32 && bottom == 0 && left == 0 && right == 0);

    // Test 2: Add bottom dock (height 64) - Review Focus #4 (Overlapping struts)
    StrutPartial bottom_dock = { .bottom = 64, .bottom_start_x = 400, .bottom_end_x = 1520 };
    strut_list_add_or_update(&list, 1002, &bottom_dock);
    strut_calculate_insets(list, 1920, 1080, &left, &right, &top, &bottom);
    assert(top == 32 && bottom == 64 && left == 0 && right == 0);

    // Test 3: Update existing strut
    top_panel.top = 40;
    strut_list_add_or_update(&list, 1001, &top_panel);
    strut_calculate_insets(list, 1920, 1080, &left, &right, &top, &bottom);
    assert(top == 40 && bottom == 64);

    // Test 4: Remove strut - Review Focus #1 (Window unmap/crash clears strut)
    strut_list_remove(&list, 1001);
    strut_calculate_insets(list, 1920, 1080, &left, &right, &top, &bottom);
    assert(top == 0 && bottom == 64);

    strut_list_remove(&list, 1002);
    strut_calculate_insets(list, 1920, 1080, &left, &right, &top, &bottom);
    assert(top == 0 && bottom == 0 && left == 0 && right == 0);

    // Test 5: Clamping bounds - Review Focus #4 (Never negative workarea)
    StrutPartial giant = { .top = 2000, .bottom = 2000 };
    strut_list_add_or_update(&list, 9999, &giant);
    strut_calculate_insets(list, 1920, 1080, &left, &right, &top, &bottom);
    assert(top <= 1080 && bottom <= 1080);
    strut_list_remove(&list, 9999);

    printf("test_struts passed!\n");
    return 0;
}
