#!/usr/bin/env python3
"""
MUZUwm Comprehensive Control Centre & Settings Manager
(เปลี่ยนภาษา, Dock, Title bar, Fullscreen Allowlist, Autostart, Top Panel, Shortcuts, Wallpaper & Theme)
"""

import os
import sys
import shutil
import subprocess
import glob
import time
import tarfile

CONFIG_DIR = os.path.expanduser("~/.config/muzuwm")
SETTINGS_FILE = os.path.join(CONFIG_DIR, "settings.conf")
DOCK_SETTINGS_FILE = os.path.join(CONFIG_DIR, "dock_settings.ini")
DOCK_APPS_FILE = os.path.join(CONFIG_DIR, "dock_apps.ini")
PANEL_FILE = os.path.join(CONFIG_DIR, "tint2rc")
FULLSCREEN_FILE = os.path.join(CONFIG_DIR, "fullscreen_allowlist")
AUTOSTART_FILE = os.path.join(CONFIG_DIR, "auto_allowlist")
SHORTCUTS_FILE = os.path.join(CONFIG_DIR, "shortcuts.conf")
PICOM_FILE = os.path.join(CONFIG_DIR, "picom.conf")
WALLPAPER_FILE = os.path.join(CONFIG_DIR, "wallpaper.png")
BACKUP_DIR = os.path.join(CONFIG_DIR, "backups")

GTK3_SETTINGS_FILE = os.path.expanduser("~/.config/gtk-3.0/settings.ini")
GTK2_SETTINGS_FILE = os.path.expanduser("~/.gtkrc-2.0")


class ConfigManager:
    @staticmethod
    def ensure_dirs():
        os.makedirs(CONFIG_DIR, exist_ok=True)
        os.makedirs(BACKUP_DIR, exist_ok=True)
        for f in [SETTINGS_FILE, DOCK_SETTINGS_FILE, DOCK_APPS_FILE,
                  PANEL_FILE, FULLSCREEN_FILE, AUTOSTART_FILE, SHORTCUTS_FILE]:
            if not os.path.exists(f):
                try:
                    with open(f, "a"):
                        pass
                except Exception:
                    pass

    # Generic Key-Value parser for .conf and .ini files
    @staticmethod
    def get_key(filepath, key, fallback=""):
        if not os.path.exists(filepath):
            return fallback
        try:
            with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue
                    if "=" in line:
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

    # List files (lines, comments, blank lines)
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

    # Signaling MUZUwm
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
            subprocess.run(
                ["xprop", "-root", "-format", "_MUZU_RELOAD_WALLPAPER", "32c", "-set", "_MUZU_RELOAD_WALLPAPER", "1"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
        except Exception:
            pass

    # 1. Keyboard & Language Management
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
        if option and option != "super_space" and option != "none":
            cmd += ["-option", option]
        elif option == "none" or option == "super_space":
            cmd += ["-option", ""]

        try:
            subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass
        ConfigManager.reload_muzuwm_settings()

    # 2. Dock Management
    @staticmethod
    def get_dock_settings():
        pos_raw = ConfigManager.get_key(DOCK_SETTINGS_FILE, "position", "0")
        try:
            pos_int = int(pos_raw)
        except ValueError:
            pos_int = 0
        # 0=Bottom, 2=Left, 3=Right. Top is intentionally mapped to Bottom.
        if pos_int == 2:
            pos_str = "Left"
        elif pos_int == 3:
            pos_str = "Right"
        else:
            pos_str = "Bottom"

        size = ConfigManager.get_key(DOCK_SETTINGS_FILE, "size", "48")
        hide = ConfigManager.get_key(DOCK_SETTINGS_FILE, "autohide", "0")
        mag = ConfigManager.get_key(DOCK_SETTINGS_FILE, "magnification", "1")
        corner = ConfigManager.get_key(DOCK_SETTINGS_FILE, "apps_corner", "right").lower()
        if corner != "left":
            corner = "right"

        return {
            "position": pos_str,
            "size": size,
            "autohide": "1" if hide in ("1", "On", "on", "true") else "0",
            "magnification": "1" if mag in ("1", "On", "on", "true") else "0",
            "apps_corner": corner
        }

    @staticmethod
    def save_dock_settings(settings):
        pos_str = settings.get("position", "Bottom")
        if pos_str == "Left":
            pos_int = 2
        elif pos_str == "Right":
            pos_int = 3
        else:
            pos_int = 0  # Bottom

        ConfigManager.set_key(DOCK_SETTINGS_FILE, "position", str(pos_int))
        ConfigManager.set_key(DOCK_SETTINGS_FILE, "size", str(settings.get("size", "48")))
        ConfigManager.set_key(DOCK_SETTINGS_FILE, "autohide", "1" if settings.get("autohide") in ("1", True) else "0")
        ConfigManager.set_key(DOCK_SETTINGS_FILE, "magnification", "1" if settings.get("magnification") in ("1", True) else "0")
        ConfigManager.set_key(DOCK_SETTINGS_FILE, "apps_corner", settings.get("apps_corner", "right").lower())
        ConfigManager.reload_muzuwm_settings()

    @staticmethod
    def get_dock_pinned_apps():
        lines = ConfigManager.read_lines(DOCK_APPS_FILE, skip_comments=True)
        apps = []
        for l in lines:
            parts = l.split("|")
            if len(parts) >= 3:
                apps.append({"name": parts[0].strip(), "exec": parts[1].strip(), "icon": parts[2].strip()})
        if not apps:
            # Default dock apps if empty
            apps = [
                {"name": "Terminal", "exec": "wezterm", "icon": "utilities-terminal"},
                {"name": "Browser", "exec": "google-chrome", "icon": "google-chrome"},
                {"name": "Files", "exec": "thunar", "icon": "system-file-manager"},
                {"name": "Settings", "exec": "muzuwm-settings", "icon": "preferences-system"}
            ]
        return apps

    @staticmethod
    def save_dock_pinned_apps(apps):
        lines = [f"{a['name']}|{a['exec']}|{a['icon']}" for a in apps]
        ConfigManager.write_lines(DOCK_APPS_FILE, lines)
        ConfigManager.reload_muzuwm_settings()

    # 3. Titlebar Settings
    @staticmethod
    def get_titlebar_settings():
        height = ConfigManager.get_key(SETTINGS_FILE, "titlebar_height", "24")
        font = ConfigManager.get_key(SETTINGS_FILE, "titlebar_font", "sans-serif")
        font_size = ConfigManager.get_key(SETTINGS_FILE, "titlebar_font_size", "11")
        align = ConfigManager.get_key(SETTINGS_FILE, "titlebar_align", "left")
        double_click = ConfigManager.get_key(SETTINGS_FILE, "titlebar_double_click", "maximize")
        dimming = ConfigManager.get_key(SETTINGS_FILE, "inactivity_dimming", "1")
        timeout = ConfigManager.get_key(SETTINGS_FILE, "inactivity_timeout", "30")
        opacity = ConfigManager.get_key(SETTINGS_FILE, "inactive_opacity", "85")

        return {
            "height": height,
            "font": font,
            "font_size": font_size,
            "align": align,
            "double_click": double_click,
            "inactivity_dimming": "1" if dimming in ("1", "true", "True") else "0",
            "inactivity_timeout": timeout,
            "inactive_opacity": opacity
        }

    @staticmethod
    def save_titlebar_settings(settings):
        for k, v in settings.items():
            ConfigManager.set_key(SETTINGS_FILE, f"titlebar_{k}" if not k.startswith("inactiv") else k, str(v))
        ConfigManager.reload_muzuwm_settings()

    # 4. Fullscreen Allowlist
    @staticmethod
    def get_fullscreen_allowlist():
        return ConfigManager.read_lines(FULLSCREEN_FILE, skip_comments=True)

    @staticmethod
    def save_fullscreen_allowlist(items):
        ConfigManager.write_lines(FULLSCREEN_FILE, items)
        ConfigManager.reload_muzuwm_settings()

    @staticmethod
    def add_fullscreen_allow(item):
        items = ConfigManager.get_fullscreen_allowlist()
        item = item.strip()
        if item and item not in items:
            items.append(item)
            ConfigManager.save_fullscreen_allowlist(items)

    @staticmethod
    def remove_fullscreen_allow(item):
        items = ConfigManager.get_fullscreen_allowlist()
        items = [i for i in items if i != item]
        ConfigManager.save_fullscreen_allowlist(items)

    # 5. Autostart Management
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
            if e["enabled"]:
                lines.append(cmd)
            else:
                lines.append(f"# {cmd}")
        ConfigManager.write_lines(AUTOSTART_FILE, lines)

    # 6. Top Panel Settings
    @staticmethod
    def get_panel_settings():
        pos = ConfigManager.get_key(PANEL_FILE, "panel_position", "top center horizontal")
        pos_str = "Bottom" if "bottom" in pos.lower() else "Top"
        size_raw = ConfigManager.get_key(PANEL_FILE, "panel_size", "100% 28")
        height = "28"
        if len(size_raw.split()) >= 2:
            height = size_raw.split()[1]

        clock_fmt = ConfigManager.get_key(PANEL_FILE, "time1_format", "%H:%M:%S")
        date_fmt = ConfigManager.get_key(PANEL_FILE, "time2_format", "%a %d %b %Y")
        return {
            "position": pos_str,
            "height": height,
            "time1_format": clock_fmt,
            "time2_format": date_fmt
        }

    @staticmethod
    def save_panel_settings(settings):
        pos_str = settings.get("position", "Top")
        pos_val = "bottom center horizontal" if pos_str == "Bottom" else "top center horizontal"
        height = settings.get("height", "28")
        ConfigManager.set_key(PANEL_FILE, "panel_position", pos_val)
        ConfigManager.set_key(PANEL_FILE, "panel_size", f"100% {height}")
        ConfigManager.set_key(PANEL_FILE, "time1_format", settings.get("time1_format", "%H:%M:%S"))
        ConfigManager.set_key(PANEL_FILE, "time2_format", settings.get("time2_format", "%a %d %b %Y"))

    # 7. Keyboard Shortcuts
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
            shortcuts = [
                {"combo": "Super+Left", "action": "tile_left"},
                {"combo": "Super+Right", "action": "tile_right"},
                {"combo": "Super+Up", "action": "fullscreen"},
                {"combo": "Super+Down", "action": "float"},
                {"combo": "Super+Return", "action": "exec wezterm"},
                {"combo": "Super+b", "action": "exec google-chrome"},
                {"combo": "Super+e", "action": "exec thunar"},
                {"combo": "Super+q", "action": "close_window"}
            ]
        return shortcuts

    @staticmethod
    def save_shortcuts(shortcuts):
        lines = [f"{s['combo']} = {s['action']}" for s in shortcuts]
        ConfigManager.write_lines(SHORTCUTS_FILE, lines)

    # 8. Wallpaper & GTK Theme
    @staticmethod
    def set_wallpaper_image(image_path):
        if not os.path.exists(image_path):
            return False
        ConfigManager.ensure_dirs()
        shutil.copyfile(image_path, WALLPAPER_FILE)
        ConfigManager.reload_muzuwm_wallpaper()
        return True

    @staticmethod
    def get_installed_gtk_themes():
        themes = set()
        for d in ["/usr/share/themes", os.path.expanduser("~/.themes")]:
            if os.path.isdir(d):
                for name in os.listdir(d):
                    if os.path.isdir(os.path.join(d, name, "gtk-3.0")):
                        themes.add(name)
        return sorted(list(themes)) if themes else ["Adwaita", "Adwaita-dark", "Arc", "Nordic"]

    @staticmethod
    def get_installed_icon_themes():
        icons = set()
        for d in ["/usr/share/icons", os.path.expanduser("~/.icons")]:
            if os.path.isdir(d):
                for name in os.listdir(d):
                    if os.path.isfile(os.path.join(d, name, "index.theme")):
                        icons.add(name)
        return sorted(list(icons)) if icons else ["Adwaita", "Papirus", "hicolor"]

    # 9. Backup & Restore
    @staticmethod
    def backup_configs():
        ConfigManager.ensure_dirs()
        ts = time.strftime("%Y%m%d_%H%M%S")
        archive_path = os.path.join(BACKUP_DIR, f"muzuwm_backup_{ts}.tar.gz")
        files_to_backup = [SETTINGS_FILE, DOCK_SETTINGS_FILE, DOCK_APPS_FILE,
                           PANEL_FILE, FULLSCREEN_FILE, AUTOSTART_FILE, SHORTCUTS_FILE]
        with tarfile.open(archive_path, "w:gz") as tar:
            for f in files_to_backup:
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
        # Reset default shortcuts
        ConfigManager.write_lines(SHORTCUTS_FILE, [
            "Super+Left = tile_left",
            "Super+Right = tile_right",
            "Super+Up = fullscreen",
            "Super+Down = float",
            "Super+Return = exec wezterm",
            "Super+b = exec google-chrome",
            "Super+e = exec thunar",
            "Super+q = close_window"
        ])
        # Reset default dock settings
        ConfigManager.write_lines(DOCK_SETTINGS_FILE, [
            "autohide=0",
            "magnification=1",
            "position=0",
            "size=48",
            "apps_corner=right"
        ])
        # Reset default dock apps
        ConfigManager.write_lines(DOCK_APPS_FILE, [
            "Terminal|wezterm|utilities-terminal",
            "Browser|google-chrome|google-chrome",
            "Files|thunar|system-file-manager",
            "Settings|muzuwm-settings|preferences-system"
        ])
        # Reset default panel
        ConfigManager.write_lines(PANEL_FILE, [
            "panel_items = T:C:P",
            "panel_size = 100% 28",
            "panel_position = top center horizontal",
            "taskbar_mode = multi_desktop",
            "time1_format = %H:%M:%S",
            "time2_format = %a %d %b %Y",
            "power_options = suspend,reboot,poweroff,lock,logout"
        ])
        # Reset default fullscreen allowlist
        ConfigManager.write_lines(FULLSCREEN_FILE, [
            "# Applications that should open maximized/fullscreen automatically",
            "google-chrome",
            "firefox",
            "chromium",
            "vlc",
            "mpv"
        ])
        # Reset default autostart
        ConfigManager.write_lines(AUTOSTART_FILE, [
            "# Commands that launch when MUZUwm starts",
            "picom -b",
            "# dunst &",
            "# nm-applet &",
            "# volumeicon &"
        ])
        # Reset default settings
        ConfigManager.write_lines(SETTINGS_FILE, [
            "keyboard_layouts=us,th",
            "layout_switch_option=super_space",
            "master_count=1",
            "titlebar_height=24",
            "titlebar_font=sans-serif",
            "titlebar_font_size=11",
            "titlebar_align=left",
            "titlebar_double_click=maximize",
            "inactivity_dimming=1",
            "inactivity_timeout=30",
            "inactive_opacity=85"
        ])
        ConfigManager.reload_muzuwm_settings()
        return True


def run_cli_menu():
    ConfigManager.ensure_dirs()
    while True:
        print("\n" + "=" * 55)
        print("          MUZUwm Control Centre (Settings)       ")
        print("=" * 55)
        print("1.  🌐 แป้นพิมพ์และเปลี่ยนภาษา (Keyboard & Language)")
        print("2.  ⚓ ตั้งค่า Dock & แอปที่ปักหมุด (Dock & Pinned Apps)")
        print("3.  🪟 กรอบหน้าต่างและ Title bar (Titlebar & Controls)")
        print("4.  ⛶ แอปเปิดเต็มจออัตโนมัติ (Fullscreen Allowlist)")
        print("5.  🚀 คำสั่งเริ่มทำงานอัตโนมัติ (Autostart Commands)")
        print("6.  📊 พาเนลด้านบน (Top Panel / Tint2rc)")
        print("7.  ⌨️  คีย์ลัดระบบ (Keyboard Shortcuts)")
        print("8.  🎨 ภาพพื้นหลังและธีม (Wallpaper & Theme)")
        print("9.  📐 การจัดเรียงหน้าต่าง (Window Layout & Tiling)")
        print("10. 💾 สำรองข้อมูลและรีเซ็ตค่าเดิม (Backup & Reset)")
        print("11. 🔄 รีโหลดการตั้งค่าทันที (Reload Settings)")
        print("0.  ❌ ออกจากโปรแกรม (Exit)")
        print("-" * 55)

        try:
            choice = input("เลือกเมนู (0-11): ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nBye!")
            break

        if choice == "0":
            break
        elif choice == "1":
            cli_keyboard_menu()
        elif choice == "2":
            cli_dock_menu()
        elif choice == "3":
            cli_titlebar_menu()
        elif choice == "4":
            cli_fullscreen_menu()
        elif choice == "5":
            cli_autostart_menu()
        elif choice == "6":
            cli_panel_menu()
        elif choice == "7":
            cli_shortcuts_menu()
        elif choice == "8":
            cli_wallpaper_menu()
        elif choice == "9":
            cli_layout_menu()
        elif choice == "10":
            cli_backup_menu()
        elif choice == "11":
            ConfigManager.reload_muzuwm_settings()
            ConfigManager.reload_muzuwm_wallpaper()
            print("✓ ส่งคำสั่งรีโหลดการตั้งค่าไปยัง MUZUwm แล้ว!")
        else:
            print("ตัวเลือกไม่ถูกต้อง กรุณาลองใหม่")


def cli_keyboard_menu():
    curr_layouts, curr_opt = ConfigManager.get_keyboard_settings()
    print("\n--- 🌐 ตั้งค่าแป้นพิมพ์และเปลี่ยนภาษา (Keyboard & Language) ---")
    print(f"ผังแป้นพิมพ์ปัจจุบัน: {curr_layouts}")
    print(f"ปุ่มสลับภาษา: {curr_opt}")
    print("\nตัวเลือกผังแป้นพิมพ์ยอดนิยม:")
    print("1) us,th        (อังกฤษ US + ไทยเกษมณี / มาตรฐาน)")
    print("2) us,th(pat)   (อังกฤษ US + ไทยปัตตะโชติ)")
    print("3) us           (อังกฤษ US อย่างเดียว)")
    print("4) us,jp        (อังกฤษ US + ญี่ปุ่น)")
    print("5) กำหนดเอง (Custom)")
    val = input("เลือกรูปแบบผังแป้นพิมพ์ [กด Enter เพื่อข้าม]: ").strip()
    new_layouts = curr_layouts
    if val == "1":
        new_layouts = "us,th"
    elif val == "2":
        new_layouts = "us,th(pat)"
    elif val == "3":
        new_layouts = "us"
    elif val == "4":
        new_layouts = "us,jp"
    elif val == "5":
        c = input("พิมพ์ผังแป้นพิมพ์คั่นด้วยลูกน้ำ (เช่น us,th,jp): ").strip()
        if c:
            new_layouts = c

    print("\nปุ่มลัดสำหรับสลับภาษา:")
    print("1) Super + Space (ปุ่มลัดหลักของ MUZUwm)")
    print("2) Alt + Shift   (grp:alt_shift_toggle)")
    print("3) Caps Lock     (grp:caps_toggle)")
    print("4) Grave Tilde ~ (grp:grave_toggle)")
    opt_val = input("เลือกปุ่มลัดสลับภาษา [กด Enter เพื่อข้าม]: ").strip()
    new_opt = curr_opt
    if opt_val == "1":
        new_opt = "super_space"
    elif opt_val == "2":
        new_opt = "grp:alt_shift_toggle"
    elif opt_val == "3":
        new_opt = "grp:caps_toggle"
    elif opt_val == "4":
        new_opt = "grp:grave_toggle"

    ConfigManager.apply_keyboard_settings(new_layouts, new_opt)
    print(f"✓ บันทึกและนำไปใช้เรียบร้อย: Layouts = {new_layouts}, Option = {new_opt}")


def cli_dock_menu():
    dock = ConfigManager.get_dock_settings()
    print("\n--- ⚓ ตั้งค่า Dock ---")
    print(f"1) ตำแหน่ง (Position): {dock['position']} (เลือก: Bottom / Left / Right) [ห้าม Top]")
    print(f"2) ขนาดไอคอน (Size): {dock['size']} px (แนะนำ: 36, 48, 64, 80)")
    print(f"3) ซ่อนอัตโนมัติ (Auto-hide): {'เปิด (On)' if dock['autohide'] == '1' else 'ปิด (Off)'}")
    print(f"4) เอฟเฟกต์ซูมคลื่น (Magnification): {'เปิด (On)' if dock['magnification'] == '1' else 'ปิด (Off)'}")
    print(f"5) มุมของปุ่ม Apps: {dock['apps_corner'].upper()}")
    print("6) จัดการแอปที่ปักหมุดบน Dock (Pinned Apps)")
    print("7) ล้างถังขยะ (Empty Trash)")
    print("0) ย้อนกลับ")

    choice = input("เลือกสิ่งที่ต้องการปรับ: ").strip()
    if choice == "1":
        p = input("เลือกตำแหน่ง (Bottom / Left / Right): ").strip().capitalize()
        if p in ["Bottom", "Left", "Right"]:
            dock["position"] = p
            ConfigManager.save_dock_settings(dock)
            print("✓ บันทึกตำแหน่งแล้ว")
    elif choice == "2":
        s = input("กรอกขนาดไอคอน (24-128 เช่น 48): ").strip()
        if s.isdigit():
            dock["size"] = s
            ConfigManager.save_dock_settings(dock)
            print("✓ บันทึกขนาดแล้ว")
    elif choice == "3":
        dock["autohide"] = "0" if dock["autohide"] == "1" else "1"
        ConfigManager.save_dock_settings(dock)
        print(f"✓ Auto-hide ตอนนี้: {'เปิด' if dock['autohide'] == '1' else 'ปิด'}")
    elif choice == "4":
        dock["magnification"] = "0" if dock["magnification"] == "1" else "1"
        ConfigManager.save_dock_settings(dock)
        print(f"✓ Magnification ตอนนี้: {'เปิด' if dock['magnification'] == '1' else 'ปิด'}")
    elif choice == "5":
        c = "left" if dock["apps_corner"] == "right" else "right"
        dock["apps_corner"] = c
        ConfigManager.save_dock_settings(dock)
        print(f"✓ Apps Corner ตอนนี้: {c.upper()}")
    elif choice == "6":
        cli_dock_apps_menu()
    elif choice == "7":
        trash_dir = os.path.expanduser("~/.local/share/Trash/files")
        if os.path.exists(trash_dir):
            shutil.rmtree(trash_dir)
            os.makedirs(trash_dir, exist_ok=True)
            print("✓ ล้างข้อมูลในถังขยะเรียบร้อย!")


def cli_dock_apps_menu():
    apps = ConfigManager.get_dock_pinned_apps()
    print("\nแอปที่ปักหมุดบน Dock ปัจจุบัน:")
    for idx, a in enumerate(apps):
        print(f"  [{idx + 1}] {a['name']} ({a['exec']})")
    print("\n1) เพิ่มแอปใหม่ | 2) ลบแอป | 3) คืนค่าเริ่มต้น | 0) ย้อนกลับ")
    c = input("เลือก: ").strip()
    if c == "1":
        name = input("ชื่อแอป (เช่น Firefox): ").strip()
        cmd = input("คำสั่งเปิด (เช่น firefox): ").strip()
        icon = input("ชื่อไอคอน (เช่น firefox): ").strip()
        if name and cmd:
            apps.append({"name": name, "exec": cmd, "icon": icon or name.lower()})
            ConfigManager.save_dock_pinned_apps(apps)
            print(f"✓ เพิ่มแอป {name} แล้ว")
    elif c == "2":
        idx = input("ใส่หมายเลขแอปที่ต้องการลบ: ").strip()
        if idx.isdigit() and 1 <= int(idx) <= len(apps):
            removed = apps.pop(int(idx) - 1)
            ConfigManager.save_dock_pinned_apps(apps)
            print(f"✓ ลบ {removed['name']} แล้ว")
    elif c == "3":
        apps = [
            {"name": "Terminal", "exec": "wezterm", "icon": "utilities-terminal"},
            {"name": "Browser", "exec": "google-chrome", "icon": "google-chrome"},
            {"name": "Files", "exec": "thunar", "icon": "system-file-manager"},
            {"name": "Settings", "exec": "muzuwm-settings", "icon": "preferences-system"}
        ]
        ConfigManager.save_dock_pinned_apps(apps)
        print("✓ คืนค่าแอปเริ่มต้นแล้ว")


def cli_titlebar_menu():
    tb = ConfigManager.get_titlebar_settings()
    print("\n--- 🪟 ตั้งค่ากรอบหน้าต่างและ Title bar ---")
    print(f"1) ความสูง Titlebar: {tb['height']} px (24, 28, 32)")
    print(f"2) ฟอนต์และขนาด: {tb['font']} {tb['font_size']} pt")
    print(f"3) ตำแหน่งข้อความชื่อหน้าต่าง: {tb['align'].upper()} (LEFT / CENTER)")
    print(f"4) การดับเบิลคลิกบน Titlebar: {tb['double_click'].upper()} (MAXIMIZE / TILE / FLOAT)")
    print(f"5) ระบบหรี่แสงเมื่อหน้าต่างไม่แอคทีฟ (Inactivity Dimming): {'เปิด' if tb['inactivity_dimming'] == '1' else 'ปิด'}")
    print(f"6) เวลาตรวจจับความไม่แอคทีฟ: {tb['inactivity_timeout']} วินาที")
    print(f"7) ความโปร่งแสงหน้าต่างที่ไม่แอคทีฟ: {tb['inactive_opacity']}%")
    print("0) ย้อนกลับ")

    c = input("เลือกข้อที่ต้องการแก้ไข: ").strip()
    if c == "1":
        h = input("กรอกความสูง (24, 28, 32): ").strip()
        if h in ["24", "28", "32"]:
            tb["height"] = h
            ConfigManager.save_titlebar_settings(tb)
    elif c == "2":
        f = input("กรอกชื่อฟอนต์ (เช่น sans-serif): ").strip()
        s = input("กรอกขนาดฟอนต์ (9-14): ").strip()
        if f:
            tb["font"] = f
        if s.isdigit():
            tb["font_size"] = s
        ConfigManager.save_titlebar_settings(tb)
    elif c == "3":
        a = input("เลือกตำแหน่งชื่อ (left / center): ").strip().lower()
        if a in ["left", "center"]:
            tb["align"] = a
            ConfigManager.save_titlebar_settings(tb)
    elif c == "4":
        d = input("ดับเบิลคลิกทำอะไร (maximize / tile / float): ").strip().lower()
        if d in ["maximize", "tile", "float"]:
            tb["double_click"] = d
            ConfigManager.save_titlebar_settings(tb)
    elif c == "5":
        tb["inactivity_dimming"] = "0" if tb["inactivity_dimming"] == "1" else "1"
        ConfigManager.save_titlebar_settings(tb)
    elif c == "6":
        t = input("เวลา Timeout เป็นวินาที (เช่น 15, 30, 60): ").strip()
        if t.isdigit():
            tb["inactivity_timeout"] = t
            ConfigManager.save_titlebar_settings(tb)
    elif c == "7":
        op = input("ความโปร่งแสง % (เช่น 70, 80, 85, 90, 100): ").strip()
        if op.isdigit():
            tb["inactive_opacity"] = op
            ConfigManager.save_titlebar_settings(tb)


def cli_fullscreen_menu():
    items = ConfigManager.get_fullscreen_allowlist()
    print("\n--- ⛶ แอปที่เปิดแบบเต็มจออัตโนมัติ (Fullscreen Allowlist) ---")
    print("แอปในรายการนี้จะเปิดขยายเต็มจอทันทีโดยไม่ต้องกดขยาย:")
    for idx, it in enumerate(items):
        print(f"  [{idx + 1}] {it}")
    print("\n1) เพิ่มแอปจากรายการแนะนำ | 2) เพิ่มแอปด้วยตนเอง | 3) ลบแอป | 0) ย้อนกลับ")
    c = input("เลือก: ").strip()
    if c == "1":
        print("รายการแนะนำ: 1) google-chrome  2) firefox  3) vlc  4) mpv  5) code  6) libreoffice")
        p = input("เลือกหมายเลข: ").strip()
        presets = {"1": "google-chrome", "2": "firefox", "3": "vlc", "4": "mpv", "5": "code", "6": "libreoffice"}
        if p in presets:
            ConfigManager.add_fullscreen_allow(presets[p])
            print(f"✓ เพิ่ม {presets[p]} แล้ว")
    elif c == "2":
        app = input("ใส่ WM_CLASS หรือชื่อแอป: ").strip()
        if app:
            ConfigManager.add_fullscreen_allow(app)
            print(f"✓ เพิ่ม {app} แล้ว")
    elif c == "3":
        idx = input("ใส่หมายเลขแอปที่ต้องการลบ: ").strip()
        if idx.isdigit() and 1 <= int(idx) <= len(items):
            ConfigManager.remove_fullscreen_allow(items[int(idx) - 1])
            print("✓ ลบแอปออกจาก Allowlist เรียบร้อย")


def cli_autostart_menu():
    entries = ConfigManager.get_autostart_entries()
    print("\n--- 🚀 คำสั่งเริ่มทำงานอัตโนมัติ (Autostart) ---")
    for idx, e in enumerate(entries):
        status = "✓ [เปิด]" if e["enabled"] else "✗ [ปิด]"
        print(f"  [{idx + 1}] {status} {e['command']}")
    print("\n1) สลับ เปิด/ปิด คำสั่ง | 2) เพิ่มคำสั่งแนะนำ | 3) เพิ่มคำสั่งเอง | 4) ลบคำสั่ง | 0) ย้อนกลับ")
    c = input("เลือก: ").strip()
    if c == "1":
        idx = input("ใส่หมายเลขคำสั่ง: ").strip()
        if idx.isdigit() and 1 <= int(idx) <= len(entries):
            entries[int(idx) - 1]["enabled"] = not entries[int(idx) - 1]["enabled"]
            ConfigManager.save_autostart_entries(entries)
            print("✓ อัปเดตสถานะแล้ว")
    elif c == "2":
        print("คำสั่งแนะนำ:")
        print("1) picom -b         (เปิด Picom Compositor)")
        print("2) dunst &          (ระบบแจ้งเตือน Notification)")
        print("3) nm-applet &      (ไอคอนเน็ตเวิร์ก Network Manager)")
        print("4) volumeicon &     (ไอคอนปรับเสียง)")
        print("5) redshift &       (ตัวตัดแสงสีฟ้า)")
        p = input("เลือก: ").strip()
        mapping = {
            "1": "picom -b",
            "2": "dunst &",
            "3": "nm-applet &",
            "4": "volumeicon &",
            "5": "redshift &"
        }
        if p in mapping:
            entries.append({"command": mapping[p], "enabled": True})
            ConfigManager.save_autostart_entries(entries)
            print("✓ เพิ่มคำสั่งเรียบร้อย")
    elif c == "3":
        cmd = input("ใส่คำสั่งเชลล์ที่ต้องการรันเมื่อเริ่มระบบ: ").strip()
        if cmd:
            entries.append({"command": cmd, "enabled": True})
            ConfigManager.save_autostart_entries(entries)
            print("✓ เพิ่มคำสั่งเรียบร้อย")
    elif c == "4":
        idx = input("ใส่หมายเลขคำสั่งที่จะลบ: ").strip()
        if idx.isdigit() and 1 <= int(idx) <= len(entries):
            entries.pop(int(idx) - 1)
            ConfigManager.save_autostart_entries(entries)
            print("✓ ลบคำสั่งเรียบร้อย")


def cli_panel_menu():
    panel = ConfigManager.get_panel_settings()
    print("\n--- 📊 พาเนลด้านบน (Top Panel / Tint2rc) ---")
    print(f"1) ตำแหน่ง: {panel['position']} (Top / Bottom)")
    print(f"2) ความสูง: {panel['height']} px (24, 28, 32, 36)")
    print(f"3) รูปแบบเวลา (Time Format): {panel['time1_format']}")
    print(f"4) รูปแบบวันที่ (Date Format): {panel['time2_format']}")
    print("0) ย้อนกลับ")

    c = input("เลือก: ").strip()
    if c == "1":
        p = input("เลือกตำแหน่ง (Top / Bottom): ").strip().capitalize()
        if p in ["Top", "Bottom"]:
            panel["position"] = p
            ConfigManager.save_panel_settings(panel)
            print("✓ บันทึกตำแหน่งพาเนลแล้ว")
    elif c == "2":
        h = input("กรอกความสูงพาเนล (เช่น 28): ").strip()
        if h.isdigit():
            panel["height"] = h
            ConfigManager.save_panel_settings(panel)
            print("✓ บันทึกความสูงแล้ว")
    elif c == "3":
        print("ตัวเลือกรูปแบบเวลา:")
        print("1) %H:%M:%S (24 ชั่วโมง มีวินาที เช่น 14:30:45)")
        print("2) %I:%M:%S %p (12 ชั่วโมง มี AM/PM เช่น 02:30:45 PM)")
        print("3) %H:%M (สั้น ไม่มีวินาที เช่น 14:30)")
        p = input("เลือก (1-3 หรือพิมพ์เอง): ").strip()
        if p == "1":
            panel["time1_format"] = "%H:%M:%S"
        elif p == "2":
            panel["time1_format"] = "%I:%M:%S %p"
        elif p == "3":
            panel["time1_format"] = "%H:%M"
        elif p:
            panel["time1_format"] = p
        ConfigManager.save_panel_settings(panel)
        print("✓ อัปเดตรูปแบบเวลาแล้ว")


def cli_shortcuts_menu():
    sc = ConfigManager.get_shortcuts()
    print("\n--- ⌨️ คีย์ลัดระบบ (Keyboard Shortcuts) ---")
    for idx, s in enumerate(sc):
        print(f"  [{idx + 1}] {s['combo']:<16} = {s['action']}")
    print("\n1) เปลี่ยนแอป Terminal (Super+Return)")
    print("2) เปลี่ยนแอป Browser (Super+b)")
    print("3) เปลี่ยนแอป File Manager (Super+e)")
    print("4) เพิ่มคีย์ลัดใหม่")
    print("5) รีเซ็ตเป็นค่าเริ่มต้น")
    print("0) ย้อนกลับ")
    c = input("เลือก: ").strip()
    if c == "1":
        cmd = input("ใส่คำสั่งเปิด Terminal (เช่น wezterm, alacritty, xterm): ").strip()
        if cmd:
            for s in sc:
                if s["combo"] == "Super+Return":
                    s["action"] = f"exec {cmd}"
            ConfigManager.save_shortcuts(sc)
            print("✓ อัปเดตคีย์ลัด Terminal แล้ว")
    elif c == "2":
        cmd = input("ใส่คำสั่งเปิด Browser (เช่น google-chrome, firefox): ").strip()
        if cmd:
            for s in sc:
                if s["combo"] == "Super+b":
                    s["action"] = f"exec {cmd}"
            ConfigManager.save_shortcuts(sc)
            print("✓ อัปเดตคีย์ลัด Browser แล้ว")
    elif c == "3":
        cmd = input("ใส่คำสั่งเปิด File Manager (เช่น thunar, pcmanfm): ").strip()
        if cmd:
            for s in sc:
                if s["combo"] == "Super+e":
                    s["action"] = f"exec {cmd}"
            ConfigManager.save_shortcuts(sc)
            print("✓ อัปเดตคีย์ลัด File Manager แล้ว")
    elif c == "4":
        combo = input("ปุ่มลัด (เช่น Super+t): ").strip()
        act = input("แอคชัน (เช่น exec gedit หรือ tile_left): ").strip()
        if combo and act:
            sc.append({"combo": combo, "action": act})
            ConfigManager.save_shortcuts(sc)
            print("✓ เพิ่มคีย์ลัดแล้ว")
    elif c == "5":
        sc = [
            {"combo": "Super+Left", "action": "tile_left"},
            {"combo": "Super+Right", "action": "tile_right"},
            {"combo": "Super+Up", "action": "fullscreen"},
            {"combo": "Super+Down", "action": "float"},
            {"combo": "Super+Return", "action": "exec wezterm"},
            {"combo": "Super+b", "action": "exec google-chrome"},
            {"combo": "Super+e", "action": "exec thunar"},
            {"combo": "Super+q", "action": "close_window"}
        ]
        ConfigManager.save_shortcuts(sc)
        print("✓ รีเซ็ตคีย์ลัดเรียบร้อย")


def cli_wallpaper_menu():
    print("\n--- 🎨 ภาพพื้นหลังและธีม (Wallpaper & Theme) ---")
    current_wall = WALLPAPER_FILE if os.path.exists(WALLPAPER_FILE) else "(ยังไม่ได้ตั้งค่า)"
    print(f"ไฟล์ภาพพื้นหลังปัจจุบัน: {current_wall}")
    path = input("ใส่ที่อยู่ไฟล์ภาพ (เช่น /home/user/Pictures/wall.jpg) [กด Enter เพื่อข้าม]: ").strip()
    if path:
        path = os.path.expanduser(path)
        if os.path.isfile(path):
            ConfigManager.set_wallpaper_image(path)
            print("✓ ตั้งค่าและโหลดภาพพื้นหลังใหม่ทันทีเรียบร้อย!")
        else:
            print("✗ ไม่พบไฟล์ภาพตามที่ระบุ")


def cli_layout_menu():
    print("\n--- 📐 การจัดเรียงหน้าต่าง (Window Layout & Tiling) ---")
    masters = ConfigManager.get_key(SETTINGS_FILE, "master_count", "1")
    print(f"จำนวนหน้าต่างหลักในฝั่ง Master: {masters}")
    val = input("ระบุจำนวน Master windows (1 - 4): ").strip()
    if val in ["1", "2", "3", "4"]:
        ConfigManager.set_key(SETTINGS_FILE, "master_count", val)
        ConfigManager.reload_muzuwm_settings()
        print(f"✓ บันทึก Master count = {val} แล้ว")


def cli_backup_menu():
    print("\n--- 💾 สำรองข้อมูลและรีเซ็ตค่าเดิม (Backup & Reset) ---")
    print("1) สร้างไฟล์สำรองการตั้งค่าทั้งหมด (Create Backup Archive)")
    print("2) คืนค่าจากไฟล์สำรอง (Restore from Backup)")
    print("3) รีเซ็ตการตั้งค่าทั้งหมดเป็นค่าเริ่มต้นโรงงาน (Factory Reset)")
    print("0) ย้อนกลับ")
    c = input("เลือก: ").strip()
    if c == "1":
        arc = ConfigManager.backup_configs()
        print(f"✓ สำรองข้อมูลลงไฟล์แล้ว:\n  {arc}")
    elif c == "2":
        backups = ConfigManager.list_backups()
        if not backups:
            print("ไม่พบไฟล์สำรองในระบบ")
            return
        print("รายการไฟล์สำรองที่มี:")
        for idx, b in enumerate(backups[:10]):
            print(f"  [{idx + 1}] {os.path.basename(b)}")
        idx = input("เลือกหมายเลขไฟล์สำรอง: ").strip()
        if idx.isdigit() and 1 <= int(idx) <= len(backups):
            ConfigManager.restore_backup(backups[int(idx) - 1])
            print("✓ คืนค่าการตั้งค่าจากไฟล์สำรองเรียบร้อย!")
    elif c == "3":
        confirm = input("คุณแน่ใจหรือไม่ว่าต้องการรีเซ็ตค่าทั้งหมด? (พิมพ์ 'yes' เพื่อยืนยัน): ").strip()
        if confirm.lower() == "yes":
            ConfigManager.reset_factory_defaults()
            print("✓ รีเซ็ตการตั้งค่าทั้งหมดกลับสู่ค่าเริ่มต้นโรงงานเรียบร้อยแล้ว!")


# ==============================================================================
# GTK 3 Graphical User Interface (Control Centre Window)
# ==============================================================================

def run_gtk_gui():
    import gi
    gi.require_version("Gtk", "3.0")
    from gi.repository import Gtk, Gdk, GLib

    class MuzuSettingsWindow(Gtk.Window):
        def __init__(self):
            super().__init__(title="MUZUwm Control Centre")
            self.set_default_size(820, 580)
            self.set_position(Gtk.WindowPosition.CENTER)

            ConfigManager.ensure_dirs()

            # Main container with Stack and StackSidebar
            main_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
            self.add(main_box)

            sidebar = Gtk.StackSidebar()
            sidebar.set_size_request(220, -1)
            main_box.pack_start(sidebar, False, False, 0)

            separator = Gtk.Separator(orientation=Gtk.Orientation.VERTICAL)
            main_box.pack_start(separator, False, False, 0)

            self.stack = Gtk.Stack()
            self.stack.set_transition_type(Gtk.StackTransitionType.SLIDE_LEFT_RIGHT)
            self.stack.set_transition_duration(150)
            sidebar.set_stack(self.stack)
            main_box.pack_start(self.stack, True, True, 0)

            # Build all pages
            self.build_keyboard_page()
            self.build_dock_page()
            self.build_titlebar_page()
            self.build_fullscreen_page()
            self.build_autostart_page()
            self.build_panel_page()
            self.build_shortcuts_page()
            self.build_appearance_page()
            self.build_system_page()

        # Helper to create a standard page layout
        def create_page_scroller(self, title, subtitle):
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
            box.set_margin_start(24)
            box.set_margin_end(24)
            box.set_margin_top(20)
            box.set_margin_bottom(20)

            header_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
            title_lbl = Gtk.Label(xalign=0)
            title_lbl.set_markup(f"<span size='x-large' weight='bold'>{title}</span>")
            subtitle_lbl = Gtk.Label(xalign=0)
            subtitle_lbl.set_markup(f"<span color='#888888'>{subtitle}</span>")
            header_box.pack_start(title_lbl, False, False, 0)
            header_box.pack_start(subtitle_lbl, False, False, 0)
            box.pack_start(header_box, False, False, 0)

            scroller = Gtk.ScrolledWindow()
            scroller.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
            scroller.add(box)
            return scroller, box

        # 1. Keyboard & Language Page
        def build_keyboard_page(self):
            scroller, box = self.create_page_scroller(
                "🌐 แป้นพิมพ์และภาษา (Keyboard & Language)",
                "ตั้งค่าภาษาสำหรับพิมพ์และปุ่มลัดสลับภาษา (XKB Layouts)"
            )

            curr_layouts, curr_opt = ConfigManager.get_keyboard_settings()

            form = Gtk.Grid(row_spacing=12, column_spacing=16)
            box.pack_start(form, False, False, 0)

            # Presets combo
            lbl1 = Gtk.Label(label="ผังแป้นพิมพ์สำเร็จรูป:", xalign=0)
            self.kbd_preset_combo = Gtk.ComboBoxText()
            presets = [
                ("us,th", "English (US) + ภาษาไทย เกษมณี (us,th) [แนะนำ]"),
                ("us,th(pat)", "English (US) + ภาษาไทย ปัตตะโชติ (us,th(pat))"),
                ("us", "English (US) อย่างเดียว"),
                ("us,jp", "English (US) + ภาษาญี่ปุ่น (us,jp)"),
                ("custom", "กำหนดเอง...")
            ]
            for code, name in presets:
                self.kbd_preset_combo.append(code, name)

            if curr_layouts in ["us,th", "us,th(pat)", "us", "us,jp"]:
                self.kbd_preset_combo.set_active_id(curr_layouts)
            else:
                self.kbd_preset_combo.set_active_id("custom")

            form.attach(lbl1, 0, 0, 1, 1)
            form.attach(self.kbd_preset_combo, 1, 0, 1, 1)

            # Custom Layout entry
            lbl2 = Gtk.Label(label="รหัส Layout (คั่นด้วยลูกน้ำ):", xalign=0)
            self.kbd_entry = Gtk.Entry()
            self.kbd_entry.set_text(curr_layouts)
            form.attach(lbl2, 0, 1, 1, 1)
            form.attach(self.kbd_entry, 1, 1, 1, 1)

            def on_preset_changed(combo):
                cid = combo.get_active_id()
                if cid and cid != "custom":
                    self.kbd_entry.set_text(cid)
            self.kbd_preset_combo.connect("changed", on_preset_changed)

            # Switch option combo
            lbl3 = Gtk.Label(label="ปุ่มลัดสลับภาษา:", xalign=0)
            self.kbd_opt_combo = Gtk.ComboBoxText()
            options = [
                ("super_space", "Super + Space (ปุ่มมาตรฐานของ MUZUwm)"),
                ("grp:alt_shift_toggle", "Alt + Shift (ยอดนิยมทั่วไป)"),
                ("grp:caps_toggle", "Caps Lock"),
                ("grp:grave_toggle", "Grave Accent / Tilde (~ / `)"),
                ("grp:win_space_toggle", "Win + Space (XKB Level)")
            ]
            for code, name in options:
                self.kbd_opt_combo.append(code, name)
            self.kbd_opt_combo.set_active_id(curr_opt if curr_opt in dict(options) else "super_space")
            form.attach(lbl3, 0, 2, 1, 1)
            form.attach(self.kbd_opt_combo, 1, 2, 1, 1)

            # Test text box
            lbl_test = Gtk.Label(label="กล่องทดสอบพิมพ์:", xalign=0)
            test_entry = Gtk.Entry()
            test_entry.set_placeholder_text("ลองกดปุ่มสลับภาษาแล้วพิมพ์ทดสอบตรงนี้ได้เลย...")
            form.attach(lbl_test, 0, 3, 1, 1)
            form.attach(test_entry, 1, 3, 1, 1)

            # Apply button
            btn_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
            apply_btn = Gtk.Button(label="บันทึกและนำไปใช้ทันที")
            apply_btn.get_style_context().add_class("suggested-action")
            apply_btn.connect("clicked", lambda b: self.save_keyboard())
            btn_box.pack_end(apply_btn, False, False, 0)
            box.pack_start(btn_box, False, False, 0)

            self.stack.add_titled(scroller, "keyboard", "🌐 ภาษาและแป้นพิมพ์")

        def save_keyboard(self):
            layouts = self.kbd_entry.get_text().strip()
            opt = self.kbd_opt_combo.get_active_id() or "super_space"
            if layouts:
                ConfigManager.apply_keyboard_settings(layouts, opt)
                self.show_toast("✓ บันทึกการตั้งค่าแป้นพิมพ์และนำไปใช้แล้ว")

        # 2. Dock Page
        def build_dock_page(self):
            scroller, box = self.create_page_scroller(
                "⚓ ตั้งค่า Dock และแอปที่ปักหมุด",
                "ปรับตำแหน่ง ขนาด เอฟเฟกต์ซูม และจัดการรายการไอคอนบน Dock"
            )

            dock = ConfigManager.get_dock_settings()

            form = Gtk.Grid(row_spacing=12, column_spacing=16)
            box.pack_start(form, False, False, 0)

            # Position (Top disabled)
            lbl1 = Gtk.Label(label="ตำแหน่งบนจอ (Position):", xalign=0)
            self.dock_pos_combo = Gtk.ComboBoxText()
            self.dock_pos_combo.append("Bottom", "ด้านล่าง (Bottom) [ค่าเริ่มต้น]")
            self.dock_pos_combo.append("Left", "ด้านซ้าย (Left)")
            self.dock_pos_combo.append("Right", "ด้านขวา (Right)")
            self.dock_pos_combo.set_active_id(dock["position"])
            form.attach(lbl1, 0, 0, 1, 1)
            form.attach(self.dock_pos_combo, 1, 0, 1, 1)

            # Size
            lbl2 = Gtk.Label(label="ขนาดไอคอน (Icon Size):", xalign=0)
            self.dock_size_combo = Gtk.ComboBoxText()
            for s in ["36", "48", "64", "80", "96"]:
                self.dock_size_combo.append(s, f"{s} พิกเซล")
            self.dock_size_combo.set_active_id(dock["size"])
            form.attach(lbl2, 0, 1, 1, 1)
            form.attach(self.dock_size_combo, 1, 1, 1, 1)

            # Auto-hide switch
            lbl3 = Gtk.Label(label="ซ่อนอัตโนมัติ (Auto-Hide):", xalign=0)
            self.dock_hide_switch = Gtk.Switch()
            self.dock_hide_switch.set_active(dock["autohide"] == "1")
            form.attach(lbl3, 0, 2, 1, 1)
            form.attach(self.dock_hide_switch, 1, 2, 1, 1)

            # Magnification switch
            lbl4 = Gtk.Label(label="เอฟเฟกต์ซูมคลื่น (Magnification Wave):", xalign=0)
            self.dock_mag_switch = Gtk.Switch()
            self.dock_mag_switch.set_active(dock["magnification"] == "1")
            form.attach(lbl4, 0, 3, 1, 1)
            form.attach(self.dock_mag_switch, 1, 3, 1, 1)

            # Apps corner
            lbl5 = Gtk.Label(label="มุมของปุ่ม Apps Launcher:", xalign=0)
            self.dock_corner_combo = Gtk.ComboBoxText()
            self.dock_corner_combo.append("right", "มุมขวา (Right)")
            self.dock_corner_combo.append("left", "มุมซ้าย (Left)")
            self.dock_corner_combo.set_active_id(dock["apps_corner"])
            form.attach(lbl5, 0, 4, 1, 1)
            form.attach(self.dock_corner_combo, 1, 4, 1, 1)

            # Save dock settings button
            save_dock_btn = Gtk.Button(label="บันทึกการตั้งค่า Dock")
            save_dock_btn.connect("clicked", lambda b: self.save_dock())
            form.attach(save_dock_btn, 1, 5, 1, 1)

            # Separator
            box.pack_start(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL), False, False, 8)

            # Pinned Apps Manager
            pinned_lbl = Gtk.Label(xalign=0)
            pinned_lbl.set_markup("<b>แอปพลิเคชันที่ปักหมุดบน Dock (Pinned Apps)</b>")
            box.pack_start(pinned_lbl, False, False, 0)

            self.apps_liststore = Gtk.ListStore(str, str, str)
            for a in ConfigManager.get_dock_pinned_apps():
                self.apps_liststore.append([a["name"], a["exec"], a["icon"]])

            treeview = Gtk.TreeView(model=self.apps_liststore)
            col1 = Gtk.TreeViewColumn("ชื่อแอป", Gtk.CellRendererText(), text=0)
            col2 = Gtk.TreeViewColumn("คำสั่งเปิด", Gtk.CellRendererText(), text=1)
            col3 = Gtk.TreeViewColumn("ไอคอน", Gtk.CellRendererText(), text=2)
            treeview.append_column(col1)
            treeview.append_column(col2)
            treeview.append_column(col3)

            tree_scroller = Gtk.ScrolledWindow()
            tree_scroller.set_min_content_height(140)
            tree_scroller.add(treeview)
            box.pack_start(tree_scroller, False, False, 0)

            # Dock apps buttons
            dbtn_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            add_app_btn = Gtk.Button(label="+ เพิ่มแอป")
            del_app_btn = Gtk.Button(label="- ลบแอปที่เลือก")
            up_app_btn = Gtk.Button(label="↑ เลื่อนขึ้น")
            dn_app_btn = Gtk.Button(label="↓ เลื่อนลง")

            def on_add_app(b):
                dialog = Gtk.Dialog(title="เพิ่มแอปบน Dock", parent=self, flags=0)
                dialog.add_buttons("ยกเลิก", Gtk.ResponseType.CANCEL, "เพิ่ม", Gtk.ResponseType.OK)
                grid = Gtk.Grid(row_spacing=8, column_spacing=8, margin=12)
                dialog.get_content_area().add(grid)
                e_name = Gtk.Entry()
                e_exec = Gtk.Entry()
                e_icon = Gtk.Entry()
                grid.attach(Gtk.Label(label="ชื่อแอป:"), 0, 0, 1, 1)
                grid.attach(e_name, 1, 0, 1, 1)
                grid.attach(Gtk.Label(label="คำสั่ง Exec:"), 0, 1, 1, 1)
                grid.attach(e_exec, 1, 1, 1, 1)
                grid.attach(Gtk.Label(label="ชื่อ Icon:"), 0, 2, 1, 1)
                grid.attach(e_icon, 1, 2, 1, 1)
                dialog.show_all()
                if dialog.run() == Gtk.ResponseType.OK:
                    name = e_name.get_text().strip()
                    cmd = e_exec.get_text().strip()
                    icon = e_icon.get_text().strip() or name.lower()
                    if name and cmd:
                        self.apps_liststore.append([name, cmd, icon])
                        self.save_pinned_apps_from_store()
                dialog.destroy()

            def on_del_app(b):
                sel = treeview.get_selection()
                model, it = sel.get_selected()
                if it:
                    model.remove(it)
                    self.save_pinned_apps_from_store()

            def on_up_app(b):
                sel = treeview.get_selection()
                model, it = sel.get_selected()
                if it:
                    path = model.get_path(it)
                    if path.get_indices()[0] > 0:
                        prev_it = model.get_iter((path.get_indices()[0] - 1,))
                        model.swap(it, prev_it)
                        self.save_pinned_apps_from_store()

            def on_dn_app(b):
                sel = treeview.get_selection()
                model, it = sel.get_selected()
                if it:
                    path = model.get_path(it)
                    if path.get_indices()[0] < len(model) - 1:
                        next_it = model.get_iter((path.get_indices()[0] + 1,))
                        model.swap(it, next_it)
                        self.save_pinned_apps_from_store()

            add_app_btn.connect("clicked", on_add_app)
            del_app_btn.connect("clicked", on_del_app)
            up_app_btn.connect("clicked", on_up_app)
            dn_app_btn.connect("clicked", on_dn_app)

            dbtn_box.pack_start(add_app_btn, False, False, 0)
            dbtn_box.pack_start(del_app_btn, False, False, 0)
            dbtn_box.pack_start(up_app_btn, False, False, 0)
            dbtn_box.pack_start(dn_app_btn, False, False, 0)
            box.pack_start(dbtn_box, False, False, 0)

            self.stack.add_titled(scroller, "dock", "⚓ ตั้งค่า Dock")

        def save_dock(self):
            ConfigManager.save_dock_settings({
                "position": self.dock_pos_combo.get_active_id() or "Bottom",
                "size": self.dock_size_combo.get_active_id() or "48",
                "autohide": self.dock_hide_switch.get_active(),
                "magnification": self.dock_mag_switch.get_active(),
                "apps_corner": self.dock_corner_combo.get_active_id() or "right"
            })
            self.show_toast("✓ บันทึกการตั้งค่า Dock เรียบร้อย")

        def save_pinned_apps_from_store(self):
            apps = []
            for row in self.apps_liststore:
                apps.append({"name": row[0], "exec": row[1], "icon": row[2]})
            ConfigManager.save_dock_pinned_apps(apps)

        # 3. Titlebar & Controls Page
        def build_titlebar_page(self):
            scroller, box = self.create_page_scroller(
                "🪟 กรอบหน้าต่างและ Title bar",
                "ปรับแต่งความสูง Titlebar ฟอนต์ ปุ่มสไตล์ macOS และการหรี่แสงหน้าต่าง"
            )

            tb = ConfigManager.get_titlebar_settings()

            form = Gtk.Grid(row_spacing=12, column_spacing=16)
            box.pack_start(form, False, False, 0)

            # Height
            lbl1 = Gtk.Label(label="ความสูง Titlebar:", xalign=0)
            self.tb_height_combo = Gtk.ComboBoxText()
            for h in ["24", "28", "32"]:
                self.tb_height_combo.append(h, f"{h} พิกเซล" + (" [ค่าเริ่มต้น]" if h == "24" else ""))
            self.tb_height_combo.set_active_id(tb["height"])
            form.attach(lbl1, 0, 0, 1, 1)
            form.attach(self.tb_height_combo, 1, 0, 1, 1)

            # Font
            lbl2 = Gtk.Label(label="ชื่อฟอนต์:", xalign=0)
            self.tb_font_entry = Gtk.Entry()
            self.tb_font_entry.set_text(tb["font"])
            form.attach(lbl2, 0, 1, 1, 1)
            form.attach(self.tb_font_entry, 1, 1, 1, 1)

            # Font Size
            lbl3 = Gtk.Label(label="ขนาดฟอนต์:", xalign=0)
            self.tb_size_spin = Gtk.SpinButton.new_with_range(9, 14, 1)
            self.tb_size_spin.set_value(float(tb["font_size"]))
            form.attach(lbl3, 0, 2, 1, 1)
            form.attach(self.tb_size_spin, 1, 2, 1, 1)

            # Alignment
            lbl4 = Gtk.Label(label="ตำแหน่งชื่อหน้าต่าง:", xalign=0)
            self.tb_align_combo = Gtk.ComboBoxText()
            self.tb_align_combo.append("left", "ชิดซ้าย ถัดจากปุ่มควบคุม (Left)")
            self.tb_align_combo.append("center", "กึ่งกลางหน้าต่าง (Center)")
            self.tb_align_combo.set_active_id(tb["align"])
            form.attach(lbl4, 0, 3, 1, 1)
            form.attach(self.tb_align_combo, 1, 3, 1, 1)

            # Double click
            lbl5 = Gtk.Label(label="ดับเบิลคลิกบน Titlebar:", xalign=0)
            self.tb_dc_combo = Gtk.ComboBoxText()
            self.tb_dc_combo.append("maximize", "ขยายเต็มจอ / คืนขนาดเดิม (Maximize/Restore)")
            self.tb_dc_combo.append("tile", "จัดชิดแบ่งครึ่งจอ (Tile 50/50)")
            self.tb_dc_combo.append("float", "ลอยหน้าต่างตรงกลาง (Float Center)")
            self.tb_dc_combo.set_active_id(tb["double_click"])
            form.attach(lbl5, 0, 4, 1, 1)
            form.attach(self.tb_dc_combo, 1, 4, 1, 1)

            # Inactivity Dimming
            lbl6 = Gtk.Label(label="หรี่แสงหน้าต่างที่ไม่แอคทีฟ (Dimming):", xalign=0)
            self.tb_dim_switch = Gtk.Switch()
            self.tb_dim_switch.set_active(tb["inactivity_dimming"] == "1")
            form.attach(lbl6, 0, 5, 1, 1)
            form.attach(self.tb_dim_switch, 1, 5, 1, 1)

            # Inactivity Timeout
            lbl7 = Gtk.Label(label="เวลาตรวจจับความไม่แอคทีฟ:", xalign=0)
            self.tb_timeout_combo = Gtk.ComboBoxText()
            for sec in ["15", "30", "60", "120"]:
                self.tb_timeout_combo.append(sec, f"{sec} วินาที")
            self.tb_timeout_combo.set_active_id(tb["inactivity_timeout"])
            form.attach(lbl7, 0, 6, 1, 1)
            form.attach(self.tb_timeout_combo, 1, 6, 1, 1)

            # Inactive Opacity
            lbl8 = Gtk.Label(label="ระดับความโปร่งแสงที่ไม่แอคทีฟ:", xalign=0)
            self.tb_opacity_spin = Gtk.SpinButton.new_with_range(50, 100, 5)
            self.tb_opacity_spin.set_value(float(tb["inactive_opacity"]))
            form.attach(lbl8, 0, 7, 1, 1)
            form.attach(self.tb_opacity_spin, 1, 7, 1, 1)

            # Save Titlebar button
            save_tb_btn = Gtk.Button(label="บันทึกการตั้งค่า Titlebar")
            save_tb_btn.get_style_context().add_class("suggested-action")
            save_tb_btn.connect("clicked", lambda b: self.save_titlebar())
            form.attach(save_tb_btn, 1, 8, 1, 1)

            self.stack.add_titled(scroller, "titlebar", "🪟 กรอบหน้าต่าง Titlebar")

        def save_titlebar(self):
            ConfigManager.save_titlebar_settings({
                "height": self.tb_height_combo.get_active_id() or "24",
                "font": self.tb_font_entry.get_text().strip() or "sans-serif",
                "font_size": str(int(self.tb_size_spin.get_value())),
                "align": self.tb_align_combo.get_active_id() or "left",
                "double_click": self.tb_dc_combo.get_active_id() or "maximize",
                "inactivity_dimming": "1" if self.tb_dim_switch.get_active() else "0",
                "inactivity_timeout": self.tb_timeout_combo.get_active_id() or "30",
                "inactive_opacity": str(int(self.tb_opacity_spin.get_value()))
            })
            self.show_toast("✓ บันทึกการตั้งค่า Titlebar เรียบร้อย")

        # 4. Fullscreen Allowlist Page
        def build_fullscreen_page(self):
            scroller, box = self.create_page_scroller(
                "⛶ แอปเปิดเต็มจออัตโนมัติ (Fullscreen Allowlist)",
                "หน้าต่างของแอปพลิเคชันในรายการนี้จะขยายเต็มจอ (Maximize) ทันทีที่เปิดใช้งาน"
            )

            self.fs_liststore = Gtk.ListStore(str)
            for it in ConfigManager.get_fullscreen_allowlist():
                self.fs_liststore.append([it])

            treeview = Gtk.TreeView(model=self.fs_liststore)
            col = Gtk.TreeViewColumn("WM_CLASS หรือชื่อหน้าต่าง", Gtk.CellRendererText(), text=0)
            treeview.append_column(col)

            tree_scroller = Gtk.ScrolledWindow()
            tree_scroller.set_min_content_height(180)
            tree_scroller.add(treeview)
            box.pack_start(tree_scroller, False, False, 0)

            # Controls
            cbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            self.fs_entry = Gtk.Entry()
            self.fs_entry.set_placeholder_text("พิมพ์ WM_CLASS (เช่น google-chrome, code)...")
            add_btn = Gtk.Button(label="+ เพิ่มเข้า Allowlist")
            del_btn = Gtk.Button(label="- ลบรายการที่เลือก")

            def on_add(b):
                val = self.fs_entry.get_text().strip()
                if val:
                    self.fs_liststore.append([val])
                    self.fs_entry.set_text("")
                    self.save_fullscreen_from_store()

            def on_del(b):
                sel = treeview.get_selection()
                model, it = sel.get_selected()
                if it:
                    model.remove(it)
                    self.save_fullscreen_from_store()

            add_btn.connect("clicked", on_add)
            del_btn.connect("clicked", on_del)

            cbox.pack_start(self.fs_entry, True, True, 0)
            cbox.pack_start(add_btn, False, False, 0)
            cbox.pack_start(del_btn, False, False, 0)
            box.pack_start(cbox, False, False, 0)

            # Common presets
            presets_lbl = Gtk.Label(label="แอปยอดนิยมที่มักเปิดเต็มจอ:", xalign=0)
            box.pack_start(presets_lbl, False, False, 0)

            flowbox = Gtk.FlowBox()
            flowbox.set_valign(Gtk.Align.START)
            flowbox.set_max_children_per_line(6)
            flowbox.set_selection_mode(Gtk.SelectionMode.NONE)

            popular_apps = ["google-chrome", "firefox", "chromium", "vlc", "mpv", "code", "libreoffice", "gimp", "steam"]
            for app in popular_apps:
                pbtn = Gtk.Button(label=f"+ {app}")
                pbtn.connect("clicked", lambda b, a=app: self.quick_add_fullscreen(a))
                flowbox.add(pbtn)
            box.pack_start(flowbox, False, False, 0)

            self.stack.add_titled(scroller, "fullscreen", "⛶ แอปเปิดเต็มจอ (Allowlist)")

        def quick_add_fullscreen(self, app_name):
            for row in self.fs_liststore:
                if row[0] == app_name:
                    return
            self.fs_liststore.append([app_name])
            self.save_fullscreen_from_store()
            self.show_toast(f"✓ เพิ่ม {app_name} ลง Fullscreen Allowlist แล้ว")

        def save_fullscreen_from_store(self):
            items = [row[0] for row in self.fs_liststore]
            ConfigManager.save_fullscreen_allowlist(items)

        # 5. Autostart Page
        def build_autostart_page(self):
            scroller, box = self.create_page_scroller(
                "🚀 คำสั่งเริ่มทำงานอัตโนมัติ (Autostart)",
                "โปรแกรมและคำสั่งเชลล์ที่จะถูกเรียกทำงานอัตโนมัติเมื่อ MUZUwm สตาร์ต"
            )

            self.auto_liststore = Gtk.ListStore(bool, str)
            for e in ConfigManager.get_autostart_entries():
                self.auto_liststore.append([e["enabled"], e["command"]])

            treeview = Gtk.TreeView(model=self.auto_liststore)
            toggle_renderer = Gtk.CellRendererToggle()
            def on_toggled(widget, path):
                self.auto_liststore[path][0] = not self.auto_liststore[path][0]
                self.save_autostart_from_store()
            toggle_renderer.connect("toggled", on_toggled)

            col1 = Gtk.TreeViewColumn("เปิดใช้งาน", toggle_renderer, active=0)
            col2 = Gtk.TreeViewColumn("คำสั่งเชลล์ (Command)", Gtk.CellRendererText(), text=1)
            treeview.append_column(col1)
            treeview.append_column(col2)

            tree_scroller = Gtk.ScrolledWindow()
            tree_scroller.set_min_content_height(180)
            tree_scroller.add(treeview)
            box.pack_start(tree_scroller, False, False, 0)

            # Controls
            cbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            self.auto_entry = Gtk.Entry()
            self.auto_entry.set_placeholder_text("พิมพ์คำสั่งเชลล์ (เช่น dunst &, nm-applet &)...")
            add_btn = Gtk.Button(label="+ เพิ่มคำสั่ง")
            del_btn = Gtk.Button(label="- ลบคำสั่งที่เลือก")

            def on_add(b):
                val = self.auto_entry.get_text().strip()
                if val:
                    self.auto_liststore.append([True, val])
                    self.auto_entry.set_text("")
                    self.save_autostart_from_store()

            def on_del(b):
                sel = treeview.get_selection()
                model, it = sel.get_selected()
                if it:
                    model.remove(it)
                    self.save_autostart_from_store()

            add_btn.connect("clicked", on_add)
            del_btn.connect("clicked", on_del)

            cbox.pack_start(self.auto_entry, True, True, 0)
            cbox.pack_start(add_btn, False, False, 0)
            cbox.pack_start(del_btn, False, False, 0)
            box.pack_start(cbox, False, False, 0)

            # Presets
            presets_lbl = Gtk.Label(label="เซอร์วิสและเดมอนที่นิยมรันตอนเริ่มระบบ:", xalign=0)
            box.pack_start(presets_lbl, False, False, 0)

            flowbox = Gtk.FlowBox()
            flowbox.set_valign(Gtk.Align.START)
            flowbox.set_max_children_per_line(4)
            flowbox.set_selection_mode(Gtk.SelectionMode.NONE)

            daemons = [
                ("picom -b", "Compositor (Picom)"),
                ("dunst &", "Notifications (Dunst)"),
                ("nm-applet &", "Network Manager Tray"),
                ("volumeicon &", "Volume Tray Icon"),
                ("redshift &", "Blue Light Filter"),
                ("feh --bg-fill ~/.config/muzuwm/wallpaper.png &", "Wallpaper (Feh)")
            ]
            for cmd, desc in daemons:
                pbtn = Gtk.Button(label=f"+ {desc}")
                pbtn.connect("clicked", lambda b, c=cmd: self.quick_add_autostart(c))
                flowbox.add(pbtn)
            box.pack_start(flowbox, False, False, 0)

            self.stack.add_titled(scroller, "autostart", "🚀 คำสั่งเริ่มอัตโนมัติ")

        def quick_add_autostart(self, cmd):
            self.auto_liststore.append([True, cmd])
            self.save_autostart_from_store()
            self.show_toast(f"✓ เพิ่มคำสั่ง {cmd} เรียบร้อย")

        def save_autostart_from_store(self):
            entries = []
            for row in self.auto_liststore:
                entries.append({"enabled": row[0], "command": row[1]})
            ConfigManager.save_autostart_entries(entries)

        # 6. Top Panel Page
        def build_panel_page(self):
            scroller, box = self.create_page_scroller(
                "📊 พาเนลด้านบน (Top Panel / Tint2rc)",
                "ตั้งค่าตำแหน่งพาเนล ความสูง และรูปแบบการแสดงนาฬิกาดิจิทัล"
            )

            panel = ConfigManager.get_panel_settings()

            form = Gtk.Grid(row_spacing=12, column_spacing=16)
            box.pack_start(form, False, False, 0)

            # Position
            lbl1 = Gtk.Label(label="ตำแหน่งบนจอ (Position):", xalign=0)
            self.panel_pos_combo = Gtk.ComboBoxText()
            self.panel_pos_combo.append("Top", "ด้านบน (Top) [ค่าเริ่มต้น]")
            self.panel_pos_combo.append("Bottom", "ด้านล่าง (Bottom)")
            self.panel_pos_combo.set_active_id(panel["position"])
            form.attach(lbl1, 0, 0, 1, 1)
            form.attach(self.panel_pos_combo, 1, 0, 1, 1)

            # Height
            lbl2 = Gtk.Label(label="ความสูงพาเนล:", xalign=0)
            self.panel_h_combo = Gtk.ComboBoxText()
            for h in ["24", "28", "32", "36", "40"]:
                self.panel_h_combo.append(h, f"{h} พิกเซล")
            self.panel_h_combo.set_active_id(panel["height"])
            form.attach(lbl2, 0, 1, 1, 1)
            form.attach(self.panel_h_combo, 1, 1, 1, 1)

            # Clock Format
            lbl3 = Gtk.Label(label="รูปแบบเวลานาฬิกา:", xalign=0)
            self.panel_clock_combo = Gtk.ComboBoxText()
            self.panel_clock_combo.append("%H:%M:%S", "24-Hour มีวินาที (%H:%M:%S เช่น 14:30:45)")
            self.panel_clock_combo.append("%I:%M:%S %p", "12-Hour มี AM/PM (%I:%M:%S %p เช่น 02:30:45 PM)")
            self.panel_clock_combo.append("%H:%M", "24-Hour สั้น (%H:%M เช่น 14:30)")
            self.panel_clock_combo.append("custom", "กำหนดเอง...")
            self.panel_clock_combo.set_active_id(panel["time1_format"] if panel["time1_format"] in ["%H:%M:%S", "%I:%M:%S %p", "%H:%M"] else "custom")
            form.attach(lbl3, 0, 2, 1, 1)
            form.attach(self.panel_clock_combo, 1, 2, 1, 1)

            # Custom Clock entry
            lbl4 = Gtk.Label(label="รหัส Strftime เวลา:", xalign=0)
            self.panel_clock_entry = Gtk.Entry()
            self.panel_clock_entry.set_text(panel["time1_format"])
            form.attach(lbl4, 0, 3, 1, 1)
            form.attach(self.panel_clock_entry, 1, 3, 1, 1)

            def on_clock_preset(combo):
                cid = combo.get_active_id()
                if cid and cid != "custom":
                    self.panel_clock_entry.set_text(cid)
            self.panel_clock_combo.connect("changed", on_clock_preset)

            # Save panel button
            save_panel_btn = Gtk.Button(label="บันทึกการตั้งค่าพาเนล")
            save_panel_btn.get_style_context().add_class("suggested-action")
            save_panel_btn.connect("clicked", lambda b: self.save_panel())
            form.attach(save_panel_btn, 1, 4, 1, 1)

            self.stack.add_titled(scroller, "panel", "📊 พาเนลด้านบน")

        def save_panel(self):
            ConfigManager.save_panel_settings({
                "position": self.panel_pos_combo.get_active_id() or "Top",
                "height": self.panel_h_combo.get_active_id() or "28",
                "time1_format": self.panel_clock_entry.get_text().strip() or "%H:%M:%S",
                "time2_format": "%a %d %b %Y"
            })
            self.show_toast("✓ บันทึกการตั้งค่าพาเนลเรียบร้อย (มีผลเต็มรูปแบบหลังรีสตาร์ต WM)")

        # 7. Keyboard Shortcuts Page
        def build_shortcuts_page(self):
            scroller, box = self.create_page_scroller(
                "⌨️ คีย์ลัดระบบ (Keyboard Shortcuts)",
                "จัดการคีย์ลัดสำหรับจัดหน้าต่างและเรียกเปิดโปรแกรมด่วน"
            )

            self.sc_liststore = Gtk.ListStore(str, str)
            for s in ConfigManager.get_shortcuts():
                self.sc_liststore.append([s["combo"], s["action"]])

            treeview = Gtk.TreeView(model=self.sc_liststore)
            col1 = Gtk.TreeViewColumn("ปุ่มลัด (Shortcut Combo)", Gtk.CellRendererText(), text=0)
            col2 = Gtk.TreeViewColumn("แอคชันคำสั่ง (Action)", Gtk.CellRendererText(), text=1)
            treeview.append_column(col1)
            treeview.append_column(col2)

            tree_scroller = Gtk.ScrolledWindow()
            tree_scroller.set_min_content_height(180)
            tree_scroller.add(treeview)
            box.pack_start(tree_scroller, False, False, 0)

            # Quick App Change Grid
            qgrid = Gtk.Grid(row_spacing=8, column_spacing=12)
            box.pack_start(qgrid, False, False, 0)

            qgrid.attach(Gtk.Label(label="แอป Terminal (Super+Return):", xalign=0), 0, 0, 1, 1)
            self.term_entry = Gtk.Entry()
            self.term_entry.set_text("wezterm")
            for row in self.sc_liststore:
                if row[0] == "Super+Return" and row[1].startswith("exec "):
                    self.term_entry.set_text(row[1][5:])
            qgrid.attach(self.term_entry, 1, 0, 1, 1)

            qgrid.attach(Gtk.Label(label="แอป Browser (Super+b):", xalign=0), 0, 1, 1, 1)
            self.browser_entry = Gtk.Entry()
            self.browser_entry.set_text("google-chrome")
            for row in self.sc_liststore:
                if row[0] == "Super+b" and row[1].startswith("exec "):
                    self.browser_entry.set_text(row[1][5:])
            qgrid.attach(self.browser_entry, 1, 1, 1, 1)

            qgrid.attach(Gtk.Label(label="แอป Files (Super+e):", xalign=0), 0, 2, 1, 1)
            self.files_entry = Gtk.Entry()
            self.files_entry.set_text("thunar")
            for row in self.sc_liststore:
                if row[0] == "Super+e" and row[1].startswith("exec "):
                    self.files_entry.set_text(row[1][5:])
            qgrid.attach(self.files_entry, 1, 2, 1, 1)

            save_qbtn = Gtk.Button(label="บันทึกแอปด่วน")
            def on_save_quick(b):
                t = self.term_entry.get_text().strip()
                br = self.browser_entry.get_text().strip()
                fl = self.files_entry.get_text().strip()
                for row in self.sc_liststore:
                    if row[0] == "Super+Return":
                        row[1] = f"exec {t}"
                    elif row[0] == "Super+b":
                        row[1] = f"exec {br}"
                    elif row[0] == "Super+e":
                        row[1] = f"exec {fl}"
                self.save_shortcuts_from_store()
                self.show_toast("✓ บันทึกคีย์ลัดแอปด่วนแล้ว")
            save_qbtn.connect("clicked", on_save_quick)
            qgrid.attach(save_qbtn, 1, 3, 1, 1)

            self.stack.add_titled(scroller, "shortcuts", "⌨️ คีย์ลัดระบบ")

        def save_shortcuts_from_store(self):
            shortcuts = []
            for row in self.sc_liststore:
                shortcuts.append({"combo": row[0], "action": row[1]})
            ConfigManager.save_shortcuts(shortcuts)

        # 8. Appearance & Wallpaper Page
        def build_appearance_page(self):
            scroller, box = self.create_page_scroller(
                "🎨 ภาพพื้นหลังและธีม (Appearance & Wallpaper)",
                "เปลี่ยนภาพ Wallpaper ปรับแต่งธีม GTK และเปิด/ปิดเอฟเฟกต์หน้าต่าง"
            )

            # Wallpaper Chooser
            wall_lbl = Gtk.Label(xalign=0)
            wall_lbl.set_markup("<b>ภาพพื้นหลังเดสก์ท็อป (Desktop Wallpaper)</b>")
            box.pack_start(wall_lbl, False, False, 0)

            wbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
            self.wall_path_entry = Gtk.Entry()
            self.wall_path_entry.set_text(WALLPAPER_FILE if os.path.exists(WALLPAPER_FILE) else "")
            browse_btn = Gtk.Button(label="เลือกไฟล์รูปภาพ…")
            apply_wall_btn = Gtk.Button(label="นำไปใช้ทันที")
            apply_wall_btn.get_style_context().add_class("suggested-action")

            def on_browse_wall(b):
                dialog = Gtk.FileChooserDialog(title="เลือกภาพพื้นหลัง", parent=self, action=Gtk.FileChooserAction.OPEN)
                dialog.add_buttons("ยกเลิก", Gtk.ResponseType.CANCEL, "เลือก", Gtk.ResponseType.ACCEPT)
                filt = Gtk.FileFilter()
                filt.set_name("ไฟล์ภาพรูปภาพ")
                filt.add_mime_type("image/png")
                filt.add_mime_type("image/jpeg")
                filt.add_mime_type("image/bmp")
                filt.add_mime_type("image/webp")
                dialog.add_filter(filt)
                if dialog.run() == Gtk.ResponseType.ACCEPT:
                    self.wall_path_entry.set_text(dialog.get_filename())
                dialog.destroy()

            def on_apply_wall(b):
                p = self.wall_path_entry.get_text().strip()
                if p and os.path.isfile(p):
                    ConfigManager.set_wallpaper_image(p)
                    self.show_toast("✓ โหลดภาพพื้นหลังใหม่ลง X11 Root Window ทันทีแล้ว")
                else:
                    self.show_toast("✗ ไม่พบไฟล์ภาพตามที่ระบุ")

            browse_btn.connect("clicked", on_browse_wall)
            apply_wall_btn.connect("clicked", on_apply_wall)

            wbox.pack_start(self.wall_path_entry, True, True, 0)
            wbox.pack_start(browse_btn, False, False, 0)
            wbox.pack_start(apply_wall_btn, False, False, 0)
            box.pack_start(wbox, False, False, 0)

            box.pack_start(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL), False, False, 8)

            # GTK Themes
            gtk_lbl = Gtk.Label(xalign=0)
            gtk_lbl.set_markup("<b>ธีมและไอคอน (GTK Theme & Icons)</b>")
            box.pack_start(gtk_lbl, False, False, 0)

            gform = Gtk.Grid(row_spacing=8, column_spacing=12)
            box.pack_start(gform, False, False, 0)

            gform.attach(Gtk.Label(label="ธีม GTK (GTK Theme):", xalign=0), 0, 0, 1, 1)
            gtk_combo = Gtk.ComboBoxText()
            for t in ConfigManager.get_installed_gtk_themes():
                gtk_combo.append(t, t)
            gtk_combo.set_active(0)
            gform.attach(gtk_combo, 1, 0, 1, 1)

            gform.attach(Gtk.Label(label="ธีมไอคอน (Icon Theme):", xalign=0), 0, 1, 1, 1)
            icon_combo = Gtk.ComboBoxText()
            for i in ConfigManager.get_installed_icon_themes():
                icon_combo.append(i, i)
            icon_combo.set_active(0)
            gform.attach(icon_combo, 1, 1, 1, 1)

            self.stack.add_titled(scroller, "appearance", "🎨 ธีมและภาพพื้นหลัง")

        # 9. System, Backup & Diagnostics Page
        def build_system_page(self):
            scroller, box = self.create_page_scroller(
                "⚙️ สำรองข้อมูลและระบบ (System & Backup)",
                "สำรองข้อมูลคอนฟิก คืนค่า หรือรีเซ็ตการตั้งค่ากลับสู่ค่าเริ่มต้นโรงงาน"
            )

            # Reload Button
            reload_btn = Gtk.Button(label="🔄 รีโหลดการตั้งค่า MUZUwm ทันที")
            reload_btn.connect("clicked", lambda b: (
                ConfigManager.reload_muzuwm_settings(),
                ConfigManager.reload_muzuwm_wallpaper(),
                self.show_toast("✓ ส่งสัญญาณรีโหลดคอนฟิกไปยัง MUZUwm แล้ว")
            ))
            box.pack_start(reload_btn, False, False, 0)

            # Backup Section
            b_lbl = Gtk.Label(xalign=0)
            b_lbl.set_markup("<b>สำรองและกู้คืนการตั้งค่า (Backup & Restore)</b>")
            box.pack_start(b_lbl, False, False, 0)

            bbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
            backup_btn = Gtk.Button(label="สร้างไฟล์สำรองเดี๋ยวนี้")
            def on_backup(b):
                arc = ConfigManager.backup_configs()
                self.show_toast(f"✓ สำรองข้อมูลแล้ว: {os.path.basename(arc)}")
            backup_btn.connect("clicked", on_backup)
            bbox.pack_start(backup_btn, False, False, 0)
            box.pack_start(bbox, False, False, 0)

            # Factory Reset
            reset_lbl = Gtk.Label(xalign=0)
            reset_lbl.set_markup("<b>คืนค่าเริ่มต้นโรงงาน (Factory Reset)</b>")
            box.pack_start(reset_lbl, False, False, 0)

            reset_btn = Gtk.Button(label="⚠️ รีเซ็ตการตั้งค่าทั้งหมดเป็นค่าเริ่มต้นโรงงาน")
            reset_btn.get_style_context().add_class("destructive-action")
            def on_reset(b):
                md = Gtk.MessageDialog(
                    parent=self,
                    flags=Gtk.DialogFlags.MODAL,
                    type=Gtk.MessageType.WARNING,
                    buttons=Gtk.ButtonsType.OK_CANCEL,
                    message_format="คุณแน่ใจหรือไม่ว่าต้องการรีเซ็ตการตั้งค่าทั้งหมด?"
                )
                md.format_secondary_text("ระบบจะสำรองไฟล์ปัจจุบันไว้ก่อนรีเซ็ต")
                res = md.run()
                md.destroy()
                if res == Gtk.ResponseType.OK:
                    ConfigManager.reset_factory_defaults()
                    self.show_toast("✓ รีเซ็ตการตั้งค่าเรียบร้อยแล้ว")
            reset_btn.connect("clicked", on_reset)
            box.pack_start(reset_btn, False, False, 0)

            self.stack.add_titled(scroller, "system", "⚙️ สำรองข้อมูลและระบบ")

        def show_toast(self, message):
            dialog = Gtk.MessageDialog(
                parent=self,
                flags=Gtk.DialogFlags.MODAL,
                type=Gtk.MessageType.INFO,
                buttons=Gtk.ButtonsType.OK,
                message_format=message
            )
            dialog.run()
            dialog.destroy()

    win = MuzuSettingsWindow()
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()


# ==============================================================================
# Main Entry Point with Automatic Interface Detection
# ==============================================================================

def main():
    # Flags handling
    if "--cli" in sys.argv:
        run_cli_menu()
        return

    # Check if GUI is available
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

    # Fallback to CLI menu if headless or GTK unavailable
    run_cli_menu()


if __name__ == "__main__":
    main()