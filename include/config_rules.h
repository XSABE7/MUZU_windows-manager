#ifndef MUZU_CONFIG_RULES_H
#define MUZU_CONFIG_RULES_H

#include <X11/Xlib.h>

#define MAX_SHORTCUTS 64
#define MAX_AUTOSTART_CMDS 64

typedef enum {
    SHORTCUT_ACT_NONE = 0,
    SHORTCUT_ACT_TILE_LEFT,
    SHORTCUT_ACT_TILE_RIGHT,
    SHORTCUT_ACT_FULLSCREEN,
    SHORTCUT_ACT_FLOAT,
    SHORTCUT_ACT_CLOSE,
    SHORTCUT_ACT_LAUNCHER,
    SHORTCUT_ACT_EXEC
} ShortcutActionType;

typedef struct {
    unsigned int mod_mask;
    KeySym keysym;
    ShortcutActionType type;
    char exec_cmd[256];
} ShortcutBinding;

typedef struct {
    ShortcutBinding bindings[MAX_SHORTCUTS];
    int count;
} ShortcutConfig;

// Create ~/.config/muzuwm/{fullscreen_allowlist,auto_allowlist,shortcuts.conf} with defaults if missing
void config_rules_ensure_files(void);

// Returns 1 if any non-empty entry in `path` equals (case-insensitive) res_name, res_class or title
int config_is_in_fullscreen_allowlist_file(const char *path, const char *res_name, const char *res_class, const char *title);

// Same as above using ~/.config/muzuwm/fullscreen_allowlist and the window's WM_CLASS
int config_client_in_fullscreen_allowlist(Display *dpy, Window win, const char *title);

// Load startup commands (one per line, '#' comments and blank lines skipped); returns count
int config_load_auto_allowlist_file(const char *path, char out_cmds[][256], int max_cmds);

// Launch a shell command asynchronously in a detached session
void config_spawn_command(const char *cmd);

// Launch every command in ~/.config/muzuwm/auto_allowlist
void config_run_autostart(void);

void shortcuts_init_defaults(ShortcutConfig *sc);
void shortcuts_load_file(ShortcutConfig *sc, const char *path);
void shortcuts_load(ShortcutConfig *sc);
void shortcuts_grab_keys(Display *dpy, Window root, const ShortcutConfig *sc);
const ShortcutBinding *shortcuts_match_event(const ShortcutConfig *sc, unsigned int state, KeySym sym);

#endif /* MUZU_CONFIG_RULES_H */
