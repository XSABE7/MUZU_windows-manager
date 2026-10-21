#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "compositor.h"

static pid_t picom_pid = 0;
static char config_path[512] = {0};

const char *compositor_get_config_path(void) {
    if (config_path[0] == '\0') {
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(config_path, sizeof(config_path), "%s/.config/muzuwm/picom.conf", home);
    }
    return config_path;
}

static void compositor_ensure_config(void) {
    const char *path = compositor_get_config_path();
    if (access(path, F_OK) == 0) {
        FILE *rf = fopen(path, "r");
        if (rf) {
            char check_buf[2048];
            size_t n = fread(check_buf, 1, sizeof(check_buf) - 1, rf);
            check_buf[n] = '\0';
            fclose(rf);
            if (strstr(check_buf, "dual_kawase") != NULL) {
                return; // Already up-to-date
            }
        }
    }

    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.config/muzuwm", home);
    mkdir(dir, 0755);

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f,
        "# MUZUwm Picom Compositor Configuration\n"
        "# Liquid / Acrylic Glass Aesthetic with Dual-Kawase Blur\n"
        "\n"
        "# Rendering Backend\n"
        "backend = \"glx\";\n"
        "glx-no-stencil = true;\n"
        "glx-no-rebind-pixmap = true;\n"
        "vsync = true;\n"
        "\n"
        "# Dual-Kawase Frosted Glass Blur\n"
        "blur: {\n"
        "    method = \"dual_kawase\";\n"
        "    strength = 5;\n"
        "    background = true;\n"
        "    background-frame = false;\n"
        "    background-fixed = false;\n"
        "};\n"
        "\n"
        "blur-background-exclude = [\n"
        "    \"window_type = 'desktop'\",\n"
        "    \"_GTK_FRAME_EXTENTS@\"\n"
        "];\n"
        "\n"
        "# Translucent Soft Shadows\n"
        "shadow = true;\n"
        "shadow-radius = 8;\n"
        "shadow-opacity = 0.50;\n"
        "shadow-offset-x = -8;\n"
        "shadow-offset-y = -8;\n"
        "shadow-exclude = [\n"
        "    \"name = 'MUZUwm Dock'\",\n"
        "    \"_NET_WM_WINDOW_TYPE@:a *= 'DOCK'\",\n"
        "    \"_GTK_FRAME_EXTENTS@\"\n"
        "];\n"
        "\n"
        "# Fading\n"
        "fading = true;\n"
        "fade-in-step = 0.08;\n"
        "fade-out-step = 0.08;\n"
        "fade-delta = 10;\n"
        "\n"
        "# Rules & Detection\n"
        "detect-client-opacity = true;\n"
        "detect-transient = true;\n"
        "detect-client-leader = true;\n"
        "use-damage = true;\n"
        "log-level = \"warn\";\n"
    );
    fclose(f);
}

void compositor_init(Display *dpy) {
    compositor_ensure_config();
    if (!dpy) return;

    // Check if a compositor is already managing this display
    int screen = DefaultScreen(dpy);
    char atom_name[64];
    snprintf(atom_name, sizeof(atom_name), "_NET_WM_CM_S%d", screen);
    Atom cm_atom = XInternAtom(dpy, atom_name, False);
    if (XGetSelectionOwner(dpy, cm_atom) != None) {
        return; // Compositor already running
    }

    // Check if picom binary is available in PATH or standard paths
    int picom_available = 0;
    if (access("/usr/bin/picom", X_OK) == 0 ||
        access("/usr/local/bin/picom", X_OK) == 0) {
        picom_available = 1;
    } else {
        const char *path_env = getenv("PATH");
        if (path_env) {
            char path_buf[1024];
            strncpy(path_buf, path_env, sizeof(path_buf) - 1);
            path_buf[sizeof(path_buf) - 1] = '\0';
            char *token = strtok(path_buf, ":");
            while (token) {
                char test_path[1024];
                snprintf(test_path, sizeof(test_path), "%s/picom", token);
                if (access(test_path, X_OK) == 0) {
                    picom_available = 1;
                    break;
                }
                token = strtok(NULL, ":");
            }
        }
    }

    if (!picom_available) {
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        setsid();
        execlp("picom", "picom", "--config", compositor_get_config_path(), NULL);
        _exit(0);
    } else if (pid > 0) {
        picom_pid = pid;
    }
}

void compositor_cleanup(void) {
    if (picom_pid > 0) {
        kill(picom_pid, SIGTERM);
        waitpid(picom_pid, NULL, WNOHANG);
        picom_pid = 0;
    }
}
