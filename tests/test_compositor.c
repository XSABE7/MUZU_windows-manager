#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "compositor.h"

int main(void) {
    // Test picom detection in ~/.local/bin
    char found_bin[1024];
    system("mkdir -p /tmp/muzuwm_test_home/.local/bin");
    system("touch /tmp/muzuwm_test_home/.local/bin/picom && chmod +x /tmp/muzuwm_test_home/.local/bin/picom");

    const char *orig_home = getenv("HOME");
    setenv("HOME", "/tmp/muzuwm_test_home", 1);
    const char *orig_path = getenv("PATH");
    setenv("PATH", "/usr/bin:/bin", 1); // exclude ~/.local/bin from PATH

    int found = compositor_find_picom(found_bin, sizeof(found_bin));
    assert(found == 1);
    assert(strstr(found_bin, "/tmp/muzuwm_test_home/.local/bin/picom") != NULL);

    // Restore environment
    if (orig_home) setenv("HOME", orig_home, 1);
    if (orig_path) setenv("PATH", orig_path, 1);
    system("rm -rf /tmp/muzuwm_test_home");

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

    printf("test_compositor passed!\n");
    return 0;
}
