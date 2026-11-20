#!/usr/bin/env bash
set -euo pipefail

if ! command -v Xvfb >/dev/null 2>&1; then
    echo "SKIP: Xvfb not installed"
    exit 0
fi

TMP_DIR=$(mktemp -d)
XVFB_PID=""
WM1_PID=""
WM2_PID=""

cleanup() {
    set +e
    [ -n "$WM2_PID" ] && kill -9 "$WM2_PID" 2>/dev/null || true
    [ -n "$WM1_PID" ] && kill -9 "$WM1_PID" 2>/dev/null || true
    [ -n "$XVFB_PID" ] && kill -9 "$XVFB_PID" 2>/dev/null || true
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

# Pick a free display :90..:99
DISP=""
for d in $(seq 90 99); do
    if [ ! -e "/tmp/.X${d}-lock" ] && [ ! -e "/tmp/.X11-unix/X${d}" ]; then
        DISP=":$d"
        break
    fi
done
[ -n "$DISP" ] || { echo "FAIL: no free Xvfb display :90..:99"; exit 1; }

CFLAGS=$(pkg-config --cflags cairo x11 xext xcursor xrandr fontconfig)
LIBS="$(pkg-config --libs cairo x11 xext xcursor xrandr fontconfig) -lm"
SRCS="src/wm.c src/titlebar.c src/client_props.c src/ewmh.c src/monitors.c src/workspace.c src/config_rules.c src/struts.c src/snap_preview.c src/menu.c src/alttab.c src/main.c"

echo "Building ASan+UBSan muzuwm in $TMP_DIR..."
gcc -Wall -Wextra -pedantic -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude \
    $CFLAGS $SRCS -o "$TMP_DIR/muzuwm_asan" -fsanitize=address,undefined $LIBS

export HOME="$TMP_DIR/home"
mkdir -p "$HOME/.config/muzuwm"
printf '/bin/true\n' > "$HOME/.config/muzuwm/auto_allowlist"

Xvfb "$DISP" -screen 0 1280x720x24 >/dev/null 2>&1 &
XVFB_PID=$!
sleep 0.4

export DISPLAY="$DISP"
printf 'leak:_XEnq\n' > "$TMP_DIR/lsan.supp"
export LSAN_OPTIONS="suppressions=$TMP_DIR/lsan.supp"

# 1. Start primary WM
"$TMP_DIR/muzuwm_asan" >"$TMP_DIR/wm1.out" 2>"$TMP_DIR/wm1.err" &
WM1_PID=$!
sleep 0.5
kill -0 "$WM1_PID" 2>/dev/null || { echo "FAIL: WM1 exited prematurely"; cat "$TMP_DIR/wm1.err"; exit 1; }

# 1b. Verify no zombie child processes remain after auto_allowlist /bin/true exits
ZOMBIES=$(ps -eo ppid=,stat= | awk -v p="$WM1_PID" '$1 == p && $2 ~ /Z/ { count++ } END { print count+0 }')
if [ "$ZOMBIES" -ne 0 ]; then
    echo "FAIL: Found $ZOMBIES zombie child process(es) under WM1"
    exit 1
fi
echo "PASS: Zero zombie child processes"

# 2. Duplicate WM conflict must exit non-zero quickly
set +e
timeout -k 1 2 "$TMP_DIR/muzuwm_asan" >"$TMP_DIR/wm2.out" 2>"$TMP_DIR/wm2.err"
WM2_RC=$?
set -e
if [ "$WM2_RC" -eq 0 ] || [ "$WM2_RC" -eq 124 ] || [ "$WM2_RC" -eq 137 ]; then
    echo "FAIL: Duplicate WM did not fail fast on BadAccess (rc=$WM2_RC)"
    cat "$TMP_DIR/wm2.err"
    exit 1
fi
echo "PASS: Duplicate WM failed fast (rc=$WM2_RC)"

# 3. Idle SIGTERM must terminate WM1 cleanly within 1.5s without needing an X event
kill -TERM "$WM1_PID"
DEADLINE=$(( $(date +%s%N) + 1500000000 ))
while kill -0 "$WM1_PID" 2>/dev/null; do
    if [ "$(date +%s%N)" -gt "$DEADLINE" ]; then
        echo "FAIL: WM1 hung on idle SIGTERM (XNextEvent blocking)"
        exit 1
    fi
    sleep 0.05
done
set +e
wait "$WM1_PID"
WM1_RC=$?
set -e
WM1_PID=""
if [ "$WM1_RC" -ne 0 ]; then
    echo "FAIL: WM1 exited with rc=$WM1_RC on SIGTERM"
    cat "$TMP_DIR/wm1.err"
    exit 1
fi
if grep -qE "ERROR: AddressSanitizer|ERROR: LeakSanitizer|runtime error:" "$TMP_DIR/wm1.err"; then
    echo "FAIL: Sanitizer error detected in WM1"
    cat "$TMP_DIR/wm1.err"
    exit 1
fi
echo "PASS: Idle SIGTERM clean exit (< 1.5s, rc=0)"
