#!/usr/bin/env bash
set -e

# Build the window manager
make muzuwm

echo "Starting nested X11 server (Xephyr) on DISPLAY=:2..."
Xephyr -br -ac -noreset -screen 1280x720 :2 &
XEPHYR_PID=$!
sleep 1

echo "Starting MUZUwm on DISPLAY=:2..."
DISPLAY=:2 ./muzuwm &
WM_PID=$!
sleep 0.5

echo "Spawning test terminal windows..."
DISPLAY=:2 xterm &
sleep 0.5
DISPLAY=:2 xterm &

echo ""
echo "========================================================"
echo " MUZUwm is now running inside the Xephyr window!"
echo " Things you can test with your mouse and keyboard:"
echo " 1. First xterm is auto-maximized, second is 50:50 tiled"
echo " 2. Drag a titlebar -> detaches window into Floating"
echo " 3. Double-click titlebar or drag to top edge -> Re-tiles"
echo " 4. Move mouse to top-left corner (0,0) -> Opens Launchpad"
echo " 5. In Launchpad, type 'wwssadadba' -> Reveals 'Will She Know' button!"
echo " 6. Move mouse to bottom edge -> Dock slides up"
echo "========================================================"
echo "Press Ctrl+C to close test environment."

cleanup() {
    echo "Stopping test environment..."
    kill -9 $WM_PID 2>/dev/null || true
    kill -9 $XEPHYR_PID 2>/dev/null || true
}
trap cleanup EXIT INT TERM

wait $XEPHYR_PID 2>/dev/null || true
