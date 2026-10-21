#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "compositor.h"

int main(void) {
    char tmp_dir[] = "/tmp/muzuwm_test_compositor_XXXXXX";
    char *created = mkdtemp(tmp_dir);
    assert(created != NULL);

    const char *orig_home = getenv("HOME");
    setenv("HOME", tmp_dir, 1);

    const char *cfg_path = compositor_get_config_path();
    assert(cfg_path != NULL);
    assert(strstr(cfg_path, "picom.conf") != NULL);

    // Initialize with NULL display (should generate config safely and not crash)
    compositor_init(NULL);

    // Verify config file was written
    FILE *f = fopen(cfg_path, "r");
    assert(f != NULL);

    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    assert(strstr(buf, "dual_kawase") != NULL);
    assert(strstr(buf, "glx") != NULL);
    assert(strstr(buf, "strength = 5") != NULL);
    assert(strstr(buf, "vsync = true") != NULL);

    compositor_cleanup();

    // Verify ~/.local/bin lookup helper
    char mock_bin_dir[256];
    snprintf(mock_bin_dir, sizeof(mock_bin_dir), "%s/.local/bin", tmp_dir);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s && touch %s/picom && chmod +x %s/picom", mock_bin_dir, mock_bin_dir, mock_bin_dir);
    assert(system(cmd) == 0);

    char found_bin[1024];
    const char *orig_path = getenv("PATH");
    setenv("PATH", "/usr/bin:/bin", 1);
    assert(compositor_find_picom(found_bin, sizeof(found_bin)) == 1);
    assert(strstr(found_bin, "/.local/bin/picom") != NULL);

    if (orig_path) setenv("PATH", orig_path, 1);
    if (orig_home) setenv("HOME", orig_home, 1);

    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp_dir);
    assert(system(cmd) == 0);

    printf("test_compositor passed!\n");
    return 0;
}
