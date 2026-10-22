#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "kbd_layout.h"

int main(void) {
    // Test 1: Short names for common layout codes
    char buf[8];
    kbd_layout_short_name("us", buf, sizeof(buf));
    assert(strcmp(buf, "EN") == 0);
    kbd_layout_short_name("gb", buf, sizeof(buf));
    assert(strcmp(buf, "EN") == 0);
    kbd_layout_short_name("th", buf, sizeof(buf));
    assert(strcmp(buf, "TH") == 0);
    kbd_layout_short_name("de", buf, sizeof(buf));
    assert(strcmp(buf, "DE") == 0);
    kbd_layout_short_name("th(pat)", buf, sizeof(buf));
    assert(strcmp(buf, "TH") == 0);
    kbd_layout_short_name("", buf, sizeof(buf));
    assert(strcmp(buf, "??") == 0);

    // Test 2: Parse the XKB layout list "us,th,us"
    KbdLayouts kl = {0};
    kbd_layout_parse(&kl, "us,th,us");
    assert(kl.count == 3);
    assert(strcmp(kl.codes[0], "us") == 0);
    assert(strcmp(kl.codes[1], "th") == 0);
    assert(strcmp(kl.codes[2], "us") == 0);
    assert(strcmp(kl.short_names[0], "EN") == 0);
    assert(strcmp(kl.short_names[1], "TH") == 0);
    assert(kl.current_group == 0);

    // Test 3: Duplicate layouts are skipped when cycling (EN -> TH -> EN)
    assert(kbd_layout_is_unique(&kl, 0) == 1);
    assert(kbd_layout_is_unique(&kl, 1) == 1);
    assert(kbd_layout_is_unique(&kl, 2) == 0);
    assert(kbd_layout_next_group(&kl, 0) == 1);
    assert(kbd_layout_next_group(&kl, 1) == 0);
    // Sitting on the duplicate group 2 (same as "us") behaves like group 0
    assert(kbd_layout_next_group(&kl, 2) == 1);

    // Test 4: Current short name follows current group
    kl.current_group = 1;
    assert(strcmp(kbd_layout_current_short(&kl), "TH") == 0);
    kl.current_group = 2;
    assert(strcmp(kbd_layout_current_short(&kl), "EN") == 0);

    // Test 5: Single / empty layout lists
    kbd_layout_parse(&kl, "us");
    assert(kl.count == 1);
    assert(kbd_layout_next_group(&kl, 0) == 0);

    kbd_layout_parse(&kl, "");
    assert(kl.count == 0);
    assert(kbd_layout_next_group(&kl, 0) == 0);
    assert(strcmp(kbd_layout_current_short(&kl), "") == 0);

    kbd_layout_parse(&kl, NULL);
    assert(kl.count == 0);

    // Test 6: Whitespace tolerant and capped at KBD_MAX_GROUPS
    kbd_layout_parse(&kl, " us , th ,de,fr,ru");
    assert(kl.count == KBD_MAX_GROUPS);
    assert(strcmp(kl.codes[0], "us") == 0);
    assert(strcmp(kl.codes[1], "th") == 0);
    assert(strcmp(kl.short_names[2], "DE") == 0);
    assert(kbd_layout_next_group(&kl, 3) == 0);

    // Test 7: Human readable names for the layout menu
    assert(strcmp(kbd_layout_display_name("us"), "English (US)") == 0);
    assert(strcmp(kbd_layout_display_name("th"), "Thai") == 0);
    assert(strcmp(kbd_layout_display_name("th(pat)"), "Thai") == 0);
    assert(strcmp(kbd_layout_display_name("zz"), "zz") == 0);
    assert(KBD_MENU_ACTION_BASE > 305);

    printf("test_kbd_layout: all tests passed\n");
    return 0;
}
