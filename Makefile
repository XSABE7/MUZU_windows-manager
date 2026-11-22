PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= /usr/share
XSESSIONSDIR ?= $(DATADIR)/xsessions

CC ?= gcc
CFLAGS ?= -Wall -Wextra -pedantic -std=c11 -O2
CFLAGS += -Iinclude $(shell pkg-config --cflags cairo x11 xext xcursor xrandr fontconfig)
LIBS = $(shell pkg-config --libs cairo x11 xext xcursor xrandr fontconfig) -lm

SRCS = src/wm.c src/titlebar.c src/client_props.c src/ewmh.c src/monitors.c src/workspace.c src/config_rules.c src/struts.c src/snap_preview.c src/menu.c src/alttab.c src/main.c
OBJS = $(SRCS:.c=.o)
MZML_SRCS = src/mzml.c src/mzml_main.c src/monitors.c src/struts.c
MZML_OBJS = $(MZML_SRCS:.c=.o)

all: muzuwm mzml

tests: test_struts test_titlebar test_snap_preview test_layout test_client_props test_ewmh test_monitors test_workspace test_config_rules test_menu test_alttab test_mzml

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

test_struts: tests/test_struts.c src/struts.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_titlebar: tests/test_titlebar.c src/titlebar.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_snap_preview: tests/test_snap_preview.c src/snap_preview.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_layout: tests/test_layout.c src/wm.c src/titlebar.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_client_props: tests/test_client_props.c src/client_props.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_ewmh: tests/test_ewmh.c src/ewmh.c src/wm.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_monitors: tests/test_monitors.c src/monitors.c src/struts.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_workspace: tests/test_workspace.c src/workspace.c src/wm.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_config_rules: tests/test_config_rules.c src/config_rules.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_menu: tests/test_menu.c src/menu.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_alttab: tests/test_alttab.c src/alttab.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_mzml: tests/test_mzml.c src/mzml.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

-include $(OBJS:.o=.d) $(MZML_OBJS:.o=.d)

muzuwm: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

mzml: $(MZML_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

asan: CFLAGS += -fsanitize=address -g -O1
asan: LIBS += -fsanitize=address
asan: clean all

clean:
	rm -f src/*.o src/*.d test_struts test_titlebar test_snap_preview test_layout test_client_props test_ewmh test_monitors test_workspace test_config_rules test_menu test_alttab test_mzml muzuwm mzml

install: muzuwm mzml
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 muzuwm $(DESTDIR)$(BINDIR)/muzuwm
	install -m 755 mzml $(DESTDIR)$(BINDIR)/mzml
	install -m 755 bin/muzu-session $(DESTDIR)$(BINDIR)/muzu-session
	install -m 755 bin/muzu $(DESTDIR)$(BINDIR)/muzu
	ln -sf muzu $(DESTDIR)$(BINDIR)/muzu-s
	install -m 755 muzuwm_settings.py $(DESTDIR)$(BINDIR)/muzuwm-settings
	install -m 755 muzuwm_updater.py $(DESTDIR)$(BINDIR)/muzuwm-updater
	install -d $(DESTDIR)$(XSESSIONSDIR)
	install -m 644 desktop/muzuwm.desktop $(DESTDIR)$(XSESSIONSDIR)/muzuwm.desktop
	install -d $(DESTDIR)/etc/muzuwm
	install -m 644 config/muzuwm.conf $(DESTDIR)/etc/muzuwm/muzuwm.conf

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/muzuwm
	rm -f $(DESTDIR)$(BINDIR)/mzml
	rm -f $(DESTDIR)$(BINDIR)/muzu-session
	rm -f $(DESTDIR)$(BINDIR)/muzu
	rm -f $(DESTDIR)$(BINDIR)/muzu-s
	rm -f $(DESTDIR)$(BINDIR)/muzuwm-settings
	rm -f $(DESTDIR)$(BINDIR)/muzuwm-updater
	rm -f $(DESTDIR)$(XSESSIONSDIR)/muzuwm.desktop
	rm -rf $(DESTDIR)/etc/muzuwm

.PHONY: all tests clean asan install uninstall
