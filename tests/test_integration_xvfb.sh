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
    if [ -n "$XVFB_PID" ]; then
        kill -TERM "$XVFB_PID" 2>/dev/null || true
        wait "$XVFB_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

CFLAGS=$(pkg-config --cflags cairo x11 xext xcursor xrandr fontconfig)
LIBS="$(pkg-config --libs cairo x11 xext xcursor xrandr fontconfig) -lm"
SRCS="src/wm.c src/titlebar.c src/client_props.c src/ewmh.c src/monitors.c src/workspace.c src/config_rules.c src/struts.c src/snap_preview.c src/menu.c src/alttab.c src/main.c"

echo "Building ASan+UBSan muzuwm in $TMP_DIR..."
gcc -Wall -Wextra -pedantic -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude \
    $CFLAGS $SRCS -o "$TMP_DIR/muzuwm_asan" -fsanitize=address,undefined $LIBS

cat > "$TMP_DIR/x11_driver.c" << 'EOF'
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

static int get_cardinal(Display *dpy, Window w, Atom prop, unsigned long *out) {
    Atom type;
    int fmt;
    unsigned long n, after;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_CARDINAL, &type, &fmt, &n, &after, &data) == Success && data) {
        int ok = (type == XA_CARDINAL && fmt == 32 && n >= 1);
        if (ok) *out = ((unsigned long *)data)[0];
        XFree(data);
        return ok;
    }
    return 0;
}

static int get_window_prop(Display *dpy, Window w, Atom prop, Window *out) {
    Atom type;
    int fmt;
    unsigned long n, after;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_WINDOW, &type, &fmt, &n, &after, &data) == Success && data) {
        int ok = (type == XA_WINDOW && fmt == 32 && n >= 1);
        if (ok) *out = ((Window *)data)[0];
        XFree(data);
        return ok;
    }
    return 0;
}

static int client_list_contains(Display *dpy, Window root, Atom net_client_list, Window target, unsigned long *out_count) {
    Atom type;
    int fmt;
    unsigned long n = 0, after;
    unsigned char *data = NULL;
    int found = 0;
    if (XGetWindowProperty(dpy, root, net_client_list, 0, 1024, False, XA_WINDOW, &type, &fmt, &n, &after, &data) == Success && data) {
        if (type == XA_WINDOW && fmt == 32) {
            Window *wins = (Window *)data;
            for (unsigned long i = 0; i < n; i++) {
                if (wins[i] == target) found = 1;
            }
        }
        XFree(data);
    }
    if (out_count) *out_count = n;
    return found;
}

static int get_workarea(Display *dpy, Window root, Atom net_workarea, unsigned long *x, unsigned long *y, unsigned long *w, unsigned long *h) {
    Atom type;
    int fmt;
    unsigned long n, after;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, root, net_workarea, 0, 64, False, XA_CARDINAL, &type, &fmt, &n, &after, &data) == Success && data) {
        int ok = (type == XA_CARDINAL && fmt == 32 && n >= 4);
        if (ok) {
            unsigned long *v = (unsigned long *)data;
            *x = v[0]; *y = v[1]; *w = v[2]; *h = v[3];
        }
        XFree(data);
        return ok;
    }
    return 0;
}

static void send_root_client_msg(Display *dpy, Window root, Window win, Atom type, long d0, long d1, long d2) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = type;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d0;
    ev.xclient.data.l[1] = d1;
    ev.xclient.data.l[2] = d2;
    ev.xclient.data.l[3] = 1;
    XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XSync(dpy, False);
}

#define WAIT_UNTIL(cond) do { \
    int _tries = 0; \
    while (!(cond) && _tries < 100) { usleep(20000); XSync(dpy, False); _tries++; } \
    assert(cond); \
} while (0)

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    assert(dpy != NULL);
    Window root = DefaultRootWindow(dpy);

    Atom a_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    Atom a_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom a_utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    Atom a_num_desk = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    Atom a_cur_desk = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    Atom a_workarea = XInternAtom(dpy, "_NET_WORKAREA", False);
    Atom a_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    Atom a_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom a_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom a_wm_state_fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    Atom a_win_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom a_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    Atom a_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);

    // 1. EWMH root publication
    Window check_win = None;
    WAIT_UNTIL(get_window_prop(dpy, root, a_wm_check, &check_win) && check_win != None);

    Atom rt; int rf; unsigned long rn, ra; unsigned char *rdata = NULL;
    assert(XGetWindowProperty(dpy, root, a_wm_name, 0, 64, False, a_utf8, &rt, &rf, &rn, &ra, &rdata) == Success && rdata);
    assert(strncmp((char *)rdata, "MUZUwm", 6) == 0);
    XFree(rdata);

    unsigned long ndesk = 0, curdesk = 99;
    assert(get_cardinal(dpy, root, a_num_desk, &ndesk) && ndesk == 4);
    assert(get_cardinal(dpy, root, a_cur_desk, &curdesk) && curdesk == 0);

    unsigned long wx = 99, wy = 99, ww = 0, wh = 0;
    assert(get_workarea(dpy, root, a_workarea, &wx, &wy, &ww, &wh));
    assert(wx == 0 && wy == 0 && ww == 1280 && wh == 720);
    printf("PASS: EWMH root atoms verified\n");

    // 2. Normal window + transient dialog + focus clear on close
    Window w1 = XCreateSimpleWindow(dpy, root, 10, 10, 400, 300, 0, 0, 0);
    XMapWindow(dpy, w1);
    XSync(dpy, False);

    unsigned long cl_count = 0;
    Window active = None;
    WAIT_UNTIL(client_list_contains(dpy, root, a_client_list, w1, &cl_count));
    WAIT_UNTIL(get_window_prop(dpy, root, a_active, &active) && active == w1);

    Window dlg = XCreateSimpleWindow(dpy, root, 0, 0, 250, 150, 0, 0, 0);
    XSetTransientForHint(dpy, dlg, w1);
    XMapWindow(dpy, dlg);
    XSync(dpy, False);
    WAIT_UNTIL(client_list_contains(dpy, root, a_client_list, dlg, &cl_count) && cl_count == 2);

    XDestroyWindow(dpy, dlg);
    XDestroyWindow(dpy, w1);
    XSync(dpy, False);
    WAIT_UNTIL(get_window_prop(dpy, root, a_active, &active) && active == None);
    printf("PASS: Window + transient lifecycle and _NET_ACTIVE_WINDOW clear\n");

    // 3. Fullscreen ClientMessage + ConfigureRequest protection
    Window w2 = XCreateSimpleWindow(dpy, root, 40, 40, 400, 300, 0, 0, 0);
    XMapWindow(dpy, w2);
    XSync(dpy, False);
    WAIT_UNTIL(client_list_contains(dpy, root, a_client_list, w2, &cl_count));

    send_root_client_msg(dpy, root, w2, a_wm_state, 1 /* ADD */, (long)a_wm_state_fs, 0);
    XWindowAttributes wa;
    WAIT_UNTIL(XGetWindowAttributes(dpy, w2, &wa) && wa.width == 1280 && wa.height == 720);

    XMoveResizeWindow(dpy, w2, 100, 100, 220, 180);
    XSync(dpy, False);
    usleep(100000);
    XSync(dpy, False);
    assert(XGetWindowAttributes(dpy, w2, &wa));
    assert(wa.x == 0 && wa.y == 0 && wa.width == 1280 && wa.height == 720);
    XDestroyWindow(dpy, w2);
    XSync(dpy, False);
    printf("PASS: Fullscreen ClientMessage and ConfigureRequest guard\n");

    // 4. Workspace switching + out-of-range ClientMessage rejection (99 and -1/0xFFFFFFFF)
    send_root_client_msg(dpy, root, root, a_cur_desk, 2L, 0, 0);
    WAIT_UNTIL(get_cardinal(dpy, root, a_cur_desk, &curdesk) && curdesk == 2);
    send_root_client_msg(dpy, root, root, a_cur_desk, 99L, 0, 0);
    send_root_client_msg(dpy, root, root, a_cur_desk, -1L, 0, 0);
    usleep(80000);
    XSync(dpy, False);
    assert(get_cardinal(dpy, root, a_cur_desk, &curdesk) && curdesk == 2);
    send_root_client_msg(dpy, root, root, a_cur_desk, 0L, 0, 0);
    WAIT_UNTIL(get_cardinal(dpy, root, a_cur_desk, &curdesk) && curdesk == 0);
    printf("PASS: Workspace switching and out-of-range payload rejection\n");

    // 5. _NET_WM_STRUT_PARTIAL dock shrinks and restores _NET_WORKAREA
    Window dock = XCreateSimpleWindow(dpy, root, 0, 0, 1280, 40, 0, 0, 0);
    XChangeProperty(dpy, dock, a_win_type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&a_type_dock, 1);
    unsigned long strut[12] = {0, 0, 40, 0, 0, 0, 0, 0, 0, 1279, 0, 0};
    XChangeProperty(dpy, dock, a_strut_partial, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)strut, 12);
    XMapWindow(dpy, dock);
    XSync(dpy, False);
    WAIT_UNTIL(get_workarea(dpy, root, a_workarea, &wx, &wy, &ww, &wh) && wy == 40 && wh == 680);

    XUnmapWindow(dpy, dock);
    XSync(dpy, False);
    WAIT_UNTIL(get_workarea(dpy, root, a_workarea, &wx, &wy, &ww, &wh) && wy == 0 && wh == 720);
    XDestroyWindow(dpy, dock);
    XCloseDisplay(dpy);
    printf("PASS: Struts shrink and restore _NET_WORKAREA\n");
    return 0;
}
EOF

gcc -Wall -Wextra -pedantic -std=c11 -O1 "$TMP_DIR/x11_driver.c" -o "$TMP_DIR/x11_driver" -lX11

export HOME="$TMP_DIR/home"
mkdir -p "$HOME/.config/muzuwm"
printf 'touch "%s/child_ran"\n' "$TMP_DIR" > "$HOME/.config/muzuwm/auto_allowlist"

Xvfb -displayfd 3 -screen 0 1280x720x24 3>"$TMP_DIR/disp.txt" >/dev/null 2>&1 &
XVFB_PID=$!
for _ in $(seq 1 40); do
    if [ -s "$TMP_DIR/disp.txt" ]; then break; fi
    sleep 0.05
done
DISP_NUM=$(tr -d '[:space:]' < "$TMP_DIR/disp.txt")
[ -n "$DISP_NUM" ] || { echo "FAIL: Xvfb failed to report displayfd"; exit 1; }
DISP=":${DISP_NUM}"

export DISPLAY="$DISP"
printf 'leak:_XEnq\n' > "$TMP_DIR/lsan.supp"
export LSAN_OPTIONS="suppressions=$TMP_DIR/lsan.supp"

# 1. Start primary WM
"$TMP_DIR/muzuwm_asan" >"$TMP_DIR/wm1.out" 2>"$TMP_DIR/wm1.err" &
WM1_PID=$!
sleep 0.5
kill -0 "$WM1_PID" 2>/dev/null || { echo "FAIL: WM1 exited prematurely"; cat "$TMP_DIR/wm1.err"; exit 1; }

# 2. Verify auto_allowlist actually spawned child and zero zombie child processes remain
for _ in $(seq 1 40); do
    if [ -f "$TMP_DIR/child_ran" ]; then break; fi
    sleep 0.05
done
if [ ! -f "$TMP_DIR/child_ran" ]; then
    echo "FAIL: auto_allowlist child never ran (config_run_autostart missing?)"
    exit 1
fi
sleep 0.1
ZOMBIES=$(ps -eo ppid=,stat= | awk -v p="$WM1_PID" '$1 == p && $2 ~ /Z/ { count++ } END { print count+0 }')
if [ "$ZOMBIES" -ne 0 ]; then
    echo "FAIL: Found $ZOMBIES zombie child process(es) under WM1"
    exit 1
fi
echo "PASS: auto_allowlist executed and zero zombie child processes"

# 3. Duplicate WM conflict must fail fast
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

# 4. Run real X11 client scenarios
"$TMP_DIR/x11_driver"

# 5. Idle SIGTERM must terminate WM1 cleanly within 1.5s without needing an X event
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
