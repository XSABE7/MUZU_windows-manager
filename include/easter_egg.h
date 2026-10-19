#ifndef MUZU_EASTER_EGG_H
#define MUZU_EASTER_EGG_H

#define SEK_LOSO_URL "https://youtu.be/-3haoCPz6MM?si=-nqANYwpOqBDr81x"

typedef struct {
    char buffer[16];
    int length;
    int triggered;
} EasterEggState;

void easter_egg_init(EasterEggState *st);
int easter_egg_feed_key(EasterEggState *st, char key);
void easter_egg_launch_music(void);

#endif /* MUZU_EASTER_EGG_H */
