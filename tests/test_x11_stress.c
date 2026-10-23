#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

int main(int argc, char **argv) {
    const char *display_name = (argc > 1) ? argv[1] : ":9";
    Display *dpy = XOpenDisplay(display_name);
    if (!dpy) {
        fprintf(stderr, "test_x11_stress: Cannot open display %s\n", display_name);
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    int cycles = (argc > 2) ? atoi(argv[2]) : 100;
    printf("Connected to %s. Running window lifecycle stress test (%d iterations)...\n", display_name, cycles);

    for (int i = 0; i < cycles; i++) {
        // Create a test client window
        Window w = XCreateSimpleWindow(dpy, root, 50, 50, 300, 200, 1,
                                       BlackPixel(dpy, screen), WhitePixel(dpy, screen));
        XStoreName(dpy, w, "StressTestWindow");
        XMapWindow(dpy, w);
        XFlush(dpy);
        usleep(15000); // 15ms

        // Unmap and destroy
        XUnmapWindow(dpy, w);
        XDestroyWindow(dpy, w);
        XFlush(dpy);
        usleep(15000); // 15ms
    }

    printf("Window lifecycle stress test completed.\n");

    // Test Alt+F1 hotkey (trigger launchpad)
    printf("Simulating Alt+F1 Launchpad trigger...\n");
    KeyCode f1_code = XKeysymToKeycode(dpy, XK_F1);

    XKeyEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = KeyPress;
    ev.display = dpy;
    ev.window = root;
    ev.root = root;
    ev.subwindow = None;
    ev.time = CurrentTime;
    ev.x = 100;
    ev.y = 100;
    ev.x_root = 100;
    ev.y_root = 100;
    ev.same_screen = True;
    ev.state = Mod1Mask;
    ev.keycode = f1_code;

    XSendEvent(dpy, root, True, KeyPressMask, (XEvent *)&ev);
    XFlush(dpy);
    usleep(100000); // 100ms

    // Send Escape to close launchpad
    KeyCode esc_code = XKeysymToKeycode(dpy, XK_Escape);
    ev.state = 0;
    ev.keycode = esc_code;
    XSendEvent(dpy, root, True, KeyPressMask, (XEvent *)&ev);
    XFlush(dpy);
    usleep(100000); // 100ms

    printf("All stress events sent cleanly.\n");
    XCloseDisplay(dpy);
    return 0;
}
