#include <assert.h>
#include <stdio.h>
#include "wm.h"
#include "snap_preview.h"

int main(void) {
    int screen_w = 1920, screen_h = 1080;
    int top_inset = 30, bottom_inset = 60, left_inset = 0, right_inset = 0;

    // Test 1: Edge detection zones
    assert(snap_preview_detect_zone(10, 500, screen_w, top_inset, SNAP_THRESHOLD) == SNAP_LEFT);
    assert(snap_preview_detect_zone(1915, 500, screen_w, top_inset, SNAP_THRESHOLD) == SNAP_RIGHT);
    assert(snap_preview_detect_zone(960, 35, screen_w, top_inset, SNAP_THRESHOLD) == SNAP_TOP);
    assert(snap_preview_detect_zone(500, 500, screen_w, top_inset, SNAP_THRESHOLD) == SNAP_NONE);

    // Test 2: Geometry calculation for SNAP_LEFT (50/50 split respecting insets)
    int x = 0, y = 0, w = 0, h = 0;
    snap_preview_calculate_rect(SNAP_LEFT, screen_w, screen_h,
                                left_inset, right_inset, top_inset, bottom_inset,
                                &x, &y, &w, &h);
    assert(x == 0 && y == 30);
    assert(w == 960 && h == (1080 - 30 - 60));

    // Test 3: Geometry calculation for SNAP_RIGHT
    snap_preview_calculate_rect(SNAP_RIGHT, screen_w, screen_h,
                                left_inset, right_inset, top_inset, bottom_inset,
                                &x, &y, &w, &h);
    assert(x == 960 && y == 30);
    assert(w == 960 && h == 990);

    // Test 4: Geometry calculation for SNAP_TOP (Maximize respecting insets)
    snap_preview_calculate_rect(SNAP_TOP, screen_w, screen_h,
                                left_inset, right_inset, top_inset, bottom_inset,
                                &x, &y, &w, &h);
    assert(x == 0 && y == 30);
    assert(w == 1920 && h == 990);

    // Test 5: Review Focus #3 (Interrupted drag hides preview cleanly)
    SnapPreview sp = { .is_visible = 1, .current_snap = SNAP_LEFT };
    snap_preview_hide(NULL, &sp);
    assert(sp.is_visible == 0 && sp.current_snap == SNAP_NONE);

    printf("test_snap_preview passed!\n");
    return 0;
}
