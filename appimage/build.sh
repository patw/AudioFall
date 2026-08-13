#!/bin/bash
# Build a portable x86_64 AppImage.
# The release workflow downloads linuxdeploy and its Qt plugin into tools/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$ROOT")"
TOOLS="$ROOT/tools"
APPDIR="$ROOT/AudioFall.AppDir"
BUILD="$PROJECT_ROOT/build_linux"

if [[ ! -x "$TOOLS/linuxdeploy-x86_64.AppImage" || ! -x "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage" ]]; then
    echo "Missing AppImage tools. See README.md or use the GitHub release workflow."
    exit 1
fi

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cmake -S "$PROJECT_ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --parallel "$(nproc)"
QT_QPA_PLATFORM=offscreen ctest --test-dir "$BUILD" --output-on-failure

install -m 755 "$BUILD/audiofall" "$APPDIR/usr/bin/audiofall"
install -m 644 "$ROOT/audiofall.desktop" "$APPDIR/usr/share/applications/audiofall.desktop"
install -m 644 "$PROJECT_ROOT/assets/audiofall.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/audiofall.png"
cp "$PROJECT_ROOT/assets/audiofall.png" "$APPDIR/audiofall.png"

export LDAI_OUTPUT="$PROJECT_ROOT/AudioFall-x86_64.AppImage"
"$TOOLS/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --plugin qt \
    --desktop-file "$APPDIR/usr/share/applications/audiofall.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/audiofall.png" \
    --output appimage

echo "Built: $PROJECT_ROOT/AudioFall-x86_64.AppImage"
