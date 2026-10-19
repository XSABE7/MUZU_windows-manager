#include <assert.h>
#include <stdio.h>
#include "easter_egg.h"

int main(void) {
    EasterEggState st;
    easter_egg_init(&st);

    const char *seq = "wwssadadba";
    for (int i = 0; seq[i] != '\0'; i++) {
        int res = easter_egg_feed_key(&st, seq[i]);
        if (i == 9) assert(res == 1);
        else assert(res == 0);
    }
    assert(st.triggered == 1);

    // Random typing resets sequence
    easter_egg_init(&st);
    assert(easter_egg_feed_key(&st, 'w') == 0);
    assert(easter_egg_feed_key(&st, 'x') == 0);
    assert(easter_egg_feed_key(&st, 'w') == 0);
    assert(st.triggered == 0);

    printf("test_easter_egg passed!\n");
    return 0;
}
