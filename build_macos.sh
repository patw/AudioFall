#!/bin/bash
# Build a self-contained macOS app bundle and DMG.  Ad-hoc signing is for local
# testing; production releases should replace it with Developer ID signing/notarization.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
ARCH="${1:-$(uname -m)}"
APP_VERSION="$(sed -nE 's/^project\(AudioFall VERSION ([0-9.]+).*$/\1/p' "$ROOT/CMakeLists.txt")"
[ -n "$APP_VERSION" ] || { echo "Could not determine AudioFall version" >&2; exit 1; }
QT_PREFIX="$(brew --prefix qt)"
export CMAKE_PREFIX_PATH="$QT_PREFIX"
export PATH="$QT_PREFIX/bin:$PATH"
cmake -S "$ROOT" -B "$ROOT/build_macos" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$ARCH"
cmake --build "$ROOT/build_macos" --parallel "$(sysctl -n hw.ncpu)"
ctest --test-dir "$ROOT/build_macos" --output-on-failure
APP="$ROOT/AudioFall.app"
rm -rf "$APP"
cp -R "$ROOT/build_macos/audiofall.app" "$APP"
mv "$APP/Contents/MacOS/audiofall" "$APP/Contents/MacOS/AudioFall"

# Use the same headphone image for Finder, Dock, and the mounted DMG volume.
ICON_PNG="$ROOT/assets/audiofall-headphones.png"
ICONSET="$APP/Contents/Resources/AudioFall.iconset"
mkdir -p "$ICONSET"
for size in 16 32 64 128 256 512; do
    sips -z "$size" "$size" "$ICON_PNG" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
    sips -z $((size * 2)) $((size * 2)) "$ICON_PNG" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AudioFall.icns"
rm -rf "$ICONSET"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>AudioFall</string>
<key>CFBundleDisplayName</key><string>AudioFall</string>
<key>CFBundleIdentifier</key><string>com.audiofall.app</string>
<key>CFBundleExecutable</key><string>AudioFall</string>
<key>CFBundleIconFile</key><string>AudioFall</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>$APP_VERSION</string>
<key>CFBundleVersion</key><string>$APP_VERSION</string>
<key>LSMinimumSystemVersion</key><string>12.0</string>
<key>NSMicrophoneUsageDescription</key><string>AudioFall records microphone audio for local transcription and summarization.</string>
</dict></plist>
PLIST
# Homebrew Qt modules have separate prefixes; plugin rpaths do not always
# include the aggregate Qt prefix or image-codec dependency directories.
DEPLOY_PATHS=("-libpath=$QT_PREFIX/lib")
for lib in brotli webp; do
    if prefix="$(brew --prefix "$lib" 2>/dev/null)"; then
        DEPLOY_PATHS+=("-libpath=$prefix/lib")
    fi
done
macdeployqt "$APP" -always-overwrite -no-codesign "${DEPLOY_PATHS[@]}"
# This Widgets app uses raster icons and native keyboard input, not PDF/SVG
# rendering or Qt's QML virtual keyboard. Exclude those optional plugins whose
# Homebrew module rpaths cannot be resolved by macdeployqt.
rm -f "$APP/Contents/PlugIns/imageformats/libqpdf.dylib" \
      "$APP/Contents/PlugIns/imageformats/libqsvg.dylib" \
      "$APP/Contents/PlugIns/imageformats/libqsvgicon.dylib" \
      "$APP/Contents/PlugIns/iconengines/libqsvgicon.dylib" \
      "$APP/Contents/PlugIns/platforminputcontexts/libqtvirtualkeyboardplugin.dylib"
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"
"$APP/Contents/MacOS/AudioFall" --transcribe-file "$ROOT/build_macos/_deps/whisper-src/samples/jfk.wav" > "$ROOT/build_macos/smoke-transcript.txt"
grep -qi "ask not what your country" "$ROOT/build_macos/smoke-transcript.txt"
STAGE="$ROOT/.dmg_staging"; DMG="$ROOT/AudioFall-macOS-$ARCH.dmg"
rm -rf "$STAGE" "$DMG"; mkdir -p "$STAGE"; cp -R "$APP" "$STAGE/"; ln -s /Applications "$STAGE/Applications"
cp "$APP/Contents/Resources/AudioFall.icns" "$STAGE/.VolumeIcon.icns"
hdiutil create -volname "AudioFall" -srcfolder "$STAGE" -ov -format UDRW "$DMG"
MOUNT="$(hdiutil attach -readwrite -noverify "$DMG" | grep -oE '/Volumes/.+$')"
if command -v SetFile >/dev/null 2>&1; then SetFile -a C "$MOUNT"; fi
hdiutil detach "$MOUNT" -quiet
hdiutil convert "$DMG" -format UDZO -o "$ROOT/AudioFall-macOS-$ARCH-compressed.dmg" -ov
mv "$ROOT/AudioFall-macOS-$ARCH-compressed.dmg" "$DMG"
rm -rf "$STAGE"
echo "Built $DMG"
