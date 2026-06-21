#!/usr/bin/env bash
# Install host-side access for the Infineon DAP debugger.
# Extracts the proprietary DAS/TAS .deb from vendor/, installs the bundled FTDI
# driver, and adds the udev rule that makes the debugger node writable.
# Needs sudo. Copyright 2026 Syuma Labs. Apache-2.0.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENDOR="$ROOT/vendor"

DEB="$(ls "$VENDOR"/DAS_*_linux_x64.deb 2>/dev/null | head -1 || true)"
if [ -z "$DEB" ]; then
    echo "No DAS_*_linux_x64.deb found in vendor/."
    echo "Download the Infineon TAS Linux package first, see docs/linux-setup.md"
    exit 1
fi
echo "Using $DEB"

DEST="$VENDOR/das"
rm -rf "$DEST"
mkdir -p "$DEST"
dpkg-deb -x "$DEB" "$DEST"

SRV="$(find "$DEST" -path '*/bin/tas_server' | head -1 || true)"
if [ -z "$SRV" ]; then
    echo "tas_server not found inside the package, unexpected layout."
    exit 1
fi
DASROOT="$(dirname "$(dirname "$SRV")")"
echo "Installed DAS tree at $DASROOT"

echo "Running postinst (FTDI driver and udev rule), sudo required"
sudo bash "$DASROOT/others/postinst.sh"

echo "Done. Start the server with scripts/run-tas-server.sh"
