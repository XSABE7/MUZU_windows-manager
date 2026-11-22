#!/usr/bin/env python3
"""
MUZUwm Updater
Check and install updates from GitHub Stable or unstable channels.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.error
import urllib.request

GITHUB_OWNER = "XSABE7"
GITHUB_REPO = "MUZU_windows-manager"
GITHUB_GIT_URL = f"https://github.com/{GITHUB_OWNER}/{GITHUB_REPO}.git"
GITHUB_API_BRANCH_URL = f"https://api.github.com/repos/{GITHUB_OWNER}/{GITHUB_REPO}/branches/{{branch}}"
GITHUB_TARBALL_URL = f"https://github.com/{GITHUB_OWNER}/{GITHUB_REPO}/archive/refs/heads/{{branch}}.tar.gz"

CHANNELS = ("Stable", "unstable")

CONFIG_DIR = os.path.expanduser("~/.config/muzuwm")
STATE_FILE = os.path.join(CONFIG_DIR, "update_state.json")
CACHE_DIR = os.path.expanduser("~/.cache/muzuwm")


def normalize_channel(name):
    if not name:
        return None
    low = name.strip().lower()
    if low == "stable":
        return "Stable"
    if low == "unstable":
        return "unstable"
    return None


def detect_install_prefix(explicit_prefix=None):
    if explicit_prefix:
        return explicit_prefix
    exe = shutil.which("muzuwm")
    if exe:
        real = os.path.realpath(exe)
        if real.startswith("/usr/local/bin/"):
            return "/usr/local"
        if real.startswith("/usr/bin/"):
            return "/usr"
        parent = os.path.dirname(os.path.dirname(real))
        if parent:
            return parent
    return "/usr/local"


def load_local_state():
    state = {
        "channel": "",
        "sha": "",
        "short_sha": "",
        "message": "",
        "prefix": detect_install_prefix(),
        "updated_at": "",
    }
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, "r", encoding="utf-8") as f:
                loaded = json.load(f)
            if isinstance(loaded, dict):
                state.update({k: v for k, v in loaded.items() if v is not None})
        except Exception:
            pass

    # If run inside a local git checkout and no saved SHA exists, inspect HEAD as context
    if not state.get("sha"):
        repo_dir = os.path.dirname(os.path.abspath(__file__))
        if os.path.isdir(os.path.join(repo_dir, ".git")) and shutil.which("git"):
            try:
                sha = subprocess.check_output(
                    ["git", "-C", repo_dir, "rev-parse", "HEAD"],
                    stderr=subprocess.DEVNULL,
                    text=True,
                    timeout=3,
                ).strip()
                branch = subprocess.check_output(
                    ["git", "-C", repo_dir, "rev-parse", "--abbrev-ref", "HEAD"],
                    stderr=subprocess.DEVNULL,
                    text=True,
                    timeout=3,
                ).strip()
                if sha:
                    state["sha"] = sha
                    state["short_sha"] = sha[:7]
                if not state.get("channel") and branch in CHANNELS:
                    state["channel"] = branch
            except Exception:
                pass
    return state


def save_local_state(channel, sha, message, prefix):
    os.makedirs(CONFIG_DIR, exist_ok=True)
    payload = {
        "channel": channel,
        "sha": sha,
        "short_sha": sha[:7] if sha else "",
        "message": message or "",
        "prefix": prefix,
        "updated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
    }
    tmp = f"{STATE_FILE}.tmp.{os.getpid()}"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)
    os.replace(tmp, STATE_FILE)


def fetch_github_branch_info(channel, timeout=8):
    chan = normalize_channel(channel)
    if not chan:
        return {"channel": channel, "error": f"Unknown channel: {channel}"}

    info = {
        "channel": chan,
        "sha": "",
        "short_sha": "",
        "date": "",
        "message": "",
        "source": "github-api",
        "error": "",
    }

    url = GITHUB_API_BRANCH_URL.format(branch=chan)
    req = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "MUZUwm-Updater/1.0",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            data = json.loads(resp.read().decode("utf-8", errors="replace"))
        commit = data.get("commit") or {}
        sha = commit.get("sha") or ""
        cmeta = commit.get("commit") or {}
        author = cmeta.get("author") or cmeta.get("committer") or {}
        raw_msg = (cmeta.get("message") or "").strip().splitlines()
        info["sha"] = sha
        info["short_sha"] = sha[:7]
        info["date"] = (author.get("date") or "")[:19].replace("T", " ")
        info["message"] = raw_msg[0][:72] if raw_msg else ""
        if sha:
            return info
    except Exception as api_err:
        info["error"] = str(api_err)

    # Fallback: git ls-remote over HTTPS (does not count against unauthenticated GitHub REST rate limits)
    if shutil.which("git"):
        try:
            out = subprocess.check_output(
                ["git", "ls-remote", "--heads", GITHUB_GIT_URL, f"refs/heads/{chan}"],
                stderr=subprocess.PIPE,
                text=True,
                timeout=timeout,
            ).strip()
            if out:
                sha = out.split()[0]
                info["sha"] = sha
                info["short_sha"] = sha[:7]
                info["source"] = "git-ls-remote"
                info["error"] = ""
                if not info["message"]:
                    info["message"] = "(fetched commit hash via git ls-remote)"
                return info
        except Exception as git_err:
            if info["error"]:
                info["error"] = f"{info['error']}; git: {git_err}"
            else:
                info["error"] = str(git_err)

    return info


def check_all_channels(timeout=8):
    return {ch: fetch_github_branch_info(ch, timeout=timeout) for ch in CHANNELS}


def _run_cmd(cmd, cwd=None, log_fn=None):
    if log_fn:
        log_fn(f"$ {' '.join(cmd)}")
    proc = subprocess.run(cmd, cwd=cwd)
    if proc.returncode != 0:
        raise RuntimeError(f"Command failed (exit {proc.returncode}): {' '.join(cmd)}")


def _download_source_tarball(channel, dest_dir, log_fn=None):
    url = GITHUB_TARBALL_URL.format(branch=channel)
    if log_fn:
        log_fn(f"Downloading {url} ...")
    tgz_path = os.path.join(dest_dir, "src.tar.gz")
    req = urllib.request.Request(url, headers={"User-Agent": "MUZUwm-Updater/1.0"})
    with urllib.request.urlopen(req, timeout=30) as resp, open(tgz_path, "wb") as out:
        shutil.copyfileobj(resp, out)

    extract_dir = os.path.join(dest_dir, "extracted")
    os.makedirs(extract_dir, exist_ok=True)
    with tarfile.open(tgz_path, "r:gz") as tar:
        base_abs = os.path.abspath(extract_dir)
        for member in tar.getmembers():
            target_abs = os.path.abspath(os.path.join(extract_dir, member.name))
            if not target_abs.startswith(base_abs + os.sep) and target_abs != base_abs:
                raise RuntimeError("Unsafe path in archive")
        tar.extractall(extract_dir)

    entries = [os.path.join(extract_dir, e) for e in os.listdir(extract_dir) if os.path.isdir(os.path.join(extract_dir, e))]
    if not entries:
        raise RuntimeError("Archive extracted cleanly but no source directory was found")
    return entries[0]


def perform_update(channel, prefix=None, dry_run=False, log_fn=None):
    chan = normalize_channel(channel)
    if not chan:
        raise ValueError(f"Invalid channel '{channel}'. Expected Stable or unstable.")

    target_prefix = detect_install_prefix(prefix)
    info = fetch_github_branch_info(chan)
    if info.get("error") and not info.get("sha"):
        raise RuntimeError(f"Unable to reach GitHub for {chan}: {info['error']}")

    os.makedirs(CACHE_DIR, exist_ok=True)
    work_dir = tempfile.mkdtemp(prefix=f"muzuwm-update-{chan}-", dir=CACHE_DIR)
    src_dir = os.path.join(work_dir, "repo")
    try:
        if shutil.which("git"):
            _run_cmd(
                ["git", "clone", "--depth", "1", "--branch", chan, GITHUB_GIT_URL, src_dir],
                log_fn=log_fn,
            )
            if not info.get("sha"):
                info["sha"] = subprocess.check_output(
                    ["git", "-C", src_dir, "rev-parse", "HEAD"], text=True
                ).strip()
                info["short_sha"] = info["sha"][:7]
            if not info.get("message") or info["message"].startswith("(fetched"):
                info["message"] = subprocess.check_output(
                    ["git", "-C", src_dir, "log", "-1", "--pretty=%s"], text=True
                ).strip()[:72]
        else:
            src_dir = _download_source_tarball(chan, work_dir, log_fn=log_fn)

        jobs = str(max(1, os.cpu_count() or 1))
        _run_cmd(["make", f"-j{jobs}"], cwd=src_dir, log_fn=log_fn)

        if dry_run:
            if log_fn:
                log_fn(f"[dry-run] Build succeeded for {chan} ({info.get('short_sha')}). Skipping install to {target_prefix}.")
            return info, target_prefix

        install_cmd = ["make", "install", f"PREFIX={target_prefix}"]
        can_write_prefix = (os.geteuid() == 0) or (
            os.path.exists(target_prefix) and os.access(target_prefix, os.W_OK)
        )
        if not can_write_prefix:
            if shutil.which("sudo"):
                install_cmd = ["sudo"] + install_cmd
            elif shutil.which("pkexec"):
                install_cmd = ["pkexec"] + install_cmd

        _run_cmd(install_cmd, cwd=src_dir, log_fn=log_fn)
        save_local_state(chan, info.get("sha", ""), info.get("message", ""), target_prefix)

        if shutil.which("xprop") and os.environ.get("DISPLAY"):
            subprocess.run(
                ["xprop", "-root", "-f", "_MUZU_RELOAD_SETTINGS", "32c", "-set", "_MUZU_RELOAD_SETTINGS", "1"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )

        if log_fn:
            log_fn(f"Updated MUZUwm to {chan} ({info.get('short_sha')}).")
            log_fn("Note: Log out or restart MUZUwm session to load the updated WM binary.")
        return info, target_prefix
    finally:
        shutil.rmtree(work_dir, ignore_errors=True)


def print_status_report():
    local = load_local_state()
    remote = check_all_channels()
    print("=== MUZUwm Updater ===")
    print(f"GitHub Repo   : {GITHUB_OWNER}/{GITHUB_REPO}")
    print(f"Local Channel : {local.get('channel') or 'unknown'}")
    print(f"Local Commit  : {local.get('short_sha') or 'unknown'}")
    print(f"Install Prefix: {local.get('prefix') or detect_install_prefix()}")
    if local.get("updated_at"):
        print(f"Last Updated  : {local['updated_at']}")
    print("")
    for ch in CHANNELS:
        r = remote[ch]
        if r.get("error") and not r.get("sha"):
            print(f"[{ch:<8}] ERROR: {r['error']}")
        else:
            same = bool(local.get("sha") and r.get("sha") and local["sha"] == r["sha"])
            tag = " (INSTALLED)" if same else ""
            print(f"[{ch:<8}] {r.get('short_sha', '?')}{tag}  {r.get('date', '')}")
            if r.get("message"):
                print(f"           {r['message']}")
    return local, remote


def run_cli_updater():
    while True:
        print("\n=== MUZUwm Updater ===")
        local, remote = print_status_report()
        print("\n1. Refresh / Check GitHub")
        print("2. Update to Stable")
        print("3. Update to unstable")
        print("0. Exit")
        try:
            choice = input("Select [0-3]: ").strip()
        except EOFError:
            break
        if choice == "0":
            break
        elif choice == "1":
            continue
        elif choice in ("2", "3"):
            chan = "Stable" if choice == "2" else "unstable"
            confirm = input(f"Build and install MUZUwm from '{chan}'? [y/N]: ").strip().lower()
            if confirm in ("y", "yes"):
                try:
                    perform_update(chan, log_fn=print)
                except Exception as exc:
                    print(f"Update failed: {exc}", file=sys.stderr)


def run_tui_updater():
    import curses
    import locale
    locale.setlocale(locale.LC_ALL, "")

    def init_colors():
        if not curses.has_colors():
            return False
        curses.start_color()
        curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_BLACK)
        curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLACK)
        curses.init_pair(3, curses.COLOR_BLACK, curses.COLOR_CYAN)
        curses.init_pair(4, curses.COLOR_CYAN, curses.COLOR_BLACK)
        curses.init_pair(5, curses.COLOR_WHITE, curses.COLOR_BLACK)
        curses.init_pair(6, curses.COLOR_CYAN, curses.COLOR_BLACK)
        curses.init_pair(7, curses.COLOR_RED, curses.COLOR_BLACK)
        curses.init_pair(8, curses.COLOR_GREEN, curses.COLOR_BLACK)
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
        ph = min(req_h, max(10, sh - 4))
        pw = min(req_w, max(40, sw - 4))
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

    def tui_confirm(stdscr, title, question, has_color):
        draw_backdrop(stdscr, "MUZUwm Updater", "[Left/Right] Choice  [Enter] Confirm  [Esc] Cancel", has_color)
        win = make_panel(stdscr, 9, 64, title, has_color)
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

    def _tui_main(stdscr):
        curses.curs_set(0)
        stdscr.keypad(True)
        has_color = init_colors()

        draw_backdrop(stdscr, "MUZUwm Updater", "Checking GitHub Stable & unstable...", has_color)
        win = make_panel(stdscr, 8, 60, "MUZUwm Updater", has_color)
        safe_addstr(win, 3, 4, "Fetching branch status from GitHub...")
        win.refresh()

        remote = check_all_channels()
        sel = 0

        while True:
            local = load_local_state()
            draw_backdrop(
                stdscr,
                "MUZUwm Updater",
                "[Up/Down] Move  [Enter] Select  [R] Refresh  [Q] Quit",
                has_color,
            )
            win = make_panel(stdscr, 20, 78, "MUZUwm Updater", has_color)
            ph, pw = win.getmaxyx()
            accent_attr = (curses.color_pair(6) | curses.A_BOLD) if has_color else curses.A_BOLD
            sel_attr = (curses.color_pair(3) | curses.A_BOLD) if has_color else curses.A_REVERSE
            err_attr = (curses.color_pair(7) | curses.A_BOLD) if has_color else curses.A_DIM
            ok_attr = (curses.color_pair(8) | curses.A_BOLD) if has_color else curses.A_BOLD

            local_ch = local.get("channel") or "unknown"
            local_sha = local.get("short_sha") or "unknown"
            prefix = local.get("prefix") or detect_install_prefix()
            safe_addstr(win, 2, 3, f"Repo           : {GITHUB_OWNER}/{GITHUB_REPO}", accent_attr)
            safe_addstr(win, 3, 3, f"Installed      : {local_ch} ({local_sha}) -> {prefix}")
            safe_addstr(win, 4, 2, "-" * (pw - 4))

            row_y = 5
            for ch_name in CHANNELS:
                r = remote.get(ch_name, {})
                if r.get("error") and not r.get("sha"):
                    safe_addstr(win, row_y, 3, f"{ch_name:<9} : Error - {r['error']}", err_attr)
                    safe_addstr(win, row_y + 1, 15, "")
                else:
                    same = bool(local.get("sha") and r.get("sha") and local["sha"] == r["sha"])
                    status_tag = " [UP TO DATE]" if same else " [AVAILABLE]"
                    attr = ok_attr if same else 0
                    safe_addstr(
                        win,
                        row_y,
                        3,
                        f"{ch_name:<9} : {r.get('short_sha', '?')}  {r.get('date', '')}{status_tag}",
                        attr,
                    )
                    safe_addstr(win, row_y + 1, 15, r.get("message") or "")
                row_y += 2

            safe_addstr(win, 9, 2, "-" * (pw - 4))
            actions = [
                "1. Refresh GitHub status (Stable & unstable)",
                "2. Update to Stable branch",
                "3. Update to unstable branch",
                "Q. Exit",
            ]
            for i, lbl in enumerate(actions):
                attr = sel_attr if i == sel else 0
                safe_addstr(win, 11 + i, 3, (" " + lbl).ljust(pw - 6), attr)

            safe_addstr(win, ph - 2, 3, "Configs in ~/.config/muzuwm are preserved during updates.", accent_attr)
            win.refresh()

            ch = win.getch()
            if ch in (curses.KEY_UP, ord("k")):
                sel = (sel - 1) % len(actions)
            elif ch in (curses.KEY_DOWN, ord("j")):
                sel = (sel + 1) % len(actions)
            elif ch in (ord("q"), ord("Q"), 27):
                break
            elif ch in (ord("r"), ord("R")):
                remote = check_all_channels()
            elif ch in (ord("1"), ord("2"), ord("3")):
                sel = ch - ord("1")
                ch = 10

            if ch in (10, 13, ord(" ")):
                if sel == 0:
                    remote = check_all_channels()
                elif sel in (1, 2):
                    target_ch = "Stable" if sel == 1 else "unstable"
                    if tui_confirm(stdscr, f"UPDATE TO {target_ch.upper()}", f"Clone, build & install '{target_ch}' from GitHub?", has_color):
                        curses.endwin()
                        print("\033[2J\033[H", end="")
                        print(f"=== Updating MUZUwm ({target_ch}) ===")
                        try:
                            perform_update(target_ch, log_fn=print)
                        except Exception as exc:
                            print(f"\nUpdate failed: {exc}", file=sys.stderr)
                        try:
                            input("\nPress Enter to return to MUZUwm Updater...")
                        except EOFError:
                            pass
                        stdscr.refresh()
                        remote = check_all_channels()
                elif sel == 3:
                    break

    curses.wrapper(_tui_main)


def main():
    parser = argparse.ArgumentParser(description="MUZUwm Updater (GitHub Stable & unstable channels)")
    parser.add_argument("--check", "-c", action="store_true", help="Check GitHub Stable and unstable versions and exit")
    parser.add_argument("--update", "-u", metavar="CHANNEL", help="Update directly to Stable or unstable")
    parser.add_argument("--prefix", help="Install prefix override (default: detected or /usr/local)")
    parser.add_argument("--dry-run", action="store_true", help="Fetch and compile without installing")
    parser.add_argument("--yes", "-y", action="store_true", help="Skip confirmation prompt for --update")
    parser.add_argument("--cli", "--plain", action="store_true", help="Use plain text CLI menu")
    args = parser.parse_args()

    if args.check:
        _, remote = print_status_report()
        has_any = any(r.get("sha") for r in remote.values())
        sys.exit(0 if has_any else 1)

    if args.update:
        chan = normalize_channel(args.update)
        if not chan:
            print(f"Error: invalid channel '{args.update}'. Use 'Stable' or 'unstable'.", file=sys.stderr)
            sys.exit(2)
        if not args.yes and not args.dry_run and sys.stdin.isatty():
            ans = input(f"Build and install MUZUwm from GitHub '{chan}'? [y/N]: ").strip().lower()
            if ans not in ("y", "yes"):
                print("Cancelled.")
                return
        try:
            perform_update(chan, prefix=args.prefix, dry_run=args.dry_run, log_fn=print)
        except Exception as exc:
            print(f"Update failed: {exc}", file=sys.stderr)
            sys.exit(1)
        return

    if args.cli:
        run_cli_updater()
        return

    if sys.stdin.isatty() and sys.stdout.isatty():
        try:
            run_tui_updater()
            return
        except Exception:
            pass

    run_cli_updater()


if __name__ == "__main__":
    main()
