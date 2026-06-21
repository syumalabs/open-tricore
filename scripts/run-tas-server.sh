#!/usr/bin/env bash
# Launch the Infineon TAS server, which bridges the USB DAP debugger to
# localhost:24817 for our clients. Copyright 2026 Syuma Labs. Apache-2.0.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

SRV="$(find "$ROOT/vendor/das" -path '*/bin/tas_server_console' | head -1 || true)"
if [ -z "$SRV" ]; then
    echo "tas_server_console not found. Run scripts/install-tas.sh first."
    exit 1
fi

echo "Starting $SRV on port 24817"
exec "$SRV"
