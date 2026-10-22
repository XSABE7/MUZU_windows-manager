#!/bin/sh
set -e

cd "$(dirname "$0")"

echo "==> Building MUZUwm..."
make muzuwm

echo "==> Installing to /usr/local/bin and /usr/share/xsessions..."
install -d /usr/local/bin
install -m 755 muzuwm /usr/local/bin/muzuwm
install -m 755 tools/muzuwm-settings /usr/local/bin/muzuwm-settings
install -d /usr/share/xsessions
install -m 644 muzuwm.desktop /usr/share/xsessions/muzuwm.desktop

echo ""
echo "================================================================="
echo "  MUZUwm installed successfully!"
echo "  You can now select MUZUwm on your login screen (GDM / LightDM)"
echo "  alongside GNOME."
echo "================================================================="
