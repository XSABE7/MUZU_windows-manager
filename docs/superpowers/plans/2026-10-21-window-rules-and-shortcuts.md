# Window Rules, Allowlists, and Custom Shortcuts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make new windows open floating on the desktop by default unless matched in `fullscreen_allowlist`, gate fullscreen/tiling actions (`Super + Arrow` and mouse snap) on window fullscreen support, replace hardcoded `xterm` autostart with `auto_allowlist`, and support custom keyboard shortcuts (`shortcuts.conf`) with defaults for `wezterm` and `google-chrome`.

**Architecture:** Introduce a dedicated configuration and keybinding module (`src/config_rules.c`, `include/config_rules.h`) that manages `~/.config/muzuwm/fullscreen_allowlist`, `~/.config/muzuwm/auto_allowlist`, and `~/.config/muzuwm/shortcuts.conf`. Extend client property inspection (`src/client_props.c`) and window state (`include/wm.h`, `src/titlebar.c`, `src/main.c`) with a `can_fullscreen` capability flag derived from ICCCM `WM_NORMAL_HINTS` and dialog types so non-fullscreen windows ignore tiling/fullscreen triggers.

**Tech Stack:** C11, Xlib (`X11/Xlib.h`, `X11/Xutil.h`, `X11/keysym.h`), Cairo, POSIX (`fork`, `setsid`, `execl`).

**Spec:** Approved inline design in conversation (`2026-10-21`).

## Global Constraints

- Compiler flags must remain `-Wall -Wextra -pedantic -std=c11 -O2` with zero warnings.
- Config files live under `$HOME/.config/muzuwm/`: `fullscreen_allowlist`, `auto_allowlist`, and `shortcuts.conf`.
- Default shortcuts in `shortcuts.conf`: `Super+Left = tile_left`, `Super+Right = tile_right`, `Super+Up = fullscreen`, `Super+Down = float`, `Super+Return = exec wezterm`, `Super+b = exec google-chrome`.
- `run-xephyr.sh` must not auto-spawn `xterm` processes.

## Review Focus

- Empty or whitespace-only assignments (`=tile_left`, `Super+Up=`, `#comment`) in `shortcuts.conf`, `fullscreen_allowlist`, and `auto_allowlist` must not underflow pointers or match empty substrings against every window.
- Active `NumLock` (`Mod2Mask`) or `CapsLock` (`LockMask`) in X11 `KeyPress` state must be masked out before matching `ShortcutBinding` modifiers so `Super+Arrow` and `Super+Return` work regardless of lock keys.
- Fixed-size windows (`PMinSize | PMaxSize` with `min_width == max_width && min_height == max_height`) and dialogs (`can_fullscreen == 0`) must ignore `client_toggle_maximize`, `handle_titlebar_double_click`, `Super + Arrow` tiling, and mouse edge-snap on `ButtonRelease`.
- Unmaximizing or pressing `Super + Down` (`float`) on a window that started auto-maximized (where `saved_width` or `saved_height` may be uninitialized or full-screen) must restore a sensible centered floating size rather than keeping fullscreen dimensions.
- `fullscreen_allowlist` matching must require a non-empty pattern and match case-insensitively against `WM_CLASS` (`res_name`, `res_class`) or window title.

---

### Task 1: Client Fullscreen Support Detection & Guarding

**Files:**
- Modify: `include/wm.h:20-43`
- Modify: `src/wm.c:4-36`
- Modify: `include/client_props.h:27-42`
- Modify: `src/client_props.c:175-185`
- Modify: `src/titlebar.c:183-295`
- Modify: `tests/test_client_props.c:85-118`
- Modify: `tests/test_layout.c:182-203`

**Interfaces:**
- Produces:
  - `Client.can_fullscreen` (`int`, initialized to `1` in `client_add`) in `include/wm.h`
  - `int client_size_hints_allow_fullscreen(long flags, int min_w, int min_h, int max_w, int max_h, int is_dialog)` in `include/client_props.h`
  - `int client_supports_fullscreen(Display *dpy, Window win)` in `include/client_props.h`
  - Guarded `client_toggle_maximize(Display *dpy, Client *c, LayoutConfig *cfg)` and `handle_titlebar_double_click(Display *dpy, Client *c, LayoutConfig *cfg)` in `src/titlebar.c`

- [ ] **Step 1: Write failing tests in `tests/test_client_props.c` and `tests/test_layout.c`**

Add to `tests/test_client_props.c`:
```c
    // Test 6: Fullscreen / resize capability detection from WM_NORMAL_HINTS and dialog state
    // Normal resizable window -> allowed (1)
    assert(client_size_hints_allow_fullscreen(PMinSize, 200, 150, 0, 0, 0) == 1);
    // Dialog window -> disallowed (0)
    assert(client_size_hints_allow_fullscreen(0, 0, 0, 0, 0, 1) == 0);
    // Fixed-size window (min == max > 0) -> disallowed (0)
    assert(client_size_hints_allow_fullscreen(PMinSize | PMaxSize, 400, 300, 400, 300, 0) == 0);
    // Small max-size window (< 800x600 cap) -> disallowed (0)
    assert(client_size_hints_allow_fullscreen(PMaxSize, 100, 100, 500, 400, 0) == 0);
    // Large max-size window (>= 800x600) -> allowed (1)
    assert(client_size_hints_allow_fullscreen(PMinSize | PMaxSize, 200, 150, 1920, 1080, 0) == 1);
    // Null display safety
    assert(client_supports_fullscreen(NULL, None) == 1);
```

Add to `tests/test_layout.c` (in Test 10 after line 201):
```c
    // Non-fullscreen client must ignore maximize and double-click tiling
    Client fixed_client = {0};
    fixed_client.x = 150;
    fixed_client.y = 120;
    fixed_client.width = 400;
    fixed_client.height = 300;
    fixed_client.mode = CLIENT_FLOATING;
    fixed_client.can_fullscreen = 0;

    client_toggle_maximize(NULL, &fixed_client, &cfg);
    assert(fixed_client.is_maximized == 0);
    assert(fixed_client.width == 400 && fixed_client.height == 300);

    handle_titlebar_double_click(NULL, &fixed_client, &cfg);
    assert(fixed_client.mode == CLIENT_FLOATING);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `make test_client_props test_layout && ./test_client_props && ./test_layout`
Expected: FAIL with implicit declaration of `client_size_hints_allow_fullscreen` / `can_fullscreen` not a member of `Client`.

- [ ] **Step 3: Implement `can_fullscreen` and hints checks in `include/wm.h`, `src/wm.c`, `include/client_props.h`, `src/client_props.c`, and `src/titlebar.c`**

1. In `include/wm.h`, add `int can_fullscreen;` to `struct Client`.
2. In `src/wm.c` (`client_add`), set `c->can_fullscreen = 1;`.
3. In `include/client_props.h` and `src/client_props.c`, declare and implement:
   - `int client_size_hints_allow_fullscreen(long flags, int min_w, int min_h, int max_w, int max_h, int is_dialog)`: returns `0` if `is_dialog`, or if `(flags & PMinSize) && (flags & PMaxSize) && max_w > 0 && max_h > 0 && min_w == max_w && min_h == max_h`, or if `(flags & PMaxSize) && max_w > 0 && max_h > 0 && (max_w < 800 || max_h < 600)`; otherwise `1`.
   - `int client_supports_fullscreen(Display *dpy, Window win)`: returns `1` if `!dpy || win == None`; checks `is_dialog = client_is_dialog(dpy, win)` and `XGetWMNormalHints(dpy, win, &hints, &supplied)` and delegates to `client_size_hints_allow_fullscreen`.
4. In `src/titlebar.c`:
   - In `client_toggle_maximize`: if `!c || !cfg || !c->can_fullscreen` (when `!c->is_maximized`), return immediately. Also when unmaximizing (`c->is_maximized == 1`), if `c->saved_width <= 0 || c->saved_width >= cfg->screen_width || c->saved_height <= 0 || c->saved_height >= cfg->screen_height - cfg->top_inset`, default restored geometry to centered `cfg->screen_width * 3 / 5` by `cfg->screen_height * 3 / 5`.
   - In `handle_titlebar_double_click`: if `!c || !c->can_fullscreen`, return immediately.

- [ ] **Step 4: Run tests to verify they pass**

Run: `make test_client_props test_layout && ./test_client_props && ./test_layout`
Expected: PASS (`test_client_props passed!` and `test_layout passed!`).

- [ ] **Step 5: Commit**

```bash
git add include/wm.h src/wm.c include/client_props.h src/client_props.c src/titlebar.c tests/test_client_props.c tests/test_layout.c
git commit -m "feat(client): detect fullscreen support from size hints and guard maximize/tile"
```

---

### Task 2: Config Rules Module (`fullscreen_allowlist`, `auto_allowlist`, and `shortcuts.conf`)

**Files:**
- Create: `include/config_rules.h`
- Create: `src/config_rules.c`
- Create: `tests/test_config_rules.c`
- Modify: `Makefile:6-48`
- Modify: `tests/test_runner.sh:10-20`

**Interfaces:**
- Produces in `include/config_rules.h`:
  ```c
  #ifndef MUZU_CONFIG_RULES_H
  #define MUZU_CONFIG_RULES_H

  #include <X11/Xlib.h>

  #define MAX_SHORTCUTS 64
  #define MAX_AUTOSTART_CMDS 64

  typedef enum {
      SHORTCUT_ACT_NONE = 0,
      SHORTCUT_ACT_TILE_LEFT,
      SHORTCUT_ACT_TILE_RIGHT,
      SHORTCUT_ACT_FULLSCREEN,
      SHORTCUT_ACT_FLOAT,
      SHORTCUT_ACT_CLOSE,
      SHORTCUT_ACT_LAUNCHER,
      SHORTCUT_ACT_EXEC
  } ShortcutActionType;

  typedef struct {
      unsigned int mod_mask;
      KeySym keysym;
      ShortcutActionType type;
      char exec_cmd[256];
  } ShortcutBinding;

  typedef struct {
      ShortcutBinding bindings[MAX_SHORTCUTS];
      int count;
  } ShortcutConfig;

  void config_rules_ensure_files(void);
  int config_is_in_fullscreen_allowlist_file(const char *path, const char *res_name, const char *res_class, const char *title);
  int config_client_in_fullscreen_allowlist(Display *dpy, Window win, const char *title);
  int config_load_auto_allowlist_file(const char *path, char out_cmds[][256], int max_cmds);
  void config_run_autostart(void);

  void shortcuts_init_defaults(ShortcutConfig *sc);
  void shortcuts_load_file(ShortcutConfig *sc, const char *path);
  void shortcuts_load(ShortcutConfig *sc);
  void shortcuts_grab_keys(Display *dpy, Window root, const ShortcutConfig *sc);
  const ShortcutBinding *shortcuts_match_event(const ShortcutConfig *sc, unsigned int state, KeySym sym);

  #endif
  ```

- [ ] **Step 1: Write failing test `tests/test_config_rules.c` and register in `Makefile` and `tests/test_runner.sh`**

Create `tests/test_config_rules.c`:
```c
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "config_rules.h"

int main(void) {
    char tmp_dir[] = "/tmp/muzuwm_test_rules_XXXXXX";
    assert(mkdtemp(tmp_dir) != NULL);
    const char *orig_home = getenv("HOME");
    setenv("HOME", tmp_dir, 1);

    // 1. Ensure default config files are created
    config_rules_ensure_files();

    char fs_path[512], auto_path[512], sc_path[512];
    snprintf(fs_path, sizeof(fs_path), "%s/.config/muzuwm/fullscreen_allowlist", tmp_dir);
    snprintf(auto_path, sizeof(auto_path), "%s/.config/muzuwm/auto_allowlist", tmp_dir);
    snprintf(sc_path, sizeof(sc_path), "%s/.config/muzuwm/shortcuts.conf", tmp_dir);

    assert(access(fs_path, R_OK) == 0);
    assert(access(auto_path, R_OK) == 0);
    assert(access(sc_path, R_OK) == 0);

    // Default fullscreen_allowlist is empty (no comments matching apps)
    assert(config_is_in_fullscreen_allowlist_file(fs_path, "wezterm", "WezTerm", "wezterm") == 0);

    // Populate fullscreen_allowlist with comments, blank lines, and entries
    FILE *f = fopen(fs_path, "w");
    assert(f != NULL);
    fprintf(f, "# Apps that open fullscreen automatically\n\n   \ngoogle-chrome\n  mpv  \n");
    fclose(f);

    assert(config_is_in_fullscreen_allowlist_file(fs_path, "google-chrome", "Google-chrome", "New Tab") == 1);
    assert(config_is_in_fullscreen_allowlist_file(fs_path, "gl", "mpv", "video.mkv") == 1);
    assert(config_is_in_fullscreen_allowlist_file(fs_path, "wezterm", "WezTerm", "Terminal") == 0);
    assert(config_is_in_fullscreen_allowlist_file(fs_path, "", "", "") == 0);

    // 2. Test auto_allowlist loading
    char cmds[MAX_AUTOSTART_CMDS][256];
    assert(config_load_auto_allowlist_file(auto_path, cmds, MAX_AUTOSTART_CMDS) == 0);

    f = fopen(auto_path, "w");
    assert(f != NULL);
    fprintf(f, "# Startup programs\n\n  wezterm  \ngoogle-chrome --restore-last-session\n");
    fclose(f);

    int n = config_load_auto_allowlist_file(auto_path, cmds, MAX_AUTOSTART_CMDS);
    assert(n == 2);
    assert(strcmp(cmds[0], "wezterm") == 0);
    assert(strcmp(cmds[1], "google-chrome --restore-last-session") == 0);

    // 3. Test default shortcuts (Super+Arrow tiling, wezterm, google-chrome)
    ShortcutConfig sc = {0};
    shortcuts_load(&sc);
    assert(sc.count >= 6);

    const ShortcutBinding *b_left = shortcuts_match_event(&sc, Mod4Mask, XK_Left);
    assert(b_left && b_left->type == SHORTCUT_ACT_TILE_LEFT);

    // Must also match when NumLock (Mod2Mask) or CapsLock (LockMask) is active
    const ShortcutBinding *b_right = shortcuts_match_event(&sc, Mod4Mask | Mod2Mask | LockMask, XK_Right);
    assert(b_right && b_right->type == SHORTCUT_ACT_TILE_RIGHT);

    const ShortcutBinding *b_up = shortcuts_match_event(&sc, Mod4Mask, XK_Up);
    assert(b_up && b_up->type == SHORTCUT_ACT_FULLSCREEN);

    const ShortcutBinding *b_down = shortcuts_match_event(&sc, Mod4Mask, XK_Down);
    assert(b_down && b_down->type == SHORTCUT_ACT_FLOAT);

    const ShortcutBinding *b_term = shortcuts_match_event(&sc, Mod4Mask, XK_Return);
    assert(b_term && b_term->type == SHORTCUT_ACT_EXEC);
    assert(strcmp(b_term->exec_cmd, "wezterm") == 0);

    const ShortcutBinding *b_browser = shortcuts_match_event(&sc, Mod4Mask, XK_b);
    assert(b_browser && b_browser->type == SHORTCUT_ACT_EXEC);
    assert(strstr(b_browser->exec_cmd, "chrome") != NULL);

    // 4. Test malformed / empty lines in shortcuts.conf
    f = fopen(sc_path, "w");
    assert(f != NULL);
    fprintf(f, "=tile_left\nSuper+Up=\n=\n   =   \nCtrl+Alt+t = exec wezterm\nSuper+q = close_window\n");
    fclose(f);

    shortcuts_load_file(&sc, sc_path);
    const ShortcutBinding *b_custom = shortcuts_match_event(&sc, ControlMask | Mod1Mask, XK_t);
    assert(b_custom && b_custom->type == SHORTCUT_ACT_EXEC);
    assert(strcmp(b_custom->exec_cmd, "wezterm") == 0);

    const ShortcutBinding *b_close = shortcuts_match_event(&sc, Mod4Mask, XK_q);
    assert(b_close && b_close->type == SHORTCUT_ACT_CLOSE);

    if (orig_home) setenv("HOME", orig_home, 1);
    char rm_cmd[600];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", tmp_dir);
    assert(system(rm_cmd) == 0);

    printf("test_config_rules passed!\n");
    return 0;
}
```

- [ ] **Step 2: Run `make test_config_rules` to verify it fails**

Run: `make test_config_rules`
Expected: FAIL (`config_rules.h: No such file or directory`).

- [ ] **Step 3: Implement `include/config_rules.h` and `src/config_rules.c`**

- `config_rules_ensure_files()`: creates `~/.config/muzuwm/fullscreen_allowlist` (commented template), `~/.config/muzuwm/auto_allowlist` (commented template), and `~/.config/muzuwm/shortcuts.conf` with defaults:
  - `Super+Left = tile_left`
  - `Super+Right = tile_right`
  - `Super+Up = fullscreen`
  - `Super+Down = float`
  - `Super+Return = exec wezterm`
  - `Super+b = exec google-chrome`
  - `Super+q = close_window`
- `shortcuts_load_file`: parses `MOD+KEY = ACTION` where modifiers support `Super`/`Mod4`, `Alt`/`Mod1`, `Ctrl`/`Control`, `Shift`, and key names are resolved via `XStringToKeysym` (plus case-insensitive aliases `Enter` -> `XK_Return`, `Esc` -> `XK_Escape`, `Space` -> `XK_space`, `Left` -> `XK_Left`, `Right` -> `XK_Right`, `Up` -> `XK_Up`, `Down` -> `XK_Down`). Updating an existing `(mod_mask, keysym)` binding overrides it in-place; new combinations append up to `MAX_SHORTCUTS`.
- `shortcuts_match_event`: masks `state & (Mod1Mask | Mod4Mask | ControlMask | ShiftMask)` and compares `keysym` (also matching lowercase `XK_a..XK_z` against `XK_A..XK_Z`).
- `shortcuts_grab_keys`: grabs each configured key on `root` with `0`, `LockMask`, `Mod2Mask`, and `LockMask | Mod2Mask` so shortcuts fire reliably regardless of CapsLock/NumLock state.

- [ ] **Step 4: Run `make test_config_rules && ./test_config_rules` to verify it passes**

Run: `make test_config_rules && ./test_config_rules`
Expected: PASS (`test_config_rules passed!`).

- [ ] **Step 5: Commit**

```bash
git add include/config_rules.h src/config_rules.c tests/test_config_rules.c Makefile tests/test_runner.sh
git commit -m "feat(config): add fullscreen_allowlist, auto_allowlist, and shortcuts.conf parser"
```

---

### Task 3: Integrate Floating-by-Default, Allowlists, and Shortcuts into WM Loop & `run-xephyr.sh`

**Files:**
- Modify: `src/main.c:20-35, 650-900, 950-1000, 1465-1500`
- Modify: `run-xephyr.sh:25-46`

**Interfaces:**
- Consumes:
  - `client_supports_fullscreen` from `include/client_props.h`
  - `config_rules_ensure_files`, `config_client_in_fullscreen_allowlist`, `config_run_autostart`, `ShortcutConfig`, `shortcuts_load`, `shortcuts_grab_keys`, `shortcuts_match_event` from `include/config_rules.h`

- [ ] **Step 1: Update `src/main.c` and `run-xephyr.sh`**

1. In `src/main.c`:
   - Include `"config_rules.h"`.
   - During startup in `main()`:
     - Call `config_rules_ensure_files();`
     - Load `ShortcutConfig shortcuts_cfg = {0}; shortcuts_load(&shortcuts_cfg);`
     - Call `shortcuts_grab_keys(dpy, root, &shortcuts_cfg);`
     - Call `config_run_autostart();` after `compositor_init(dpy);`.
   - In `case MapRequest:` (`src/main.c`):
     - Compute `int can_fs = client_supports_fullscreen(dpy, w);`
     - Compute `int is_csd = client_is_csd(dpy, w);`
     - Compute centered floating initial geometry via `client_calculate_dialog_geometry(screen_w, screen_h, win_w, win_h, &init_x, &init_y, &win_w, &win_h)` (clamping `init_y >= cfg.top_inset`).
     - Add client with `CLIENT_FLOATING` by default (`client_add(&clients, w, CLIENT_FLOATING)`), setting `c->can_fullscreen = can_fs`.
     - After resolving `c->title`, check `if (can_fs && config_client_in_fullscreen_allowlist(dpy, w, c->title))`: call `client_toggle_maximize(dpy, c, &cfg);`.
   - In `case KeyPress:` (`src/main.c`):
     - Match event via `const ShortcutBinding *b = shortcuts_match_event(&shortcuts_cfg, ev.xkey.state, sym);`
     - Handle actions:
       - `SHORTCUT_ACT_TILE_LEFT`: if `focused_client && focused_client->can_fullscreen`, set `focused_client->is_maximized = 0; focused_client->mode = CLIENT_TILED; focused_client->snap_state = SNAP_LEFT; reflow_all(dpy, clients, &cfg);`
       - `SHORTCUT_ACT_TILE_RIGHT`: if `focused_client && focused_client->can_fullscreen`, set `focused_client->is_maximized = 0; focused_client->mode = CLIENT_TILED; focused_client->snap_state = SNAP_RIGHT; reflow_all(dpy, clients, &cfg);`
       - `SHORTCUT_ACT_FULLSCREEN`: if `focused_client && focused_client->can_fullscreen`, toggle maximize via `client_toggle_maximize(dpy, focused_client, &cfg);` and redraw titlebar / reflow.
       - `SHORTCUT_ACT_FLOAT`: if `focused_client`, set `focused_client->is_maximized = 0; focused_client->mode = CLIENT_FLOATING; focused_client->snap_state = SNAP_NONE;`, restore centered floating dimensions if currently full-width/tiled, `XMoveResizeWindow`, redraw titlebar, and `reflow_all(dpy, clients, &cfg);`.
       - `SHORTCUT_ACT_CLOSE`: if `focused_client`, call `client_close(dpy, focused_client->win);`.
       - `SHORTCUT_ACT_LAUNCHER`: call `rofi_launcher_toggle(dpy, &rofi_launcher);`.
       - `SHORTCUT_ACT_EXEC`: fork and execute `b->exec_cmd` via `/bin/sh -c`.
   - In `case ButtonRelease:` (`src/main.c` mouse edge-snap):
     - Only snap to top/left/right screen edges if `drag_client->can_fullscreen` is non-zero; otherwise leave `drag_client` floating at its dropped position.
2. In `run-xephyr.sh`:
   - Remove the automatic `xterm` spawns and update the banner instructions to mention `~/.config/muzuwm/auto_allowlist`, `~/.config/muzuwm/fullscreen_allowlist`, and `~/.config/muzuwm/shortcuts.conf` (`Super+Return` for `wezterm`, `Super+b` for Chrome, `Super+Arrow` to tile/fullscreen).

- [ ] **Step 2: Build and run full test suite**

Run: `./tests/test_runner.sh`
Expected: All 11 unit test suites pass with zero compiler warnings and memory under 200 MB.

- [ ] **Step 3: Verify live in Xephyr**

Run a live Xephyr verification checking:
- Startup spawns zero `xterm` windows when `auto_allowlist` is empty.
- A normal window opens floating (centered on the desktop) by default instead of full screen.
- Pressing `Super + Left`, `Super + Right`, `Super + Up`, and `Super + Down` tiles left, tiles right, maximizes, and restores to floating.
- A fixed-size window (`min_width == max_width && min_height == max_height`) stays floating and ignores `Super + Up` / `Super + Left`.

- [ ] **Step 4: Commit**

```bash
git add src/main.c run-xephyr.sh
git commit -m "feat(wm): float windows by default, enforce allowlists, and bind custom shortcuts"
```
