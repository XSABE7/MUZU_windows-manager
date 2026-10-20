CC ?= gcc
CFLAGS ?= -Wall -Wextra -pedantic -std=c11 -O2
CFLAGS += -Iinclude $(shell pkg-config --cflags cairo x11 xext fontconfig gdk-pixbuf-2.0)
LIBS = $(shell pkg-config --libs cairo x11 xext fontconfig gdk-pixbuf-2.0) -lm

SRCS = src/wm.c src/titlebar.c src/trigger.c src/rofi_launcher.c src/dock.c src/client_props.c src/icon_fetcher.c src/gtk_theme.c src/wallpaper.c src/menu.c src/main.c
OBJS = $(SRCS:.c=.o)

all: muzuwm

tests: test_layout test_trigger test_rofi_launcher test_wallpaper test_client_props test_icon_fetcher test_gtk_theme test_menu test_x11_stress

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test_layout: tests/test_layout.c src/wm.c src/dock.c src/gtk_theme.c src/icon_fetcher.c src/rofi_launcher.c src/titlebar.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_trigger: tests/test_trigger.c src/trigger.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_rofi_launcher: tests/test_rofi_launcher.c src/rofi_launcher.c src/icon_fetcher.c src/gtk_theme.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_wallpaper: tests/test_wallpaper.c src/wallpaper.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_client_props: tests/test_client_props.c src/client_props.c src/icon_fetcher.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_icon_fetcher: tests/test_icon_fetcher.c src/icon_fetcher.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_gtk_theme: tests/test_gtk_theme.c src/gtk_theme.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_menu: tests/test_menu.c src/menu.c src/gtk_theme.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_x11_stress: tests/test_x11_stress.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

muzuwm: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

asan: CFLAGS += -fsanitize=address -g -O1
asan: LIBS += -fsanitize=address
asan: clean all

clean:
	rm -f src/*.o test_layout test_trigger test_rofi_launcher test_wallpaper test_client_props test_icon_fetcher test_gtk_theme test_menu test_x11_stress muzuwm

.PHONY: all tests clean asan
