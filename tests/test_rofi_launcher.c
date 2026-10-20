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

    rofi_launcher_cleanup(&rofi);
    printf("test_rofi_launcher passed!\n");
    return 0;
}
