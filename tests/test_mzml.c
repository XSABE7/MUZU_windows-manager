#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mzml.h"

static void write_text(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    fputs(content, f);
    fclose(f);
}

int main(void) {
    char clean[256];

    // 1. Exec field-code stripping and %% unescaping
    mzml_clean_exec_cmd("firefox --new-window %u", clean, sizeof(clean));
    assert(strcmp(clean, "firefox --new-window") == 0);

    mzml_clean_exec_cmd("env VAR=1 app %%f %F %i %c %k", clean, sizeof(clean));
    assert(strcmp(clean, "env VAR=1 app %f") == 0);

    mzml_clean_exec_cmd("   alacritty   --working-directory   %d   ", clean, sizeof(clean));
    assert(strcmp(clean, "alacritty --working-directory") == 0);

    // 2. .desktop file parsing
    char tmp_dir[] = "/tmp/muzu_test_mzml_XXXXXX";
    assert(mkdtemp(tmp_dir) != NULL);

    char dir_user[512], dir_sys[512], path[600];
    snprintf(dir_user, sizeof(dir_user), "%s/user", tmp_dir);
    snprintf(dir_sys, sizeof(dir_sys), "%s/sys", tmp_dir);
    assert(mkdir(dir_user, 0755) == 0);
    assert(mkdir(dir_sys, 0755) == 0);

    snprintf(path, sizeof(path), "%s/term.desktop", dir_user);
    write_text(path,
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Terminal\n"
        "GenericName=Shell Emulator\n"
        "Comment=Run command line\n"
        "Exec=wezterm start %F\n"
        "\n"
        "[Desktop Action NewWindow]\n"
        "Name=New Terminal Window\n"
        "Exec=wezterm --new\n");

    MzmlAppEntry entry;
    assert(mzml_parse_desktop_file(path, "term.desktop", &entry) == 1);
    assert(strcmp(entry.name, "Terminal") == 0);
    assert(strcmp(entry.generic_name, "Shell Emulator") == 0);
    assert(strcmp(entry.comment, "Run command line") == 0);
    assert(strcmp(entry.exec_clean, "wezterm start") == 0);
    assert(entry.terminal == 0);

    char launch_cmd[1024];
    mzml_build_launch_command(&entry, launch_cmd, sizeof(launch_cmd));
    assert(strcmp(launch_cmd, "wezterm start") == 0);

    snprintf(path, sizeof(path), "%s/htop.desktop", dir_user);
    write_text(path,
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Htop\n"
        "Exec=htop -d 10\n"
        "Terminal=true\n");
    assert(mzml_parse_desktop_file(path, "htop.desktop", &entry) == 1);
    assert(entry.terminal == 1);
    mzml_build_launch_command(&entry, launch_cmd, sizeof(launch_cmd));
    assert(strstr(launch_cmd, "wezterm") != NULL && strstr(launch_cmd, "htop -d 10") != NULL);
    unlink(path);

    // Reject NoDisplay, Hidden, Type=Link, missing Exec, missing TryExec
    snprintf(path, sizeof(path), "%s/nodisp.desktop", dir_sys);
    write_text(path, "[Desktop Entry]\nType=Application\nName=Secret\nExec=true\nNoDisplay=true\n");
    assert(mzml_parse_desktop_file(path, "nodisp.desktop", &entry) == 0);

    snprintf(path, sizeof(path), "%s/hidden.desktop", dir_sys);
    write_text(path, "[Desktop Entry]\nType=Application\nName=Hidden\nExec=true\nHidden=true\n");
    assert(mzml_parse_desktop_file(path, "hidden.desktop", &entry) == 0);

    snprintf(path, sizeof(path), "%s/link.desktop", dir_sys);
    write_text(path, "[Desktop Entry]\nType=Link\nName=Docs\nURL=https://example.com\n");
    assert(mzml_parse_desktop_file(path, "link.desktop", &entry) == 0);

    snprintf(path, sizeof(path), "%s/noexec.desktop", dir_sys);
    write_text(path, "[Desktop Entry]\nType=Application\nName=NoExec\n");
    assert(mzml_parse_desktop_file(path, "noexec.desktop", &entry) == 0);

    snprintf(path, sizeof(path), "%s/badtry.desktop", dir_sys);
    write_text(path, "[Desktop Entry]\nType=Application\nName=BadTry\nTryExec=/nonexistent/muzu_bin_999\nExec=true\n");
    assert(mzml_parse_desktop_file(path, "badtry.desktop", &entry) == 0);

    // System override target + second app for sorting/filtering
    snprintf(path, sizeof(path), "%s/term.desktop", dir_sys);
    write_text(path, "[Desktop Entry]\nType=Application\nName=System Terminal Overridden\nExec=xterm\n");

    snprintf(path, sizeof(path), "%s/myterm.desktop", dir_sys);
    write_text(path, "[Desktop Entry]\nType=Application\nName=My Terminal\nComment=Custom shell\nExec=alacritty\n");

    snprintf(path, sizeof(path), "%s/browser.desktop", dir_sys);
    write_text(path, "[Desktop Entry]\nType=Application\nName=Alpha Browser\nComment=Web viewer\nExec=firefox %u\n");

    // 3. Scan deduplicates by desktop_id (user wins) and sorts case-insensitively
    MzmlAppList list = {0};
    mzml_scan_dir(dir_user, &list);
    mzml_scan_dir(dir_sys, &list);
    assert(list.count == 3);
    assert(strcmp(list.entries[0].name, "Alpha Browser") == 0);
    assert(strcmp(list.entries[1].name, "My Terminal") == 0);
    assert(strcmp(list.entries[2].name, "Terminal") == 0);
    assert(strcmp(list.entries[2].exec_clean, "wezterm start") == 0);

    // 4. Filter and prefix-first ranking
    int idx[16];
    int n = mzml_filter_apps(&list, "", idx, 16);
    assert(n == 3);
    assert(idx[0] == 0 && idx[1] == 1 && idx[2] == 2);

    n = mzml_filter_apps(&list, "term", idx, 16);
    assert(n == 2);
    assert(strcmp(list.entries[idx[0]].name, "Terminal") == 0);
    assert(strcmp(list.entries[idx[1]].name, "My Terminal") == 0);

    n = mzml_filter_apps(&list, "VIEWER", idx, 16);
    assert(n == 1);
    assert(strcmp(list.entries[idx[0]].name, "Alpha Browser") == 0);

    n = mzml_filter_apps(&list, "alacritty", idx, 16);
    assert(n == 1);
    assert(strcmp(list.entries[idx[0]].name, "My Terminal") == 0);

    assert(mzml_filter_apps(&list, "no_match_xyz", idx, 16) == 0);

    mzml_list_free(&list);
    assert(list.entries == NULL && list.count == 0);

    // 5. CLI mode parsing, terminal command builder, and emergency settings recovery
    char *argv_norm[] = { "mzml", NULL };
    char *argv_magic[] = { "mzml", "--magic", NULL };
    char *argv_fb[] = { "mzml", "--fallback", NULL };
    assert(mzml_parse_cli_mode(1, argv_norm) == MZML_MODE_LAUNCHER);
    assert(mzml_parse_cli_mode(2, argv_magic) == MZML_MODE_MAGIC);
    assert(mzml_parse_cli_mode(2, argv_fb) == MZML_MODE_MAGIC);

    char term_cmd[1024];
    mzml_build_terminal_cmd(term_cmd, sizeof(term_cmd));
    assert(strstr(term_cmd, "wezterm") != NULL && strstr(term_cmd, "xterm") != NULL);

    char rec_settings[600];
    snprintf(rec_settings, sizeof(rec_settings), "%s/settings.conf", tmp_dir);
    write_text(rec_settings, "master_count=2\nenable_desktop_menu=0\nborder_width=4\n");
    assert(mzml_enable_desktop_menu_in_file(rec_settings) == 1);
    FILE *rf = fopen(rec_settings, "r");
    assert(rf != NULL);
    char rbuf[512] = {0};
    size_t rlen = fread(rbuf, 1, sizeof(rbuf) - 1, rf);
    rbuf[rlen] = '\0';
    fclose(rf);
    assert(strstr(rbuf, "enable_desktop_menu=1") != NULL);
    assert(strstr(rbuf, "master_count=2") != NULL);
    assert(strstr(rbuf, "border_width=4") != NULL);

    char rm_cmd[600];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", tmp_dir);
    assert(system(rm_cmd) == 0);

    printf("test_mzml passed!\n");
    return 0;
}
