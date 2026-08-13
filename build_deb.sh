#!/bin/bash
# Build a Debian/Ubuntu package. This intentionally relies on the distribution's
# Qt6 runtime packages; use the AppImage for a fully portable Linux download.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
VERSION="${VERSION:-$(grep -oP 'project\(\w+ VERSION \K[\d.]+' "$ROOT/CMakeLists.txt" | head -1)}"
VERSION="${VERSION#v}"
ARCH="$(dpkg --print-architecture)"
PACKAGE="audiofall_${VERSION}_${ARCH}"
BUILD="$ROOT/build_linux"
STAGING="$ROOT/.deb_staging/$PACKAGE"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --parallel "$(nproc)"
ctest --test-dir "$BUILD" --output-on-failure

rm -rf "$ROOT/.deb_staging"
mkdir -p "$STAGING/DEBIAN" \
         "$STAGING/usr/bin" \
         "$STAGING/usr/share/applications" \
         "$STAGING/usr/share/icons/hicolor/256x256/apps" \
         "$STAGING/usr/share/doc/audiofall"

install -m 755 "$BUILD/audiofall" "$STAGING/usr/bin/audiofall"
install -m 644 "$ROOT/assets/audiofall.png" "$STAGING/usr/share/icons/hicolor/256x256/apps/audiofall.png"
install -m 644 "$ROOT/appimage/audiofall.desktop" "$STAGING/usr/share/applications/audiofall.desktop"
install -m 644 "$ROOT/LICENSE" "$STAGING/usr/share/doc/audiofall/copyright"

INSTALLED_KB="$(du -sk "$STAGING/usr" | cut -f1)"
cat > "$STAGING/DEBIAN/control" <<EOF
Package: audiofall
Version: $VERSION
Architecture: $ARCH
Maintainer: Pat Wendorf <dungeons@gmail.com>
Installed-Size: $INSTALLED_KB
Depends: libqt6core6 | libqt6core6t64, libqt6gui6 | libqt6gui6t64, libqt6widgets6 | libqt6widgets6t64, libqt6network6 | libqt6network6t64, libqt6multimedia6 | libqt6multimedia6t64, libgl1
Section: sound
Priority: optional
Homepage: https://github.com/patw/AudioFall
Description: Local microphone transcription and meeting summarization
 AudioFall records microphone audio, trims long quiet stretches, sends it to
 a Whisper-compatible service, and writes Markdown summaries using an
 OpenAI-compatible language model service.
EOF

cat > "$STAGING/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q /usr/share/applications || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
EOF
chmod 755 "$STAGING/DEBIAN/postinst"

dpkg-deb --build --root-owner-group "$STAGING" "$ROOT/${PACKAGE}.deb"
rm -rf "$ROOT/.deb_staging"
echo "Built: $ROOT/${PACKAGE}.deb"
