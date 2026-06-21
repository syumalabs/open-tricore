#!/usr/bin/env bash
# Build the upstream TAS client library, then the open-tricore tools on top.
# Copyright 2026 Syuma Labs. Apache-2.0.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONAN="conan"
[ -x "$ROOT/.venv/bin/conan" ] && CONAN="$ROOT/.venv/bin/conan"

echo "[1/2] Building upstream third_party/tas_client_api"
cd "$ROOT/third_party/tas_client_api"
"$CONAN" install . --build=missing -s compiler.cppstd=17
cmake --preset conan-release
cmake --build --preset conan-release

echo "[2/2] Building open-tricore tools"
cd "$ROOT"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

echo "Done. Run ./build/tools/led-demo/led-demo"
