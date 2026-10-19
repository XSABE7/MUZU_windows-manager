CC ?= gcc
CFLAGS ?= -Wall -Wextra -pedantic -std=c11 -O2
CFLAGS += -Iinclude $(shell pkg-config --cflags cairo x11 xext)
LIBS = $(shell pkg-config --libs cairo x11 xext) -lm

SRCS = src/wm.c src/titlebar.c src/trigger.c src/launchpad.c src/easter_egg.c src/dock.c src/main.c
OBJS = $(SRCS:.c=.o)

all: muzuwm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test_layout: tests/test_layout.c src/wm.c src/dock.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_trigger: tests/test_trigger.c src/trigger.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_easter_egg: tests/test_easter_egg.c src/easter_egg.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_calendar: tests/test_calendar.c src/launchpad.c src/easter_egg.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

muzuwm: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f src/*.o test_layout test_trigger test_easter_egg test_calendar muzuwm

.PHONY: all clean
