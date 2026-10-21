# Code Review Fixes and Dock Top Removal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Address all 7 review findings across compositor, dock, rofi launcher, tint2 panel, and test suites, and disable the dock top position to permanently eliminate overlap with the top panel.

**Architecture:**
1. Modular Path & Identity Detection: Expose explicit helpers `compositor_find_picom` and `dock_setup_ewmh` to enable clean testing and deterministic execution of user binaries and EWMH hints without environment pollution.
2. Geometry & Edge Constraints: Constrain dock positions to Bottom, Left, and Right (removing Top), migrating legacy top-dock configurations to bottom to guarantee zero overlap with the tint2 top panel (`y = 0..28`).
3. Resource Management & Parser Robustness: Cleanly dispose of cached Cairo surfaces upon launcher re-scanning, safely guard string trimming against empty keys/values in parsers, and free all client linked lists in unit tests to make AddressSanitizer a 100% clean gate.

**Tech Stack:** C11, Xlib (X11), Cairo Graphics (cairo-xlib), GdkPixbuf, Picom Compositor, AddressSanitizer.

**Spec:** `docs/superpowers/specs/2026-10-21-code-review-fixes.md`

## Global Constraints

- Must compile cleanly with `gcc -Wall -Wextra -pedantic -std=c11 -O2` with zero warnings.
- Memory consumption must remain strictly under the 200 MB RSS budget ceiling.
- Zero memory leaks under AddressSanitizer (`-fsanitize=address`).
- All unit tests in `make tests` must pass cleanly without failures.

## Review Focus

- Picom fallback path: Detection must find `$HOME/.local/bin/picom` even if `$HOME/.local/bin` is absent from `$PATH`.
- Top dock config fallback: An existing user config file specifying `pos = top` or `DOCK_TOP` must safely fall back to `DOCK_BOTTOM`.
- Dock EWMH atom setting: Window property `_NET_WM_WINDOW_TYPE` must be set to `_NET_WM_WINDOW_TYPE_DOCK` and name to `"MUZUwm Dock"`.
- Rofi re-scan idempotence: Calling `rofi_launcher_scan_apps()` multiple consecutive times must not leak cached `icon_surface` memory.
- Malformed config safety: Config lines formatted as `=foo`, `foo=`, `:bar`, or whitespace-only must not produce negative pointer arithmetic.

---

### Task 1: Picom ~/.local/bin Detection & Launch

**Files:**
- Modify: `include/compositor.h:1-12`
- Modify: `src/compositor.c:150-190`
- Test: `tests/test_compositor.c`

**Interfaces:**
- Consumes: `int compositor_find_picom(char *out_bin, size_t out_len)`
- Produces: `compositor_find_picom` returns 1 and populates the executable path if picom exists in `$HOME/.local/bin`, `/usr/bin`, `/usr/local/bin`, or `$PATH`.

- [ ] **Step 1: Write the failing test**

In `tests/test_compositor.c`, declare and test `compositor_find_picom`:

```c
// In tests/test_compositor.c
char found_bin[1024];
system("mkdir -p /tmp/muzuwm_test_home/.local/bin");
system("touch /tmp/muzuwm_test_home/.local/bin/picom && chmod +x /tmp/muzuwm_test_home/.local/bin/picom");

const char *orig_home = getenv("HOME");
setenv("HOME", "/tmp/muzuwm_test_home", 1);
const char *orig_path = getenv("PATH");
setenv("PATH", "/usr/bin:/bin", 1); // exclude ~/.local/bin from PATH

int found = compositor_find_picom(found_bin, sizeof(found_bin));
assert(found == 1);
assert(strstr(found_bin, "/tmp/muzuwm_test_home/.local/bin/picom") != NULL);

// Restore environment
if (orig_home) setenv("HOME", orig_home, 1);
if (orig_path) setenv("PATH", orig_path, 1);
system("rm -rf /tmp/muzuwm_test_home");
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_compositor`
Expected: FAIL with undefined reference or implicit declaration of `compositor_find_picom`.

- [ ] **Step 3: Implement `compositor_find_picom` and update `compositor_init`**

In `include/compositor.h`:
```c
int compositor_find_picom(char *out_bin, size_t out_len);
```

In `src/compositor.c`:
Implement `compositor_find_picom(char *out_bin, size_t out_len)` checking:
1. `$HOME/.local/bin/picom`
2. `/usr/bin/picom`
3. `/usr/local/bin/picom`
4. Directory tokens in `$PATH`
And in `compositor_init(Display *dpy)`:
Use `char picom_bin[1024]; if (!compositor_find_picom(picom_bin, sizeof(picom_bin))) return;`
When fork child runs: `execl(picom_bin, "picom", "--config", compositor_get_config_path(), NULL);`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_compositor && ./test_compositor`
Expected: PASS for the picom detection assertion.

- [ ] **Step 5: Commit**

```bash
git add include/compositor.h src/compositor.c tests/test_compositor.c
git commit -m "fix(compositor): support picom in ~/.local/bin with full path execution"
```

---

### Task 2: Deterministic test_compositor with Temporary Isolation

**Files:**
- Modify: `tests/test_compositor.c`

**Interfaces:**
- Consumes: `compositor_init(NULL)`, `compositor_get_config_path()`, `compositor_cleanup()`
- Produces: A deterministic unit test that creates a temporary isolated `$HOME`, asserts auto-generated configuration, and cleans up.

- [ ] **Step 1: Write the isolated test in `tests/test_compositor.c`**

Replace the main function in `tests/test_compositor.c` to isolate `$HOME`:

```c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "compositor.h"

int main(void) {
    char tmp_dir[] = "/tmp/muzuwm_test_compositor_XXXXXX";
    char *created = mkdtemp(tmp_dir);
    assert(created != NULL);

    const char *orig_home = getenv("HOME");
    setenv("HOME", tmp_dir, 1);

    const char *cfg_path = compositor_get_config_path();
    assert(cfg_path != NULL);
    assert(strstr(cfg_path, "picom.conf") != NULL);

    // Initialize with NULL display (should generate config safely and not crash)
    compositor_init(NULL);

    // Verify config file was written
    FILE *f = fopen(cfg_path, "r");
    assert(f != NULL);

    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    assert(strstr(buf, "dual_kawase") != NULL);
    assert(strstr(buf, "glx") != NULL);
    assert(strstr(buf, "strength = 5") != NULL);
    assert(strstr(buf, "vsync = true") != NULL);

    compositor_cleanup();

    // Verify ~/.local/bin lookup helper
    char mock_bin_dir[512];
    snprintf(mock_bin_dir, sizeof(mock_bin_dir), "%s/.local/bin", tmp_dir);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s && touch %s/picom && chmod +x %s/picom", mock_bin_dir, mock_bin_dir, mock_bin_dir);
    system(cmd);

    char found_bin[1024];
    const char *orig_path = getenv("PATH");
    setenv("PATH", "/usr/bin:/bin", 1);
    assert(compositor_find_picom(found_bin, sizeof(found_bin)) == 1);
    assert(strstr(found_bin, "/.local/bin/picom") != NULL);

    if (orig_path) setenv("PATH", orig_path, 1);
    if (orig_home) setenv("HOME", orig_home, 1);

    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp_dir);
    system(cmd);

    printf("test_compositor passed!\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it passes deterministically**

Run: `make test_compositor && ./test_compositor`
Expected: PASS with output `test_compositor passed!`.

- [ ] **Step 3: Commit**

```bash
git add tests/test_compositor.c
git commit -m "test(compositor): isolate HOME directory for deterministic assertions"
```

---

### Task 3: Disable Dock Top Position & Fallback Migration

**Files:**
- Modify: `include/dock.h`
- Modify: `src/dock.c`
- Modify: `src/main.c`
- Test: `tests/test_layout.c`

**Interfaces:**
- Consumes: `dock_cycle_position(Display *dpy, Dock *dock, int screen_w, int screen_h)`, `dock_calculate_nearest_edge(int center_x, int center_y, int screen_w, int screen_h)`
- Produces: `dock_cycle_position` cycles Bottom -> Left -> Right -> Bottom; `dock_calculate_nearest_edge` only evaluates Bottom, Left, and Right edges; any position set to `DOCK_TOP` falls back to `DOCK_BOTTOM`.

- [ ] **Step 1: Write the failing test**

In `tests/test_layout.c`, add tests verifying:
1. `dock_cycle_position` cycles `DOCK_BOTTOM` -> `DOCK_LEFT` -> `DOCK_RIGHT` -> `DOCK_BOTTOM` (never `DOCK_TOP`).
2. `dock_calculate_nearest_edge` with coordinates closest to top (e.g. x=960, y=10) selects a non-top position (`DOCK_BOTTOM`, `DOCK_LEFT`, or `DOCK_RIGHT`, never `DOCK_TOP`).
3. Geometry calculation with `dock->pos == DOCK_TOP` forces position to `DOCK_BOTTOM`.

```c
// In tests/test_layout.c
Dock d;
memset(&d, 0, sizeof(d));
d.size = 48;
d.pos = DOCK_BOTTOM;

dock_cycle_position(NULL, &d, 1920, 1080);
assert(d.pos == DOCK_LEFT);
dock_cycle_position(NULL, &d, 1920, 1080);
assert(d.pos == DOCK_RIGHT);
dock_cycle_position(NULL, &d, 1920, 1080);
assert(d.pos == DOCK_BOTTOM);

DockPosition nearest = dock_calculate_nearest_edge(960, 10, 1920, 1080);
assert(nearest != DOCK_TOP);

d.pos = DOCK_TOP;
dock_calculate_geometry(&d, d.pos, 0, 1920, 1080);
assert(d.pos == DOCK_BOTTOM);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_layout && ./test_layout`
Expected: FAIL on `assert(d.pos == DOCK_RIGHT)` because `DOCK_LEFT` currently cycles to `DOCK_TOP`.

- [ ] **Step 3: Implement Dock Top position removal and fallback**

1. In `src/dock.c`:
   - In `dock_calculate_geometry`: If `pos == DOCK_TOP` or `dock->pos == DOCK_TOP`, remap `pos = DOCK_BOTTOM` and `dock->pos = DOCK_BOTTOM`.
   - In `dock_cycle_position`:
     ```c
     switch (dock->pos) {
         case DOCK_BOTTOM: next_pos = DOCK_LEFT; break;
         case DOCK_LEFT:   next_pos = DOCK_RIGHT; break;
         case DOCK_RIGHT:  next_pos = DOCK_BOTTOM; break;
         default:          next_pos = DOCK_BOTTOM; break;
     }
     ```
   - In `dock_calculate_nearest_edge`: Remove `dist_top` check; compare only `dist_bottom`, `dist_left`, and `dist_right`.
   - In `dock_load_settings`: If loaded position string is `"top"`, set `dock->pos = DOCK_BOTTOM`.
2. In `src/main.c`:
   - Remove `"Position: Top"` from the dock context menu.
   - In `dock_action_execute`: map `DOCK_ACTION_POS_TOP` to `DOCK_BOTTOM` or no-op.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_layout && ./test_layout`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/dock.c src/main.c tests/test_layout.c
git commit -m "fix(dock): disable top dock position and cycle between bottom, left, and right"
```

---

### Task 4: Restore Dock EWMH Identity & Exclusions

**Files:**
- Modify: `include/dock.h`
- Modify: `src/dock.c:744-775`
- Test: `tests/test_client_props.c`

**Interfaces:**
- Consumes: `void dock_setup_ewmh(Display *dpy, Window win)`
- Produces: Assigns `_NET_WM_WINDOW_TYPE_DOCK` to `_NET_WM_WINDOW_TYPE`, sets `_NET_WM_NAME` to `"MUZUwm Dock"`, and sets `WM_NAME` via `XStoreName`.

- [ ] **Step 1: Write the failing test**

In `tests/test_client_props.c`, test dock EWMH helper logic or verify signature:

```c
// In tests/test_client_props.c
// Test dock setup helper signature and window property setting
#include "dock.h"
// Verify dock atom function is defined and callable
dock_setup_ewmh(NULL, 0);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_client_props`
Expected: FAIL with implicit declaration or undefined reference to `dock_setup_ewmh`.

- [ ] **Step 3: Implement `dock_setup_ewmh` and invoke in `dock_create`**

In `include/dock.h`:
```c
void dock_setup_ewmh(Display *dpy, Window win);
```

In `src/dock.c`:
```c
void dock_setup_ewmh(Display *dpy, Window win) {
    if (!dpy || win == 0) return;

    XStoreName(dpy, win, "MUZUwm Dock");

    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    const char *dock_name = "MUZUwm Dock";
    XChangeProperty(dpy, win, net_wm_name, utf8_string, 8,
                    PropModeReplace, (const unsigned char *)dock_name, (int)strlen(dock_name));

    Atom net_wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(dpy, win, net_wm_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&net_wm_dock, 1);
}
```
In `dock_create(Display *dpy, ...)`:
Call `dock_setup_ewmh(dpy, dock->win);` immediately after `XCreateWindow`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_client_props test_layout && ./test_client_props && ./test_layout`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/dock.h src/dock.c tests/test_client_props.c
git commit -m "fix(dock): restore EWMH dock window type and name properties"
```

---

### Task 5: Fix Icon Surface Leak in Rofi Launcher Re-scan

**Files:**
- Modify: `src/rofi_launcher.c:187-205`
- Test: `tests/test_rofi_launcher.c`

**Interfaces:**
- Consumes: `rofi_launcher_scan_apps(RofiLauncher *rofi)`
- Produces: Fully frees any previously cached `icon_surface` before resetting `rofi->app_count = 0`.

- [ ] **Step 1: Write the failing test**

In `tests/test_rofi_launcher.c`, add a test that populates mock icon surfaces and calls `rofi_launcher_scan_apps`:

```c
// In tests/test_rofi_launcher.c
RofiLauncher mock_rofi;
memset(&mock_rofi, 0, sizeof(mock_rofi));
mock_rofi.app_count = 2;
mock_rofi.apps[0].icon_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 16, 16);
mock_rofi.apps[1].icon_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 16, 16);

// Call scan_apps which must destroy and reset old icon_surfaces without leaking
rofi_launcher_scan_apps(&mock_rofi);
// After rescan, surfaces must be newly loaded or NULL, old pointers safely finished
for (int i = 0; i < mock_rofi.app_count; i++) {
    if (mock_rofi.apps[i].icon_surface) {
        cairo_surface_finish(mock_rofi.apps[i].icon_surface);
        cairo_surface_destroy(mock_rofi.apps[i].icon_surface);
        mock_rofi.apps[i].icon_surface = NULL;
    }
}
```

- [ ] **Step 2: Run test under AddressSanitizer or check leak**

Run: `make test_rofi_launcher && ./test_rofi_launcher`

- [ ] **Step 3: Implement clean icon destruction in `rofi_launcher_scan_apps`**

In `src/rofi_launcher.c` (`rofi_launcher_scan_apps`):
```c
void rofi_launcher_scan_apps(RofiLauncher *rofi) {
    if (!rofi) return;

    for (int i = 0; i < rofi->app_count; i++) {
        if (rofi->apps[i].icon_surface) {
            cairo_surface_finish(rofi->apps[i].icon_surface);
            cairo_surface_destroy(rofi->apps[i].icon_surface);
            rofi->apps[i].icon_surface = NULL;
        }
    }
    rofi->app_count = 0;
    ...
```

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_rofi_launcher && ./test_rofi_launcher`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/rofi_launcher.c tests/test_rofi_launcher.c
git commit -m "fix(rofi): destroy cached icon surfaces before re-scanning applications"
```

---

### Task 6: Guard Against Negative Pointer Arithmetic in Config Parsers

**Files:**
- Modify: `src/tint2_panel.c:135-150`
- Modify: `src/rofi_launcher.c:435-450`
- Test: `tests/test_tint2_panel.c`, `tests/test_rofi_launcher.c`

**Interfaces:**
- Consumes: Config file streams containing empty keys or empty values
- Produces: Safe trimming using `strlen > 0` checks before pointer subtraction.

- [ ] **Step 1: Write the failing test**

In `tests/test_tint2_panel.c`:
Write a test file `/tmp/test_empty_tint2.conf` containing:
```
=bar
foo=
=
   =   
valid_key = valid_val
```
Load the config with `tint2_panel_load_config(&panel, "/tmp/test_empty_tint2.conf")` and assert no crash.

In `tests/test_rofi_launcher.c`:
Write a test file `/tmp/test_empty_rofi.rasi` containing:
```
:bar
foo:
:
   :   
bg: #123456;
```
Load the config with `rofi_launcher_load_config(&rofi, "/tmp/test_empty_rofi.rasi")` and assert no crash.

- [ ] **Step 2: Run tests to observe behavior**

Run: `make test_tint2_panel test_rofi_launcher && ./test_tint2_panel && ./test_rofi_launcher`

- [ ] **Step 3: Implement safe length check before pointer trimming**

In `src/tint2_panel.c`:
```c
while (*key == ' ' || *key == '\t') key++;
size_t klen = strlen(key);
if (klen > 0) {
    char *end_k = key + klen - 1;
    while (end_k > key && (*end_k == ' ' || *end_k == '\t')) *end_k-- = '\0';
}

while (*val == ' ' || *val == '\t') val++;
size_t vlen = strlen(val);
if (vlen > 0) {
    char *end_v = val + vlen - 1;
    while (end_v > val && (*end_v == ' ' || *end_v == '\t')) *end_v-- = '\0';
}
```

In `src/rofi_launcher.c`:
```c
while (*key && isspace((unsigned char)*key)) key++;
size_t klen = strlen(key);
if (klen > 0) {
    char *end_k = key + klen - 1;
    while (end_k > key && isspace((unsigned char)*end_k)) *end_k-- = '\0';
}

while (*val && isspace((unsigned char)*val)) val++;
size_t vlen = strlen(val);
if (vlen > 0) {
    char *end_v = val + vlen - 1;
    while (end_v > val && (*end_v == ';' || isspace((unsigned char)*end_v))) *end_v-- = '\0';
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `make test_tint2_panel test_rofi_launcher && ./test_tint2_panel && ./test_rofi_launcher`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/tint2_panel.c src/rofi_launcher.c tests/test_tint2_panel.c tests/test_rofi_launcher.c
git commit -m "fix(parser): prevent underflow pointer arithmetic on empty keys and values"
```

---

### Task 7: Clean Leak Sanitizer Gate for test_layout

**Files:**
- Modify: `tests/test_layout.c`

**Interfaces:**
- Consumes: `Client *head`, `client_remove(Client **head, Window win)`
- Produces: `free_clients(Client **head)` helper that frees all nodes in a linked list, resolving all 156 client allocations in `tests/test_layout.c`.

- [ ] **Step 1: Write helper `free_clients` in `tests/test_layout.c`**

```c
static void free_clients(Client **head) {
    if (!head) return;
    while (*head) {
        client_remove(head, (*head)->win);
    }
}
```

- [ ] **Step 2: Add `free_clients` calls after each test block in `tests/test_layout.c`**

Call `free_clients(&head);`, `free_clients(&many_head);`, `free_clients(&head_800);`, `free_clients(&csd_list);`, `free_clients(&snap_head);` after their corresponding assertions.

- [ ] **Step 3: Compile and run under AddressSanitizer**

Run: `gcc -Wall -Wextra -pedantic -std=c11 -O1 -fsanitize=address -Iinclude $(pkg-config --cflags cairo x11 xext fontconfig gdk-pixbuf-2.0) tests/test_layout.c src/wm.c src/dock.c src/gtk_theme.c src/icon_fetcher.c src/rofi_launcher.c src/titlebar.c -o test_layout_asan $(pkg-config --libs cairo x11 xext fontconfig gdk-pixbuf-2.0) -lm && ./test_layout_asan`
Expected: PASS with 0 leaks reported by LeakSanitizer.

- [ ] **Step 4: Commit**

```bash
git add tests/test_layout.c
git commit -m "test(layout): free client linked lists after test cases for clean leak gate"
```

---

### Task 8: Full Test Suite and Build Verification

**Files:**
- Verification only

- [ ] **Step 1: Run all unit tests**

Run: `make clean && make tests && for t in test_layout test_trigger test_rofi_launcher test_wallpaper test_client_props test_icon_fetcher test_gtk_theme test_menu test_compositor test_tint2_panel; do echo "=== $t ==="; ./$t; done`
Expected: All 10 test suites pass with return code 0.

- [ ] **Step 2: Run whole-program build**

Run: `make muzuwm`
Expected: Compiles with 0 warnings and links executable `muzuwm`.
