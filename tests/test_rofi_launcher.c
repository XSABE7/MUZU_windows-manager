#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rofi_launcher.h"

int main(void) {
    RofiLauncher rofi = {0};
    rofi_launcher_init(NULL, &rofi, 1920, 1080, NULL);

    // Test 1: Desktop application scanning
    assert(rofi.app_count > 0);
    printf("Scanned %d desktop applications\n", rofi.app_count);

    // Initial state: empty query matches all apps
    assert(rofi.match_count == rofi.app_count);
    assert(rofi.selected_idx == 0);

    // Test 2: Query editing - append character
    rofi_launcher_query_append(&rofi, 't');
    assert(strcmp(rofi.query, "t") == 0);
    assert(rofi.match_count <= rofi.app_count);

    // Test 3: Query editing - backspace
    rofi_launcher_query_backspace(&rofi);
    assert(strcmp(rofi.query, "") == 0);
    assert(rofi.match_count == rofi.app_count);

    // Test 4: Substring filtering
    rofi_launcher_set_query(&rofi, "term");
    assert(rofi.match_count >= 0);
    // If any match found, verify it contains "term" in name or exec
    for (int i = 0; i < rofi.match_count; i++) {
        int app_idx = rofi.matches[i];
        assert(strcasestr(rofi.apps[app_idx].name, "term") != NULL ||
               strcasestr(rofi.apps[app_idx].exec, "term") != NULL);
    }

    // Test 5: Selection navigation
    rofi_launcher_set_query(&rofi, ""); // all apps
    assert(rofi.selected_idx == 0);
    rofi_launcher_select_next(&rofi);
    assert(rofi.selected_idx == 1);
    rofi_launcher_select_prev(&rofi);
    assert(rofi.selected_idx == 0);
    // Wrap around or clamp
    rofi_launcher_select_prev(&rofi);
    assert(rofi.selected_idx == rofi.match_count - 1); // wrap around to bottom
    rofi_launcher_select_next(&rofi);
    assert(rofi.selected_idx == 0); // wrap around to top

    // Test 6: Exec string sanitization
    char clean_exec[256];
    rofi_sanitize_exec("gnome-terminal -- %u", clean_exec, sizeof(clean_exec));
    assert(strcmp(clean_exec, "gnome-terminal --") == 0);

    rofi_sanitize_exec("/usr/bin/firefox -P %U %F", clean_exec, sizeof(clean_exec));
    assert(strcmp(clean_exec, "/usr/bin/firefox -P") == 0);

    // Test 7: Motion hover updates selection
    rofi.is_visible = 1;
    rofi.scroll_offset = 0;
    // Row 2 is at y = ROFI_HEADER_HEIGHT + 6 + 2 * ROFI_ROW_HEIGHT = 64 + 6 + 104 = 174
    rofi_launcher_handle_motion(NULL, &rofi, 200, 180);
    assert(rofi.selected_idx == 2);

    // Test 8: Click outside dismisses launcher
    rofi_launcher_handle_click(NULL, &rofi, Button1, -10, -10);
    assert(rofi.is_visible == 0);

    // Test 9: Drag-to-dock detection
    rofi.is_visible = 1;
    rofi_launcher_handle_press(NULL, &rofi, Button1, rofi.x + 200, rofi.y + 180);
    assert(rofi.is_pressing == 1);
    assert(rofi.is_dragging_out == 0);
    assert(rofi.drag_app_idx >= 0);

    // Small motion <= 8px does not trigger drag
    rofi_launcher_handle_motion(NULL, &rofi, 202, 182);
    assert(rofi.is_dragging_out == 0);

    // Large motion > 8px triggers drag-to-dock mode
    rofi_launcher_handle_motion(NULL, &rofi, 230, 230);
    assert(rofi.is_dragging_out == 1);
    rofi.is_pressing = 0;
    rofi.is_dragging_out = 0;

    // Test 10: Mouse wheel scrolling (Button5 = down, Button4 = up)
    rofi_launcher_set_query(&rofi, "");
    rofi.is_visible = 1;
    assert(rofi.selected_idx == 0);
    assert(rofi.scroll_offset == 0);

    // Scroll down with Button5 via handle_press
    rofi_launcher_handle_press(NULL, &rofi, Button5, rofi.x + 100, rofi.y + 100);
    assert(rofi.is_visible == 1);
    assert(rofi.selected_idx == 1);
    assert(rofi.scroll_offset == 1);

    // Scroll up with Button4 via handle_press
    rofi_launcher_handle_press(NULL, &rofi, Button4, rofi.x + 100, rofi.y + 100);
    assert(rofi.selected_idx == 0);
    assert(rofi.scroll_offset == 0);

    // Scrolling up at index 0 clamps at 0 (does not wrap to bottom)
    rofi_launcher_handle_press(NULL, &rofi, Button4, rofi.x + 100, rofi.y + 100);
    assert(rofi.selected_idx == 0);
    assert(rofi.scroll_offset == 0);

    // Scrolling even when cursor is outside rofi window does NOT close rofi
    rofi_launcher_handle_press(NULL, &rofi, Button5, 0, 0);
    assert(rofi.is_visible == 1);
    assert(rofi.selected_idx == 1);

    // Also test rofi_launcher_scroll directly
    rofi_launcher_scroll(&rofi, -1);
    assert(rofi.selected_idx == 0);
    assert(rofi.scroll_offset == 0);

    // Test: Continuous multi-step scrolling and motion stability
    rofi_launcher_set_query(&rofi, "");
    rofi.is_visible = 1;
    rofi.scroll_offset = 0;
    rofi.selected_idx = 0;

    for (int step = 0; step < 5; step++) {
        rofi_launcher_handle_press(NULL, &rofi, Button5, rofi.x + 100, rofi.y + 100);
    }
    assert(rofi.scroll_offset == 5);
    assert(rofi.selected_idx >= 5);

    // Mouse motion inside visible rows updates selection without reverting scroll_offset
    rofi_launcher_handle_motion(NULL, &rofi, 100, ROFI_HEADER_HEIGHT + 6 + 2 * ROFI_ROW_HEIGHT);
    assert(rofi.scroll_offset == 5);
    assert(rofi.selected_idx == 5 + 2);

    for (int step = 0; step < 3; step++) {
        rofi_launcher_handle_press(NULL, &rofi, Button4, rofi.x + 100, rofi.y + 100);
    }
    assert(rofi.scroll_offset == 2);

    // Test 11: Rasi config compatibility
    const char *r_path = rofi_launcher_get_config_path(&rofi);
    assert(r_path != NULL);
    assert(strstr(r_path, ".rasi") != NULL);
    if (rofi.has_custom_colors) {
        assert(rofi.bg_r >= 0.0 && rofi.bg_r <= 1.0);
        assert(rofi.fg_r >= 0.0 && rofi.fg_r <= 1.0);
        assert(rofi.accent_r >= 0.0 && rofi.accent_r <= 1.0);
        printf("Rofi loaded config: %s | font: '%s' | prompt: '%s'\n",
               r_path, rofi.font_name, rofi.prompt_text);
    }

    rofi_launcher_cleanup(&rofi);
    printf("test_rofi_launcher passed!\n");
    return 0;
}
