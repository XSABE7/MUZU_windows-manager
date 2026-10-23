#ifndef MUZU_KBD_LAYOUT_H
#define MUZU_KBD_LAYOUT_H

#include <X11/Xlib.h>

/* XKB supports at most 4 layout groups */
#define KBD_MAX_GROUPS 4

/* Menu action ids for picking a layout: KBD_MENU_ACTION_BASE + group */
#define KBD_MENU_ACTION_BASE 401

typedef struct {
    int count;
    char codes[KBD_MAX_GROUPS][32];      /* e.g. "us", "th" */
    char short_names[KBD_MAX_GROUPS][8]; /* e.g. "EN", "TH" */
    int current_group;
    int xkb_event_base;                  /* -1 when XKB is unavailable */
} KbdLayouts;

/* Pure helpers (unit tested) */
void kbd_layout_short_name(const char *code, char *out, size_t out_len);
void kbd_layout_parse(KbdLayouts *kl, const char *layout_csv);
int kbd_layout_is_unique(const KbdLayouts *kl, int group);
int kbd_layout_next_group(const KbdLayouts *kl, int current);
const char *kbd_layout_current_short(const KbdLayouts *kl);
/* Human readable layout name, falls back to the code itself */
const char *kbd_layout_display_name(const char *code);

/* X11 / XKB glue */
int kbd_layout_init(Display *dpy, KbdLayouts *kl);
int kbd_layout_apply_layouts(const char *layouts);
void kbd_layout_reload(Display *dpy, KbdLayouts *kl);
void kbd_layout_set_group(Display *dpy, KbdLayouts *kl, int group);
void kbd_layout_cycle(Display *dpy, KbdLayouts *kl);
/* Returns 1 if ev was an XKB event that changed the layout state or names */
int kbd_layout_handle_event(Display *dpy, KbdLayouts *kl, const XEvent *ev);

#endif /* MUZU_KBD_LAYOUT_H */
