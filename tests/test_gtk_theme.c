#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "gtk_theme.h"

int main(void) {
    printf("Running test_gtk_theme...\n");

    // 1. Test Default Initialization
    GtkTheme theme;
    gtk_theme_init(&theme);
    assert(theme.is_dark == 1);
    assert(strlen(theme.theme_name) > 0);
    assert(strlen(theme.icon_theme_name) > 0);
    assert(theme.bg_titlebar.a > 0.0);
    assert(theme.fg_titlebar.a > 0.0);

    // 2. Test INI Parsing with Dark Theme
    const char *tmp_dark_ini = "/tmp/muzuwm_test_gtk_dark.ini";
    FILE *f = fopen(tmp_dark_ini, "w");
    assert(f != NULL);
    fprintf(f, "[Settings]\n");
    fprintf(f, "gtk-theme-name=WhiteSur-Dark\n");
    fprintf(f, "gtk-icon-theme-name=WhiteSur-dark\n");
    fprintf(f, "gtk-font-name=Ubuntu 11\n");
    fprintf(f, "gtk-application-prefer-dark-theme=1\n");
    fclose(f);

    gtk_theme_load_from_file(&theme, tmp_dark_ini);
    unlink(tmp_dark_ini);

    assert(strcmp(theme.theme_name, "WhiteSur-Dark") == 0);
    assert(strcmp(theme.icon_theme_name, "WhiteSur-dark") == 0);
    assert(strcmp(theme.font_family, "Ubuntu") == 0);
    assert(theme.font_size == 11.0);
    assert(theme.is_dark == 1);
    assert(theme.bg_titlebar.r < 0.3); // Dark titlebar

    // 3. Test INI Parsing with Light Theme
    const char *tmp_light_ini = "/tmp/muzuwm_test_gtk_light.ini";
    f = fopen(tmp_light_ini, "w");
    assert(f != NULL);
    fprintf(f, "[Settings]\n");
    fprintf(f, "gtk-theme-name=Adwaita\n");
    fprintf(f, "gtk-icon-theme-name=hicolor\n");
    fprintf(f, "gtk-font-name=Cantarell 12\n");
    fprintf(f, "gtk-application-prefer-dark-theme=0\n");
    fclose(f);

    gtk_theme_load_from_file(&theme, tmp_light_ini);
    unlink(tmp_light_ini);

    assert(strcmp(theme.theme_name, "Adwaita") == 0);
    assert(strcmp(theme.icon_theme_name, "hicolor") == 0);
    assert(strcmp(theme.font_family, "Cantarell") == 0);
    assert(theme.font_size == 12.0);
    assert(theme.is_dark == 0);
    assert(theme.bg_titlebar.r > 0.8); // Light titlebar

    // 4. Test Environment Variable Export
    gtk_theme_export_env(&theme);
    const char *env_theme = getenv("GTK_THEME");
    assert(env_theme != NULL && strcmp(env_theme, "Adwaita") == 0);

    printf("test_gtk_theme passed!\n");
    return 0;
}
