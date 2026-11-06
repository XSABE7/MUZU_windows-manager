#!/bin/bash
set -e

echo "Testing session script syntax..."
bash -n bin/muzu-session

echo "Checking config existence..."
test -f config/muzuwm.conf
test -f config/picom.conf
test -f config/rofi/config.rasi
test -f config/panel/tint2rc
test -f desktop/muzuwm.desktop

echo "Testing DESTDIR install..."
DEST="/tmp/muzu_test_install"
rm -rf "$DEST"
make install DESTDIR="$DEST" PREFIX=/usr

test -x "$DEST/usr/bin/muzuwm"
test -x "$DEST/usr/bin/muzu-session"
test -f "$DEST/usr/share/xsessions/muzuwm.desktop"
test -d "$DEST/etc/muzuwm"

rm -rf "$DEST"
echo "test_session_install passed!"
