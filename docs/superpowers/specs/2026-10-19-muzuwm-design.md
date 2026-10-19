# MUZUwm (むず) — System Design Document
**Date:** 2026-10-19  
**Status:** Approved by Human Partner  
**Topic:** Mouse UserZ Window Manager for X11  

---

## 1. Intent & The Why

### Background & Motivation
Traditional lightweight window managers (e.g., i3, bspwm, dwm) are heavily keyboard-centric, requiring extensive keybinding memorization and complex scripting configuration. Meanwhile, mainstream desktop environments (e.g., GNOME, KDE) provide mouse friendliness and rich UI out-of-the-box but consume significant system resources (often 800 MB–1.5 GB+ RAM).

**MUZUwm** (*Mouse UserZ Window Manager*, aka **むず / Muzu**) fills the gap:
* **The User Experience (UX):** Effortless, single-handed mouse-first control. Zero muscle-memory learning curve. Works immediately Out-of-the-Box (OOTB) with polished Launchpad, Dock, Window Bars, and Calendar.
* **The Developer Reality (むず - Muzukashii):** The developer absorbs the complexity of building low-latency X11 event loops, Cairo-based software overlays, double-tap timers, and window management state.
* **Codebase Foundation:** Based on the minimalist event loop architecture of `tinywm` (`tinywm/tinywm.c`).

---

## 2. Goals, Non-Goals, and Anti-Goals

### Goals
1. **Mouse-First Hybrid Windowing (Tiling & Floating):**
   - 1st window automatically maximizes (100% of workspace).
   - 2nd window automatically tiles side-by-side (50:50 split).
   - 3rd and subsequent windows sub-split in a Master-Stack layout (Window 1 stays Master 50% left; Windows 2, 3... stack vertically on the right 50%).
   - Dragging a window's titlebar instantly detaches it into a free-floating window.
   - Re-tiling: Dragging a floating window to the top edge maximizes; dragging to left/right edge snaps to tiling slot; double-clicking titlebar toggles maximize/re-tile.
2. **Integrated Full Launchpad:**
   - Triggerable via **Double-tap Super** (within 200–250 ms) or **GNOME-style Hot Corner** (top-left $0, 0$).
   - Split Dashboard layout: Left side contains search input + categorized app grid; Right side contains digital clock + month-to-month calendar.
   - Wallpaper overlay background.
3. **Flexible Quick-Access Dock:**
   - Positionable at any screen edge: **Bottom, Top, Left, or Right** dynamically.
   - **Auto-Hide** behavior (slides out when mouse hovers the corresponding edge; overlay mode so windows don't stutter).
4. **Lightweight Footprint:**
   - Total system memory consumption of **MUZUwm + picom < 200 MB RAM** (MUZUwm ~30–50 MB, picom ~50–70 MB).
5. **Special Easter Egg ("Will She Know"):**
   - Typing `wwssadadba` while on Launchpad dynamically reveals the hidden **"Will She Know"** button at the right corner.
   - Clicking "Will She Know" immediately opens the YouTube music video *“เธอจะรู้บ้างไหม - เสก โลโซ”* (`https://youtu.be/-3haoCPz6MM?si=-nqANYwpOqBDr81x`) via `xdg-open`.

### Non-Goals
* Multi-monitor (XRandR) in Phase 1: Strictly focused on single-monitor displays.
* Complex calendar event sync / database: Month-to-month calendar navigation only.
* Wayland support in Phase 1 (dedicated to X11 for predictable minimal footprint).

### Anti-Goals
* Requiring user shell scripts or config files just to get a functional desktop environment.
* Stuttering animations or input lag (> 16ms frame drops on redraws).

---

## 3. Architecture & Tech Stack

* **Platform / Protocol:** Linux X11 based on `tinywm` event foundation, using Xlib/XCB (`libX11`, `libXext`, `libXrandr` [optional in future]).
* **UI Rendering:** **Cairo** (`cairo-xlib`) for vector titlebars, Launchpad dashboard, calendar, clock, and Dock icons.
* **Compositor:** **picom** (external X11 compositor for transparent overlays, subtle drop shadows, and wallpaper background dimming/blur).
* **Language & Build Tooling:** C (C11 standard) with `gcc`/`clang` and a clean `Makefile`.

---

## 4. User-Decided Specifications

### 4.1 Launchpad
* **Invocation:**
  * Double-tap the `Super` key within a 200–250 ms window.
  * Move pointer into the **Top-Left Hot Corner** ($0, 0$ screen coordinates).
* **Layout (Dashboard Split):**
  * **Left Column:**
    * Search bar with instant real-time filtering as the user types.
    * Grid of installed desktop applications (discovered from standard `.desktop` files).
  * **Right Column:**
    * Prominent digital clock (HH:MM) and full date.
    * Monthly calendar grid with today highlighted and month navigation (< / >).
* **Dismissal:**
  * Clicking any app icon (launches app and closes Launchpad).
  * Clicking anywhere outside interactive elements or pressing `Esc` / `Super`.

### 4.2 Easter Egg ("Will She Know")
* **Sequence:** When Launchpad is open, user enters keyboard sequence: `w` `w` `s` `s` `a` `d` `a` `d` `b` `a`.
* **Behavior:**
  * A dedicated button labeled **"Will She Know"** dynamically appears at the right corner of the Launchpad.
  * Clicking the button executes: `xdg-open "https://youtu.be/-3haoCPz6MM?si=-nqANYwpOqBDr81x"`.

### 4.3 Dock
* **Contents:** Pinned favorite applications and running window indicators.
* **Edge Configuration:** Can be placed on any edge (**Top, Bottom, Left, Right**).
* **Auto-Hide:**
  * Floating overlay (`override_redirect = True`) that does not trigger window resizes.
  * When mouse cursor leaves the dock area, it smoothly hides.
  * When mouse cursor hits that screen edge, it reveals.

### 4.4 Window Management (Auto-Tiling to Floating & Re-tiling)
* **Single Window:** 1st window auto-maximizes to fill the screen.
* **Two Windows:** 2nd window auto-tiles side-by-side (50% left, 50% right).
* **Three+ Windows:** 3rd and subsequent windows sub-tile on the right side (Master on left 50%, stack on right divided equally vertically).
* **Closing / Detaching:** When a tiled window closes, remaining tiled windows reflow automatically to fill space.
* **Drag-to-Float:** Left-clicking and dragging a window by its titlebar immediately detaches it into an unconstrained floating window.
* **Re-Tiling & Snapping:**
  * Dragging a floating window to the top screen edge snaps to Maximize.
  * Dragging to left or right screen edge snaps back into Tiling layout.
  * Double-clicking titlebar toggles between Floating and Maximize/Tiled.

---

## 5. Builder's Decisions (Technical Calls)
* **Dock Overlay Mechanism:** Implemented as a top-level unmanaged (`override_redirect`) window to avoid window reflow stutter.
* **Calendar Scope:** Self-contained date calculation using standard `<time.h>` for month-to-month view without external database dependencies.
* **App Scanner:** Reads `.desktop` entries from `/usr/share/applications` and caches names and icon paths.
