#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "config_rules.h"

#define RELEVANT_MODS (Mod1Mask | Mod4Mask | ControlMask | ShiftMask)

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

static int config_dir(char *out, size_t len) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    return snprintf(out, len, "%s/.config/muzuwm", home) < (int)len;
}

static int config_path(const char *name, char *out, size_t len) {
    char dir[512];
    if (!config_dir(dir, sizeof(dir))) return 0;
    return snprintf(out, len, "%s/%s", dir, name) < (int)len;
}

static void write_default(const char *name, const char *content) {
    char path[600];
    if (!config_path(name, path, sizeof(path))) return;
    if (access(path, F_OK) == 0) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

void config_rules_ensure_files(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    if (!config_dir(dir, sizeof(dir))) return;
    mkdir(dir, 0755);

    write_default("fullscreen_allowlist",
        "# Apps listed here open maximized. One WM_CLASS name or window title per line\n"
        "# (case-insensitive, exact match). Every other app opens floating.\n"
        "# Example:\n"
        "# mpv\n");
    write_default("auto_allowlist",
        "# Commands listed here are launched when MUZUwm starts. One command per line.\n"
        "# Example:\n"
        "# wezterm\n");
    write_default("shortcuts.conf",
        "# MUZUwm keyboard shortcuts:  MODS+KEY = ACTION\n"
        "# Modifiers: Super, Alt, Ctrl, Shift\n"
        "# Actions: tile_left tile_right fullscreen float close_window launcher exec <command>\n"
        "Super+Left = tile_left\n"
        "Super+Right = tile_right\n"
        "Super+Up = fullscreen\n"
        "Super+Down = float\n"
        "Super+Return = exec wezterm\n"
        "Super+b = exec google-chrome\n"
        "Super+q = close_window\n");
}

int config_is_in_fullscreen_allowlist_file(const char *path, const char *res_name, const char *res_class, const char *title) {
    if (!path) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int found = 0;
    char line[512];
    while (!found && fgets(line, sizeof(line), f)) {
        char *entry = trim(line);
        if (entry[0] == '\0' || entry[0] == '#') continue;
        if ((res_name && res_name[0] && strcasecmp(entry, res_name) == 0) ||
            (res_class && res_class[0] && strcasecmp(entry, res_class) == 0) ||
            (title && title[0] && strcasecmp(entry, title) == 0)) {
            found = 1;
        }
    }
    fclose(f);
    return found;
}

int config_client_in_fullscreen_allowlist(Display *dpy, Window win, const char *title) {
    char path[600];
    if (!config_path("fullscreen_allowlist", path, sizeof(path))) return 0;

    const char *res_name = NULL, *res_class = NULL;
    XClassHint ch = {0};
    int have_hint = (dpy && win != None && XGetClassHint(dpy, win, &ch));
    if (have_hint) {
        res_name = ch.res_name;
        res_class = ch.res_class;
    }
    int r = config_is_in_fullscreen_allowlist_file(path, res_name, res_class, title);
    if (have_hint) {
        if (ch.res_name) XFree(ch.res_name);
        if (ch.res_class) XFree(ch.res_class);
    }
    return r;
}

int config_load_auto_allowlist_file(const char *path, char out_cmds[][256], int max_cmds) {
    if (!path || !out_cmds || max_cmds <= 0) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int n = 0;
    char line[512];
    while (n < max_cmds && fgets(line, sizeof(line), f)) {
        char *cmd = trim(line);
        if (cmd[0] == '\0' || cmd[0] == '#') continue;
        snprintf(out_cmds[n], 256, "%s", cmd);
        n++;
    }
    fclose(f);
    return n;
}

void config_spawn_command(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(1);
    }
}

void config_run_autostart(void) {
    char path[600];
    if (!config_path("auto_allowlist", path, sizeof(path))) return;
    char cmds[MAX_AUTOSTART_CMDS][256];
    int n = config_load_auto_allowlist_file(path, cmds, MAX_AUTOSTART_CMDS);
    for (int i = 0; i < n; i++) {
        config_spawn_command(cmds[i]);
    }
}

/* ---------------------------------------------------------------- shortcuts */

static KeySym lower_keysym(KeySym s) {
    KeySym lo = s, up = s;
    XConvertCase(s, &lo, &up);
    return lo;
}

static KeySym parse_key_name(const char *name) {
    static const struct { const char *alias; KeySym sym; } aliases[] = {
        {"enter", XK_Return}, {"return", XK_Return}, {"esc", XK_Escape},
        {"escape", XK_Escape}, {"space", XK_space}, {"tab", XK_Tab},
        {"left", XK_Left}, {"right", XK_Right}, {"up", XK_Up}, {"down", XK_Down},
    };
    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (strcasecmp(name, aliases[i].alias) == 0) return aliases[i].sym;
    }
    KeySym s = XStringToKeysym(name);
    if (s == NoSymbol && name[0] && name[1] == '\0') {
        char lc[2] = { (char)tolower((unsigned char)name[0]), '\0' };
        s = XStringToKeysym(lc);
    }
    return s;
}

// Parses "Super+Shift+b" into modifier mask and keysym; returns 0 on malformed input
static int parse_combo(char *combo, unsigned int *mods, KeySym *sym) {
    *mods = 0;
    *sym = NoSymbol;
    char *save = NULL;
    char *tok = strtok_r(combo, "+", &save);
    char *last = NULL;
    while (tok) {
        char *t = trim(tok);
        if (t[0] == '\0') return 0;
        if (last) {
            if (strcasecmp(last, "super") == 0 || strcasecmp(last, "mod4") == 0) *mods |= Mod4Mask;
            else if (strcasecmp(last, "alt") == 0 || strcasecmp(last, "mod1") == 0) *mods |= Mod1Mask;
            else if (strcasecmp(last, "ctrl") == 0 || strcasecmp(last, "control") == 0) *mods |= ControlMask;
            else if (strcasecmp(last, "shift") == 0) *mods |= ShiftMask;
            else return 0;
        }
        last = t;
        tok = strtok_r(NULL, "+", &save);
    }
    if (!last) return 0;
    *sym = parse_key_name(last);
    return *sym != NoSymbol;
}

static int parse_action(char *action, ShortcutActionType *type, char *cmd, size_t cmd_len) {
    cmd[0] = '\0';
    if (strcasecmp(action, "none") == 0) *type = SHORTCUT_ACT_NONE;
    else if (strcmp(action, "tile_left") == 0) *type = SHORTCUT_ACT_TILE_LEFT;
    else if (strcmp(action, "tile_right") == 0) *type = SHORTCUT_ACT_TILE_RIGHT;
    else if (strcmp(action, "fullscreen") == 0) *type = SHORTCUT_ACT_FULLSCREEN;
    else if (strcmp(action, "float") == 0) *type = SHORTCUT_ACT_FLOAT;
    else if (strcmp(action, "close_window") == 0 || strcmp(action, "close") == 0) *type = SHORTCUT_ACT_CLOSE;
    else if (strcmp(action, "launcher") == 0) *type = SHORTCUT_ACT_LAUNCHER;
    else if (strncmp(action, "exec", 4) == 0 && isspace((unsigned char)action[4])) {
        char *c = trim(action + 4);
        if (c[0] == '\0') return 0;
        snprintf(cmd, cmd_len, "%s", c);
        *type = SHORTCUT_ACT_EXEC;
    } else {
        return 0;
    }
    return 1;
}

static void shortcuts_set(ShortcutConfig *sc, unsigned int mods, KeySym sym, ShortcutActionType type, const char *cmd) {
    KeySym lo = lower_keysym(sym);
    int idx = -1;
    for (int i = 0; i < sc->count; i++) {
        if (sc->bindings[i].mod_mask == mods && lower_keysym(sc->bindings[i].keysym) == lo) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (sc->count >= MAX_SHORTCUTS) return;
        idx = sc->count++;
    }
    sc->bindings[idx].mod_mask = mods;
    sc->bindings[idx].keysym = sym;
    sc->bindings[idx].type = type;
    snprintf(sc->bindings[idx].exec_cmd, sizeof(sc->bindings[idx].exec_cmd), "%s", cmd ? cmd : "");
}

void shortcuts_init_defaults(ShortcutConfig *sc) {
    if (!sc) return;
    sc->count = 0;
    shortcuts_set(sc, Mod4Mask, XK_Left, SHORTCUT_ACT_TILE_LEFT, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Right, SHORTCUT_ACT_TILE_RIGHT, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Up, SHORTCUT_ACT_FULLSCREEN, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Down, SHORTCUT_ACT_FLOAT, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Return, SHORTCUT_ACT_EXEC, "wezterm");
    shortcuts_set(sc, Mod4Mask, XK_b, SHORTCUT_ACT_EXEC, "google-chrome");
    shortcuts_set(sc, Mod4Mask, XK_q, SHORTCUT_ACT_CLOSE, NULL);
}

void shortcuts_load_file(ShortcutConfig *sc, const char *path) {
    if (!sc || !path) return;
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *l = trim(line);
        if (l[0] == '\0' || l[0] == '#') continue;
        char *eq = strchr(l, '=');
        if (!eq) continue;
        *eq = '\0';
        char *combo = trim(l);
        char *action = trim(eq + 1);
        if (combo[0] == '\0' || action[0] == '\0') continue;

        unsigned int mods;
        KeySym sym;
        if (!parse_combo(combo, &mods, &sym)) continue;
        ShortcutActionType type;
        char cmd[256];
        if (!parse_action(action, &type, cmd, sizeof(cmd))) continue;
        shortcuts_set(sc, mods, sym, type, cmd);
    }
    fclose(f);
}

void shortcuts_load(ShortcutConfig *sc) {
    if (!sc) return;
    shortcuts_init_defaults(sc);
    char path[600];
    if (config_path("shortcuts.conf", path, sizeof(path))) {
        shortcuts_load_file(sc, path);
    }
}

void shortcuts_grab_keys(Display *dpy, Window root, const ShortcutConfig *sc) {
    if (!dpy || !sc) return;
    const unsigned int locks[4] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (int i = 0; i < sc->count; i++) {
        if (sc->bindings[i].type == SHORTCUT_ACT_NONE) continue;
        KeyCode kc = XKeysymToKeycode(dpy, sc->bindings[i].keysym);
        if (kc == 0) continue;
        for (int l = 0; l < 4; l++) {
            XGrabKey(dpy, kc, sc->bindings[i].mod_mask | locks[l], root, True,
                     GrabModeAsync, GrabModeAsync);
        }
    }
}

const ShortcutBinding *shortcuts_match_event(const ShortcutConfig *sc, unsigned int state, KeySym sym) {
    if (!sc) return NULL;
    unsigned int mods = state & RELEVANT_MODS;
    KeySym lo = lower_keysym(sym);
    for (int i = 0; i < sc->count; i++) {
        if (sc->bindings[i].mod_mask == mods && lower_keysym(sc->bindings[i].keysym) == lo) {
            return &sc->bindings[i];
        }
    }
    return NULL;
}
