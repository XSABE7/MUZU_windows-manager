#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
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

static void discard_rest_of_line(FILE *f, const char *buf) {
    if (!f || !buf || strchr(buf, '\n') != NULL) return;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {}
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
        "#          workspace_1..workspace_10 move_workspace_1..move_workspace_10 workspace_next workspace_prev\n"
        "Super+Left = tile_left\n"
        "Super+Right = tile_right\n"
        "Super+Up = fullscreen\n"
        "Super+Down = float\n"
        "Super+Return = exec wezterm\n"
        "Super+space = launcher\n"
        "Super+b = exec google-chrome\n"
        "Super+q = close_window\n"
        "Super+x = close_window\n"
        "Super+m = fullscreen\n"
        "Super+z = minimize\n"
        "Super+Ctrl+1 = workspace_1\n"
        "Super+Ctrl+2 = workspace_2\n"
        "Super+Ctrl+3 = workspace_3\n"
        "Super+Ctrl+4 = workspace_4\n"
        "Super+Shift+1 = move_workspace_1\n"
        "Super+Shift+2 = move_workspace_2\n"
        "Super+Shift+3 = move_workspace_3\n"
        "Super+Shift+4 = move_workspace_4\n"
        "Super+Page_Up = workspace_prev\n"
        "Super+Page_Down = workspace_next\n");
    write_default("settings.conf",
        "# MUZUwm Core Settings\n"
        "master_count = 1\n"
        "workspace_count = 4\n"
        "enable_alttab = 1\n"
        "focus_follows_mouse = 1\n"
        "enable_desktop_menu = 1\n"
        "border_width = 2\n"
        "focus_border_color = #89b4fa\n"
        "unfocus_border_color = #45475a\n");
}

static int read_setting_from_file(const char *path, const char *key, char *out, size_t out_len) {
    if (!path || !key || !out || out_len == 0) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int found = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        discard_rest_of_line(f, line);
        char *entry = trim(line);
        if (entry[0] == '\0' || entry[0] == '#') continue;
        char *eq = strchr(entry, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(trim(entry), key) == 0) {
            snprintf(out, out_len, "%s", trim(eq + 1));
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

int config_get_setting(const char *key, char *out, size_t out_len) {
    if (!key || !out || out_len == 0) return 0;
    out[0] = '\0';
    char path[600];
    if (config_path("settings.conf", path, sizeof(path)) &&
        read_setting_from_file(path, key, out, out_len)) {
        return 1;
    }
    if (config_path("muzuwm.conf", path, sizeof(path)) &&
        read_setting_from_file(path, key, out, out_len)) {
        return 1;
    }
    if (read_setting_from_file("/etc/muzuwm/muzuwm.conf", key, out, out_len)) {
        return 1;
    }
    return 0;
}

int config_get_master_count(void) {
    char value[32];
    if (!config_get_setting("master_count", value, sizeof(value))) return 1;
    char *end = NULL;
    long count = strtol(value, &end, 10);
    if (!end || *end != '\0' || count < 1 || count > 8) return 1;
    return (int)count;
}

int config_get_workspace_count(void) {
    char value[32];
    if (!config_get_setting("workspace_count", value, sizeof(value))) return 4;
    char *end = NULL;
    long count = strtol(value, &end, 10);
    if (!end || *end != '\0') return 4;
    if (count < 1) return 1;
    if (count > 10) return 10;
    return (int)count;
}

static int parse_bool_setting(const char *key, int default_val) {
    char val[32];
    if (!config_get_setting(key, val, sizeof(val))) return default_val;
    if (strcasecmp(val, "0") == 0 || strcasecmp(val, "false") == 0 ||
        strcasecmp(val, "no") == 0 || strcasecmp(val, "off") == 0) {
        return 0;
    }
    return 1;
}

int config_is_alttab_enabled(void) {
    return parse_bool_setting("enable_alttab", 1);
}

int config_is_focus_follows_mouse_enabled(void) {
    return parse_bool_setting("focus_follows_mouse", 1);
}

int config_is_desktop_menu_enabled(void) {
    return parse_bool_setting("enable_desktop_menu", 1);
}

int config_get_border_width(void) {
    char value[32];
    if (!config_get_setting("border_width", value, sizeof(value))) return 2;
    char *end = NULL;
    long bw = strtol(value, &end, 10);
    if (!end || *end != '\0') return 2;
    if (bw < 0) return 0;
    if (bw > 16) return 16;
    return (int)bw;
}

static int hex_nibble(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

int config_parse_hex_color(const char *hex, unsigned short *r16, unsigned short *g16, unsigned short *b16) {
    if (!hex) return 0;
    if (hex[0] == '#') hex++;
    if (strlen(hex) != 6) return 0;

    int d[6];
    for (int i = 0; i < 6; i++) {
        d[i] = hex_nibble(hex[i]);
        if (d[i] < 0) return 0;
    }

    unsigned int r8 = (unsigned int)((d[0] << 4) | d[1]);
    unsigned int g8 = (unsigned int)((d[2] << 4) | d[3]);
    unsigned int b8 = (unsigned int)((d[4] << 4) | d[5]);
    if (r16) *r16 = (unsigned short)(r8 * 257U);
    if (g16) *g16 = (unsigned short)(g8 * 257U);
    if (b16) *b16 = (unsigned short)(b8 * 257U);
    return 1;
}

static void read_validated_hex_setting(const char *key, const char *fallback, char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    char val[32];
    if (config_get_setting(key, val, sizeof(val)) &&
        config_parse_hex_color(val, NULL, NULL, NULL)) {
        if (val[0] == '#') {
            snprintf(out, out_len, "%s", val);
        } else {
            snprintf(out, out_len, "#%s", val);
        }
        return;
    }
    snprintf(out, out_len, "%s", fallback);
}

void config_get_focus_border_color(char *out, size_t out_len) {
    read_validated_hex_setting("focus_border_color", "#89b4fa", out, out_len);
}

void config_get_unfocus_border_color(char *out, size_t out_len) {
    read_validated_hex_setting("unfocus_border_color", "#45475a", out, out_len);
}

int config_is_in_fullscreen_allowlist_file(const char *path, const char *res_name, const char *res_class, const char *title) {
    if (!path) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int found = 0;
    char line[512];
    while (!found && fgets(line, sizeof(line), f)) {
        discard_rest_of_line(f, line);
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
        discard_rest_of_line(f, line);
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
        signal(SIGCHLD, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
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
        {"page_up", XK_Page_Up}, {"page_down", XK_Page_Down},
        {"prior", XK_Prior}, {"next", XK_Next},
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

static int parse_action(char *action, ShortcutActionType *type, int *target_ws, char *cmd, size_t cmd_len) {
    cmd[0] = '\0';
    *target_ws = 0;
    if (strcasecmp(action, "none") == 0) *type = SHORTCUT_ACT_NONE;
    else if (strcmp(action, "tile_left") == 0) *type = SHORTCUT_ACT_TILE_LEFT;
    else if (strcmp(action, "tile_right") == 0) *type = SHORTCUT_ACT_TILE_RIGHT;
    else if (strcmp(action, "fullscreen") == 0 || strcmp(action, "maximize") == 0) *type = SHORTCUT_ACT_FULLSCREEN;
    else if (strcmp(action, "float") == 0) *type = SHORTCUT_ACT_FLOAT;
    else if (strcmp(action, "close_window") == 0 || strcmp(action, "close") == 0) *type = SHORTCUT_ACT_CLOSE;
    else if (strcmp(action, "minimize") == 0 || strcmp(action, "hide") == 0) *type = SHORTCUT_ACT_MINIMIZE;
    else if (strcmp(action, "launcher") == 0) *type = SHORTCUT_ACT_LAUNCHER;
    else if (strcmp(action, "workspace_next") == 0) *type = SHORTCUT_ACT_WS_NEXT;
    else if (strcmp(action, "workspace_prev") == 0) *type = SHORTCUT_ACT_WS_PREV;
    else if (strncmp(action, "workspace_", 10) == 0) {
        char *end = NULL;
        long n = strtol(action + 10, &end, 10);
        if (!end || *end != '\0' || n < 1 || n > 10) return 0;
        *type = SHORTCUT_ACT_WS_SWITCH;
        *target_ws = (int)(n - 1);
    } else if (strncmp(action, "move_workspace_", 15) == 0) {
        char *end = NULL;
        long n = strtol(action + 15, &end, 10);
        if (!end || *end != '\0' || n < 1 || n > 10) return 0;
        *type = SHORTCUT_ACT_WS_MOVE;
        *target_ws = (int)(n - 1);
    } else if (strncmp(action, "exec", 4) == 0 && isspace((unsigned char)action[4])) {
        char *c = trim(action + 4);
        if (c[0] == '\0') return 0;
        snprintf(cmd, cmd_len, "%s", c);
        *type = SHORTCUT_ACT_EXEC;
    } else {
        return 0;
    }
    return 1;
}

static void shortcuts_set(ShortcutConfig *sc, unsigned int mods, KeySym sym,
                          ShortcutActionType type, int target_ws, const char *cmd) {
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
    sc->bindings[idx].target_ws = target_ws;
    snprintf(sc->bindings[idx].exec_cmd, sizeof(sc->bindings[idx].exec_cmd), "%s", cmd ? cmd : "");
}

void shortcuts_init_defaults(ShortcutConfig *sc) {
    if (!sc) return;
    sc->count = 0;
    shortcuts_set(sc, Mod4Mask, XK_Left, SHORTCUT_ACT_TILE_LEFT, 0, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Right, SHORTCUT_ACT_TILE_RIGHT, 0, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Up, SHORTCUT_ACT_FULLSCREEN, 0, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Down, SHORTCUT_ACT_FLOAT, 0, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Return, SHORTCUT_ACT_EXEC, 0, "wezterm");
    shortcuts_set(sc, Mod4Mask, XK_space, SHORTCUT_ACT_LAUNCHER, 0, NULL);
    shortcuts_set(sc, Mod4Mask, XK_b, SHORTCUT_ACT_EXEC, 0, "google-chrome");
    shortcuts_set(sc, Mod4Mask, XK_q, SHORTCUT_ACT_CLOSE, 0, NULL);
    shortcuts_set(sc, Mod4Mask, XK_x, SHORTCUT_ACT_CLOSE, 0, NULL);
    shortcuts_set(sc, Mod4Mask, XK_m, SHORTCUT_ACT_FULLSCREEN, 0, NULL);
    shortcuts_set(sc, Mod4Mask, XK_z, SHORTCUT_ACT_MINIMIZE, 0, NULL);
    shortcuts_set(sc, Mod4Mask | ControlMask, XK_1, SHORTCUT_ACT_WS_SWITCH, 0, NULL);
    shortcuts_set(sc, Mod4Mask | ControlMask, XK_2, SHORTCUT_ACT_WS_SWITCH, 1, NULL);
    shortcuts_set(sc, Mod4Mask | ControlMask, XK_3, SHORTCUT_ACT_WS_SWITCH, 2, NULL);
    shortcuts_set(sc, Mod4Mask | ControlMask, XK_4, SHORTCUT_ACT_WS_SWITCH, 3, NULL);
    shortcuts_set(sc, Mod4Mask | ShiftMask, XK_1, SHORTCUT_ACT_WS_MOVE, 0, NULL);
    shortcuts_set(sc, Mod4Mask | ShiftMask, XK_2, SHORTCUT_ACT_WS_MOVE, 1, NULL);
    shortcuts_set(sc, Mod4Mask | ShiftMask, XK_3, SHORTCUT_ACT_WS_MOVE, 2, NULL);
    shortcuts_set(sc, Mod4Mask | ShiftMask, XK_4, SHORTCUT_ACT_WS_MOVE, 3, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Page_Up, SHORTCUT_ACT_WS_PREV, 0, NULL);
    shortcuts_set(sc, Mod4Mask, XK_Page_Down, SHORTCUT_ACT_WS_NEXT, 0, NULL);
}

void shortcuts_load_file(ShortcutConfig *sc, const char *path) {
    if (!sc || !path) return;
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        discard_rest_of_line(f, line);
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
        int target_ws = 0;
        char cmd[256];
        if (!parse_action(action, &type, &target_ws, cmd, sizeof(cmd))) continue;
        shortcuts_set(sc, mods, sym, type, target_ws, cmd);
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
