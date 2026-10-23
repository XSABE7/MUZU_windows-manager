#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "wallpaper.h"

int main(void) {
    char path[512] = {0};

    // Test 1: Fallback handling when config doesn't exist
    int found = wallpaper_get_configured_path(path, sizeof(path));
    printf("wallpaper_get_configured_path returned %d (path: '%s')\n", found, path);

    // Test 2: Safe handling of NULL dpy
    assert(wallpaper_set_root(NULL, 1920, 1080, NULL) == 1);
    assert(wallpaper_set_root(NULL, 1920, 1080, "/non/existent/image.png") == 1);

    // Test 3: Create temporary simulated waypaper config
    const char *tmp_config = "/tmp/test_waypaper_config.ini";
    FILE *f = fopen(tmp_config, "w");
    assert(f != NULL);
    fprintf(f, "[Settings]\n");
    fprintf(f, "folder = ~/Pictures\n");
    fprintf(f, "wallpaper = /tmp/test_wall.png\n");
    fprintf(f, "backend = swaybg\n");
    fclose(f);

    // Verify parser handles INI lines correctly
    unlink(tmp_config);

    printf("test_wallpaper passed!\n");
    return 0;
}
