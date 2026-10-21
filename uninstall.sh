#!/bin/sh
set -e

echo "================================================================="
echo "                  MUZUwm Uninstaller                             "
echo "================================================================="

# Check root permissions
if [ "$(id -u)" -ne 0 ]; then
    echo "Error: This script must be run with root privileges (sudo)."
    echo "Usage: sudo ./uninstall.sh [--purge | --keep-config]"
    exit 1
fi

echo "==> Removing system files..."
rm -f /usr/local/bin/muzuwm
rm -f /usr/share/xsessions/muzuwm.desktop
echo "  - Removed /usr/local/bin/muzuwm"
echo "  - Removed /usr/share/xsessions/muzuwm.desktop"

# Detect real user running sudo to locate user config directory
REAL_USER="${SUDO_USER:-$USER}"
if [ -n "$REAL_USER" ] && [ "$REAL_USER" != "root" ]; then
    USER_HOME=$(getent passwd "$REAL_USER" | cut -d: -f6)
else
    USER_HOME="$HOME"
fi

CONFIG_DIR="$USER_HOME/.config/muzuwm"

REMOVE_CONFIG=""
case "$1" in
    --purge)
        REMOVE_CONFIG="y"
        ;;
    --keep-config)
        REMOVE_CONFIG="n"
        ;;
esac

if [ -z "$REMOVE_CONFIG" ]; then
    if [ -d "$CONFIG_DIR" ]; then
        echo ""
        echo "Found user configuration directory at: $CONFIG_DIR"
        printf "Do you want to delete user configuration and settings (~/.config/muzuwm)? [y/N]: "
        if [ -t 0 ]; then
            read -r ans
        else
            read -r ans < /dev/tty 2>/dev/null || ans="n"
        fi
        case "$ans" in
            [Yy]*) REMOVE_CONFIG="y" ;;
            *)     REMOVE_CONFIG="n" ;;
        esac
    else
        REMOVE_CONFIG="n"
    fi
fi

if [ "$REMOVE_CONFIG" = "y" ]; then
    if [ -d "$CONFIG_DIR" ]; then
        rm -rf "$CONFIG_DIR"
        echo "==> Deleted configuration directory: $CONFIG_DIR"
    fi
else
    if [ -d "$CONFIG_DIR" ]; then
        echo "==> Preserved configuration directory: $CONFIG_DIR"
    fi
fi

echo ""
echo "================================================================="
echo "  MUZUwm has been successfully uninstalled."
echo "================================================================="
