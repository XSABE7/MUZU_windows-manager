#ifndef MUZU_GTK_THEME_H
#define MUZU_GTK_THEME_H

typedef struct {
    double r, g, b, a;
} ColorRGBA;

typedef struct {
    char theme_name[128];
    char icon_theme_name[128];
    char font_family[64];
    double font_size;
    int is_dark;
    ColorRGBA bg_titlebar;
    ColorRGBA fg_titlebar;
    ColorRGBA accent_color;
    ColorRGBA bg_surface;
    ColorRGBA border_color;
} GtkTheme;

void gtk_theme_init(GtkTheme *theme);
void gtk_theme_load_from_file(GtkTheme *theme, const char *filepath);
void gtk_theme_export_env(const GtkTheme *theme);

#endif /* MUZU_GTK_THEME_H */
