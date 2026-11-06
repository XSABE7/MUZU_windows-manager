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

echo "Starting MUZUwm Session on DISPLAY=:2..."
DISPLAY=:2 PATH="$PWD/bin:$PWD:$PATH" bin/muzu-session &
WM_PID=$!
sleep 0.5

echo ""
echo "========================================================"
echo " MUZUwm is now running inside the Xephyr window ($RES)!"
echo " Things you can test with your mouse and keyboard:"
echo " 1. Windows open floating; mouse-snapping preview when dragged to edges"
echo " 2. Super+Left / Super+Right tile 50/50, Super+Up maximize, Super+Down float"
echo " 3. Super+Return opens terminal, Super+q closes window, Super+Space opens Rofi"
echo " 4. macOS Traffic Lights: Red (Close), Yellow (Minimize), Green (Maximize/Disabled)"
echo " 5. Dynamic EWMH Struts support for external Tint2/Polybar/Plank"
echo "========================================================"
echo "Press Ctrl+C to close test environment."

cleanup() {
    echo "Stopping test environment..."
    kill -9 $WM_PID 2>/dev/null || true
    kill -9 $XEPHYR_PID 2>/dev/null || true
}
trap cleanup EXIT INT TERM

wait $XEPHYR_PID 2>/dev/null || true
