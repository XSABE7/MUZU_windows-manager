#!/bin/bash
set -e

echo "Testing session script syntax..."
bash -n bin/muzu-session

echo "Testing session script handles missing optional tools without legacy warnings..."
ERR_OUT=$(DISPLAY="" PATH="/nonexistent" /bin/sh -c "bin/muzu-session 2>&1" || true)
! echo "$ERR_OUT" | grep -qE "tint2|picom|rofi|plank"

echo "Checking config existence..."
test -f config/muzuwm.conf
grep -q "enable_alttab" config/muzuwm.conf
test ! -e config/picom.conf
test ! -e config/rofi/config.rasi
test ! -e config/panel/tint2rc
! grep -qE "picom|tint2|rofi|plank" bin/muzu-session
python3 -B -c "import py_compile; py_compile.compile('muzuwm_settings.py', '/tmp/muzuwm_settings_check.pyc', doraise=True); py_compile.compile('muzuwm_updater.py', '/tmp/muzuwm_updater_check.pyc', doraise=True); py_compile.compile('bin/muzu', '/tmp/muzu_cli_check.pyc', doraise=True)"
rm -f /tmp/muzuwm_settings_check.pyc /tmp/muzuwm_updater_check.pyc /tmp/muzu_cli_check.pyc
test -f desktop/muzuwm.desktop

echo "Testing DESTDIR install..."
DEST="/tmp/muzu_test_install"
rm -rf "$DEST"
make install DESTDIR="$DEST" PREFIX=/usr

test -x "$DEST/usr/bin/muzuwm"
test -x "$DEST/usr/bin/mzml"
test -x "$DEST/usr/bin/muzu-session"
test -x "$DEST/usr/bin/muzu"
test -x "$DEST/usr/bin/muzu-s"
test -x "$DEST/usr/bin/muzuwm-settings"
test -x "$DEST/usr/bin/muzuwm-updater"
test -f "$DEST/usr/share/xsessions/muzuwm.desktop"
test -d "$DEST/etc/muzuwm"
test ! -e "$DEST/etc/muzuwm/picom.conf"
test ! -e "$DEST/etc/muzuwm/tint2rc"
test ! -e "$DEST/etc/muzuwm/rofi"

"$DEST/usr/bin/muzu" --help >/dev/null
"$DEST/usr/bin/muzu" -v >/dev/null
"$DEST/usr/bin/muzu" --uninstall --yes --prefix "$DEST/usr" >/dev/null
test ! -e "$DEST/usr/bin/muzuwm"
test ! -e "$DEST/usr/bin/muzu"
test ! -e "$DEST/usr/bin/muzu-s"
test ! -e "$DEST/usr/bin/muzuwm-updater"
test ! -e "$DEST/usr/share/xsessions/muzuwm.desktop"
test ! -e "$DEST/etc/muzuwm"

rm -rf "$DEST"
echo "test_session_install passed!"
