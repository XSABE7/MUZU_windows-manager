#!/usr/bin/env bash
set -euo pipefail

echo "================================================="
echo "       MUZUwm Automated Test & Benchmark         "
echo "================================================="

echo "1. Running Unit Tests..."
make -s tests muzuwm
./test_layout
./test_trigger
./test_rofi_launcher
./test_wallpaper
./test_client_props
./test_icon_fetcher
./test_gtk_theme
./test_menu
echo "--> All Unit Tests Passed!"

echo ""
echo "2. Measuring MUZUwm Memory Footprint (RAM Budget)..."
DISPLAY="${DISPLAY:-:1}"

./muzuwm &
WM_PID=$!
sleep 1

if ! kill -0 $WM_PID 2>/dev/null; then
    echo "ERROR: muzuwm crashed on startup"
    exit 1
fi

RSS_KB=$(ps -o rss= -p $WM_PID | tr -d ' ')
RSS_MB=$(echo "scale=2; $RSS_KB / 1024" | bc || echo "$((RSS_KB / 1024))")
echo "MUZUwm PID: $WM_PID"
echo "MUZUwm RSS Memory: ${RSS_KB} KB (~${RSS_MB} MB)"

# Gracefully terminate test instance
kill -9 $WM_PID 2>/dev/null || true
wait $WM_PID 2>/dev/null || true

# Check hard gate < 200 MB
MAX_LIMIT_KB=$((200 * 1024))
if [ "$RSS_KB" -gt "$MAX_LIMIT_KB" ]; then
    echo "FAIL: MUZUwm exceeded 200 MB limit (${RSS_MB} MB)"
    exit 1
fi

echo "--> Memory Check Passed: ${RSS_MB} MB is well under 200 MB ceiling!"
echo "================================================="
echo "           ALL TESTS PASSED CLEANLY              "
echo "================================================="
exit 0
