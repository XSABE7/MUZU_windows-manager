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

    printf("test_client_props passed!\n");
    return 0;
}
