#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include "easter_egg.h"

static const char TARGET_SEQ[] = "wwssadadba";

void easter_egg_init(EasterEggState *st) {
    if (!st) return;
    memset(st->buffer, 0, sizeof(st->buffer));
    st->length = 0;
    st->triggered = 0;
}

int easter_egg_feed_key(EasterEggState *st, char key) {
    if (!st) return 0;
    char lower = (char)tolower((unsigned char)key);

    if (st->length < 10) {
        st->buffer[st->length++] = lower;
    } else {
        memmove(st->buffer, st->buffer + 1, 9);
        st->buffer[9] = lower;
    }

    if (st->length == 10 && memcmp(st->buffer, TARGET_SEQ, 10) == 0) {
        st->triggered = 1;
        return 1;
    }
    return 0;
}

void easter_egg_launch_music(void) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        setsid();
        execlp("xdg-open", "xdg-open", SEK_LOSO_URL, (char *)NULL);
        _exit(1);
    }
}
