#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "config_rules.h"

int main(void) {
    char tmp_dir[] = "/tmp/muzuwm_test_rules_XXXXXX";
    assert(mkdtemp(tmp_dir) != NULL);
    const char *orig_home = getenv("HOME");
    setenv("HOME", tmp_dir, 1);

    // 1. Ensure default config files are created
    config_rules_ensure_files();

    char fs_path[512], auto_path[512], sc_path[512];
    snprintf(fs_path, sizeof(fs_path), "%s/.config/muzuwm/fullscreen_allowlist", tmp_dir);
    snprintf(auto_path, sizeof(auto_path), "%s/.config/muzuwm/auto_allowlist", tmp_dir);
    snprintf(sc_path, sizeof(sc_path), "%s/.config/muzuwm/shortcuts.conf", tmp_dir);

    assert(access(fs_path, R_OK) == 0);
    assert(access(auto_path, R_OK) == 0);
    assert(access(sc_path, R_OK) == 0);

    // Default fullscreen_allowlist matches nothing
    assert(config_is_in_fullscreen_allowlist_file(fs_path, "wezterm", "WezTerm", "wezterm") == 0);

    // Populate fullscreen_allowlist with comments, blank lines, and entries
    FILE *f = fopen(fs_path, "w");
    assert(f != NULL);
    fprintf(f, "# Apps that open fullscreen automatically\n\n   \ngoogle-chrome\n  mpv  \n");
    fclose(f);

    assert(config_is_in_fullscreen_allowlist_file(fs_path, "google-chrome", "Google-chrome", "New Tab") == 1);
    assert(config_is_in_fullscreen_allowlist_file(fs_path, "gl", "mpv", "video.mkv") == 1);
    assert(config_is_in_fullscreen_allowlist_file(fs_path, "wezterm", "WezTerm", "Terminal") == 0);
    assert(config_is_in_fullscreen_allowlist_file(fs_path, "", "", "") == 0);
    assert(config_is_in_fullscreen_allowlist_file(fs_path, NULL, NULL, NULL) == 0);
    assert(config_is_in_fullscreen_allowlist_file("/nonexistent/file", "mpv", "mpv", "mpv") == 0);

    // 2. auto_allowlist loading
    char cmds[MAX_AUTOSTART_CMDS][256];
    assert(config_load_auto_allowlist_file(auto_path, cmds, MAX_AUTOSTART_CMDS) == 0);

    f = fopen(auto_path, "w");
    assert(f != NULL);
    fprintf(f, "# Startup programs\n\n  wezterm  \ngoogle-chrome --restore-last-session\n");
    fclose(f);

    int n = config_load_auto_allowlist_file(auto_path, cmds, MAX_AUTOSTART_CMDS);
    assert(n == 2);
    assert(strcmp(cmds[0], "wezterm") == 0);
    assert(strcmp(cmds[1], "google-chrome --restore-last-session") == 0);

    // 3. Default shortcuts
    ShortcutConfig sc = {0};
    shortcuts_load(&sc);
    assert(sc.count >= 7);

    const ShortcutBinding *b_left = shortcuts_match_event(&sc, Mod4Mask, XK_Left);
    assert(b_left && b_left->type == SHORTCUT_ACT_TILE_LEFT);

    // Must match when NumLock (Mod2Mask) or CapsLock (LockMask) is active
    const ShortcutBinding *b_right = shortcuts_match_event(&sc, Mod4Mask | Mod2Mask | LockMask, XK_Right);
    assert(b_right && b_right->type == SHORTCUT_ACT_TILE_RIGHT);

    const ShortcutBinding *b_up = shortcuts_match_event(&sc, Mod4Mask, XK_Up);
    assert(b_up && b_up->type == SHORTCUT_ACT_FULLSCREEN);

    const ShortcutBinding *b_down = shortcuts_match_event(&sc, Mod4Mask, XK_Down);
    assert(b_down && b_down->type == SHORTCUT_ACT_FLOAT);

    const ShortcutBinding *b_term = shortcuts_match_event(&sc, Mod4Mask, XK_Return);
    assert(b_term && b_term->type == SHORTCUT_ACT_EXEC);
    assert(strcmp(b_term->exec_cmd, "wezterm") == 0);

    const ShortcutBinding *b_browser = shortcuts_match_event(&sc, Mod4Mask, XK_b);
    assert(b_browser && b_browser->type == SHORTCUT_ACT_EXEC);
    assert(strstr(b_browser->exec_cmd, "chrome") != NULL);

    // Unbound combination matches nothing
    assert(shortcuts_match_event(&sc, Mod4Mask | ShiftMask, XK_Left) == NULL);

    // 4. Malformed / empty lines in shortcuts.conf, plus override and new binding
    f = fopen(sc_path, "w");
    assert(f != NULL);
    fprintf(f, "=tile_left\nSuper+Up=\n=\n   =   \nCtrl+Alt+t = exec wezterm\nSuper+q = close_window\nSuper+Return = exec xterm\n");
    fclose(f);

    shortcuts_load_file(&sc, sc_path);
    const ShortcutBinding *b_custom = shortcuts_match_event(&sc, ControlMask | Mod1Mask, XK_t);
    assert(b_custom && b_custom->type == SHORTCUT_ACT_EXEC);
    assert(strcmp(b_custom->exec_cmd, "wezterm") == 0);

    const ShortcutBinding *b_close = shortcuts_match_event(&sc, Mod4Mask, XK_q);
    assert(b_close && b_close->type == SHORTCUT_ACT_CLOSE);

    // Override replaces in place; empty action line leaves default Super+Up intact
    const ShortcutBinding *b_over = shortcuts_match_event(&sc, Mod4Mask, XK_Return);
    assert(b_over && strcmp(b_over->exec_cmd, "xterm") == 0);
    b_up = shortcuts_match_event(&sc, Mod4Mask, XK_Up);
    assert(b_up && b_up->type == SHORTCUT_ACT_FULLSCREEN);

    if (orig_home) setenv("HOME", orig_home, 1);
    char rm_cmd[600];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", tmp_dir);
    assert(system(rm_cmd) == 0);

    printf("test_config_rules passed!\n");
    return 0;
}
