#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "wm.h"
#include "titlebar.h"

static void free_clients(Client **head) {
    if (!head) return;
    while (*head) {
        client_remove(head, (*head)->win);
    }
}

int main(void) {
    LayoutConfig cfg = {
        .screen_width = 1920,
        .screen_height = 1080,
        .master_count = 1,
        .top_inset = 0,
        .bottom_inset = 0,
        .current_workspace = 0
    };
    Client *head = NULL;

    // Test 1: Single window auto-maximize
    client_add(&head, 101, CLIENT_TILED);
    layout_calculate(&cfg, head, 1);
    assert(head->width == 1920);
    assert(head->height == 1080);
    assert(head->x == 0 && head->y == 0);

    // Test 2: Two windows 50:50 split
    client_add(&head, 102, CLIENT_TILED);
    layout_calculate(&cfg, head, 2);
    Client *first = client_find(head, 101);
    Client *second = client_find(head, 102);
    assert(first->width == 960);
    assert(second->width == 960);
    assert(first->x == 0 && second->x == 960);

    // Test 3: Three windows (Master 50% left, Stack 2x 50% right split)
    client_add(&head, 103, CLIENT_TILED);
    layout_calculate(&cfg, head, 3);
    Client *third = client_find(head, 103);
    assert(first->width == 960 && first->height == 1080);
    assert(second->width == 960 && second->height == 540);
    assert(third->width == 960 && third->height == 540);
    assert(third->y == 540);

    // Test 4: Drag-to-Float state transition and reflow
    Client *c = client_find(head, 101);
    assert(c->mode == CLIENT_TILED);
    c->mode = CLIENT_FLOATING;
    layout_calculate(&cfg, head, 2);
    assert(client_find(head, 102)->width == 960);
    assert(client_find(head, 103)->width == 960);

    Client *c3 = client_find(head, 103);
    c3->mode = CLIENT_FLOATING;
    layout_calculate(&cfg, head, 1);
    assert(client_find(head, 102)->width == 1920);

    // Test 5: Dynamic insets applied to layout
    cfg.top_inset = 32;
    cfg.bottom_inset = 48;
    client_find(head, 102)->mode = CLIENT_TILED;
    layout_calculate(&cfg, head, 1);
    Client *c2 = client_find(head, 102);
    assert(c2->y == 32);
    assert(c2->height == (1080 - 32 - 48));

    free_clients(&head);

    // Test 6: Slot assignment 1..10 for Super 1-0 recall
    Client *slot_head = NULL;
    client_add(&slot_head, 201, CLIENT_FLOATING);
    assert(client_find(slot_head, 201)->slot == 1);
    client_add(&slot_head, 202, CLIENT_FLOATING);
    assert(client_find(slot_head, 202)->slot == 2);
    assert(client_find_by_slot(slot_head, 1)->win == 201);
    assert(client_find_by_slot(slot_head, 2)->win == 202);

    // Slot reuse on window close
    client_remove(&slot_head, 201);
    client_add(&slot_head, 203, CLIENT_FLOATING);
    assert(client_find(slot_head, 203)->slot == 1); // slot 1 reused

    free_clients(&slot_head);

    // Test 7: Multi-monitor layout offset and left/right insets
    LayoutConfig mon_cfg = {
        .mon_x = 1920,
        .mon_y = 0,
        .screen_width = 1920,
        .screen_height = 1080,
        .master_count = 1,
        .left_inset = 10,
        .right_inset = 0,
        .top_inset = 32,
        .bottom_inset = 0,
        .current_workspace = 0
    };
    Client *mon_head = NULL;
    client_add(&mon_head, 301, CLIENT_TILED);
    layout_calculate(&mon_cfg, mon_head, 1);
    assert(mon_head->x == 1930 && mon_head->y == 32);
    assert(mon_head->width == 1910 && mon_head->height == 1048);
    free_clients(&mon_head);

    // Test 8: wm_find_readable_path skips leading NULL entries
    char tmp_path[] = "/tmp/muzu_cand_XXXXXX";
    int tmp_fd = mkstemp(tmp_path);
    assert(tmp_fd >= 0);
    close(tmp_fd);
    const char *cands[] = { NULL, NULL, tmp_path, NULL };
    char found_path[256] = {0};
    assert(wm_find_readable_path(cands, 4, found_path, sizeof(found_path)) == 1);
    assert(strcmp(found_path, tmp_path) == 0);
    const char *none_cands[] = { NULL, "/nonexistent/muzu_no_such_file" };
    assert(wm_find_readable_path(none_cands, 2, found_path, sizeof(found_path)) == 0);
    unlink(tmp_path);

    // Test 9: wm_wait_x11_or_signal wakes immediately on self-pipe byte
    int xp[2], sp[2];
    assert(pipe(xp) == 0 && pipe(sp) == 0);
    assert(write(sp[1], "x", 1) == 1);
    assert(wm_wait_x11_or_signal(xp[0], sp[0]) == 1);
    close(xp[0]); close(xp[1]);
    close(sp[0]); close(sp[1]);

    // Test 10: DragState grab check, resize detach, and cancel helpers
    Client *drag_head = NULL;
    client_add(&drag_head, 401, CLIENT_TILED);
    Client *dc = client_find(drag_head, 401);
    dc->snap_state = SNAP_LEFT;
    dc->is_maximized = 1;
    DragState ds = {0};
    assert(wm_drag_commit_if_grabbed(&ds, dc, 1, 10, 20, AlreadyGrabbed) == 0);
    assert(ds.client == NULL && ds.is_resize == 0);
    assert(wm_drag_commit_if_grabbed(&ds, dc, 1, 10, 20, GrabSuccess) == 1);
    assert(ds.client == dc && ds.is_resize == 1);
    assert(wm_drag_prepare_resize(dc) == 1);
    assert(dc->mode == CLIENT_FLOATING && dc->snap_state == SNAP_NONE && dc->is_maximized == 0);
    assert(wm_drag_should_cancel(&ds, dc) == 1);
    wm_drag_reset(&ds);
    assert(ds.client == NULL && ds.is_resize == 0);
    free_clients(&drag_head);

    // Test 11: border_width geometry compensation and small-screen clamping
    LayoutConfig bcfg = {
        .screen_width = 1920,
        .screen_height = 1080,
        .master_count = 1,
        .border_width = 2
    };
    Client *bhead = NULL;
    client_add(&bhead, 501, CLIENT_TILED);
    layout_calculate(&bcfg, bhead, 1);
    assert(bhead->x == 0 && bhead->y == 0);
    assert(bhead->width == 1916 && bhead->height == 1076);

    client_add(&bhead, 502, CLIENT_TILED);
    layout_calculate(&bcfg, bhead, 2);
    Client *bf1 = client_find(bhead, 501);
    Client *bf2 = client_find(bhead, 502);
    assert(bf1->width == 956 && bf1->height == 1076);
    assert(bf2->width == 956 && bf2->height == 1076);
    free_clients(&bhead);

    LayoutConfig tiny_cfg = {
        .screen_width = 40,
        .screen_height = 40,
        .master_count = 1,
        .border_width = 16
    };
    Client *thead = NULL;
    client_add(&thead, 503, CLIENT_TILED);
    layout_calculate(&tiny_cfg, thead, 1);
    assert(thead->width >= 10 && thead->height >= 10);
    free_clients(&thead);

    // Test 12: EnterNotify focus predicate and double-tap timestamp helper
    Client enter_c = {0};
    enter_c.workspace = 1;
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyAncestor, 0, 0, 0, &enter_c, 1) == 1);
    assert(wm_should_focus_on_enter(0, NotifyNormal, NotifyAncestor, 0, 0, 0, &enter_c, 1) == 0);
    assert(wm_should_focus_on_enter(1, NotifyGrab, NotifyAncestor, 0, 0, 0, &enter_c, 1) == 0);
    assert(wm_should_focus_on_enter(1, NotifyUngrab, NotifyAncestor, 0, 0, 0, &enter_c, 1) == 0);
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyInferior, 0, 0, 0, &enter_c, 1) == 0);
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyAncestor, 1, 0, 0, &enter_c, 1) == 0);
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyAncestor, 0, 1, 0, &enter_c, 1) == 0);
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyAncestor, 0, 0, 1, &enter_c, 1) == 0);
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyAncestor, 0, 0, 0, NULL, 1) == 0);
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyAncestor, 0, 0, 0, &enter_c, 0) == 0);
    enter_c.is_minimized = 1;
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyAncestor, 0, 0, 0, &enter_c, 1) == 0);
    enter_c.is_minimized = 0;
    enter_c.ws_hidden = 1;
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyAncestor, 0, 0, 0, &enter_c, 1) == 0);
    enter_c.ws_hidden = 0;
    enter_c.is_focused = 1;
    assert(wm_should_focus_on_enter(1, NotifyNormal, NotifyAncestor, 0, 0, 0, &enter_c, 1) == 0);

    assert(wm_is_double_tap_within(1000, 1400, 0, 500) == 0);
    assert(wm_is_double_tap_within(1000, 1400, 1, 500) == 1);
    assert(wm_is_double_tap_within(1000, 1500, 1, 500) == 1);
    assert(wm_is_double_tap_within(1000, 1501, 1, 500) == 0);
    assert(wm_is_double_tap_within((Time)0xFFFFFF00UL, (Time)100UL, 1, 500) == 1);
    assert(wm_is_double_tap_within((Time)0xFFFFFF00UL, (Time)300UL, 1, 500) == 0);

    printf("test_layout passed!\n");
    return 0;
}
