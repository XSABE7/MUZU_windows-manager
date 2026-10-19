#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include "icon_fetcher.h"

int main(void) {
    printf("Running test_icon_fetcher...\n");

    // Test 1: Empty or NULL icon name
    char path[512];
    assert(icon_find_png(NULL, path, sizeof(path)) == 0);
    assert(icon_find_png("", path, sizeof(path)) == 0);
    assert(icon_find_png("non_existent_app_icon_xyz_12345", path, sizeof(path)) == 0);

    // Test 2: Standard icon lookup
    // If htop, debian-logo, or any common png exists, it should locate it
    if (access("/usr/share/pixmaps/debian-logo.png", R_OK) == 0) {
        assert(icon_find_png("debian-logo", path, sizeof(path)) == 1);
        assert(strstr(path, "debian-logo.png") != NULL);

        // Absolute path lookup
        assert(icon_find_png("/usr/share/pixmaps/debian-logo.png", path, sizeof(path)) == 1);
        assert(strcmp(path, "/usr/share/pixmaps/debian-logo.png") == 0);
    }

    // Test 3: Icon surface loading and status check
    cairo_surface_t *surf = icon_load_surface("non_existent_icon_name");
    assert(surf == NULL);

    if (access("/usr/share/pixmaps/debian-logo.png", R_OK) == 0) {
        surf = icon_load_surface("debian-logo");
        assert(surf != NULL);
        assert(cairo_surface_status(surf) == CAIRO_STATUS_SUCCESS);
        cairo_surface_destroy(surf);
    }

    // Test 4: Rendering fallback and real icon on Cairo surface
    cairo_surface_t *target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
    cairo_t *cr = cairo_create(target);

    // Fallback icon render (NULL surface)
    icon_render(cr, NULL, "Firefox", 10, 10, 38);
    assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);

    // Fallback with empty name
    icon_render(cr, NULL, "", 10, 10, 38);
    assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);

    cairo_destroy(cr);
    cairo_surface_destroy(target);

    printf("test_icon_fetcher passed!\n");
    return 0;
}
