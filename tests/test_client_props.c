#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
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

    // Test 5: Client opacity manipulation
    client_set_opacity(NULL, None, 0xA6AAAAAA); // Safe null check
    Display *test_dpy = XOpenDisplay(NULL);
    if (test_dpy) {
        Window root_w = DefaultRootWindow(test_dpy);
        Window test_win = XCreateSimpleWindow(test_dpy, root_w, 0, 0, 100, 100, 0, 0, 0);
        client_set_opacity(test_dpy, test_win, 0xA6AAAAAA);

        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = NULL;
        Atom net_opacity = XInternAtom(test_dpy, "_NET_WM_WINDOW_OPACITY", False);
        XGetWindowProperty(test_dpy, test_win, net_opacity, 0, 1, False, XA_CARDINAL,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop);
        assert(nitems == 1);
        assert((uint32_t)*(unsigned long *)prop == (uint32_t)0xA6AAAAAA);
        XFree(prop);

        // Reset opacity to full (removes property)
        client_set_opacity(test_dpy, test_win, 0xFFFFFFFF);
        XGetWindowProperty(test_dpy, test_win, net_opacity, 0, 1, False, XA_CARDINAL,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop);
        assert(nitems == 0);
        if (prop) XFree(prop);

        XDestroyWindow(test_dpy, test_win);
        XCloseDisplay(test_dpy);
    }

    // Test 6: Fullscreen / resize capability detection from WM_NORMAL_HINTS and dialog state
    assert(client_size_hints_allow_fullscreen(PMinSize, 200, 150, 0, 0, 0) == 1);
    assert(client_size_hints_allow_fullscreen(0, 0, 0, 0, 0, 1) == 0);
    assert(client_size_hints_allow_fullscreen(PMinSize | PMaxSize, 400, 300, 400, 300, 0) == 0);
    assert(client_size_hints_allow_fullscreen(PMaxSize, 100, 100, 500, 400, 0) == 0);
    assert(client_size_hints_allow_fullscreen(PMinSize | PMaxSize, 200, 150, 1920, 1080, 0) == 1);
    assert(client_supports_fullscreen(NULL, None) == 1);

    // Test 7: Initial geometry resolution preserves multi-window docked positions (e.g. Audacious)
    int rx = 0, ry = 0, rw = 0, rh = 0;
    // Audacious main player at (376, 142) size 425x100 (CSD)
    client_resolve_initial_geometry(1280, 700, 28, 0, 376, 142, 425, 100, 1, &rx, &ry, &rw, &rh);
    assert(rx == 376 && ry == 142 && rw == 425 && rh == 100);
    // Audacious playlist editor docked below at (526, 242) size 275x319 (CSD)
    int px = 0, py = 0, pw = 0, ph = 0;
    client_resolve_initial_geometry(1280, 700, 28, 0, 526, 242, 275, 319, 1, &px, &py, &pw, &ph);
    assert(px == 526 && py == 242 && pw == 275 && ph == 319);
    // Verify main player (y: 142..242) and playlist editor (y: 242..561) do not overlap vertically
    assert(ry + rh <= py);

    // Unpositioned window (0, 0) centers on workarea and preserves compact height (e.g. 325x112 dialog)
    client_resolve_initial_geometry(1280, 700, 28, 0, 0, 0, 325, 112, 0, &rx, &ry, &rw, &rh);
    assert(rw == 325 && rh == 112);
    assert(rx == (1280 - 325) / 2);
    assert(ry >= 28);

    printf("test_client_props passed!\n");
    return 0;
}
