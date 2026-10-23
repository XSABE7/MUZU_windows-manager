#ifndef MUZU_TRIGGER_H
#define MUZU_TRIGGER_H

typedef struct {
    unsigned long last_super_press_ms;
    int hot_corner_active;
} TriggerState;

int trigger_check_super(TriggerState *state, unsigned long event_time_ms);
int trigger_check_hot_corner(int x, int y, int threshold_px);

#endif /* MUZU_TRIGGER_H */
