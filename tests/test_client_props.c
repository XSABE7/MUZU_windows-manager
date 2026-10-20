#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "client_props.h"

int main(void) {
    printf("Running test_client_props...\n");

    // Test 1: Helper protocol matching logic
    Atom test_protocols[] = { 10, 20, 30, 40 };
    assert(client_protocols_contain(test_protocols, 4, 30) == 1);
    assert(client_protocols_contain(test_protocols, 4, 99) == 0);
    assert(client_protocols_contain(NULL, 0, 30) == 0);

    // Test 2: Safe title truncation and fallback
    char title_buf[32];
    client_sanitize_title(title_buf, sizeof(title_buf), "Normal Title");
    assert(strcmp(title_buf, "Normal Title") == 0);

    // Empty or NULL string should fallback to default
    client_sanitize_title(title_buf, sizeof(title_buf), NULL);
    assert(strcmp(title_buf, "MUZUwm Window") == 0);
    client_sanitize_title(title_buf, sizeof(title_buf), "");
    assert(strcmp(title_buf, "MUZUwm Window") == 0);

    // Long string should be safely truncated
    const char *long_str = "This is a very long window title that exceeds thirty-two characters by quite a bit";
    client_sanitize_title(title_buf, sizeof(title_buf), long_str);
    assert(strlen(title_buf) == sizeof(title_buf) - 1);
    assert(title_buf[sizeof(title_buf) - 1] == '\0');

    // Test 3: Dialog geometry centering and clamping
    int x = 0, y = 0, w = 0, h = 0;
    client_calculate_dialog_geometry(1366, 768, 500, 300, &x, &y, &w, &h);
    assert(w == 500);
    assert(h == 300);
    assert(x == (1366 - 500) / 2);
    assert(y == (768 - 300) / 2);

    // Dialog uninitialized (0x0 or negative)
    client_calculate_dialog_geometry(1366, 768, 0, 0, &x, &y, &w, &h);
    assert(w == 600);
    assert(h == 400);
    assert(x == (1366 - 600) / 2);
    assert(y == (768 - 400) / 2);

    // Dialog oversized (larger than screen)
    client_calculate_dialog_geometry(1366, 768, 1600, 1000, &x, &y, &w, &h);
    assert(w <= 1366);
    assert(h <= 768);
    assert(x >= 0 && y >= 0);

    // Dialog on small screen (e.g. 800x600)
    client_calculate_dialog_geometry(800, 600, 0, 0, &x, &y, &w, &h);
    assert(w == 600);
    assert(h == 400);
    assert(x == 100 && y == 100);

    // Dialog on ultra-compact screen (e.g. 640x480) with oversized window
    client_calculate_dialog_geometry(640, 480, 800, 800, &x, &y, &w, &h);
    assert(w == 640 - 40);
    assert(h == 480 - 60);
    assert(x == 20 && y == 30);

    // Test 4: GTK CSD / Motif hints parsing
    assert(client_is_csd(NULL, None) == 0);

    // Case A: MWM hints with decorations = 0 (GNOME Files / Nautilus CSD)
    unsigned long csd_hints[] = { 0x3, 0x1, 0x0, 0x0, 0x0 }; // flags=0x3, func=0x1, decor=0x0
    assert(client_parse_motif_decorations(csd_hints, 5) == 0); // 0 decorations -> CSD

    // Case B: MWM hints with decorations = 1 (Standard decorations enabled)
    unsigned long decor_hints[] = { 0x3, 0x1, 0x1, 0x0, 0x0 };
    assert(client_parse_motif_decorations(decor_hints, 5) == 1); // Has decorations

    // Case C: MWM hints with MWM_HINTS_DECORATIONS flag unset
    unsigned long no_flag_hints[] = { 0x1, 0x1, 0x0, 0x0, 0x0 };
    assert(client_parse_motif_decorations(no_flag_hints, 5) == 1); // Default to decorated

    // Case D: Invalid/truncated hints
    assert(client_parse_motif_decorations(NULL, 0) == 1);
    assert(client_parse_motif_decorations(csd_hints, 2) == 1);

    printf("test_client_props passed!\n");
    return 0;
}
