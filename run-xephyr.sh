#!/usr/bin/env bash
set -e

# Support resolution argument: default to 1280x700 (fits cleanly inside 1366x768 host screens), allow 800x600, 1366x768, 1920x1080, etc.
RES="${1:-1280x700}"
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

echo ""
echo "========================================================"
echo " MUZUwm is now running inside the Xephyr window ($RES)!"
echo " Things you can test with your mouse and keyboard:"
echo " 1. Windows open floating; only apps in ~/.config/muzuwm/fullscreen_allowlist open maximized"
echo " 2. Apps in ~/.config/muzuwm/auto_allowlist start with MUZUwm (none by default)"
echo " 3. Super+Left / Super+Right tile, Super+Up maximize, Super+Down float"
echo " 4. Super+Return opens wezterm, Super+b opens google-chrome (edit shortcuts.conf)"
echo " 5. Drag a titlebar to the left/right/top edge to snap"
echo " 6. Tap Super -> Launchpad; move mouse to bottom edge -> Dock slides up"
echo "========================================================"
echo "Press Ctrl+C to close test environment."

cleanup() {
    echo "Stopping test environment..."
    kill -9 $WM_PID 2>/dev/null || true
    kill -9 $XEPHYR_PID 2>/dev/null || true
}
trap cleanup EXIT INT TERM

wait $XEPHYR_PID 2>/dev/null || true
