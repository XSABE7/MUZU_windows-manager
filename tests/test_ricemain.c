#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ricemain.h"
#include "config_rules.h"
#include "menu.h"

static int g_action_triggered = 0;
static char g_last_cmd[256] = {0};

static void test_action_callback(int action_id, const char *cmd, void *userdata) {
    (void)userdata;
    g_action_triggered = action_id;
    if (cmd) {
        snprintf(g_last_cmd, sizeof(g_last_cmd), "%s", cmd);
    }
}

int main(void) {
    char tmp_dir[] = "/tmp/muzuwm_test_ricemain_XXXXXX";
    assert(mkdtemp(tmp_dir) != NULL);
    setenv("HOME", tmp_dir, 1);

    config_rules_ensure_files();

    /* 1. Test config functions */
    assert(config_is_ricewm_enabled() == 1);
    char mode[32];
    assert(config_get_desktop_menu_mode(mode, sizeof(mode)) == 1);
    assert(strcmp(mode, "radial") == 0);

    /* 2. Test ricemain_init without X display */
    int rc = ricemain_init(NULL, None, 1920, 1080, NULL, test_action_callback, NULL);
    assert(rc == 1);

    /* 3. Test radial item management */
    ricemain_radial_clear_items();
    assert(ricemain_radial_add_item("settings", "Settings", "preferences-system", "", DESKTOP_ACTION_SETTINGS) == 1);
    assert(ricemain_radial_add_item("apps", "App Menu", "system-run", "", DESKTOP_ACTION_APPLICATIONS) == 1);
    assert(ricemain_radial_add_item("terminal", "Terminal", "utilities-terminal", "", DESKTOP_ACTION_TERMINAL) == 1);

    /* 4. Test callback via test_action_callback */
    test_action_callback(DESKTOP_ACTION_SETTINGS, "", NULL);
    assert(g_action_triggered == DESKTOP_ACTION_SETTINGS);

    test_action_callback(DESKTOP_ACTION_TERMINAL, "wezterm", NULL);
    assert(g_action_triggered == DESKTOP_ACTION_TERMINAL);
    assert(strcmp(g_last_cmd, "wezterm") == 0);

    /* 5. Test cleanup */
    ricemain_cleanup();

    printf("test_ricemain passed successfully!\n");
    return 0;
}
