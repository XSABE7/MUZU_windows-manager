#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include <fontconfig/fontconfig.h>
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

    // Test 5: Themed Icon Lookup and Inheritance
    icon_theme_set("WhiteSur-dark");
    char themed_path[512];
    int res = icon_find_themed_png("debian-logo", themed_path, sizeof(themed_path));
    if (access("/usr/share/pixmaps/debian-logo.png", R_OK) == 0) {
        assert(res == 1);
        assert(strstr(themed_path, "debian-logo.png") != NULL);
    }

    // Test with mock theme inheritance and transitive inheritance
    system("mkdir -p /tmp/muzuwm_test_icons/ChildTheme/apps/48 "
           "/tmp/muzuwm_test_icons/ParentTheme/apps/48 "
           "/tmp/muzuwm_test_icons/GrandparentTheme/apps/48");
    setenv("MUZUWM_TEST_ICON_DIR", "/tmp/muzuwm_test_icons", 1);

    FILE *ft = fopen("/tmp/muzuwm_test_icons/ChildTheme/index.theme", "w");
    if (ft) {
        fprintf(ft, "[Icon Theme]\nName=ChildTheme\nInherits=ParentTheme\n");
        fclose(ft);
    }
    FILE *ft2 = fopen("/tmp/muzuwm_test_icons/ParentTheme/index.theme", "w");
    if (ft2) {
        fprintf(ft2, "[Icon Theme]\nName=ParentTheme\nInherits=GrandparentTheme\n");
        fclose(ft2);
    }

    // Create a 1x1 test png in ParentTheme
    cairo_surface_t *dummy = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 48, 48);
    cairo_surface_write_to_png(dummy, "/tmp/muzuwm_test_icons/ParentTheme/apps/48/custom_tool.png");

    // Create a test png in GrandparentTheme for transitive inheritance
    cairo_surface_write_to_png(dummy, "/tmp/muzuwm_test_icons/GrandparentTheme/apps/48/grandparent_tool.png");

    // Create a long reverse-DNS icon name (> 64 chars) to test buffer expansion
    const char *long_name = "org.gnome.Settings.Troubleshooting.Network.DiagnosticService.AppExt";
    char long_png_path[512];
    snprintf(long_png_path, sizeof(long_png_path),
             "/tmp/muzuwm_test_icons/ChildTheme/apps/48/%s.png", long_name);
    cairo_surface_write_to_png(dummy, long_png_path);
    cairo_surface_destroy(dummy);

    // Set theme to ChildTheme
    icon_theme_set("ChildTheme");

    int found_inherited = icon_find_themed_png("custom_tool", themed_path, sizeof(themed_path));
    assert(found_inherited == 1);
    assert(strstr(themed_path, "custom_tool.png") != NULL);

    int found_transitive = icon_find_themed_png("grandparent_tool", themed_path, sizeof(themed_path));
    assert(found_transitive == 1);
    assert(strstr(themed_path, "grandparent_tool.png") != NULL);

    int found_long = icon_find_themed_png(long_name, themed_path, sizeof(themed_path));
    assert(found_long == 1);
    assert(strstr(themed_path, long_name) != NULL);

    // Clean up mock directory and environment
    system("rm -rf /tmp/muzuwm_test_icons");
    unsetenv("MUZUWM_TEST_ICON_DIR");

    cairo_debug_reset_static_data();
    FcFini();

    printf("test_icon_fetcher passed!\n");
    return 0;
}
