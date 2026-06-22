#!/usr/bin/env bash
# Build the open-source tricore-elf-gcc toolchain for AURIX TriCore, including
# TriCore 1.8 used by the TC4x. Installs into toolchain/install.
# Copyright 2026 Syuma Labs. Apache-2.0.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/toolchain/tricore-gcc-toolchain"
PREFIX="$ROOT/toolchain/install"

UPSTREAM="https://github.com/NoMore201/tricore-gcc-toolchain.git"
PIN="eb167cf281d95e220546aff2c4ecb99a2f91b90c"

if [ ! -d "$SRC/.git" ]; then
    echo "Cloning toolchain build harness, pinned to $PIN"
    git clone "$UPSTREAM" "$SRC"
    git -C "$SRC" checkout "$PIN"
fi

echo "Fetching source submodules, gcc binutils newlib, shallow"
git -C "$SRC" submodule update --init --recursive --depth 1

if ! command -v makeinfo >/dev/null; then
    echo "Build dependencies are missing. Install them once with sudo, then re-run this script."
    echo "  sudo $SRC/scripts/install-apt-dependencies"
    exit 1
fi

echo "Building GCC stage 2. This takes 20 to 40 minutes on a many-core host."
rm -rf "$SRC/build"
mkdir "$SRC/build"
( cd "$SRC/build" && ../configure --prefix="$PREFIX" && make -j"$(nproc)" stamps/build-gcc-stage2 )

echo "Done. Compiler is at $PREFIX/bin/tricore-elf-gcc"
echo "Add it to PATH with"
echo "  export PATH=\"$PREFIX/bin:\$PATH\""
