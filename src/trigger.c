#include "trigger.h"

int trigger_check_super(TriggerState *state, unsigned long event_time_ms) {
    if (!state) return 0;
    if (state->last_super_press_ms > 0 &&
        (event_time_ms >= state->last_super_press_ms) &&
        (event_time_ms - state->last_super_press_ms <= 250)) {
        state->last_super_press_ms = 0;
        return 1;
    }
    state->last_super_press_ms = event_time_ms;
    return 0;
}

int trigger_check_hot_corner(int x, int y, int threshold_px) {
    if (x >= 0 && x <= threshold_px && y >= 0 && y <= threshold_px) {
        return 1;
    }
    return 0;
}
