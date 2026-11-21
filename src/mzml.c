#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mzml.h"

static char *trim_ws(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

MzmlRunMode mzml_parse_cli_mode(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!argv[i]) continue;
        if (strcmp(argv[i], "--magic") == 0 ||
            strcmp(argv[i], "--fallback") == 0 ||
            strcmp(argv[i], "--emergency") == 0) {
            return MZML_MODE_MAGIC;
        }
    }
    return MZML_MODE_LAUNCHER;
}

void mzml_build_terminal_cmd(char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    snprintf(out, out_len,
             "if command -v wezterm >/dev/null 2>&1; then "
             "  exec wezterm start --always-new-process; "
             "elif command -v alacritty >/dev/null 2>&1; then "
             "  exec alacritty; "
             "elif command -v xterm >/dev/null 2>&1; then "
             "  exec xterm; "
             "else "
             "  exec x-terminal-emulator; "
             "fi");
}

int mzml_enable_desktop_menu_in_file(const char *path) {
    if (!path || path[0] == '\0') return 0;
    FILE *in = fopen(path, "r");
    size_t cap = 4096;
    char *buf = malloc(cap);
    if (!buf) {
        if (in) fclose(in);
        return 0;
    }
    size_t used = 0;
    buf[0] = '\0';
    int replaced = 0;

    if (in) {
        char line[512];
        while (fgets(line, sizeof(line), in)) {
            char copy[512];
            snprintf(copy, sizeof(copy), "%s", line);
            char *eq = strchr(copy, '=');
            const char *write_line = line;
            if (eq) {
                *eq = '\0';
                char *key = trim_ws(copy);
                if (key[0] != '#' && strcmp(key, "enable_desktop_menu") == 0) {
                    write_line = "enable_desktop_menu=1\n";
                    replaced = 1;
                }
            }
            size_t wlen = strlen(write_line);
            if (used + wlen + 32 >= cap) {
                size_t new_cap = cap * 2 + wlen + 64;
                char *grown = realloc(buf, new_cap);
                if (!grown) {
                    free(buf);
                    fclose(in);
                    return 0;
                }
                buf = grown;
                cap = new_cap;
            }
            memcpy(buf + used, write_line, wlen);
            used += wlen;
            buf[used] = '\0';
        }
        fclose(in);
    }

    if (!replaced) {
        const char *add = (used > 0 && buf[used - 1] != '\n')
                              ? "\nenable_desktop_menu=1\n"
                              : "enable_desktop_menu=1\n";
        size_t alen = strlen(add);
        if (used + alen + 1 >= cap) {
            char *grown = realloc(buf, used + alen + 64);
            if (!grown) {
                free(buf);
                return 0;
            }
            buf = grown;
        }
        memcpy(buf + used, add, alen + 1);
        used += alen;
    }

    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", path, (long)getpid());
    FILE *out = fopen(tmp, "w");
    if (!out) {
        free(buf);
        return 0;
    }
    size_t written = fwrite(buf, 1, used, out);
    int close_ok = (fclose(out) == 0);
    free(buf);
    if (written != used || !close_ok) {
        unlink(tmp);
        return 0;
    }
    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return 0;
    }
    return 1;
}

static void discard_line_tail(FILE *f, const char *buf) {
    if (!f || !buf || strchr(buf, '\n') != NULL) return;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {}
}

static int ci_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    if (needle[0] == '\0') return 1;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++) {
        if (strncasecmp(p, needle, nlen) == 0) return 1;
    }
    return 0;
}

static int ci_starts_with(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    size_t nlen = strlen(needle);
    return strncasecmp(haystack, needle, nlen) == 0;
}

static int is_field_code(char c) {
    switch (c) {
        case 'f': case 'F':
        case 'u': case 'U':
        case 'd': case 'D':
        case 'n': case 'N':
        case 'v': case 'm':
        case 'i': case 'c':
        case 'k':
            return 1;
        default:
            return 0;
    }
}

void mzml_clean_exec_cmd(const char *raw, char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';
    if (!raw) return;

    size_t j = 0;
    int prev_space = 1;
    for (size_t i = 0; raw[i] != '\0' && j + 1 < out_len; i++) {
        if (raw[i] == '%') {
            char next = raw[i + 1];
            if (next == '%') {
                out[j++] = '%';
                prev_space = 0;
                i++;
                continue;
            }
            if (next != '\0' && is_field_code(next)) {
                i++;
                continue;
            }
        }
        if (isspace((unsigned char)raw[i])) {
            if (!prev_space && j + 1 < out_len) {
                out[j++] = ' ';
                prev_space = 1;
            }
            continue;
        }
        out[j++] = raw[i];
        prev_space = 0;
    }
    while (j > 0 && out[j - 1] == ' ') j--;
    out[j] = '\0';
}

static int executable_exists(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return 0;
    if (strchr(cmd, '/') != NULL) {
        return access(cmd, X_OK) == 0;
    }
    const char *path_env = getenv("PATH");
    if (!path_env) return 0;

    char *copy = strdup(path_env);
    if (!copy) return 0;
    int found = 0;
    char *save = NULL;
    for (char *dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        char candidate[1024];
        if (snprintf(candidate, sizeof(candidate), "%s/%s", dir, cmd) < (int)sizeof(candidate)) {
            if (access(candidate, X_OK) == 0) {
                found = 1;
                break;
            }
        }
    }
    free(copy);
    return found;
}

int mzml_parse_desktop_file(const char *path, const char *desktop_id, MzmlAppEntry *out) {
    if (!path || !out) return 0;
    memset(out, 0, sizeof(*out));
    if (desktop_id) {
        snprintf(out->desktop_id, sizeof(out->desktop_id), "%s", desktop_id);
    }

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int in_entry = 0;
    int saw_entry = 0;
    int is_app = 0;
    int hidden = 0;
    char try_exec[256] = {0};

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        discard_line_tail(f, line);
        char *s = trim_ws(line);
        if (s[0] == '\0' || s[0] == '#') continue;

        if (s[0] == '[') {
            if (strcmp(s, "[Desktop Entry]") == 0) {
                in_entry = 1;
                saw_entry = 1;
            } else if (saw_entry) {
                break;
            } else {
                in_entry = 0;
            }
            continue;
        }
        if (!in_entry) continue;

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_ws(s);
        char *val = trim_ws(eq + 1);

        if (strcmp(key, "Type") == 0) {
            is_app = (strcmp(val, "Application") == 0);
        } else if (strcmp(key, "Name") == 0 && out->name[0] == '\0') {
            snprintf(out->name, sizeof(out->name), "%s", val);
        } else if (strcmp(key, "GenericName") == 0 && out->generic_name[0] == '\0') {
            snprintf(out->generic_name, sizeof(out->generic_name), "%s", val);
        } else if (strcmp(key, "Comment") == 0 && out->comment[0] == '\0') {
            snprintf(out->comment, sizeof(out->comment), "%s", val);
        } else if (strcmp(key, "Exec") == 0 && out->exec_raw[0] == '\0') {
            snprintf(out->exec_raw, sizeof(out->exec_raw), "%s", val);
        } else if (strcmp(key, "TryExec") == 0) {
            snprintf(try_exec, sizeof(try_exec), "%s", val);
        } else if (strcmp(key, "Terminal") == 0) {
            out->terminal = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0;
        } else if (strcmp(key, "NoDisplay") == 0 || strcmp(key, "Hidden") == 0) {
            if (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0) {
                hidden = 1;
            }
        }
    }
    fclose(f);

    if (!saw_entry || !is_app || hidden) return 0;
    if (out->name[0] == '\0' || out->exec_raw[0] == '\0') return 0;
    if (try_exec[0] != '\0' && !executable_exists(try_exec)) return 0;

    mzml_clean_exec_cmd(out->exec_raw, out->exec_clean, sizeof(out->exec_clean));
    if (out->exec_clean[0] == '\0') return 0;
    return 1;
}

static int list_has_id(const MzmlAppList *list, const char *desktop_id) {
    if (!list || !desktop_id) return 0;
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->entries[i].desktop_id, desktop_id) == 0) return 1;
    }
    return 0;
}

static int compare_entries(const void *a, const void *b) {
    const MzmlAppEntry *ea = (const MzmlAppEntry *)a;
    const MzmlAppEntry *eb = (const MzmlAppEntry *)b;
    return strcasecmp(ea->name, eb->name);
}

static int append_entry(MzmlAppList *list, const MzmlAppEntry *entry) {
    if (list->count >= list->capacity) {
        int next_cap = (list->capacity > 0) ? list->capacity * 2 : 64;
        MzmlAppEntry *grown = realloc(list->entries, (size_t)next_cap * sizeof(MzmlAppEntry));
        if (!grown) return 0;
        list->entries = grown;
        list->capacity = next_cap;
    }
    list->entries[list->count++] = *entry;
    return 1;
}

void mzml_scan_dir(const char *dir_path, MzmlAppList *list) {
    if (!dir_path || !list) return;
    DIR *d = opendir(dir_path);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len <= 8 || strcmp(ent->d_name + len - 8, ".desktop") != 0) continue;
        if (list_has_id(list, ent->d_name)) continue;

        char full_path[1024];
        if (snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, ent->d_name) >= (int)sizeof(full_path)) {
            continue;
        }
        MzmlAppEntry item;
        if (mzml_parse_desktop_file(full_path, ent->d_name, &item)) {
            append_entry(list, &item);
        }
    }
    closedir(d);

    if (list->count > 1) {
        qsort(list->entries, (size_t)list->count, sizeof(MzmlAppEntry), compare_entries);
    }
}

void mzml_scan_default_dirs(MzmlAppList *list) {
    if (!list) return;
    const char *xdg_home = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    char user_apps[768];
    if (xdg_home && xdg_home[0] != '\0') {
        snprintf(user_apps, sizeof(user_apps), "%s/applications", xdg_home);
        mzml_scan_dir(user_apps, list);
    } else if (home && home[0] != '\0') {
        snprintf(user_apps, sizeof(user_apps), "%s/.local/share/applications", home);
        mzml_scan_dir(user_apps, list);
    }

    const char *xdg_dirs = getenv("XDG_DATA_DIRS");
    const char *dirs = (xdg_dirs && xdg_dirs[0] != '\0') ? xdg_dirs : "/usr/local/share:/usr/share";
    char *copy = strdup(dirs);
    if (!copy) return;
    char *save = NULL;
    for (char *base = strtok_r(copy, ":", &save); base; base = strtok_r(NULL, ":", &save)) {
        char app_dir[768];
        if (snprintf(app_dir, sizeof(app_dir), "%s/applications", base) < (int)sizeof(app_dir)) {
            mzml_scan_dir(app_dir, list);
        }
    }
    free(copy);
}

int mzml_filter_apps(const MzmlAppList *all, const char *query, int *out_indices, int max_out) {
    if (!all || !out_indices || max_out <= 0) return 0;
    const char *q = query ? query : "";
    while (*q && isspace((unsigned char)*q)) q++;

    int out_n = 0;
    if (q[0] == '\0') {
        for (int i = 0; i < all->count && out_n < max_out; i++) {
            out_indices[out_n++] = i;
        }
        return out_n;
    }

    // Pass 1: name prefix matches
    for (int i = 0; i < all->count && out_n < max_out; i++) {
        if (ci_starts_with(all->entries[i].name, q)) {
            out_indices[out_n++] = i;
        }
    }
    // Pass 2: name substring matches
    for (int i = 0; i < all->count && out_n < max_out; i++) {
        if (!ci_starts_with(all->entries[i].name, q) && ci_contains(all->entries[i].name, q)) {
            out_indices[out_n++] = i;
        }
    }
    // Pass 3: generic_name, comment, or exec_clean matches
    for (int i = 0; i < all->count && out_n < max_out; i++) {
        if (ci_contains(all->entries[i].name, q)) continue;
        if (ci_contains(all->entries[i].generic_name, q) ||
            ci_contains(all->entries[i].comment, q) ||
            ci_contains(all->entries[i].exec_clean, q)) {
            out_indices[out_n++] = i;
        }
    }
    return out_n;
}

void mzml_build_launch_command(const MzmlAppEntry *app, char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';
    if (!app || app->exec_clean[0] == '\0') return;
    if (!app->terminal) {
        snprintf(out, out_len, "%s", app->exec_clean);
        return;
    }

    char escaped[1024];
    size_t j = 0;
    for (size_t i = 0; app->exec_clean[i] != '\0' && j + 5 < sizeof(escaped); i++) {
        if (app->exec_clean[i] == '\'') {
            memcpy(escaped + j, "'\\''", 4);
            j += 4;
        } else {
            escaped[j++] = app->exec_clean[i];
        }
    }
    escaped[j] = '\0';

    snprintf(out, out_len,
             "if command -v wezterm >/dev/null 2>&1; then "
             "  exec wezterm start --always-new-process -- sh -c '%s'; "
             "elif command -v alacritty >/dev/null 2>&1; then "
             "  exec alacritty -e sh -c '%s'; "
             "elif command -v xterm >/dev/null 2>&1; then "
             "  exec xterm -e sh -c '%s'; "
             "else "
             "  exec x-terminal-emulator -e sh -c '%s'; "
             "fi",
             escaped, escaped, escaped, escaped);
}

void mzml_list_free(MzmlAppList *list) {
    if (!list) return;
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
}
