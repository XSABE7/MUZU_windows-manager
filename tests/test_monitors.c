#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "monitors.h"
#include "struts.h"

int main(void) {
    // Test 1 (Review Focus #3a): Deduplicate mirrored/identical outputs and drop empty CRTCs
    Monitor dup[4] = {
        { .x = 0, .y = 0, .width = 1920, .height = 1080, .primary = 0 },
        { .x = 0, .y = 0, .width = 0, .height = 0, .primary = 0 },
        { .x = 0, .y = 0, .width = 1920, .height = 1080, .primary = 1 },
        { .x = 1920, .y = 0, .width = 1920, .height = 1080, .primary = 0 }
    };
    int dedup_count = monitors_deduplicate(dup, 4);
    assert(dedup_count == 2);
    assert(dup[0].x == 0 && dup[0].width == 1920 && dup[0].primary == 1);
    assert(dup[1].x == 1920 && dup[1].width == 1920 && dup[1].primary == 0);

    // Test 2: Point and rectangle monitor lookup
    assert(monitor_find_for_point(dup, dedup_count, 500, 500) == 0);
    assert(monitor_find_for_point(dup, dedup_count, 2500, 500) == 1);
    assert(monitor_find_for_point(dup, dedup_count, -10, -10) == 0);

    // Window spanning Mon 0 (220px) and Mon 1 (380px) selects Mon 1
    assert(monitor_find_for_rect(dup, dedup_count, 1700, 100, 600, 400) == 1);
    // Window mostly on Mon 0
    assert(monitor_find_for_rect(dup, dedup_count, 1500, 100, 500, 400) == 0);

    // Test 3 (Review Focus #3b): Per-monitor _NET_WM_STRUT_PARTIAL isolation
    StrutList *struts = NULL;
    StrutPartial mon1_top_panel = {
        .top = 32, .top_start_x = 1920, .top_end_x = 3839
    };
    StrutPartial mon0_bottom_dock = {
        .bottom = 48, .bottom_start_x = 400, .bottom_end_x = 1520
    };
    strut_list_add_or_update(&struts, 1001, &mon1_top_panel);
    strut_list_add_or_update(&struts, 1002, &mon0_bottom_dock);

    monitor_apply_struts(dup, dedup_count, struts, 3840, 1080);
    assert(dup[0].top_inset == 0);
    assert(dup[0].bottom_inset == 48);
    assert(dup[1].top_inset == 32);
    assert(dup[1].bottom_inset == 0);

    strut_list_remove(&struts, 1001);
    strut_list_remove(&struts, 1002);

    // Test 4: Legacy 4-cardinal _NET_WM_STRUT applies only to outer screen edge monitors
    StrutPartial legacy_left = { .left = 64 };
    strut_list_add_or_update(&struts, 2001, &legacy_left);
    monitor_apply_struts(dup, dedup_count, struts, 3840, 1080);
    assert(dup[0].left_inset == 64);
    assert(dup[1].left_inset == 0);
    strut_list_remove(&struts, 2001);

    // Test 5: Safe NULL display handling
    Monitor fallback[MAX_MONITORS];
    assert(monitors_query_xrandr(NULL, None, fallback, MAX_MONITORS) == 0);

    // Test 6: Multi-monitor global _NET_WORKAREA spans all monitors minus global edge struts
    int gwx = 0, gwy = 0, gww = 0, gwh = 0;
    Monitor dual[2] = {
        { .x = 0, .y = 0, .width = 1920, .height = 1080, .primary = 1 },
        { .x = 1920, .y = 0, .width = 1280, .height = 720, .primary = 0 }
    };
    StrutList *gstruts = NULL;
    StrutPartial top_bar = { .top = 32, .top_start_x = 0, .top_end_x = 1919 };
    strut_list_add_or_update(&gstruts, 3001, &top_bar);
    monitor_apply_struts(dual, 2, gstruts, 3200, 1080);
    monitor_calculate_global_workarea(dual, 2, gstruts, 3200, 1080, &gwx, &gwy, &gww, &gwh);
    assert(gwx == 0 && gwy == 32 && gww == 3200 && gwh == 1048);
    strut_list_remove(&gstruts, 3001);

    // Single monitor honors that monitor's inset
    dual[0].left_inset = 10;
    dual[0].top_inset = 28;
    monitor_calculate_global_workarea(dual, 1, NULL, 1920, 1080, &gwx, &gwy, &gww, &gwh);
    assert(gwx == 10 && gwy == 28 && gww == 1910 && gwh == 1052);

    printf("test_monitors passed!\n");
    return 0;
}
