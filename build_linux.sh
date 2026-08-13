#!/bin/bash
# Build AudioFall for Linux from source.
# Debian/Ubuntu prerequisites:
#   sudo apt install build-essential cmake qt6-base-dev qtmultimedia5-dev libgl-dev
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build_linux"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --parallel "$(nproc)"
ctest --test-dir "$BUILD" --output-on-failure

echo "Built: $BUILD/audiofall"
