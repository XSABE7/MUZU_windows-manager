#!/usr/bin/env bash
set -e

# Support resolution argument: default to 1366x768, allow 800x600, 1920x1080, etc.
RES="${1:-1366x768}"
if [ "$RES" = "800" ]; then
    RES="800x600"
elif [ "$RES" = "1366" ]; then
    RES="1366x768"
elif [ "$RES" = "1080" ] || [ "$RES" = "1920" ]; then
    RES="1920x1080"
fi

# Build the window manager
make muzuwm

echo "========================================================"
echo " Starting nested X11 server (Xephyr $RES) on DISPLAY=:2..."
echo " Usage: ./run-xephyr.sh [resolution] (e.g. ./run-xephyr.sh 800x600)"
echo "========================================================"
Xephyr -br -ac -noreset -screen "$RES" :2 &
XEPHYR_PID=$!
sleep 1

echo "Starting MUZUwm on DISPLAY=:2..."
DISPLAY=:2 ./muzuwm &
WM_PID=$!
sleep 0.5

echo "Spawning test terminal windows..."
DISPLAY=:2 xterm -fa "Monospace" -fs 10 &
sleep 0.5
DISPLAY=:2 xterm -fa "Monospace" -fs 10 &

echo ""
echo "========================================================"
echo " MUZUwm is now running inside the Xephyr window ($RES)!"
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
