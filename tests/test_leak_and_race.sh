#!/usr/bin/env bash
set -euo pipefail

echo "================================================="
echo "   MUZUwm Leak & Race Condition Stress Test      "
echo "================================================="

# Start Xephyr on :9
Xephyr -br -ac -noreset -screen 800x600 :9 >/dev/null 2>&1 &
XEPHYR_PID=$!
sleep 1

cleanup() {
    echo "Cleaning up processes..."
    kill -TERM "$WM_PID" 2>/dev/null || true
    sleep 0.5
    kill -9 "$WM_PID" 2>/dev/null || true
    kill -9 "$XEPHYR_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo "Starting MUZUwm on DISPLAY=:9..."
DISPLAY=:9 ./muzuwm &
WM_PID=$!
sleep 1

if ! kill -0 "$WM_PID" 2>/dev/null; then
    echo "ERROR: muzuwm failed to start!"
    exit 1
fi

get_rss_kb() {
    ps -o rss= -p "$1" 2>/dev/null | tr -d ' ' || echo 0
}

XEPHYR_RSS_INIT=$(get_rss_kb "$XEPHYR_PID")
WM_RSS_INIT=$(get_rss_kb "$WM_PID")
echo "Initial Xephyr RSS: ${XEPHYR_RSS_INIT} KB"
echo "Initial MUZUwm RSS: ${WM_RSS_INIT} KB"

echo ""
echo "--- Running Native X11 High-Speed Stress Test (50 cycles) ---"
./test_x11_stress :9

sleep 1

XEPHYR_RSS_FINAL=$(get_rss_kb "$XEPHYR_PID")
WM_RSS_FINAL=$(get_rss_kb "$WM_PID")
echo ""
echo "Post-Stress Xephyr RSS: ${XEPHYR_RSS_FINAL} KB (delta: $((XEPHYR_RSS_FINAL - XEPHYR_RSS_INIT)) KB)"
echo "Post-Stress MUZUwm RSS: ${WM_RSS_FINAL} KB (delta: $((WM_RSS_FINAL - WM_RSS_INIT)) KB)"

if ! kill -0 "$WM_PID" 2>/dev/null; then
    echo "ERROR: muzuwm crashed during stress test!"
    exit 1
fi

# Assertions: 120MB ceiling for ASan instrumentation (shadow map overhead is ~40MB)
if [ "$WM_RSS_FINAL" -gt 120000 ]; then
    echo "FAIL: MUZUwm RSS exceeded 120MB (${WM_RSS_FINAL} KB)!"
    exit 1
fi

echo ""
echo "================================================="
echo "  STRESS & LEAK TEST PASSED WITH ZERO CRASHES!   "
echo "================================================="
