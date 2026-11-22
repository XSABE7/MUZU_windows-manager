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

    // Settings app values: absent settings use a safe single master default.
    char value[64];
    assert(config_get_setting("keyboard_layouts", value, sizeof(value)) == 0);
    assert(config_get_master_count() == 1);
    assert(config_is_alttab_enabled() == 1);
    char settings_path[512];
    snprintf(settings_path, sizeof(settings_path), "%s/.config/muzuwm/settings.conf", tmp_dir);
    FILE *f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "keyboard_layouts=us,th\nmaster_count=2\nenable_alttab=0\n");
    fclose(f);
    assert(config_get_setting("keyboard_layouts", value, sizeof(value)) == 1);
    assert(strcmp(value, "us,th") == 0);
    assert(config_get_master_count() == 2);
    assert(config_is_alttab_enabled() == 0);

    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "enable_alttab=off\n");
    fclose(f);
    assert(config_is_alttab_enabled() == 0);

    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "enable_alttab=1\n");
    fclose(f);
    assert(config_is_alttab_enabled() == 1);

    // Fallback to muzuwm.conf when key is absent in settings.conf
    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "master_count=1\n");
    fclose(f);
    char muzuwm_conf_path[512];
    snprintf(muzuwm_conf_path, sizeof(muzuwm_conf_path), "%s/.config/muzuwm/muzuwm.conf", tmp_dir);
    f = fopen(muzuwm_conf_path, "w");
    assert(f != NULL);
    fprintf(f, "enable_alttab = false\n");
    fclose(f);
    assert(config_is_alttab_enabled() == 0);

    // Default fullscreen_allowlist matches nothing
    assert(config_is_in_fullscreen_allowlist_file(fs_path, "wezterm", "WezTerm", "wezterm") == 0);

    // Populate fullscreen_allowlist with comments, blank lines, and entries
    f = fopen(fs_path, "w");
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
    assert(b_up && b_up->type == SHORTCUT_ACT_MAXIMIZE);

    const ShortcutBinding *b_down = shortcuts_match_event(&sc, Mod4Mask, XK_Down);
    assert(b_down && b_down->type == SHORTCUT_ACT_FLOAT);

    const ShortcutBinding *b_term = shortcuts_match_event(&sc, Mod4Mask, XK_Return);
    assert(b_term && b_term->type == SHORTCUT_ACT_EXEC);
    assert(strcmp(b_term->exec_cmd, "wezterm") == 0);

    const ShortcutBinding *b_browser = shortcuts_match_event(&sc, Mod4Mask, XK_b);
    assert(b_browser && b_browser->type == SHORTCUT_ACT_EXEC);
    assert(strstr(b_browser->exec_cmd, "chrome") != NULL);

    const ShortcutBinding *b_x = shortcuts_match_event(&sc, Mod4Mask, XK_x);
    assert(b_x && b_x->type == SHORTCUT_ACT_CLOSE);

    const ShortcutBinding *b_m = shortcuts_match_event(&sc, Mod4Mask, XK_m);
    assert(b_m && b_m->type == SHORTCUT_ACT_MAXIMIZE);

    const ShortcutBinding *b_z = shortcuts_match_event(&sc, Mod4Mask, XK_z);
    assert(b_z && b_z->type == SHORTCUT_ACT_MINIMIZE);

    // Super+space is intentionally unbound by default (reserved for keyboard layout switch; use Ctrl+Alt+Delete for mzml)
    assert(shortcuts_match_event(&sc, Mod4Mask, XK_space) == NULL);

    // Unbound combination matches nothing
    assert(shortcuts_match_event(&sc, Mod4Mask | ShiftMask, XK_Left) == NULL);

    // 4. Malformed / empty lines in shortcuts.conf, plus override, unbind (none), and new binding
    f = fopen(sc_path, "w");
    assert(f != NULL);
    fprintf(f, "=tile_left\nSuper+Up=\n=\n   =   \nCtrl+Alt+t = exec wezterm\nSuper+q = close_window\nSuper+Return = exec xterm\nSuper+f = fullscreen\nSuper+b = none\n");
    fclose(f);

    shortcuts_load_file(&sc, sc_path);
    const ShortcutBinding *b_custom = shortcuts_match_event(&sc, ControlMask | Mod1Mask, XK_t);
    assert(b_custom && b_custom->type == SHORTCUT_ACT_EXEC);
    assert(strcmp(b_custom->exec_cmd, "wezterm") == 0);

    const ShortcutBinding *b_close = shortcuts_match_event(&sc, Mod4Mask, XK_q);
    assert(b_close && b_close->type == SHORTCUT_ACT_CLOSE);

    // Override replaces in place; empty action line leaves default Super+Up intact; fullscreen != maximize; none unbinds Super+b
    const ShortcutBinding *b_over = shortcuts_match_event(&sc, Mod4Mask, XK_Return);
    assert(b_over && strcmp(b_over->exec_cmd, "xterm") == 0);
    b_up = shortcuts_match_event(&sc, Mod4Mask, XK_Up);
    assert(b_up && b_up->type == SHORTCUT_ACT_MAXIMIZE);
    const ShortcutBinding *b_fs = shortcuts_match_event(&sc, Mod4Mask, XK_f);
    assert(b_fs && b_fs->type == SHORTCUT_ACT_FULLSCREEN);
    b_browser = shortcuts_match_event(&sc, Mod4Mask, XK_b);
    assert(b_browser && b_browser->type == SHORTCUT_ACT_NONE);

    // 5. Workspace count setting & clamping [1, 10]
    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "master_count=1\n");
    fclose(f);
    assert(config_get_workspace_count() == 4);

    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "workspace_count=6\n");
    fclose(f);
    assert(config_get_workspace_count() == 6);

    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "workspace_count=0\n");
    fclose(f);
    assert(config_get_workspace_count() == 1);

    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "workspace_count=99\n");
    fclose(f);
    assert(config_get_workspace_count() == 10);

    // 6. Workspace switch/move/next/prev shortcuts
    f = fopen(sc_path, "w");
    assert(f != NULL);
    fprintf(f, "Super+Ctrl+2 = workspace_2\nSuper+Shift+3 = move_workspace_3\nSuper+Page_Up = workspace_prev\nSuper+Page_Down = workspace_next\n");
    fclose(f);
    shortcuts_load_file(&sc, sc_path);

    const ShortcutBinding *b_ws2 = shortcuts_match_event(&sc, Mod4Mask | ControlMask, XK_2);
    assert(b_ws2 && b_ws2->type == SHORTCUT_ACT_WS_SWITCH && b_ws2->target_ws == 1);
    const ShortcutBinding *b_mws3 = shortcuts_match_event(&sc, Mod4Mask | ShiftMask, XK_3);
    assert(b_mws3 && b_mws3->type == SHORTCUT_ACT_WS_MOVE && b_mws3->target_ws == 2);
    const ShortcutBinding *b_wsp = shortcuts_match_event(&sc, Mod4Mask, XK_Page_Up);
    assert(b_wsp && b_wsp->type == SHORTCUT_ACT_WS_PREV);
    const ShortcutBinding *b_wsn = shortcuts_match_event(&sc, Mod4Mask, XK_Page_Down);
    assert(b_wsn && b_wsn->type == SHORTCUT_ACT_WS_NEXT);

    // 7. Overlong config/shortcut lines (> 512 bytes) must not split tail into fake entries
    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "# ");
    for (int i = 0; i < 509; i++) fputc('a', f);
    fprintf(f, "workspace_count=9\nmaster_count=2\n");
    fclose(f);
    assert(config_get_workspace_count() == 4);
    assert(config_get_master_count() == 2);

    f = fopen(sc_path, "w");
    assert(f != NULL);
    fprintf(f, "# ");
    for (int i = 0; i < 509; i++) fputc('b', f);
    fprintf(f, "Super+q = none\n");
    fclose(f);
    shortcuts_load_file(&sc, sc_path);
    assert(shortcuts_match_event(&sc, Mod4Mask, XK_q)->type == SHORTCUT_ACT_CLOSE);

    // 8. Focus, desktop menu, border width, and hex border color settings
    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "master_count=1\n");
    fclose(f);
    assert(config_is_focus_follows_mouse_enabled() == 1);
    assert(config_is_desktop_menu_enabled() == 1);
    assert(config_get_border_width() == 2);

    char border_hex[32];
    config_get_focus_border_color(border_hex, sizeof(border_hex));
    assert(strcmp(border_hex, "#89b4fa") == 0);
    config_get_unfocus_border_color(border_hex, sizeof(border_hex));
    assert(strcmp(border_hex, "#45475a") == 0);

    unsigned short r16 = 0, g16 = 0, b16 = 0;
    assert(config_parse_hex_color("#89b4fa", &r16, &g16, &b16) == 1);
    assert(r16 == 0x8989 && g16 == 0xb4b4 && b16 == 0xfafa);
    assert(config_parse_hex_color("ff5500", &r16, &g16, &b16) == 1);
    assert(r16 == 0xffff && g16 == 0x5555 && b16 == 0x0000);
    assert(config_parse_hex_color("#123", &r16, &g16, &b16) == 0);
    assert(config_parse_hex_color("#zzzzzz", &r16, &g16, &b16) == 0);
    assert(config_parse_hex_color("", &r16, &g16, &b16) == 0);
    assert(config_parse_hex_color(NULL, &r16, &g16, &b16) == 0);

    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "focus_follows_mouse=0\nenable_desktop_menu=off\nborder_width=4\nfocus_border_color=#ff5500\nunfocus_border_color=#112233\n");
    fclose(f);
    assert(config_is_focus_follows_mouse_enabled() == 0);
    assert(config_is_desktop_menu_enabled() == 0);
    assert(config_get_border_width() == 4);
    config_get_focus_border_color(border_hex, sizeof(border_hex));
    assert(strcmp(border_hex, "#ff5500") == 0);
    config_get_unfocus_border_color(border_hex, sizeof(border_hex));
    assert(strcmp(border_hex, "#112233") == 0);

    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "border_width=-5\nfocus_border_color=invalid\n");
    fclose(f);
    assert(config_get_border_width() == 0);
    config_get_focus_border_color(border_hex, sizeof(border_hex));
    assert(strcmp(border_hex, "#89b4fa") == 0);

    f = fopen(settings_path, "w");
    assert(f != NULL);
    fprintf(f, "border_width=99\n");
    fclose(f);
    assert(config_get_border_width() == 16);

    if (orig_home) setenv("HOME", orig_home, 1);
    char rm_cmd[600];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", tmp_dir);
    assert(system(rm_cmd) == 0);

    printf("test_config_rules passed!\n");
    return 0;
}
