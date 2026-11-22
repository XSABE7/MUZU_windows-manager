#ifndef MUZU_MZML_H
#define MUZU_MZML_H

#include <stddef.h>

typedef enum {
    MZML_MODE_LAUNCHER = 0,
    MZML_MODE_MAGIC = 1
} MzmlRunMode;

typedef struct {
    char name[128];
    char generic_name[128];
    char comment[192];
    char exec_raw[512];
    char exec_clean[512];
    char desktop_id[128];
    int terminal;
} MzmlAppEntry;

typedef struct {
    MzmlAppEntry *entries;
    int count;
    int capacity;
} MzmlAppList;

MzmlRunMode mzml_parse_cli_mode(int argc, char **argv);
void mzml_clean_exec_cmd(const char *raw, char *out, size_t out_len);
void mzml_build_terminal_cmd(char *out, size_t out_len);
void mzml_build_launch_command(const MzmlAppEntry *app, char *out, size_t out_len);
int mzml_enable_desktop_menu_in_file(const char *path);
int mzml_parse_desktop_file(const char *path, const char *desktop_id, MzmlAppEntry *out);
void mzml_scan_dir(const char *dir_path, MzmlAppList *list);
void mzml_scan_default_dirs(MzmlAppList *list);
int mzml_filter_apps(const MzmlAppList *all, const char *query, int *out_indices, int max_out);
void mzml_list_free(MzmlAppList *list);

#endif /* MUZU_MZML_H */
