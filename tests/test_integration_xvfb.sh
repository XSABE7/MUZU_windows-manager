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
#include <X11/keysym.h>

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

static int has_visible_popup_menu(Display *dpy, Window root, Atom a_win_type, Atom a_popup) {
    Window rr = None, pr = None, *children = NULL;
    unsigned int n = 0;
    int found = 0;
    if (!XQueryTree(dpy, root, &rr, &pr, &children, &n) || !children) return 0;
    for (unsigned int i = 0; i < n; i++) {
        XWindowAttributes wa;
        if (!XGetWindowAttributes(dpy, children[i], &wa) || wa.map_state != IsViewable) continue;
        Atom rt; int rf; unsigned long rn, ra; unsigned char *rdata = NULL;
        if (XGetWindowProperty(dpy, children[i], a_win_type, 0, 8, False, XA_ATOM,
                               &rt, &rf, &rn, &ra, &rdata) == Success && rdata) {
            if (rt == XA_ATOM && rf == 32) {
                Atom *atoms = (Atom *)rdata;
                for (unsigned long j = 0; j < rn; j++) {
                    if (atoms[j] == a_popup) found = 1;
                }
            }
            XFree(rdata);
        }
        if (found) break;
    }
    XFree(children);
    return found;
}

static void send_key_on_root(Display *dpy, Window root, unsigned int state, KeySym sym, unsigned long time_ms) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = KeyPress;
    ev.xkey.display = dpy;
    ev.xkey.window = root;
    ev.xkey.root = root;
    ev.xkey.subwindow = None;
    ev.xkey.time = time_ms;
    ev.xkey.state = state;
    ev.xkey.keycode = XKeysymToKeycode(dpy, sym);
    ev.xkey.same_screen = True;
    XSendEvent(dpy, root, False, KeyPressMask, &ev);
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
    Atom a_type_popup = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
    Atom a_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    Atom a_reload = XInternAtom(dpy, "_MUZU_RELOAD_SETTINGS", False);

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
    printf("PASS: Struts shrink and restore _NET_WORKAREA\n");

    // 6. Focus borders, EnterNotify focus-follows-mouse, reload, and Ctrl+Alt+2xDel
    Window fa = XCreateSimpleWindow(dpy, root, 40, 40, 300, 200, 0, 0, 0);
    XMapWindow(dpy, fa);
    XSync(dpy, False);
    WAIT_UNTIL(get_window_prop(dpy, root, a_active, &active) && active == fa);
    WAIT_UNTIL(XGetWindowAttributes(dpy, fa, &wa) && wa.border_width == 2);

    Window fb = XCreateSimpleWindow(dpy, root, 420, 40, 300, 200, 0, 0, 0);
    XMapWindow(dpy, fb);
    XSync(dpy, False);
    WAIT_UNTIL(get_window_prop(dpy, root, a_active, &active) && active == fb);

    // Warp pointer into fa -> EnterNotify moves focus to fa
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, 120, 120);
    XSync(dpy, False);
    WAIT_UNTIL(get_window_prop(dpy, root, a_active, &active) && active == fa);

    // Fullscreen strips border_width to 0, exiting restores border_width == 2
    send_root_client_msg(dpy, root, fa, a_wm_state, 1 /* ADD */, (long)a_wm_state_fs, 0);
    WAIT_UNTIL(XGetWindowAttributes(dpy, fa, &wa) && wa.border_width == 0 && wa.width == 1280);
    send_root_client_msg(dpy, root, fa, a_wm_state, 0 /* REMOVE */, (long)a_wm_state_fs, 0);
    WAIT_UNTIL(XGetWindowAttributes(dpy, fa, &wa) && wa.border_width == 2);

    // Live reload border_width=4 and enable_desktop_menu=0
    const char *home_dir = getenv("HOME");
    assert(home_dir != NULL);
    char cfg_path[512];
    snprintf(cfg_path, sizeof(cfg_path), "%s/.config/muzuwm/settings.conf", home_dir);
    FILE *sf = fopen(cfg_path, "w");
    assert(sf != NULL);
    fprintf(sf, "border_width=4\nfocus_border_color=#ff5500\nunfocus_border_color=#223344\nenable_desktop_menu=0\nfocus_follows_mouse=1\n");
    fclose(sf);
    unsigned long rel_val = 1;
    XChangeProperty(dpy, root, a_reload, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&rel_val, 1);
    XSync(dpy, False);
    WAIT_UNTIL(XGetWindowAttributes(dpy, fa, &wa) && wa.border_width == 4);

    // Root right-click must NOT open popup when enable_desktop_menu=0
    XEvent bev;
    memset(&bev, 0, sizeof(bev));
    bev.type = ButtonPress;
    bev.xbutton.display = dpy;
    bev.xbutton.window = root;
    bev.xbutton.root = root;
    bev.xbutton.button = Button3;
    bev.xbutton.x = 900;
    bev.xbutton.y = 500;
    bev.xbutton.x_root = 900;
    bev.xbutton.y_root = 500;
    bev.xbutton.same_screen = True;
    XSendEvent(dpy, root, False, ButtonPressMask, &bev);
    XSync(dpy, False);
    usleep(80000);
    assert(has_visible_popup_menu(dpy, root, a_win_type, a_type_popup) == 0);

    // Single Ctrl+Alt+Delete must NOT open popup
    send_key_on_root(dpy, root, ControlMask | Mod1Mask, XK_Delete, 10000UL);
    usleep(80000);
    assert(has_visible_popup_menu(dpy, root, a_win_type, a_type_popup) == 0);

    // Second Ctrl+Alt+Delete within 500ms force-opens popup menu
    send_key_on_root(dpy, root, ControlMask | Mod1Mask, XK_Delete, 10200UL);
    WAIT_UNTIL(has_visible_popup_menu(dpy, root, a_win_type, a_type_popup) == 1);

    // Escape closes popup menu cleanly and restores X11 input focus to fa
    send_key_on_root(dpy, root, 0, XK_Escape, 10300UL);
    WAIT_UNTIL(has_visible_popup_menu(dpy, root, a_win_type, a_type_popup) == 0);
    Window focus_win = None; int revert_to = 0;
    XGetInputFocus(dpy, &focus_win, &revert_to);
    assert(focus_win == fa);

    // Clicking outside a force-opened menu with Button3 when enable_desktop_menu=0 closes it without re-opening
    send_key_on_root(dpy, root, ControlMask | Mod1Mask, XK_Delete, 20000UL);
    send_key_on_root(dpy, root, ControlMask | Mod1Mask, XK_Delete, 20200UL);
    WAIT_UNTIL(has_visible_popup_menu(dpy, root, a_win_type, a_type_popup) == 1);
    bev.xbutton.window = root;
    bev.xbutton.x = 1200; bev.xbutton.y = 650;
    bev.xbutton.x_root = 1200; bev.xbutton.y_root = 650;
    XSendEvent(dpy, root, False, ButtonPressMask, &bev);
    XSync(dpy, False);
    WAIT_UNTIL(has_visible_popup_menu(dpy, root, a_win_type, a_type_popup) == 0);
    XGetInputFocus(dpy, &focus_win, &revert_to);
    assert(focus_win == fa);

    // Maximized window shrinks cleanly when border_width increases on live reload
    send_root_client_msg(dpy, root, fa, a_wm_state, 1 /* ADD */, (long)XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False), (long)XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False));
    WAIT_UNTIL(XGetWindowAttributes(dpy, fa, &wa) && wa.width == 1272 && wa.height == 712);
    sf = fopen(cfg_path, "w");
    assert(sf != NULL);
    fprintf(sf, "border_width=8\nfocus_border_color=#89b4fa\nunfocus_border_color=#45475a\nenable_desktop_menu=0\nfocus_follows_mouse=1\n");
    fclose(sf);
    XChangeProperty(dpy, root, a_reload, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&rel_val, 1);
    XSync(dpy, False);
    WAIT_UNTIL(XGetWindowAttributes(dpy, fa, &wa) && wa.border_width == 8 && wa.width == 1264 && wa.height == 704);

    XDestroyWindow(dpy, fb);
    XDestroyWindow(dpy, fa);
    XCloseDisplay(dpy);
    printf("PASS: Focus borders, EnterNotify, live reload, and Ctrl+Alt+2xDel verified\n");
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
printf 'leak:_XEnq\nleak:libcairo.so\nleak:libfontconfig.so\n' > "$TMP_DIR/lsan.supp"
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

# 6. Standalone mzml ASan+UBSan smoke test
MZML_SRCS="src/mzml.c src/mzml_main.c src/monitors.c src/struts.c"
gcc -Wall -Wextra -pedantic -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude \
    $CFLAGS $MZML_SRCS -o "$TMP_DIR/mzml_asan" -fsanitize=address,undefined $LIBS

mkdir -p "$TMP_DIR/xdg/applications"
cat > "$TMP_DIR/xdg/applications/hello.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Hello World
Comment=Test entry
Exec=true
EOF

XDG_DATA_HOME="$TMP_DIR/xdg" "$TMP_DIR/mzml_asan" >"$TMP_DIR/mzml.out" 2>"$TMP_DIR/mzml.err" &
MZML_PID=$!
sleep 0.4
kill -0 "$MZML_PID" 2>/dev/null || { echo "FAIL: mzml exited prematurely"; cat "$TMP_DIR/mzml.err"; exit 1; }
kill -TERM "$MZML_PID"
set +e
wait "$MZML_PID"
MZML_RC=$?
set -e
if [ "$MZML_RC" -ne 0 ]; then
    echo "FAIL: mzml exited with rc=$MZML_RC on SIGTERM"
    cat "$TMP_DIR/mzml.err"
    exit 1
fi
if grep -qE "ERROR: AddressSanitizer|ERROR: LeakSanitizer|runtime error:" "$TMP_DIR/mzml.err"; then
    echo "FAIL: Sanitizer error detected in mzml"
    cat "$TMP_DIR/mzml.err"
    exit 1
fi
echo "PASS: mzml ASan+UBSan startup and SIGTERM clean exit (rc=0)"

# 7. mzml Escape key clean exit under ASan+UBSan
XDG_DATA_HOME="$TMP_DIR/xdg" "$TMP_DIR/mzml_asan" >"$TMP_DIR/mzml_esc.out" 2>"$TMP_DIR/mzml_esc.err" &
MZML_ESC_PID=$!
cat > "$TMP_DIR/send_esc.c" << 'EOF'
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;
    Window root = DefaultRootWindow(dpy);
    Atom a_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom a_dlg = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    Window target = None;
    for (int t = 0; t < 60 && target == None; t++) {
        Window rr, pr, *ch = NULL;
        unsigned int n = 0;
        if (XQueryTree(dpy, root, &rr, &pr, &ch, &n) && ch) {
            for (unsigned int i = 0; i < n; i++) {
                XWindowAttributes wa;
                if (!XGetWindowAttributes(dpy, ch[i], &wa) || wa.map_state != IsViewable) continue;
                Atom rt; int rf; unsigned long rn, ra; unsigned char *rd = NULL;
                if (XGetWindowProperty(dpy, ch[i], a_type, 0, 4, False, XA_ATOM, &rt, &rf, &rn, &ra, &rd) == Success && rd) {
                    if (rt == XA_ATOM && rf == 32 && rn >= 1 && ((Atom *)rd)[0] == a_dlg) target = ch[i];
                    XFree(rd);
                }
            }
            XFree(ch);
        }
        if (target == None) usleep(20000);
    }
    if (target == None) { XCloseDisplay(dpy); return 2; }
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = KeyPress;
    ev.xkey.display = dpy;
    ev.xkey.window = target;
    ev.xkey.root = root;
    ev.xkey.keycode = XKeysymToKeycode(dpy, XK_Escape);
    ev.xkey.same_screen = True;
    XSendEvent(dpy, target, False, KeyPressMask, &ev);
    XSync(dpy, False);
    XCloseDisplay(dpy);
    return 0;
}
EOF
gcc -Wall -Wextra -pedantic -std=c11 -O1 "$TMP_DIR/send_esc.c" -o "$TMP_DIR/send_esc" -lX11
"$TMP_DIR/send_esc"
set +e
wait "$MZML_ESC_PID"
MZML_ESC_RC=$?
set -e
if [ "$MZML_ESC_RC" -ne 0 ] || grep -qE "ERROR: AddressSanitizer|ERROR: LeakSanitizer|runtime error:" "$TMP_DIR/mzml_esc.err"; then
    echo "FAIL: mzml Escape exit failed (rc=$MZML_ESC_RC)"
    cat "$TMP_DIR/mzml_esc.err"
    exit 1
fi
echo "PASS: mzml ASan+UBSan Escape clean exit (rc=0)"
