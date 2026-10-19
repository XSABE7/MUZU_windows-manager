# MUZUwm (むず) — System Design Document
**Date:** 2026-10-19  
**Status:** Draft (Awaiting Builder Check & User Approval)  
**Topic:** Mouse UserZ Window Manager for X11  

---

## 1. Intent & The Why

### Background & Motivation
Traditional lightweight window managers (e.g., i3, bspwm, dwm) are heavily keyboard-centric, requiring extensive keybinding memorization and complex scripting configuration. Meanwhile, mainstream desktop environments (e.g., GNOME, KDE) provide mouse friendliness and rich UI out-of-the-box but consume significant system resources (often 800 MB–1.5 GB+ RAM).

**MUZUwm** (*Mouse UserZ Window Manager*, aka **むず / Muzu**) fills the gap:
* **The User Experience (UX):** Effortless, single-handed mouse-first control. Zero muscle-memory learning curve. Works immediately Out-of-the-Box (OOTB) with polished Launchpad, Dock, Window Bars, and Calendar.
* **The Developer Reality (むず - Muzukashii):** The developer absorbs the complexity of building low-latency XCB event loops, Cairo-based software overlays, double-tap timers, and EWMH/ICCCM compliance.

---

## 2. Goals, Non-Goals, and Anti-Goals

### Goals
1. **Mouse-First Hybrid Windowing:**
   - 1st window automatically maximizes.
   - 2nd window automatically tiles side-by-side (50:50).
   - Dragging a window's titlebar instantly detaches it into a free-floating window.
2. **Integrated Full Launchpad:**
   - Triggerable via **Double-tap Super** (within 200–250 ms) or **GNOME-style Hot Corner** (top-left).
   - Split Dashboard layout: Left side contains search input + categorized app grid; Right side contains digital clock + interactive monthly calendar.
   - Wallpaper overlay background.
3. **Flexible Quick-Access Dock:**
   - Positionable at any screen edge: **Bottom, Top, Left, or Right** dynamically.
   - **Auto-Hide** behavior (slides out when mouse hovers the corresponding edge).
4. **Lightweight Footprint:**
   - Total system memory consumption of **MUZUwm + picom < 200 MB RAM** (MUZUwm ~35–50 MB, picom ~50–70 MB).
5. **Special Easter Egg:**
   - Typing `wwssadadba` while on Launchpad dynamically reveals the hidden **"Will She Know"** button at the right corner.
   - Clicking "Will She Know" immediately opens the YouTube music video *“เธอจะรู้บ้างไหม - เสก โลโซ”* (`https://youtu.be/-3haoCPz6MM?si=-nqANYwpOqBDr81x`) via `xdg-open`.

### Non-Goals
* Re-implementing a heavy desktop suite (no bespoke systemd desktop portals, notifications daemon, or network manager applets in core WM).
* Complex multi-node manual tiling trees (e.g., BSP tree splitting, master-stack ratios) — only clean auto-maximize, side-by-side split, and floating windows.
* Wayland support in phase 1 (dedicated to X11 with XCB for maximum lightweight predictability).

### Anti-Goals
* Requiring user shell scripts or config files just to get a functional desktop environment.
* Stuttering animations or input lag (> 16ms frame drops on Cairo redraws).

---

## 3. Architecture & Tech Stack

* **Platform / Protocol:** Linux X11 using **XCB** (`libxcb`, `xcb-ewmh`, `xcb-icccm`, `xcb-keysyms`, `xcb-util`).
* **UI Rendering:** **Cairo** (`cairo-xcb` / `cairo`) for rendering vector titlebars, Launchpad dashboard, calendar, clock, and Dock icons.
* **Compositor:** **picom** (external X11 compositor for transparent overlays, subtle drop shadows, and wallpaper background dimming/blur).
* **Language & Build Tooling:** C (C11 standard) with `gcc`/`clang` and a clean `Makefile`.

---

## 4. User-Decided Specifications

### 4.1 Launchpad
* **Invocation:**
  * Double-tap the `Super` (Windows) key within a 200–250 ms window.
  * Move pointer into the **Top-Left Hot Corner** ($0, 0$ screen coordinates).
* **Layout (Dashboard Split):**
  * **Left Column:**
    * Search bar with instant real-time filtering as the user types.
    * Grid of installed desktop applications (discovered from standard `.desktop` files).
  * **Right Column:**
    * Prominent digital clock (HH:MM) and full Thai/Gregorian date.
    * Monthly calendar grid with today highlighted.
    * *(Note: RAM stats removed as requested by user).*
* **Dismissal:**
  * Clicking any app icon (launches app and closes Launchpad).
  * Clicking anywhere outside interactive elements or pressing `Esc` / `Super`.

### 4.2 Easter Egg ("Will She Know")
* **Sequence:** When Launchpad is open, user enters keyboard sequence: `w` `w` `s` `s` `a` `d` `a` `d` `b` `a`.
* **Behavior:**
  * A dedicated button labeled **"Will She Know"** (or guitar/heart icon) dynamically appears at the right corner of the Launchpad.
  * Clicking the button executes: `xdg-open "https://youtu.be/-3haoCPz6MM?si=-nqANYwpOqBDr81x"`.

### 4.3 Dock
* **Contents:** Pinned favorite applications and running window indicators.
* **Edge Configuration:** Can be placed on any edge (**Top, Bottom, Left, Right**).
* **Auto-Hide:**
  * When mouse cursor leaves the dock area, it smoothly slides into the screen edge.
  * When mouse cursor hits that screen edge, it pops up immediately (with a tiny 100ms threshold to prevent accidental triggers).

### 4.4 Window Management (Auto-Tiling to Floating)
* **Single Window Rule:** First managed window auto-maximizes to fill the screen (accounting for any non-hidden dock margins).
* **Two Windows Rule:** Second managed window auto-tiles side-by-side ($50\% : 50\%$ split).
* **Drag-to-Float:**
  * Left-clicking and dragging a window by its titlebar immediately detaches it from the tiling container into an unconstrained floating window.
  * Floating windows can be freely moved and resized using edge/corner handles or mouse drag.

---

## 5. What's Left to the Builder

The builder has full discretion over:
1. **Double-Tap Super Implementation:** Choice between `timerfd_create` or event loop timestamp comparison (`event->time - last_super_time < 250ms`).
2. **App Discovery:** Parsing `/usr/share/applications` and `~/.local/share/applications` for `.desktop` files (extracting `Name`, `Exec`, `Icon`).
3. **Wallpaper Drawing:** Calling `feh`/`hsetroot` or setting the root pixmap directly via XCB if no external background setter is found.
4. **Dock Right-Click Menu:** Providing a simple pop-up menu on right-click to choose dock placement: [Top, Bottom, Left, Right].
5. **Titlebar Controls:** Exact dimensions (e.g. 24px height) with minimal Close/Minimize buttons drawn with Cairo.
