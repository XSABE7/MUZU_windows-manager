# MUZUwm Openbox-Inspired Features Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement dynamic window title resolution, ICCCM graceful window closing, and smart dialog/transient window floating in MUZUwm, inspired by Openbox.

**Architecture:** Create a modular client property module (`src/client_props.c`, `include/client_props.h`) handling X11 property queries for `_NET_WM_NAME`, `WM_NAME`, `WM_CLASS`, `WM_PROTOCOLS` (`WM_DELETE_WINDOW`), `WM_TRANSIENT_FOR`, and `_NET_WM_WINDOW_TYPE`. Store title in `Client` struct to cache Cairo titlebar draws and update dynamically on `PropertyNotify`.

**Tech Stack:** C11, Xlib (`libX11`, `libXext`), Cairo (`cairo-xlib`), POSIX.

**Spec:** Adapted from Openbox codebase inspection (`/home/peach/git/MUZUwm/openbox`) and `docs/superpowers/specs/2026-10-19-muzuwm-design.md`.

## Global Constraints

- Must stay in pure C11 without GLib or Libxml2 dependencies.
- Total memory footprint must remain under 200 MB (measured < 10 MB RSS).
- All unit tests and benchmarks must pass cleanly.
- Preserves 1366x768 layout responsiveness.

## Review Focus

1. **Titlebar Buffer Truncation:** Long UTF-8 titles (e.g. 500+ characters) must be safely null-terminated within buffer boundaries without memory corruption.
2. **Graceful Close Fallback:** Windows without `WM_DELETE_WINDOW` protocol support must still terminate via `XKillClient` without leaving orphan windows.
3. **PropertyNotify Event Loop Throttling:** Rapid title changes (e.g., download counters or timer titles) must redraw titlebars cleanly without memory leaks.
4. **Dialog Centering Clamping:** Transient dialogs wider or taller than screen dimensions must be clamped safely to screen boundaries.
5. **Client Destroy / Unmap Lifecycle:** Closing via `WM_DELETE_WINDOW` triggers `UnmapNotify`/`DestroyNotify` asynchronously; frame teardown must remain clean.

---

### Task 1: Client Properties Module & Graceful Close Protocol

**Files:**
- Create: `include/client_props.h`
- Create: `src/client_props.c`
- Create: `tests/test_client_props.c`
- Modify: `Makefile:6,14-30`

- [x] **Step 1: Write failing unit test for `client_props` helper functions**
  - Test protocol detection logic: matching `WM_DELETE_WINDOW` against protocol list.
  - Test fallback title generator when properties are absent.
  - Compile with test target in Makefile.

- [x] **Step 2: Implement `src/client_props.c` and `include/client_props.h`**
  - `client_has_protocol(Display *dpy, Window win, Atom protocol)`: Checks `XGetWMProtocols`.
  - `client_close(Display *dpy, Window win)`: Sends `WM_PROTOCOLS` `ClientMessage` with `WM_DELETE_WINDOW`. If unsupported, invokes `XKillClient(dpy, win)`.

- [x] **Step 3: Run unit test and verify it passes**
  - Run `./test_client_props` and ensure assertions pass.

- [x] **Step 4: Commit Task 1 changes**
  - Git commit: `feat: implement client_props module and ICCCM graceful close`

---

### Task 2: Dynamic Window Title Resolution & PropertyNotify

**Files:**
- Modify: `include/wm.h:17-26` (Add `char title[256]` to `Client` struct)
- Modify: `include/client_props.h`
- Modify: `src/client_props.c`
- Modify: `src/titlebar.c:25-51`
- Modify: `src/main.c:29-45,78-83,105-124,174-178,253-261`
- Modify: `tests/test_client_props.c`

- [x] **Step 1: Write tests for title resolution chain**
  - Test fallback priority: UTF-8 `_NET_WM_NAME` -> `WM_NAME` -> `WM_CLASS` -> default `"MUZUwm Window"`.
  - Verify truncation to 255 characters with null termination.

- [x] **Step 2: Implement `client_get_title` in `src/client_props.c`**
  - Query `_NET_WM_NAME` (atom `UTF8_STRING`).
  - Fallback to `XGetWMName` or `XGetTextProperty(dpy, win, &text_prop, XA_WM_NAME)`.
  - Fallback to `XGetClassHint(dpy, win, &class_hints)`.
  - Copy into target buffer with safe bounds.

- [x] **Step 3: Update `src/titlebar.c` and `src/main.c`**
  - In `src/titlebar.c`, render actual `c->title` using Cairo.
  - In `src/main.c`:
    - Listen for `PropertyChangeMask` on managed client windows.
    - On `MapRequest`, fetch initial title into `c->title` and pass to `titlebar_draw`.
    - In `reflow_all()`, pass `curr->title` instead of static `"Application"`.
    - Add `case PropertyNotify:` in event loop: if `ev.xproperty.atom == XA_WM_NAME` or `_NET_WM_NAME`, update `curr->title` and call `titlebar_draw(dpy, curr->frame, curr->title, curr->width)`.
    - Replace `XDestroyWindow(dpy, curr->win)` with `client_close(dpy, curr->win)`.

- [x] **Step 4: Verify tests and build**
  - Run `make test_client_props test_layout test_trigger test_easter_egg test_calendar`.
  - Run all unit tests.

- [x] **Step 5: Commit Task 2 changes**
  - Git commit: `feat: add dynamic titlebar updates with PropertyNotify support`

---

### Task 3: Smart Dialog & Transient Floating Detection

**Files:**
- Modify: `include/client_props.h`
- Modify: `src/client_props.c`
- Modify: `src/main.c:105-124`
- Modify: `tests/test_client_props.c`

- [x] **Step 1: Write tests for dialog detection and centering logic**
  - Test helper that identifies dialog window types (`_NET_WM_WINDOW_TYPE_DIALOG`, `_NET_WM_WINDOW_TYPE_SPLASH`, `_NET_WM_WINDOW_TYPE_UTILITY`).
  - Test geometry centering math: given screen 1366x768 and dialog 500x300, verify coordinates `(433, 234)`.

- [x] **Step 2: Implement `client_is_dialog` in `src/client_props.c`**
  - Check `XGetTransientForHint(dpy, win, &transient_for_win)`.
  - Query `_NET_WM_WINDOW_TYPE` for dialog / splash / utility atoms.
  - Return 1 if dialog/transient, 0 otherwise.

- [x] **Step 3: Integrate Dialog Floating in `src/main.c`**
  - On `MapRequest`, if `client_is_dialog(dpy, w)`:
    - Add client with `CLIENT_FLOATING` mode.
    - Read requested window dimensions (via `XGetGeometry`).
    - If dimensions are uninitialized or 0, default to `600x400`.
    - Clamp dimensions so they do not exceed screen boundaries.
    - Center window at `((screen_w - w) / 2, (screen_h - h) / 2)`.
    - Create titlebar frame, position floating window, and map.
    - Do NOT reflow existing tiled windows when a dialog appears!

- [x] **Step 4: Verify test suite and interactive run**
  - Run `./test_client_props`.
  - Run `bash tests/test_runner.sh`.

- [x] **Step 5: Commit Task 3 changes**
  - Git commit: `feat: add smart dialog and transient window floating support`

---

### Task 4: Full Test Suite Verification & RAM Benchmark

**Files:**
- Modify: `tests/test_runner.sh`

- [x] **Step 1: Update `tests/test_runner.sh` to include `test_client_props`**
- [x] **Step 2: Run all tests and measure memory consumption**
  - Verify unit tests pass: `test_layout`, `test_trigger`, `test_easter_egg`, `test_calendar`, `test_client_props`.
  - Verify RSS memory remains well under 200 MB (target < 10 MB).
- [x] **Step 3: Commit Task 4**
  - Git commit: `test: update test runner with client_props verification`
