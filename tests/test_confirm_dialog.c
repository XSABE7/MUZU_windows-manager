#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "confirm_dialog.h"
#include "tint2_panel.h"

int main(void) {
    // Test 1: Which power actions require confirmation
    assert(power_action_needs_confirm(POWER_ACTION_SUSPEND) == 1);
    assert(power_action_needs_confirm(POWER_ACTION_REBOOT) == 1);
    assert(power_action_needs_confirm(POWER_ACTION_POWEROFF) == 1);
    assert(power_action_needs_confirm(POWER_ACTION_LOGOUT) == 1);
    assert(power_action_needs_confirm(POWER_ACTION_LOCK) == 0);
    assert(power_action_needs_confirm(0) == 0);

    // Test 2: Prompt texts per action
    ConfirmDialog d;
    confirm_dialog_init(NULL, &d, NULL);
    assert(d.is_visible == 0);

    confirm_dialog_prepare(&d, POWER_ACTION_POWEROFF, 1920, 1080);
    assert(d.action_id == POWER_ACTION_POWEROFF);
    assert(strcmp(d.title, "Power off now?") == 0);
    assert(strcmp(d.confirm_label, "Power Off") == 0);

    confirm_dialog_prepare(&d, POWER_ACTION_REBOOT, 1920, 1080);
    assert(strcmp(d.title, "Restart now?") == 0);
    assert(strcmp(d.confirm_label, "Restart") == 0);

    confirm_dialog_prepare(&d, POWER_ACTION_SUSPEND, 1920, 1080);
    assert(strcmp(d.title, "Suspend now?") == 0);
    assert(strcmp(d.confirm_label, "Suspend") == 0);

    confirm_dialog_prepare(&d, POWER_ACTION_LOGOUT, 1920, 1080);
    assert(strcmp(d.title, "Log out now?") == 0);
    assert(strcmp(d.confirm_label, "Log Out") == 0);
    assert(d.message[0] != '\0');

    // Test 3: Dialog is centered on screen
    assert(d.width == CONFIRM_DIALOG_WIDTH);
    assert(d.height == CONFIRM_DIALOG_HEIGHT);
    assert(d.x == (1920 - CONFIRM_DIALOG_WIDTH) / 2);
    assert(d.y == (1080 - CONFIRM_DIALOG_HEIGHT) / 2);

    // Test 4: Button hit testing (local coordinates)
    int bx, by, bw, bh;
    confirm_dialog_button_rect(&d, CONFIRM_BTN_CANCEL, &bx, &by, &bw, &bh);
    assert(confirm_dialog_button_at(&d, bx + bw / 2, by + bh / 2) == CONFIRM_BTN_CANCEL);
    confirm_dialog_button_rect(&d, CONFIRM_BTN_OK, &bx, &by, &bw, &bh);
    assert(confirm_dialog_button_at(&d, bx + bw / 2, by + bh / 2) == CONFIRM_BTN_OK);
    assert(confirm_dialog_button_at(&d, 2, 2) == CONFIRM_BTN_NONE);
    assert(confirm_dialog_button_at(&d, -5, 40) == CONFIRM_BTN_NONE);

    // Test 5: Hover tracking
    assert(confirm_dialog_handle_motion(NULL, &d, bx + 2, by + 2) == 1);
    assert(d.hover_btn == CONFIRM_BTN_OK);
    assert(confirm_dialog_handle_motion(NULL, &d, bx + 3, by + 3) == 0);
    assert(confirm_dialog_handle_motion(NULL, &d, 1, 1) == 1);
    assert(d.hover_btn == CONFIRM_BTN_NONE);

    confirm_dialog_cleanup(&d);
    printf("test_confirm_dialog: all tests passed\n");
    return 0;
}
