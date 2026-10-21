#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "compositor.h"

int main(void) {
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
    assert(strstr(buf, "strength =") != NULL);
    assert(strstr(buf, "vsync = true") != NULL);

    compositor_cleanup();

    printf("test_compositor passed!\n");
    return 0;
}
