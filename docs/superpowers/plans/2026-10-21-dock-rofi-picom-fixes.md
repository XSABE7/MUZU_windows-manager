# Dock Icons, Rofi Mouse Wheel, and Picom Transparency Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix dock icon missing issue (auto-hide on/off), restore mouse wheel scrolling in rofi launcher, and achieve true composited transparency for dock and titlebars with picom without black border artifacts.

**Architecture:**
1. Icon Lookup: Standardize `icon_fetcher` directory search paths to include standard XDG hierarchical directories (`48x48/apps`, `scalable/apps`, etc.) and ensure minimized/running windows resolve icons via `client_get_icon`.
2. Rofi Scrolling: Enhance Button4/Button5 event handling and selection state management in `rofi_launcher` so mouse wheel scrolling responds smoothly without pointer motion race conditions.
3. Liquid Glass Transparency: Equip the Dock window with a 32-bit ARGB TrueColor visual and dedicated colormap, allowing Cairo to render per-pixel alpha transparency (zero black corner artifacts), and adjust titlebar opacity properties and picom rules for seamless Dual-Kawase blur.

**Tech Stack:** C11, Xlib (X11), Cairo Graphics (cairo-xlib), GdkPixbuf, Picom Compositor.

**Spec:** Chat specification recorded on branch `Dev_Debug` (`Bata-3.0.2026.10.21`).

## Global Constraints

- Must compile cleanly with `gcc -Wall -Wextra -pedantic -std=c11 -O2` with zero warnings.
- Memory consumption must remain strictly under the 200 MB RSS budget ceiling.
- Preserve backward compatibility for users running without a compositor (clean fallback colors).
- All unit tests in `make tests` must pass cleanly.

## Review Focus

- Themed icon directory structure: Ensure lookup handles both legacy `apps/48` and standard XDG `48x48/apps`, `scalable/apps`, and symbolic variants.
- Minimized app icon fallback: Ensure windows without `_NET_WM_ICON` fall back to class names and title keywords rather than failing.
- Rofi scroll wheel boundary behavior: Ensure scrolling at list top or bottom never indices out-of-bounds or causes negative offsets.
- ARGB visual availability fallback: On X11 servers where 32-bit TrueColor visual is unavailable, fall back gracefully to `DefaultVisual`.
- Picom configuration auto-detection: Ensure updated opacity rules in `~/.config/muzuwm/picom.conf` match actual window names and classes.

---

### Task 1: Fix Dock Icon Retrieval & Standard XDG Subdirectory Lookup

**Files:**
- Modify: `src/icon_fetcher.c:52-65`
- Modify: `src/dock.c:343-348`
- Test: `tests/test_icon_fetcher.c`

**Interfaces:**
- Consumes: `icon_load_surface(const char *icon_name)`, `client_get_icon(Display *dpy, Window win, const char *title, int target_size)`
- Produces: Loaded Cairo ARGB surfaces for standard desktop application icons and minimized windows

- [ ] **Step 1: Write the failing test**

In `tests/test_icon_fetcher.c`, add tests verifying:
1. Lookup in mock directories formatted as `48x48/apps` and `scalable/apps`.
2. Real-world lookups for `google-chrome` and `audacious` resolving valid surface pointers.

```c
// In tests/test_icon_fetcher.c
system("mkdir -p /tmp/muzuwm_test_icons/XdgTheme/48x48/apps /tmp/muzuwm_test_icons/XdgTheme/scalable/apps");
FILE *ft_xdg = fopen("/tmp/muzuwm_test_icons/XdgTheme/index.theme", "w");
if (ft_xdg) {
    fprintf(ft_xdg, "[Icon Theme]\nName=XdgTheme\nDirectories=48x48/apps,scalable/apps\n");
    fclose(ft_xdg);
}
cairo_surface_t *dummy_xdg = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 48, 48);
cairo_surface_write_to_png(dummy_xdg, "/tmp/muzuwm_test_icons/XdgTheme/48x48/apps/xdg_test_tool.png");
cairo_surface_destroy(dummy_xdg);

icon_theme_set("XdgTheme");
char xdg_found_path[512];
assert(icon_find_themed_png("xdg_test_tool", xdg_found_path, sizeof(xdg_found_path)) == 1);
assert(strstr(xdg_found_path, "48x48/apps/xdg_test_tool.png") != NULL);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make test_icon_fetcher && ./test_icon_fetcher`
Expected: FAIL (assertion fails because `app_subdirs` only looks for `apps/48`, not `48x48/apps`)

- [ ] **Step 3: Implement standard XDG subdirectories and dock minimized icon loader**

In `src/icon_fetcher.c`, expand `app_subdirs` array:
```c
static const char *app_subdirs[] = {
    "48x48/apps",
    "scalable/apps",
    "32x32/apps",
    "64x64/apps",
    "128x128/apps",
    "256x256/apps",
    "apps/48",
    "apps/scalable",
    "apps/64",
    "apps/32",
    "apps/24",
    "apps/22",
    "apps/16",
    "apps/128",
    "apps/256",
    "apps",
    "symbolic/apps",
    "symbolic/categories",
    "symbolic/legacy",
    NULL
};
```
In `src/dock.c` (`dock_add_minimized`):
Ensure minimized clients obtain their icon using `client_get_icon(dpy, c->win, c->title, 32)` rather than `icon_load_surface(c->title)`.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_icon_fetcher && ./test_icon_fetcher`
Expected: PASS (`test_icon_fetcher passed!`)

- [ ] **Step 5: Commit**

```bash
git add src/icon_fetcher.c src/dock.c tests/test_icon_fetcher.c
git commit -m "fix(dock,icons): support standard XDG icon directories and client_get_icon fallback"
```

---

### Task 2: Fix Rofi Launcher Mouse Wheel Scrolling & Event Routing

**Files:**
- Modify: `src/rofi_launcher.c:300-330, 1077-1103`
- Modify: `src/main.c:1004-1008`
- Test: `tests/test_rofi_launcher.c`

**Interfaces:**
- Consumes: `Button4`, `Button5`, `rofi_launcher_scroll(RofiLauncher *rofi, int direction)`
- Produces: Smooth multi-step scrolling without selection jitter or motion overriding scroll position

- [ ] **Step 1: Write the failing test**

In `tests/test_rofi_launcher.c`, add tests verifying:
1. Scrolling multiple steps down and up with Button5 and Button4.
2. Motion events immediately following scrolling do not cause unexpected selection resets.

```c
// In tests/test_rofi_launcher.c
// Test: Multiple scroll steps
rofi.is_visible = 1;
rofi_launcher_set_query(&rofi, "");
rofi.scroll_offset = 0;
rofi.selected_idx = 0;

for (int step = 0; step < 5; step++) {
    rofi_launcher_handle_press(NULL, &rofi, Button5, rofi.x + 100, rofi.y + 100);
}
assert(rofi.scroll_offset == 5);
assert(rofi.selected_idx >= 5);

for (int step = 0; step < 3; step++) {
    rofi_launcher_handle_press(NULL, &rofi, Button4, rofi.x + 100, rofi.y + 100);
}
assert(rofi.scroll_offset == 2);
```

- [ ] **Step 2: Run test to verify it passes or fails**

Run: `make test_rofi_launcher && ./test_rofi_launcher`
Verify baseline behavior.

- [ ] **Step 3: Implement robust scroll & motion coordination in `rofi_launcher.c` and `main.c`**

In `src/rofi_launcher.c`:
1. In `rofi_launcher_scroll`: Ensure `scroll_offset` and `selected_idx` increment/decrement in lockstep when scrolling through long app lists.
2. In `rofi_launcher_handle_press`: Support Button4 and Button5 anywhere within launcher screen coordinates without falling through to outside clicks.
3. In `src/main.c`: In `case ButtonPress`, ensure that when `rofi_launcher.is_visible` is true, Button4 and Button5 call `rofi_launcher_handle_press` and immediately break out of the event loop so background frames don't intercept.

- [ ] **Step 4: Run test to verify it passes**

Run: `make test_rofi_launcher && ./test_rofi_launcher`
Expected: PASS (`test_rofi_launcher passed!`)

- [ ] **Step 5: Commit**

```bash
git add src/rofi_launcher.c src/main.c tests/test_rofi_launcher.c
git commit -m "fix(rofi): enable responsive mouse wheel scrolling and event interception"
```

---

### Task 3: Enable True ARGB 32-bit Visual Composited Transparency for Dock and Titlebar

**Files:**
- Modify: `src/dock.c:747-785, 804-818`
- Modify: `src/titlebar.c:56-78, 82-95`
- Modify: `src/compositor.c:127-148`
- Test: `tests/test_compositor.c`

**Interfaces:**
- Consumes: `XMatchVisualInfo`, `XCreateColormap`, `cairo_xlib_surface_create`
- Produces: 32-bit TrueColor windows with per-pixel alpha transparency under picom, avoiding opaque black corners

- [ ] **Step 1: Write the failing test**

In `tests/test_compositor.c`, add assertions checking that the generated picom configuration correctly applies transparency and blur rules for `MUZUwm Dock` and `MUZUwm Titlebar`.

```c
// In tests/test_compositor.c
assert(strstr(buf, "\"90:name = 'MUZUwm Titlebar'\"") != NULL ||
       strstr(buf, "\"94:name = 'MUZUwm Dock'\"") != NULL);
assert(strstr(buf, "blur-background = true") != NULL || strstr(buf, "background = true") != NULL);
```

- [ ] **Step 2: Run test to verify it passes/fails**

Run: `make test_compositor && ./test_compositor`
Verify current config output.

- [ ] **Step 3: Implement 32-bit ARGB visual creation for Dock and Titlebar**

In `src/dock.c`:
1. Query 32-bit TrueColor visual:
```c
XVisualInfo vinfo;
if (XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
    Colormap cmap = XCreateColormap(dpy, root, vinfo.visual, AllocNone);
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.colormap = cmap;
    attrs.border_pixel = 0;
    attrs.background_pixel = 0; // Alpha 0 (transparent)
    attrs.event_mask = ExposureMask | EnterWindowMask | LeaveWindowMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    dock->win = XCreateWindow(dpy, root, dock->x, dock->y, dock->width, dock->height, 0,
                              vinfo.depth, InputOutput, vinfo.visual,
                              CWOverrideRedirect | CWColormap | CWBorderPixel | CWBackPixel | CWEventMask,
                              &attrs);
    dock->visual = vinfo.visual;
} else {
    // Fallback to DefaultVisual
    dock->visual = DefaultVisual(dpy, screen);
    ...
}
```
2. In `dock_render`:
Create `dock->surface` using `dock->visual` (depth 32).
When `cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR); cairo_paint(cr);` runs on a 32-bit visual, the background becomes completely clear (alpha = 0). The rounded squircle plate with alpha 0.75-0.82 paints smoothly, leaving corners 100% transparent.

In `src/titlebar.c`:
Set window opacity `_NET_WM_WINDOW_OPACITY` on `c->frame` or create frame with appropriate visual/opacity so titlebar glass shines through with picom blur.

In `src/compositor.c`:
Update `opacity-rule` to:
```
opacity-rule = [
    "94:name = 'MUZUwm Dock'",
    "94:window_type = 'dock'",
    "88:name = 'MUZUwm Titlebar'",
    "96:window_type = 'dialog'"
];
```

- [ ] **Step 4: Run tests and verify all pass**

Run: `make clean && make tests && ./tests/test_runner.sh`
Expected: PASS across all unit tests and benchmarks.

- [ ] **Step 5: Commit**

```bash
git add src/dock.c src/titlebar.c src/compositor.c tests/test_compositor.c
git commit -m "fix(compositor,dock,titlebar): add 32-bit ARGB visual transparency and frosted glass blur support"
```
