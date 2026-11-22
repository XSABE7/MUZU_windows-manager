#!/usr/bin/env python3
"""
MUZUwm Terminal Settings Manager
(Focus, Borders, Desktop Menu, Alt+Tab, Workspaces, Keyboard, Shortcuts, Autostart, Wallpaper & Backup)
"""

import os
import sys
import subprocess
import glob
import time
import tarfile

CONFIG_DIR = os.path.expanduser("~/.config/muzuwm")
SETTINGS_FILE = os.path.join(CONFIG_DIR, "settings.conf")
AUTOSTART_FILE = os.path.join(CONFIG_DIR, "auto_allowlist")
SHORTCUTS_FILE = os.path.join(CONFIG_DIR, "shortcuts.conf")
WALLPAPER_FILE = os.path.join(CONFIG_DIR, "wallpaper.png")
BACKUP_DIR = os.path.join(CONFIG_DIR, "backups")

DEFAULT_SHORTCUT_LINES = [
    "Super+Left = tile_left",
    "Super+Right = tile_right",
    "Super+Up = fullscreen",
    "Super+Down = float",
    "Super+Return = exec wezterm",
    "Super+space = launcher",
    "Super+b = exec google-chrome",
    "Super+q = close_window",
    "Super+x = close_window",
    "Super+m = fullscreen",
    "Super+z = minimize",
    "Super+Ctrl+1 = workspace_1",
    "Super+Ctrl+2 = workspace_2",
    "Super+Ctrl+3 = workspace_3",
    "Super+Ctrl+4 = workspace_4",
    "Super+Shift+1 = move_workspace_1",
    "Super+Shift+2 = move_workspace_2",
    "Super+Shift+3 = move_workspace_3",
    "Super+Shift+4 = move_workspace_4",
    "Super+Page_Up = workspace_prev",
    "Super+Page_Down = workspace_next",
]


class ConfigManager:
    @staticmethod
    def ensure_dirs():
        os.makedirs(CONFIG_DIR, exist_ok=True)
        os.makedirs(BACKUP_DIR, exist_ok=True)
        for f in [SETTINGS_FILE, AUTOSTART_FILE, SHORTCUTS_FILE]:
            if not os.path.exists(f):
                try:
                    with open(f, "a", encoding="utf-8"):
                        pass
                except Exception:
                    pass

    @staticmethod
    def get_key(filepath, key, fallback=""):
        if not os.path.exists(filepath):
            return fallback
        try:
            with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith("#") or "=" not in line:
                        continue
                    k, v = line.split("=", 1)
                    if k.strip() == key.strip():
                        return v.strip()
        except Exception:
            pass
        return fallback

    @staticmethod
    def set_key(filepath, key, value):
        ConfigManager.ensure_dirs()
        lines = []
        found = False
        if os.path.exists(filepath):
            try:
                with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                    lines = f.readlines()
            except Exception:
                lines = []

        new_lines = []
        for line in lines:
            stripped = line.strip()
            if stripped and not stripped.startswith("#") and "=" in stripped:
                k, _ = stripped.split("=", 1)
                if k.strip() == key.strip():
                    new_lines.append(f"{key}={value}\n")
                    found = True
                    continue
            new_lines.append(line)

        if not found:
            if new_lines and not new_lines[-1].endswith("\n"):
                new_lines.append("\n")
            new_lines.append(f"{key}={value}\n")

        temp_path = f"{filepath}.tmp.{os.getpid()}"
        try:
            with open(temp_path, "w", encoding="utf-8") as f:
                f.writelines(new_lines)
            os.replace(temp_path, filepath)
        except Exception as e:
            if os.path.exists(temp_path):
                os.remove(temp_path)
            raise e

    @staticmethod
    def read_lines(filepath, skip_comments=False):
        if not os.path.exists(filepath):
            return []
        res = []
        try:
            with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    s = line.strip()
                    if skip_comments and (not s or s.startswith("#")):
                        continue
                    if s:
                        res.append(s)
        except Exception:
            pass
        return res

    @staticmethod
    def write_lines(filepath, lines):
        ConfigManager.ensure_dirs()
        temp_path = f"{filepath}.tmp.{os.getpid()}"
        with open(temp_path, "w", encoding="utf-8") as f:
            for l in lines:
                f.write(l.strip() + "\n")
        os.replace(temp_path, filepath)

    @staticmethod
    def reload_muzuwm_settings():
        try:
            subprocess.run(
                ["xprop", "-root", "-format", "_MUZU_RELOAD_SETTINGS", "32c", "-set", "_MUZU_RELOAD_SETTINGS", "1"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
        except Exception:
            pass

    @staticmethod
    def reload_muzuwm_wallpaper():
        try:
            if os.path.isfile(WALLPAPER_FILE):
                for cmd in (["feh", "--bg-fill", WALLPAPER_FILE], ["xwallpaper", "--zoom", WALLPAPER_FILE]):
                    try:
                        if subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0:
                            break
                    except FileNotFoundError:
                        continue
            subprocess.run(
                ["xprop", "-root", "-format", "_MUZU_RELOAD_WALLPAPER", "32c", "-set", "_MUZU_RELOAD_WALLPAPER", "1"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
        except Exception:
            pass

    @staticmethod
    def _parse_bool(key, default=True):
        raw = ConfigManager.get_key(SETTINGS_FILE, key, "1" if default else "0").strip().lower()
        return raw not in ("0", "false", "no", "off")

    @staticmethod
    def _parse_int(key, default_val, low, high):
        raw = ConfigManager.get_key(SETTINGS_FILE, key, str(default_val)).strip()
        try:
            val = int(raw)
        except ValueError:
            return default_val
        return max(low, min(high, val))

    @staticmethod
    def _normalize_hex(val, fallback):
        s = (val or "").strip()
        if s.startswith("#"):
            s = s[1:]
        if len(s) == 6 and all(c in "0123456789abcdefABCDEF" for c in s):
            return "#" + s.lower()
        return fallback

    @staticmethod
    def get_alttab_enabled():
        return ConfigManager._parse_bool("enable_alttab", True)

    @staticmethod
    def set_alttab_enabled(enabled):
        ConfigManager.set_key(SETTINGS_FILE, "enable_alttab", "1" if enabled else "0")
        ConfigManager.reload_muzuwm_settings()

    @staticmethod
    def get_workspace_count():
        return ConfigManager._parse_int("workspace_count", 4, 1, 10)

    @staticmethod
    def set_workspace_count(count):
        c = max(1, min(10, int(count)))
        ConfigManager.set_key(SETTINGS_FILE, "workspace_count", str(c))
        ConfigManager.reload_muzuwm_settings()

    @staticmethod
    def get_master_count():
        return ConfigManager._parse_int("master_count", 1, 1, 8)

    @staticmethod
    def set_master_count(count):
        c = max(1, min(8, int(count)))
        ConfigManager.set_key(SETTINGS_FILE, "master_count", str(c))
        ConfigManager.reload_muzuwm_settings()

    @staticmethod
    def get_focus_follows_mouse():
        return ConfigManager._parse_bool("focus_follows_mouse", True)

    @staticmethod
    def get_desktop_menu_enabled():
        return ConfigManager._parse_bool("enable_desktop_menu", True)

    @staticmethod
    def get_border_width():
        return ConfigManager._parse_int("border_width", 2, 0, 16)

    @staticmethod
    def get_focus_border_color():
        return ConfigManager._normalize_hex(
            ConfigManager.get_key(SETTINGS_FILE, "focus_border_color", "#89b4fa"), "#89b4fa"
        )

    @staticmethod
    def get_unfocus_border_color():
        return ConfigManager._normalize_hex(
            ConfigManager.get_key(SETTINGS_FILE, "unfocus_border_color", "#45475a"), "#45475a"
        )

    @staticmethod
    def set_wm_behavior_settings(ffm, desktop_menu, border_width, focus_color, unfocus_color):
        bw = max(0, min(16, int(border_width)))
        fc = ConfigManager._normalize_hex(focus_color, "#89b4fa")
        uc = ConfigManager._normalize_hex(unfocus_color, "#45475a")
        ConfigManager.set_key(SETTINGS_FILE, "focus_follows_mouse", "1" if ffm else "0")
        ConfigManager.set_key(SETTINGS_FILE, "enable_desktop_menu", "1" if desktop_menu else "0")
        ConfigManager.set_key(SETTINGS_FILE, "border_width", str(bw))
        ConfigManager.set_key(SETTINGS_FILE, "focus_border_color", fc)
        ConfigManager.set_key(SETTINGS_FILE, "unfocus_border_color", uc)
        ConfigManager.reload_muzuwm_settings()

    @staticmethod
    def get_keyboard_settings():
        layouts = ConfigManager.get_key(SETTINGS_FILE, "keyboard_layouts", "")
        if not layouts:
            try:
                out = subprocess.check_output(["setxkbmap", "-query"], stderr=subprocess.DEVNULL).decode()
                for line in out.splitlines():
                    if line.startswith("layout:"):
                        layouts = line.split(":", 1)[1].strip()
                        break
            except Exception:
                layouts = "us,th"
        if not layouts:
            layouts = "us,th"

        option = ConfigManager.get_key(SETTINGS_FILE, "layout_switch_option", "super_space")
        return layouts, option

    @staticmethod
    def apply_keyboard_settings(layouts, option):
        ConfigManager.set_key(SETTINGS_FILE, "keyboard_layouts", layouts)
        ConfigManager.set_key(SETTINGS_FILE, "layout_switch_option", option)

        cmd = ["setxkbmap", "-layout", layouts]
        if option and option not in ("super_space", "none"):
            cmd += ["-option", option]
        elif option in ("none", "super_space"):
            cmd += ["-option", ""]

        try:
            subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass
        ConfigManager.reload_muzuwm_settings()

    @staticmethod
    def get_autostart_entries():
        lines = ConfigManager.read_lines(AUTOSTART_FILE, skip_comments=False)
        entries = []
        for l in lines:
            s = l.strip()
            if not s:
                continue
            if s.startswith("#"):
                clean = s.lstrip("#").strip()
                if clean:
                    entries.append({"command": clean, "enabled": False})
            else:
                entries.append({"command": s, "enabled": True})
        return entries

    @staticmethod
    def save_autostart_entries(entries):
        lines = []
        for e in entries:
            cmd = e["command"].strip()
            if not cmd:
                continue
            lines.append(cmd if e["enabled"] else f"# {cmd}")
        ConfigManager.write_lines(AUTOSTART_FILE, lines)

    @staticmethod
    def get_default_shortcuts():
        res = []
        for line in DEFAULT_SHORTCUT_LINES:
            combo, action = line.split("=", 1)
            res.append({"combo": combo.strip(), "action": action.strip()})
        return res

    @staticmethod
    def get_shortcuts():
        lines = ConfigManager.read_lines(SHORTCUTS_FILE, skip_comments=False)
        shortcuts = []
        for l in lines:
            s = l.strip()
            if not s or s.startswith("#") or "=" not in s:
                continue
            combo, action = s.split("=", 1)
            shortcuts.append({"combo": combo.strip(), "action": action.strip()})
        if not shortcuts:
            return ConfigManager.get_default_shortcuts()
        return shortcuts

    @staticmethod
    def save_shortcuts(shortcuts):
        lines = [f"{s['combo']} = {s['action']}" for s in shortcuts]
        ConfigManager.write_lines(SHORTCUTS_FILE, lines)

    @staticmethod
    def set_wallpaper_image(image_path):
        if not os.path.exists(image_path):
            return False
        ConfigManager.ensure_dirs()
        shutil_copy = False
        try:
            with open(image_path, "rb") as src, open(WALLPAPER_FILE, "wb") as dst:
                dst.write(src.read())
            shutil_copy = True
        except Exception:
            pass
        if shutil_copy:
            ConfigManager.reload_muzuwm_wallpaper()
        return shutil_copy

    @staticmethod
    def backup_configs():
        ConfigManager.ensure_dirs()
        ts = time.strftime("%Y%m%d_%H%M%S")
        archive_path = os.path.join(BACKUP_DIR, f"muzuwm_backup_{ts}.tar.gz")
        for f in [SETTINGS_FILE, AUTOSTART_FILE, SHORTCUTS_FILE]:
            pass
        with tarfile.open(archive_path, "w:gz") as tar:
            for f in [SETTINGS_FILE, AUTOSTART_FILE, SHORTCUTS_FILE]:
                if os.path.exists(f):
                    tar.add(f, arcname=os.path.basename(f))
        return archive_path

    @staticmethod
    def list_backups():
        if not os.path.isdir(BACKUP_DIR):
            return []
        files = glob.glob(os.path.join(BACKUP_DIR, "muzuwm_backup_*.tar.gz"))
        return sorted(files, reverse=True)

    @staticmethod
    def restore_backup(archive_path):
        if not os.path.isfile(archive_path):
            return False
        with tarfile.open(archive_path, "r:gz") as tar:
            tar.extractall(CONFIG_DIR)
        ConfigManager.reload_muzuwm_settings()
        ConfigManager.reload_muzuwm_wallpaper()
        return True

    @staticmethod
    def reset_factory_defaults():
        ConfigManager.backup_configs()
        ConfigManager.write_lines(SHORTCUTS_FILE, DEFAULT_SHORTCUT_LINES)
        ConfigManager.write_lines(AUTOSTART_FILE, [
            "# Commands that launch when MUZUwm starts",
        ])
        ConfigManager.write_lines(SETTINGS_FILE, [
            "keyboard_layouts=us,th",
            "layout_switch_option=super_space",
            "master_count=1",
            "workspace_count=4",
            "enable_alttab=1",
            "focus_follows_mouse=1",
            "enable_desktop_menu=1",
            "border_width=2",
            "focus_border_color=#89b4fa",
            "unfocus_border_color=#45475a",
        ])
        ConfigManager.reload_muzuwm_settings()
        return True


# ==============================================================================
# Plain CLI Mode (--cli / fallback when no TTY)
# ==============================================================================

def run_cli_menu():
    ConfigManager.ensure_dirs()
    while True:
        ffm = ConfigManager.get_focus_follows_mouse()
        dmenu = ConfigManager.get_desktop_menu_enabled()
        alttab = ConfigManager.get_alttab_enabled()
        bw = ConfigManager.get_border_width()
        fc = ConfigManager.get_focus_border_color()
        uc = ConfigManager.get_unfocus_border_color()
        ws = ConfigManager.get_workspace_count()
        mc = ConfigManager.get_master_count()

        print("\n" + "=" * 54)
        print("                  MUZUwm Settings")
        print("=" * 54)
        print(f"1. Alt+Tab Switcher      : {'ON' if alttab else 'OFF'}")
        print(f"2. Focus-Follows-Mouse   : {'ON' if ffm else 'OFF'}")
        print(f"3. Desktop Right-Click   : {'ON' if dmenu else 'OFF'}")
        print(f"4. Focus Borders         : {bw}px ({fc} / {uc})")
        print(f"5. Workspaces & Layout   : {ws} workspaces, master={mc}")
        print("6. Keyboard & Language")
        print("7. Keyboard Shortcuts")
        print("8. Autostart Commands")
        print("9. Wallpaper")
        print("10. Backup & Reset")
        print("11. Reload MUZUwm")
        print("12. Update MUZUwm (GitHub)")
        print("0. Exit")
        print("-" * 54)

        try:
            choice = input("Select (0-12): ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if choice == "0":
            break
        elif choice == "1":
            ConfigManager.set_alttab_enabled(not alttab)
        elif choice == "2":
            ConfigManager.set_wm_behavior_settings(not ffm, dmenu, bw, fc, uc)
        elif choice == "3":
            ConfigManager.set_wm_behavior_settings(ffm, not dmenu, bw, fc, uc)
        elif choice == "4":
            nbw = input(f"Border width 0-16 [{bw}]: ").strip() or str(bw)
            nfc = input(f"Focus color #RRGGBB [{fc}]: ").strip() or fc
            nuc = input(f"Unfocus color #RRGGBB [{uc}]: ").strip() or uc
            if nbw.lstrip("-").isdigit():
                ConfigManager.set_wm_behavior_settings(ffm, dmenu, int(nbw), nfc, nuc)
        elif choice == "5":
            nws = input(f"Workspace count 1-10 [{ws}]: ").strip() or str(ws)
            nmc = input(f"Master count 1-8 [{mc}]: ").strip() or str(mc)
            if nws.isdigit():
                ConfigManager.set_workspace_count(int(nws))
            if nmc.isdigit():
                ConfigManager.set_master_count(int(nmc))
        elif choice == "6":
            curr_l, curr_o = ConfigManager.get_keyboard_settings()
            nl = input(f"Layouts [{curr_l}]: ").strip() or curr_l
            no = input(f"Switch option [{curr_o}]: ").strip() or curr_o
            ConfigManager.apply_keyboard_settings(nl, no)
        elif choice == "7":
            sc = ConfigManager.get_shortcuts()
            for idx, s in enumerate(sc):
                print(f"  [{idx + 1}] {s['combo']} = {s['action']}")
            combo = input("Add/Update combo (blank to skip): ").strip()
            if combo:
                act = input("Action: ").strip()
                if act:
                    sc = [s for s in sc if s["combo"] != combo]
                    sc.append({"combo": combo, "action": act})
                    ConfigManager.save_shortcuts(sc)
        elif choice == "8":
            entries = ConfigManager.get_autostart_entries()
            for idx, e in enumerate(entries):
                print(f"  [{idx + 1}] {'[ON]' if e['enabled'] else '[OFF]'} {e['command']}")
            cmd = input("Add startup command (blank to skip): ").strip()
            if cmd:
                entries.append({"command": cmd, "enabled": True})
                ConfigManager.save_autostart_entries(entries)
        elif choice == "9":
            path = input("Wallpaper image path: ").strip()
            if path:
                ConfigManager.set_wallpaper_image(os.path.expanduser(path))
        elif choice == "10":
            sub = input("1) Backup  2) Restore latest  3) Factory reset: ").strip()
            if sub == "1":
                print(ConfigManager.backup_configs())
            elif sub == "2":
                bks = ConfigManager.list_backups()
                if bks:
                    ConfigManager.restore_backup(bks[0])
            elif sub == "3":
                if input("Type yes to reset: ").strip().lower() == "yes":
                    ConfigManager.reset_factory_defaults()
        elif choice == "11":
            ConfigManager.reload_muzuwm_settings()
            ConfigManager.reload_muzuwm_wallpaper()
        elif choice == "12":
            local_up = os.path.join(os.path.dirname(os.path.abspath(__file__)), "muzuwm_updater.py")
            if os.path.exists(local_up):
                subprocess.run([sys.executable, local_up, "--cli"])
            else:
                subprocess.run(["muzuwm-updater", "--cli"])


# ==============================================================================
# Dark Terminal TUI (Black Background)
# ==============================================================================

def run_tui_menu():
    import curses
    import locale
    locale.setlocale(locale.LC_ALL, "")
    ConfigManager.ensure_dirs()

    def init_colors():
        if not curses.has_colors():
            return False
        curses.start_color()
        curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_BLACK)   # Backdrop
        curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLACK)   # Panel
        curses.init_pair(3, curses.COLOR_BLACK, curses.COLOR_CYAN)    # Selected item
        curses.init_pair(4, curses.COLOR_CYAN, curses.COLOR_BLACK)    # Header / title
        curses.init_pair(5, curses.COLOR_WHITE, curses.COLOR_BLACK)   # Status bar
        curses.init_pair(6, curses.COLOR_CYAN, curses.COLOR_BLACK)    # Accent / labels
        curses.init_pair(7, curses.COLOR_RED, curses.COLOR_BLACK)     # Disabled / warning
        curses.init_pair(8, curses.COLOR_BLACK, curses.COLOR_WHITE)   # Text input box
        return True

    def safe_addstr(win, y, x, text, attr=0):
        h, w = win.getmaxyx()
        if y < 0 or y >= h or x >= w:
            return
        max_len = max(0, w - x - 1)
        if max_len <= 0:
            return
        try:
            win.addnstr(y, x, text, max_len, attr)
        except curses.error:
            pass

    def draw_backdrop(stdscr, title, footer, has_color):
        stdscr.erase()
        h, w = stdscr.getmaxyx()
        bg_attr = curses.color_pair(1) if has_color else 0
        hdr_attr = (curses.color_pair(4) | curses.A_BOLD) if has_color else curses.A_BOLD
        bar_attr = curses.color_pair(5) if has_color else curses.A_DIM
        stdscr.bkgd(" ", bg_attr)
        safe_addstr(stdscr, 0, 0, (" " + title).ljust(max(1, w - 1)), hdr_attr)
        if h > 1:
            safe_addstr(stdscr, 1, 0, ("-" * max(1, w - 1)), bg_attr)
        if h > 2:
            safe_addstr(stdscr, h - 1, 0, (" " + footer).ljust(max(1, w - 1)), bar_attr)
        stdscr.refresh()

    def make_panel(stdscr, req_h, req_w, title, has_color):
        sh, sw = stdscr.getmaxyx()
        ph = min(req_h, max(8, sh - 4))
        pw = min(req_w, max(30, sw - 4))
        py = max(2, (sh - ph) // 2)
        px = max(2, (sw - pw) // 2)
        win = curses.newwin(ph, pw, py, px)
        win.keypad(True)
        panel_attr = curses.color_pair(2) if has_color else 0
        accent_attr = (curses.color_pair(6) | curses.A_BOLD) if has_color else curses.A_BOLD
        win.bkgd(" ", panel_attr)
        win.erase()
        win.border("|", "|", "-", "-", "+", "+", "+", "+")
        hdr = f"[ {title} ]"
        safe_addstr(win, 0, max(2, (pw - len(hdr)) // 2), hdr, accent_attr)
        return win

    def tui_message(stdscr, title, lines, has_color):
        draw_backdrop(stdscr, "MUZUwm Settings", "[Enter/Space/Esc] OK", has_color)
        win = make_panel(stdscr, len(lines) + 7, 62, title, has_color)
        ph, pw = win.getmaxyx()
        sel_attr = (curses.color_pair(3) | curses.A_BOLD) if has_color else curses.A_REVERSE
        for idx, line in enumerate(lines[:max(1, ph - 5)]):
            safe_addstr(win, 2 + idx, 3, line)
        ok_lbl = "<   OK   >"
        safe_addstr(win, ph - 2, max(2, (pw - len(ok_lbl)) // 2), ok_lbl, sel_attr)
        win.refresh()
        while True:
            ch = win.getch()
            if ch in (10, 13, 27, ord(" "), ord("q"), ord("Q")):
                break

    def tui_confirm(stdscr, title, question, has_color):
        draw_backdrop(stdscr, "MUZUwm Settings", "[Left/Right] Choice  [Enter] Confirm  [Esc] Cancel", has_color)
        win = make_panel(stdscr, 9, 60, title, has_color)
        ph, pw = win.getmaxyx()
        sel_attr = (curses.color_pair(3) | curses.A_BOLD) if has_color else curses.A_REVERSE
        safe_addstr(win, 2, 3, question)
        choice = 1
        while True:
            yes_attr = sel_attr if choice == 0 else 0
            no_attr = sel_attr if choice == 1 else 0
            safe_addstr(win, ph - 2, pw // 2 - 12, "< YES >", yes_attr)
            safe_addstr(win, ph - 2, pw // 2 + 4, "<  NO >", no_attr)
            win.refresh()
            ch = win.getch()
            if ch in (curses.KEY_LEFT, curses.KEY_RIGHT, 9):
                choice = 1 - choice
            elif ch in (ord("y"), ord("Y")):
                return True
            elif ch in (ord("n"), ord("N"), 27, ord("q"), ord("Q")):
                return False
            elif ch in (10, 13):
                return choice == 0

    def tui_input(stdscr, title, prompt, initial, has_color):
        draw_backdrop(stdscr, "MUZUwm Settings", "[Enter] Save  [Esc] Cancel", has_color)
        win = make_panel(stdscr, 9, 68, title, has_color)
        ph, pw = win.getmaxyx()
        box_attr = curses.color_pair(8) if has_color else curses.A_REVERSE
        safe_addstr(win, 2, 3, prompt)
        buf = list(initial or "")
        curses.curs_set(1)
        try:
            while True:
                field_w = max(10, pw - 8)
                text = "".join(buf)
                view = text[-field_w:] if len(text) > field_w else text
                safe_addstr(win, 4, 3, (" " + view).ljust(field_w), box_attr)
                win.move(4, min(pw - 3, 4 + len(view)))
                win.refresh()
                ch = win.get_wch()
                if ch in ("\n", "\r"):
                    return "".join(buf).strip()
                if ch == "\x1b":
                    return None
                if ch in ("\b", "\x7f") or ch == curses.KEY_BACKSPACE:
                    if buf:
                        buf.pop()
                elif isinstance(ch, str) and ch.isprintable():
                    buf.append(ch)
        finally:
            curses.curs_set(0)

    def tui_list_menu(stdscr, title, subtitle, items, has_color, footer="[Up/Down] Move  [Enter] Select  [Esc/Q] Back"):
        idx = 0
        scroll = 0
        while True:
            draw_backdrop(stdscr, "MUZUwm Settings", footer, has_color)
            win = make_panel(stdscr, max(14, min(22, len(items) + 7)), 74, title, has_color)
            ph, pw = win.getmaxyx()
            accent_attr = (curses.color_pair(6) | curses.A_BOLD) if has_color else curses.A_BOLD
            sel_attr = (curses.color_pair(3) | curses.A_BOLD) if has_color else curses.A_REVERSE
            safe_addstr(win, 2, 3, subtitle, accent_attr)
            safe_addstr(win, 3, 2, "-" * (pw - 4))
            vis_rows = max(1, ph - 6)
            if idx >= len(items):
                idx = max(0, len(items) - 1)
            if idx < scroll:
                scroll = idx
            elif idx >= scroll + vis_rows:
                scroll = idx - vis_rows + 1
            for row in range(vis_rows):
                item_i = scroll + row
                if item_i >= len(items):
                    break
                label = items[item_i]
                line_text = (" " + label).ljust(pw - 6)
                safe_addstr(win, 4 + row, 3, line_text, sel_attr if item_i == idx else 0)
            win.refresh()
            ch = win.getch()
            if ch in (curses.KEY_UP, ord("k")):
                if items:
                    idx = (idx - 1) % len(items)
            elif ch in (curses.KEY_DOWN, ord("j")):
                if items:
                    idx = (idx + 1) % len(items)
            elif ch in (27, ord("q"), ord("Q")):
                return -1, None
            elif ch in (10, 13, ord(" ")):
                return idx, ch

    def tui_borders_sub(stdscr, has_color):
        while True:
            ffm = ConfigManager.get_focus_follows_mouse()
            dmenu = ConfigManager.get_desktop_menu_enabled()
            bw = ConfigManager.get_border_width()
            fc = ConfigManager.get_focus_border_color()
            uc = ConfigManager.get_unfocus_border_color()
            items = [
                f"Border Width        : {bw} px (0 - 16)",
                f"Focus Border Color  : {fc}",
                f"Unfocus Border Color: {uc}",
                "< Back"
            ]
            idx, _ = tui_list_menu(stdscr, "FOCUS BORDERS", "Configure X11 focus border width & #RRGGBB colors:", items, has_color)
            if idx < 0 or idx == 3:
                return
            if idx == 0:
                val = tui_input(stdscr, "BORDER WIDTH", "Width in pixels (0 - 16):", str(bw), has_color)
                if val is not None and val.lstrip("-").isdigit():
                    ConfigManager.set_wm_behavior_settings(ffm, dmenu, int(val), fc, uc)
            elif idx == 1:
                val = tui_input(stdscr, "FOCUS BORDER COLOR", "Hex color (#RRGGBB, e.g. #89b4fa):", fc, has_color)
                if val:
                    ConfigManager.set_wm_behavior_settings(ffm, dmenu, bw, val, uc)
            elif idx == 2:
                val = tui_input(stdscr, "UNFOCUS BORDER COLOR", "Hex color (#RRGGBB, e.g. #45475a):", uc, has_color)
                if val:
                    ConfigManager.set_wm_behavior_settings(ffm, dmenu, bw, fc, val)

    def tui_workspace_layout_sub(stdscr, has_color):
        while True:
            ws = ConfigManager.get_workspace_count()
            mc = ConfigManager.get_master_count()
            items = [
                f"Workspace Count : {ws} (1 - 10)",
                f"Master Count    : {mc} (1 - 8)",
                "< Back"
            ]
            idx, _ = tui_list_menu(stdscr, "WORKSPACES & LAYOUT", "Configure virtual desktops and tiled master count:", items, has_color)
            if idx < 0 or idx == 2:
                return
            if idx == 0:
                val = tui_input(stdscr, "WORKSPACES", "Number of workspaces (1 - 10):", str(ws), has_color)
                if val is not None and val.isdigit():
                    ConfigManager.set_workspace_count(int(val))
            elif idx == 1:
                val = tui_input(stdscr, "MASTER WINDOWS", "Number of master windows (1 - 8):", str(mc), has_color)
                if val is not None and val.isdigit():
                    ConfigManager.set_master_count(int(val))

    def tui_shortcuts_sub(stdscr, has_color):
        while True:
            sc = ConfigManager.get_shortcuts()
            items = [f"[{i + 1:2d}] {s['combo']:<18} = {s['action']}" for i, s in enumerate(sc)]
            items.extend([
                "+ Add new shortcut binding",
                "* Reset shortcuts to defaults",
                "< Back"
            ])
            idx, _ = tui_list_menu(stdscr, "KEYBOARD SHORTCUTS", "Edit ~/.config/muzuwm/shortcuts.conf (restart WM to re-grab)", items, has_color)
            if idx < 0 or idx == len(items) - 1:
                return
            if idx < len(sc):
                act = tui_input(stdscr, "EDIT SHORTCUT", f"Action for {sc[idx]['combo']} (or 'delete'):", sc[idx]["action"], has_color)
                if act is not None:
                    if act.lower() == "delete":
                        sc.pop(idx)
                    elif act:
                        sc[idx]["action"] = act
                    ConfigManager.save_shortcuts(sc)
            elif idx == len(sc):
                combo = tui_input(stdscr, "NEW SHORTCUT", "Key combo (e.g. Super+t, Alt+Tab):", "", has_color)
                if combo:
                    act = tui_input(stdscr, "NEW ACTION", f"Action for {combo} (e.g. exec xterm, close_window):", "", has_color)
                    if act:
                        sc.append({"combo": combo, "action": act})
                        ConfigManager.save_shortcuts(sc)
            else:
                if tui_confirm(stdscr, "RESET SHORTCUTS", "Reset shortcuts.conf to MUZUwm defaults?", has_color):
                    ConfigManager.save_shortcuts(ConfigManager.get_default_shortcuts())

    def tui_autostart_sub(stdscr, has_color):
        while True:
            entries = ConfigManager.get_autostart_entries()
            items = [f"[{'ON ' if e['enabled'] else 'OFF'}] {e['command']}" for e in entries]
            items.extend(["+ Add startup command", "< Back"])
            idx, ch = tui_list_menu(stdscr, "AUTOSTART COMMANDS", "[Space/Enter] Toggle ON/OFF, or add new command:", items, has_color)
            if idx < 0 or idx == len(items) - 1:
                return
            if idx < len(entries):
                if ch == ord(" "):
                    entries[idx]["enabled"] = not entries[idx]["enabled"]
                    ConfigManager.save_autostart_entries(entries)
                else:
                    act_idx, _ = tui_list_menu(stdscr, "ENTRY ACTION", entries[idx]["command"], ["Toggle ON / OFF", "Delete command", "< Back"], has_color)
                    if act_idx == 0:
                        entries[idx]["enabled"] = not entries[idx]["enabled"]
                        ConfigManager.save_autostart_entries(entries)
                    elif act_idx == 1:
                        entries.pop(idx)
                        ConfigManager.save_autostart_entries(entries)
            else:
                cmd = tui_input(stdscr, "ADD AUTOSTART", "Shell command (e.g. mzshell &):", "", has_color)
                if cmd:
                    entries.append({"command": cmd, "enabled": True})
                    ConfigManager.save_autostart_entries(entries)

    def tui_keyboard_sub(stdscr, has_color):
        while True:
            curr_l, curr_o = ConfigManager.get_keyboard_settings()
            items = [
                "Layout Preset : us,th         (US + Thai Kedmanee)",
                "Layout Preset : us,th(pat)    (US + Thai Pattachote)",
                "Layout Preset : us            (US English only)",
                f"Layout Preset : custom        (Current: {curr_l})",
                "Switch Option : super_space   (Super + Space)",
                "Switch Option : alt_shift     (Alt + Shift)",
                "Switch Option : caps_toggle   (Caps Lock)",
                "< Back"
            ]
            idx, _ = tui_list_menu(stdscr, "KEYBOARD & LANGUAGE", f"Active: layout={curr_l}  option={curr_o}", items, has_color)
            if idx < 0 or idx == len(items) - 1:
                return
            new_l, new_o = curr_l, curr_o
            if idx == 0:
                new_l = "us,th"
            elif idx == 1:
                new_l = "us,th(pat)"
            elif idx == 2:
                new_l = "us"
            elif idx == 3:
                val = tui_input(stdscr, "CUSTOM LAYOUTS", "Comma-separated XKB layouts:", curr_l, has_color)
                if val is None or not val:
                    continue
                new_l = val
            elif idx == 4:
                new_o = "super_space"
            elif idx == 5:
                new_o = "grp:alt_shift_toggle"
            elif idx == 6:
                new_o = "grp:caps_toggle"
            ConfigManager.apply_keyboard_settings(new_l, new_o)

    def tui_backup_sub(stdscr, has_color):
        while True:
            backups = ConfigManager.list_backups()
            items = [
                "Create Backup Archive Now",
                f"Restore Latest Backup ({os.path.basename(backups[0]) if backups else 'none'})",
                "Factory Reset All Configs",
                "< Back"
            ]
            idx, _ = tui_list_menu(stdscr, "BACKUP & RESET", f"Stored in {BACKUP_DIR}", items, has_color)
            if idx < 0 or idx == len(items) - 1:
                return
            if idx == 0:
                arc = ConfigManager.backup_configs()
                tui_message(stdscr, "BACKUP CREATED", ["Backup archive saved:", os.path.basename(arc)], has_color)
            elif idx == 1:
                if not backups:
                    tui_message(stdscr, "NO BACKUP", ["No backup archives found."], has_color)
                elif tui_confirm(stdscr, "RESTORE BACKUP", f"Restore {os.path.basename(backups[0])}?", has_color):
                    ConfigManager.restore_backup(backups[0])
                    tui_message(stdscr, "RESTORED", ["Settings restored and reloaded."], has_color)
            elif idx == 2:
                if tui_confirm(stdscr, "FACTORY RESET", "Reset all MUZUwm configs to factory defaults?", has_color):
                    ConfigManager.reset_factory_defaults()
                    tui_message(stdscr, "RESET COMPLETE", ["All settings reset to defaults."], has_color)

    def _tui_main(stdscr):
        curses.curs_set(0)
        stdscr.keypad(True)
        has_color = init_colors()
        sel = 0
        while True:
            alttab_on = ConfigManager.get_alttab_enabled()
            ffm_on = ConfigManager.get_focus_follows_mouse()
            dmenu_on = ConfigManager.get_desktop_menu_enabled()
            bw = ConfigManager.get_border_width()
            fc = ConfigManager.get_focus_border_color()
            uc = ConfigManager.get_unfocus_border_color()
            ws = ConfigManager.get_workspace_count()
            masters = ConfigManager.get_master_count()
            layouts, switch_opt = ConfigManager.get_keyboard_settings()
            shortcuts = ConfigManager.get_shortcuts()
            auto_entries = ConfigManager.get_autostart_entries()
            auto_active = sum(1 for e in auto_entries if e["enabled"])
            wall_state = "SET" if os.path.exists(WALLPAPER_FILE) else "NONE"
            backups = ConfigManager.list_backups()

            menu_items = [
                f"1. Alt+Tab Switcher     : [{'ON ' if alttab_on else 'OFF'}]",
                f"2. Focus-Follows-Mouse  : [{'ON ' if ffm_on else 'OFF'}]",
                f"3. Desktop Right-Click  : [{'ON ' if dmenu_on else 'OFF'}]  (Ctrl+Alt+Del always works)",
                f"4. Focus Borders        : {bw}px ({fc} / {uc})",
                f"5. Workspaces & Layout  : {ws} workspaces, master={masters}",
                f"6. Keyboard & Language  : {layouts} ({switch_opt})",
                f"7. Keyboard Shortcuts   : {len(shortcuts)} bindings",
                f"8. Autostart Commands   : {auto_active}/{len(auto_entries)} enabled",
                f"9. Wallpaper Image      : {wall_state}",
                f"10. Backup & Reset      : {len(backups)} archives",
                "U. Update MUZUwm        : GitHub (Stable / unstable)",
                "R. Reload MUZUwm Config : Apply changes now",
                "Q. Exit"
            ]

            draw_backdrop(
                stdscr,
                "MUZUwm Settings",
                "[Up/Down] Move  [Enter/Space] Select/Toggle  [R] Reload  [Q] Quit",
                has_color
            )
            win = make_panel(stdscr, 21, 76, "MUZUwm Settings", has_color)
            ph, pw = win.getmaxyx()
            accent_attr = (curses.color_pair(6) | curses.A_BOLD) if has_color else curses.A_BOLD
            sel_attr = (curses.color_pair(3) | curses.A_BOLD) if has_color else curses.A_REVERSE
            warn_attr = (curses.color_pair(7) | curses.A_BOLD) if has_color else curses.A_DIM

            safe_addstr(win, 2, 3, f"Config Dir : {CONFIG_DIR}", accent_attr)
            safe_addstr(win, 3, 2, "-" * (pw - 4))
            for i, label in enumerate(menu_items):
                if 4 + i >= ph - 2:
                    break
                is_off = (i == 0 and not alttab_on) or (i == 1 and not ffm_on) or (i == 2 and not dmenu_on)
                attr = sel_attr if i == sel else (warn_attr if is_off else 0)
                safe_addstr(win, 4 + i, 3, (" " + label).ljust(pw - 6), attr)
            safe_addstr(win, ph - 2, 3, "Changes apply immediately to MUZUwm via _MUZU_RELOAD_SETTINGS", accent_attr)
            win.refresh()

            ch = win.getch()
            if ch in (curses.KEY_UP, ord("k")):
                sel = (sel - 1) % len(menu_items)
            elif ch in (curses.KEY_DOWN, ord("j")):
                sel = (sel + 1) % len(menu_items)
            elif ch in (ord("q"), ord("Q"), 27):
                break
            elif ch in (ord("r"), ord("R")):
                ConfigManager.reload_muzuwm_settings()
                ConfigManager.reload_muzuwm_wallpaper()
                tui_message(stdscr, "RELOAD", ["Sent _MUZU_RELOAD_SETTINGS to MUZUwm."], has_color)
            elif ord("1") <= ch <= ord("9"):
                sel = ch - ord("1")
                ch = 10

            if ch in (10, 13, ord(" ")):
                if sel == 0:
                    ConfigManager.set_alttab_enabled(not alttab_on)
                elif sel == 1:
                    ConfigManager.set_wm_behavior_settings(not ffm_on, dmenu_on, bw, fc, uc)
                elif sel == 2:
                    ConfigManager.set_wm_behavior_settings(ffm_on, not dmenu_on, bw, fc, uc)
                elif ch == ord(" "):
                    continue
                elif sel == 3:
                    tui_borders_sub(stdscr, has_color)
                elif sel == 4:
                    tui_workspace_layout_sub(stdscr, has_color)
                elif sel == 5:
                    tui_keyboard_sub(stdscr, has_color)
                elif sel == 6:
                    tui_shortcuts_sub(stdscr, has_color)
                elif sel == 7:
                    tui_autostart_sub(stdscr, has_color)
                elif sel == 8:
                    path = tui_input(stdscr, "WALLPAPER", "Path to wallpaper image (.png/.jpg):", "", has_color)
                    if path:
                        expanded = os.path.expanduser(path)
                        if os.path.isfile(expanded):
                            ConfigManager.set_wallpaper_image(expanded)
                            tui_message(stdscr, "WALLPAPER UPDATED", ["Copied and applied:", expanded], has_color)
                        else:
                            tui_message(stdscr, "FILE NOT FOUND", ["Image file does not exist:", expanded], has_color)
                elif sel == 9:
                    tui_backup_sub(stdscr, has_color)
                elif sel == 10:
                    curses.endwin()
                    local_up = os.path.join(os.path.dirname(os.path.abspath(__file__)), "muzuwm_updater.py")
                    try:
                        if os.path.exists(local_up):
                            subprocess.run([sys.executable, local_up])
                        else:
                            subprocess.run(["muzuwm-updater"])
                    except Exception as exc:
                        print(f"Updater error: {exc}")
                        input("Press Enter...")
                    stdscr.refresh()
                elif sel == 11:
                    ConfigManager.reload_muzuwm_settings()
                    ConfigManager.reload_muzuwm_wallpaper()
                    tui_message(stdscr, "RELOAD", ["Sent _MUZU_RELOAD_SETTINGS to MUZUwm."], has_color)
                elif sel == 12:
                    break

    curses.wrapper(_tui_main)


# ==============================================================================
# Optional Dark GTK 3 GUI (--gui only)
# ==============================================================================

def run_gtk_gui():
    import gi
    gi.require_version("Gtk", "3.0")
    from gi.repository import Gtk, Gdk

    css = Gtk.CssProvider()
    css.load_from_data(b"""
        window, scrolledwindow, viewport, box, stack, stacksidebar, list, row {
            background-color: #000000;
            color: #e5e7eb;
        }
        entry, spinbutton, button {
            background-color: #111111;
            color: #f3f4f6;
            border: 1px solid #333333;
        }
    """)
    Gtk.StyleContext.add_provider_for_screen(
        Gdk.Screen.get_default(), css, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
    )

    class MuzuSettingsWindow(Gtk.Window):
        def __init__(self):
            super().__init__(title="MUZUwm Settings")
            self.set_default_size(760, 520)
            self.set_position(Gtk.WindowPosition.CENTER)
            ConfigManager.ensure_dirs()

            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
            box.set_margin_start(20)
            box.set_margin_end(20)
            box.set_margin_top(20)
            box.set_margin_bottom(20)
            self.add(box)

            grid = Gtk.Grid(row_spacing=10, column_spacing=16)
            box.pack_start(grid, False, False, 0)

            self.alttab_sw = Gtk.Switch()
            self.alttab_sw.set_active(ConfigManager.get_alttab_enabled())
            grid.attach(Gtk.Label(label="Alt+Tab Switcher:", xalign=0), 0, 0, 1, 1)
            grid.attach(self.alttab_sw, 1, 0, 1, 1)

            self.ffm_sw = Gtk.Switch()
            self.ffm_sw.set_active(ConfigManager.get_focus_follows_mouse())
            grid.attach(Gtk.Label(label="Focus-Follows-Mouse:", xalign=0), 0, 1, 1, 1)
            grid.attach(self.ffm_sw, 1, 1, 1, 1)

            self.dmenu_sw = Gtk.Switch()
            self.dmenu_sw.set_active(ConfigManager.get_desktop_menu_enabled())
            grid.attach(Gtk.Label(label="Desktop Right-Click Menu:", xalign=0), 0, 2, 1, 1)
            grid.attach(self.dmenu_sw, 1, 2, 1, 1)

            self.ws_spin = Gtk.SpinButton.new_with_range(1, 10, 1)
            self.ws_spin.set_value(ConfigManager.get_workspace_count())
            grid.attach(Gtk.Label(label="Workspaces (1-10):", xalign=0), 0, 3, 1, 1)
            grid.attach(self.ws_spin, 1, 3, 1, 1)

            self.mc_spin = Gtk.SpinButton.new_with_range(1, 8, 1)
            self.mc_spin.set_value(ConfigManager.get_master_count())
            grid.attach(Gtk.Label(label="Master Count (1-8):", xalign=0), 0, 4, 1, 1)
            grid.attach(self.mc_spin, 1, 4, 1, 1)

            self.bw_spin = Gtk.SpinButton.new_with_range(0, 16, 1)
            self.bw_spin.set_value(ConfigManager.get_border_width())
            grid.attach(Gtk.Label(label="Border Width (0-16 px):", xalign=0), 0, 5, 1, 1)
            grid.attach(self.bw_spin, 1, 5, 1, 1)

            self.fc_entry = Gtk.Entry()
            self.fc_entry.set_text(ConfigManager.get_focus_border_color())
            grid.attach(Gtk.Label(label="Focus Border Color:", xalign=0), 0, 6, 1, 1)
            grid.attach(self.fc_entry, 1, 6, 1, 1)

            self.uc_entry = Gtk.Entry()
            self.uc_entry.set_text(ConfigManager.get_unfocus_border_color())
            grid.attach(Gtk.Label(label="Unfocus Border Color:", xalign=0), 0, 7, 1, 1)
            grid.attach(self.uc_entry, 1, 7, 1, 1)

            save_btn = Gtk.Button(label="Save & Reload")
            def on_save(b):
                ConfigManager.set_key(SETTINGS_FILE, "enable_alttab", "1" if self.alttab_sw.get_active() else "0")
                ConfigManager.set_key(SETTINGS_FILE, "workspace_count", str(self.ws_spin.get_value_as_int()))
                ConfigManager.set_key(SETTINGS_FILE, "master_count", str(self.mc_spin.get_value_as_int()))
                ConfigManager.set_wm_behavior_settings(
                    self.ffm_sw.get_active(),
                    self.dmenu_sw.get_active(),
                    self.bw_spin.get_value_as_int(),
                    self.fc_entry.get_text(),
                    self.uc_entry.get_text()
                )
            save_btn.connect("clicked", on_save)
            box.pack_start(save_btn, False, False, 0)

    win = MuzuSettingsWindow()
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()


def main():
    if "--cli" in sys.argv or "--plain" in sys.argv:
        run_cli_menu()
        return

    if "--gui" in sys.argv:
        display = os.environ.get("DISPLAY")
        if display:
            try:
                import gi
                gi.require_version("Gtk", "3.0")
                from gi.repository import Gtk
                if Gtk.init_check()[0]:
                    run_gtk_gui()
                    return
            except Exception:
                pass

    if sys.stdin.isatty() and sys.stdout.isatty():
        try:
            run_tui_menu()
            return
        except Exception:
            pass

    run_cli_menu()


if __name__ == "__main__":
    main()
