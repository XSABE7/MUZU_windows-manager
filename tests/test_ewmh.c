#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xatom.h>
#include "wm.h"
#include "ewmh.h"

int main(void) {
    // Test 1: _NET_WM_STATE action resolution (0=REMOVE, 1=ADD, 2=TOGGLE)
    assert(ewmh_resolve_state_action(0, 1) == 0);
    assert(ewmh_resolve_state_action(0, 0) == 0);
    assert(ewmh_resolve_state_action(1, 0) == 1);
    assert(ewmh_resolve_state_action(1, 1) == 1);
    assert(ewmh_resolve_state_action(2, 0) == 1);
    assert(ewmh_resolve_state_action(2, 1) == 0);
    assert(ewmh_resolve_state_action(99, 1) == 1);
    assert(ewmh_resolve_state_action(-1, 0) == 0);

    // Test 2: _NET_WM_STATE atom flag parsing
    const Atom A_FS = 10, A_MH = 11, A_MV = 12, A_HID = 13, A_OTHER = 99;
    EwmhStateFlags f1 = ewmh_parse_state_atoms(A_FS, None, A_FS, A_MH, A_MV, A_HID);
    assert(f1.has_fullscreen == 1 && f1.has_max_horz == 0 && f1.has_max_vert == 0 && f1.has_hidden == 0);

    EwmhStateFlags f2 = ewmh_parse_state_atoms(A_MH, A_MV, A_FS, A_MH, A_MV, A_HID);
    assert(f2.has_fullscreen == 0 && f2.has_max_horz == 1 && f2.has_max_vert == 1 && f2.has_hidden == 0);

    EwmhStateFlags f3 = ewmh_parse_state_atoms(A_OTHER, A_HID, A_FS, A_MH, A_MV, A_HID);
    assert(f3.has_fullscreen == 0 && f3.has_hidden == 1);

    // Test 3 (Review Focus #4): Fullscreen preserves and restores pre-fullscreen tiled geometry
    LayoutConfig cfg = {
        .screen_width = 1920,
        .screen_height = 1080,
        .master_count = 1,
        .top_inset = 32,
        .bottom_inset = 48,
        .current_workspace = 0
    };
    Client *head = NULL;
    client_add(&head, 501, CLIENT_TILED);
    Client *c = client_find(head, 501);
    c->x = 0;
    c->y = 32;
    c->width = 960;
    c->height = 1000;
    c->snap_state = SNAP_LEFT;
    c->is_maximized = 0;

    ewmh_apply_fullscreen(NULL, c, &cfg, 1);
    assert(c->is_fullscreen == 1);
    assert(c->x == 0 && c->y == 0);
    assert(c->width == 1920 && c->height == 1080);

    ewmh_apply_fullscreen(NULL, c, &cfg, 0);
    assert(c->is_fullscreen == 0);
    assert(c->mode == CLIENT_TILED);
    assert(c->snap_state == SNAP_LEFT);
    assert(c->x == 0 && c->y == 32);
    assert(c->width == 960 && c->height == 1000);
    assert(c->is_maximized == 0);

    // Test 4: Fullscreen entered from maximized state restores is_maximized == 1
    c->is_maximized = 1;
    c->mode = CLIENT_FLOATING;
    c->snap_state = SNAP_TOP;
    c->x = 0;
    c->y = 32;
    c->width = 1920;
    c->height = 1000;

    ewmh_apply_fullscreen(NULL, c, &cfg, 1);
    assert(c->is_fullscreen == 1);
    assert(c->saved_fs_maximized == 1);

    ewmh_apply_fullscreen(NULL, c, &cfg, 0);
    assert(c->is_fullscreen == 0);
    assert(c->is_maximized == 1);
    assert(c->snap_state == SNAP_TOP);
    assert(c->y == 32 && c->height == 1000);

    // Test 5: Null-safe EWMH property sync & _NET_ACTIVE_WINDOW resolution
    ewmh_sync_client_state(NULL, None, 1, 0, 0);
    assert(ewmh_resolve_active_window(NULL) == None);
    c->is_minimized = 0;
    c->ws_hidden = 0;
    assert(ewmh_resolve_active_window(c) == 501);
    c->ws_hidden = 1;
    assert(ewmh_resolve_active_window(c) == None);
    c->ws_hidden = 0;
    c->is_minimized = 1;
    assert(ewmh_resolve_active_window(c) == None);
    ewmh_set_active_window(NULL, None, None);

    // Test 6: Property type/format validation & 64-bit long desktop index bounds
    Atom fs_atoms[2] = { 99, 10 };
    assert(ewmh_atoms_contain((const unsigned char *)fs_atoms, 2, XA_ATOM, 32, 10) == 1);
    assert(ewmh_atoms_contain((const unsigned char *)fs_atoms, 2, XA_CARDINAL, 32, 10) == 0);
    assert(ewmh_atoms_contain((const unsigned char *)fs_atoms, 2, XA_ATOM, 8, 10) == 0);
    assert(ewmh_atoms_contain((const unsigned char *)fs_atoms, 0, XA_ATOM, 32, 10) == 0);

    assert(ewmh_parse_desktop_index(2L, 4) == 2);
    assert(ewmh_parse_desktop_index(-1L, 4) == -1);
    assert(ewmh_parse_desktop_index(4L, 4) == -1);
    assert(ewmh_parse_desktop_index(0x100000000L, 4) == -1);

    // Test 7: Fullscreen ConfigureRequest guard & drag/maximize state helpers
    c->mode = CLIENT_FLOATING;
    c->is_maximized = 0;
    c->is_fullscreen = 0;
    assert(ewmh_allows_client_configure(c) == 1);
    assert(ewmh_can_drag_client(c) == 1);
    assert(ewmh_begin_move_clears_maximized(c) == 0);

    c->is_fullscreen = 1;
    assert(ewmh_allows_client_configure(c) == 0);
    assert(ewmh_can_drag_client(c) == 0);
    assert(ewmh_can_drag_client(NULL) == 0);

    c->is_fullscreen = 0;
    c->is_maximized = 1;
    assert(ewmh_allows_client_configure(c) == 0);
    assert(ewmh_begin_move_clears_maximized(c) == 1);
    assert(c->is_maximized == 0);
    assert(ewmh_allows_client_configure(c) == 1);

    c->mode = CLIENT_TILED;
    assert(ewmh_allows_client_configure(c) == 0);

    // Test 8: Fullscreen ignores border_width and fills exact monitor size
    LayoutConfig fs_bcfg = {
        .screen_width = 1920,
        .screen_height = 1080,
        .master_count = 1,
        .top_inset = 32,
        .bottom_inset = 48,
        .current_workspace = 0,
        .border_width = 4
    };
    c->mode = CLIENT_FLOATING;
    c->is_maximized = 0;
    c->is_fullscreen = 0;
    c->x = 100;
    c->y = 100;
    c->width = 500;
    c->height = 400;
    ewmh_apply_fullscreen(NULL, c, &fs_bcfg, 1);
    assert(c->is_fullscreen == 1);
    assert(c->x == 0 && c->y == 0 && c->width == 1920 && c->height == 1080);

    // Test 9: Null-safe EWMH extended helpers & directional _NET_WM_MOVERESIZE geometry
    ewmh_sync_client_state_ex(NULL, None, 1, 1, 0, 1);
    ewmh_set_frame_extents(NULL, None, 0, 0, 0, 0);
    ewmh_set_allowed_actions(NULL, None, 1);
    ewmh_set_wm_state(NULL, None, 1);
    assert(ewmh_window_has_maximized_state(NULL, None) == 0);

    int rx = 0, ry = 0, rw = 0, rh = 0;
    // Top-left resize moves x/y and shrinks w/h, clamped at min 100x60
    wm_compute_resize_rect(100, 100, 400, 300, 50, 30, WM_MOVERESIZE_SIZE_TOPLEFT, 100, 60, &rx, &ry, &rw, &rh);
    assert(rx == 150 && ry == 130 && rw == 350 && rh == 270);
    wm_compute_resize_rect(100, 100, 400, 300, 350, 280, WM_MOVERESIZE_SIZE_TOPLEFT, 100, 60, &rx, &ry, &rw, &rh);
    assert(rx == 400 && ry == 340 && rw == 100 && rh == 60);
    // Right/bottom resize keeps x/y fixed
    wm_compute_resize_rect(100, 100, 400, 300, -350, -280, WM_MOVERESIZE_SIZE_BOTTOMRIGHT, 100, 60, &rx, &ry, &rw, &rh);
    assert(rx == 100 && ry == 100 && rw == 100 && rh == 60);
    // Left-edge resize changes only x and width
    wm_compute_resize_rect(200, 150, 300, 200, -40, 25, WM_MOVERESIZE_SIZE_LEFT, 100, 60, &rx, &ry, &rw, &rh);
    assert(rx == 160 && ry == 150 && rw == 340 && rh == 200);

    client_remove(&head, 501);
    printf("test_ewmh passed!\n");
    return 0;
}
