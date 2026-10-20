#!/usr/bin/env bash
# MUZUwm Real-Time Memory & Leak Monitor
# Tracks Resident Set Size (RSS), Heap, and detects memory trends

set -u

echo "========================================================"
echo "          MUZUwm Real-Time RAM & Leak Monitor          "
echo "========================================================"

find_pid() {
    pidof muzuwm 2>/dev/null | awk '{print $1}'
}

WM_PID=$(find_pid)

if [ -z "$WM_PID" ]; then
    echo "Waiting for 'muzuwm' process to start..."
    while [ -z "$WM_PID" ]; do
        sleep 0.5
        WM_PID=$(find_pid)
    done
fi

echo "Found MUZUwm (PID: $WM_PID)"
echo "Monitoring memory every 0.5s... (Press Ctrl+C to stop)"
echo "--------------------------------------------------------"
printf "%-10s %-12s %-12s %-12s %-8s\n" "TIME" "RSS (KB)" "RSS (MB)" "CHANGE" "TREND"
echo "--------------------------------------------------------"

PREV_RSS=0
START_RSS=0

while kill -0 "$WM_PID" 2>/dev/null; do
    # Read VmRSS from /proc/$PID/status
    RSS_KB=$(grep -i "VmRSS:" "/proc/$WM_PID/status" 2>/dev/null | awk '{print $2}')
    if [ -z "$RSS_KB" ]; then
        break
    fi

    if [ "$START_RSS" -eq 0 ]; then
        START_RSS=$RSS_KB
        PREV_RSS=$RSS_KB
    fi

    DIFF=$((RSS_KB - PREV_RSS))
    TOTAL_DIFF=$((RSS_KB - START_RSS))
    RSS_MB=$(awk "BEGIN {printf \"%.2f\", $RSS_KB / 1024}")
    TIME=$(date +"%H:%M:%S")

    TREND="="
    COLOR="\033[0;32m" # Green

    if [ "$DIFF" -gt 0 ]; then
        TREND="▲ +${DIFF} KB"
        COLOR="\033[0;31m" # Red (growth)
    elif [ "$DIFF" -lt 0 ]; then
        TREND="▼ ${DIFF} KB"
        COLOR="\033[0;34m" # Blue (freed)
    else
        TREND="-- steady"
    fi

    TOTAL_LABEL=""
    if [ "$TOTAL_DIFF" -gt 0 ]; then
        TOTAL_LABEL="(Total: +${TOTAL_DIFF} KB)"
    elif [ "$TOTAL_DIFF" -lt 0 ]; then
        TOTAL_LABEL="(Total: ${TOTAL_DIFF} KB)"
    fi

    printf "${COLOR}%-10s %-12s %-12s %-16s %-15s\033[0m\n" \
           "$TIME" "${RSS_KB} KB" "${RSS_MB} MB" "$TREND" "$TOTAL_LABEL"

    PREV_RSS=$RSS_KB
    sleep 0.5
done

echo ""
echo "MUZUwm process exited."
