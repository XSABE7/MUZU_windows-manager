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
        // If it is our own ~/.config/muzuwm/picom.conf from an older version lacking DOCK exclusion, upgrade it
        if (strstr(path, "/.config/muzuwm/picom.conf") != NULL) {
            FILE *chk = fopen(path, "r");
            if (chk) {
                char buf[4096];
                size_t n = fread(buf, 1, sizeof(buf) - 1, chk);
                buf[n] = '\0';
                fclose(chk);
                if (strstr(buf, "name = 'MUZUwm Dock'") != NULL &&
                    strstr(buf, "glx-no-stencil") == NULL) {
                    return;
                }
            }
        } else {
            return; // Valid user or upstream config found, do not overwrite
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
        "    \"window_type = 'dock'\",\n"
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

int compositor_find_picom(char *out_bin, size_t out_len) {
    if (!out_bin || out_len == 0) return 0;

    const char *home = getenv("HOME");
    if (home) {
        char user_picom[1024];
        snprintf(user_picom, sizeof(user_picom), "%s/.local/bin/picom", home);
        if (access(user_picom, X_OK) == 0) {
            snprintf(out_bin, out_len, "%s", user_picom);
            return 1;
        }
    }

    if (access("/usr/bin/picom", X_OK) == 0) {
        snprintf(out_bin, out_len, "/usr/bin/picom");
        return 1;
    }

    if (access("/usr/local/bin/picom", X_OK) == 0) {
        snprintf(out_bin, out_len, "/usr/local/bin/picom");
        return 1;
    }

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
                snprintf(out_bin, out_len, "%s", test_path);
                return 1;
            }
            token = strtok(NULL, ":");
        }
    }

    return 0;
}

static int compositor_has_hardware_glx(Display *dpy) {
    if (!dpy) return 0;
    int major = 0, first_event = 0, first_error = 0;
    if (XQueryExtension(dpy, "DRI3", &major, &first_event, &first_error)) return 1;
    if (XQueryExtension(dpy, "DRI2", &major, &first_event, &first_error)) return 1;
    if (XQueryExtension(dpy, "NV-GLX", &major, &first_event, &first_error)) return 1;
    return 0;
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

    // Check if picom binary is available in ~/.local/bin, PATH, or standard paths
    char picom_bin[1024];
    if (!compositor_find_picom(picom_bin, sizeof(picom_bin))) {
        return;
    }

    int hw_glx = compositor_has_hardware_glx(dpy);

    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        setsid();
        if (hw_glx) {
            execl(picom_bin, "picom", "--config", compositor_get_config_path(), NULL);
        } else {
            execl(picom_bin, "picom", "--config", compositor_get_config_path(),
                  "--backend", "xrender", "--no-vsync", "--blur-method", "none", NULL);
        }
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
