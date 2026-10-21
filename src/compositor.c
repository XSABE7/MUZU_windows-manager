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
    if (config_path[0] != '\0') return config_path;

    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char candidate[512];
    // 1. Check ~/.config/muzuwm/picom.conf
    snprintf(candidate, sizeof(candidate), "%s/.config/muzuwm/picom.conf", home);
    if (access(candidate, R_OK) == 0) {
        snprintf(config_path, sizeof(config_path), "%s", candidate);
        return config_path;
    }

    // 2. Check upstream standard ~/.config/picom/picom.conf
    snprintf(candidate, sizeof(candidate), "%s/.config/picom/picom.conf", home);
    if (access(candidate, R_OK) == 0) {
        snprintf(config_path, sizeof(config_path), "%s", candidate);
        return config_path;
    }

    // 3. Check legacy ~/.config/picom.conf
    snprintf(candidate, sizeof(candidate), "%s/.config/picom.conf", home);
    if (access(candidate, R_OK) == 0) {
        snprintf(config_path, sizeof(config_path), "%s", candidate);
        return config_path;
    }

    // 4. Check user desktop ~/.config/i3/picom/picom.conf
    snprintf(candidate, sizeof(candidate), "%s/.config/i3/picom/picom.conf", home);
    if (access(candidate, R_OK) == 0) {
        snprintf(config_path, sizeof(config_path), "%s", candidate);
        return config_path;
    }

    // 5. Check system-wide /etc/xdg/picom.conf
    if (access("/etc/xdg/picom.conf", R_OK) == 0) {
        snprintf(config_path, sizeof(config_path), "/etc/xdg/picom.conf");
        return config_path;
    }

    // 6. Check system-wide /etc/xdg/picom/picom.conf
    if (access("/etc/xdg/picom/picom.conf", R_OK) == 0) {
        snprintf(config_path, sizeof(config_path), "/etc/xdg/picom/picom.conf");
        return config_path;
    }

    // Default target for auto-generation: ~/.config/muzuwm/picom.conf
    snprintf(config_path, sizeof(config_path), "%s/.config/muzuwm/picom.conf", home);
    return config_path;
}

static void compositor_ensure_config(void) {
    const char *path = compositor_get_config_path();
    if (access(path, F_OK) == 0) {
        return; // Valid user or upstream config found, do not overwrite
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
        "vsync = true;\n"
        "\n"
        "# Dual-Kawase Frosted Glass Blur\n"
        "blur: {\n"
        "    method = \"dual_kawase\";\n"
        "    strength = 6;\n"
        "    background = true;\n"
        "    background-frame = true;\n"
        "    background-fixed = false;\n"
        "};\n"
        "\n"
        "blur-background-exclude = [\n"
        "    \"window_type = 'desktop'\",\n"
        "    \"_GTK_FRAME_EXTENTS@\"\n"
        "];\n"
        "\n"
        "# Rounded Corners\n"
        "corner-radius = 12;\n"
        "rounded-corners-exclude = [\n"
        "    \"window_type = 'desktop'\"\n"
        "];\n"
        "\n"
        "# Translucent Soft Shadows\n"
        "shadow = true;\n"
        "shadow-radius = 12;\n"
        "shadow-opacity = 0.55;\n"
        "shadow-offset-x = -10;\n"
        "shadow-offset-y = -10;\n"
        "shadow-exclude = [\n"
        "    \"window_type = 'desktop'\",\n"
        "    \"_GTK_FRAME_EXTENTS@\"\n"
        "];\n"
        "\n"
        "# Opacity Rules for MUZUwm UI Subsystems\n"
        "opacity-rule = [\n"
        "    \"94:name = 'MUZUwm Dock'\",\n"
        "    \"94:window_type = 'dock'\",\n"
        "    \"88:name = 'MUZUwm Titlebar'\",\n"
        "    \"96:window_type = 'dialog'\"\n"
        "];\n"
        "\n"
        "# Fading\n"
        "fading = true;\n"
        "fade-in-step = 0.05;\n"
        "fade-out-step = 0.05;\n"
        "fade-delta = 8;\n"
        "\n"
        "# Rules & Detection\n"
        "detect-client-opacity = true;\n"
        "detect-rounded-corners = true;\n"
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

    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    // Check picom binary locations (~/.local/bin, /usr/bin, /usr/local/bin, PATH)
    char picom_bin[1024] = {0};
    char user_picom[1024];
    snprintf(user_picom, sizeof(user_picom), "%s/.local/bin/picom", home);

    if (access(user_picom, X_OK) == 0) {
        snprintf(picom_bin, sizeof(picom_bin), "%s", user_picom);
    } else if (access("/usr/bin/picom", X_OK) == 0) {
        snprintf(picom_bin, sizeof(picom_bin), "%s", "/usr/bin/picom");
    } else if (access("/usr/local/bin/picom", X_OK) == 0) {
        snprintf(picom_bin, sizeof(picom_bin), "%s", "/usr/local/bin/picom");
    } else {
        const char *path_env = getenv("PATH");
        if (path_env) {
            char path_buf[1024];
            snprintf(path_buf, sizeof(path_buf), "%s", path_env);
            char *token = strtok(path_buf, ":");
            while (token) {
                char test_path[1024];
                snprintf(test_path, sizeof(test_path), "%s/picom", token);
                if (access(test_path, X_OK) == 0) {
                    snprintf(picom_bin, sizeof(picom_bin), "%s", test_path);
                    break;
                }
                token = strtok(NULL, ":");
            }
        }
    }

    if (picom_bin[0] == '\0') {
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        setsid();

        char path_env[2048];
        const char *old_path = getenv("PATH");
        snprintf(path_env, sizeof(path_env), "%s/.local/bin:%s", home, old_path ? old_path : "/usr/bin:/bin");
        setenv("PATH", path_env, 1);

        char lib_env[2048];
        const char *old_lib = getenv("LD_LIBRARY_PATH");
        snprintf(lib_env, sizeof(lib_env), "%s/.local/lib%s%s", home, old_lib ? ":" : "", old_lib ? old_lib : "");
        setenv("LD_LIBRARY_PATH", lib_env, 1);

        execl(picom_bin, "picom", "--config", compositor_get_config_path(), NULL);
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
