# AudioFall

AudioFall is a native Qt6 desktop application that records microphone audio, trims extended quiet sections, transcribes offline with bundled whisper.cpp and Whisper large-v3-turbo Q5, and writes Markdown summaries through an OpenAI-compatible LLM service.

It is a clean C++ implementation: no Python, virtual environment, pip packages, or AudioSumma configuration migration is required.

## Runtime requirements

- A microphone allowed by the operating system.
- An OpenAI-compatible chat-completions endpoint; default: `http://localhost:8080/v1`.

Transcription is fully local: release packages include the native inference engine and
`ggml-large-v3-turbo-q5_0.bin` (~547 MiB). No transcription server, Python, model
setup, or first-run download is required. Audio is not uploaded for transcription.
Settings → Whisper selects auto-detection or a known language (English/German/French).
Transcripts stay in the spoken language; Whisper does not translate them to English.
Legacy `whisper_url` settings are ignored and removed when settings are next saved.

**Summarization is separate:** transcripts are sent to the configured LLM endpoint.
Keep that endpoint local if the entire workflow must stay on-device. See
[LLM providers and local summarization](docs/llm-providers.md).

See [Embedded transcription and packaging](docs/local-whisper.md) for build-time
model caching, licenses, and hardware support.

The app requests **16 kHz mono signed-16-bit PCM** from the selected microphone. The current native implementation explicitly rejects devices that cannot provide that format rather than silently recording an incompatible WAV.

Settings are stored as independent JSON in the operating system's application-config location.

## Downloading releases

Tagged GitHub releases produce these native artifacts:

| OS | Artifact | Notes |
|---|---|---|
| macOS (Apple Silicon) | `AudioFall-macOS-arm64.dmg` | Bundled Qt app, ad-hoc signed like PengyCPP. Gatekeeper may require **Open Anyway** in Privacy & Security. |
| Windows x64 | `AudioFall-Windows-vX.Y.Z.zip` | Extract and run `AudioFall.exe`; Qt DLLs, plugins, and MSVC runtime are bundled. |
| Linux x86_64 | `AudioFall-x86_64.AppImage` | Portable self-contained app; make executable and run. |
| Debian/Ubuntu | `audiofall_X.Y.Z_amd64.deb` | Uses the distro Qt6 packages declared as dependencies. |

## Build from source

CMake downloads a pinned whisper.cpp source archive and the model on the first
configure, verifies their SHA-256 hashes, and copies the model into the application.
Allow roughly 547 MiB for the model plus build/package copies. Model weights are
cached under `models/` (gitignored); subsequent builds reuse them. A verified existing
copy may be supplied with `-DAUDIOFALL_MODEL_FILE=/absolute/path/to/ggml-large-v3-turbo-q5_0.bin`.
This is a **build-time** input, not an end-user model setting. Releases always bundle
that same model. Source and model downloads need internet at build time only.

### macOS

```bash
brew install qt cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
open build/audiofall.app
```

Create the self-contained app and DMG:

```bash
./build_macos.sh arm64
# AudioFall.app
# AudioFall-macOS-arm64.dmg
```

The packaging script uses the same practical approach as PengyCPP: `macdeployqt` bundles Qt, then `codesign --force --deep --sign -` applies an ad-hoc signature. It is intentionally not Developer-ID-signed or notarized.

### Linux

For Debian/Ubuntu development:

```bash
sudo apt install build-essential cmake qt6-base-dev qtmultimedia5-dev libgl-dev
./build_linux.sh
./build_linux/audiofall
```

Create a Debian package:

```bash
sudo apt install dpkg-dev
./build_deb.sh
# audiofall_<version>_amd64.deb
```

Create an AppImage manually:

```bash
sudo apt install libfuse2 fuse3
mkdir -p appimage/tools
wget -P appimage/tools https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -P appimage/tools https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x appimage/tools/*.AppImage
(cd appimage && ./build.sh)
# AudioFall-x86_64.AppImage
```

### Windows x64

Install a Qt 6 **MSVC 2022 x64** kit including Multimedia, CMake, and Visual Studio 2022 Build Tools with Desktop C++ development. From a VS 2022 Developer Command Prompt:

```bat
set QT6_DIR=C:\Qt\6.8.2\msvc2022_64
build_windows.bat
```

This creates `AudioFall-Windows\`, a self-contained folder ready to zip and distribute.

## Release automation

Push a version tag to build and attach all artifacts to a GitHub release:

```bash
git tag v0.1.0
git push origin v0.1.0
```

`.github/workflows/release.yml` builds macOS arm64 DMG, Linux x86_64 AppImage and `.deb`, and a Windows x64 ZIP. The normal CI workflow builds and tests all three platforms on branches and pull requests.

## License

MIT
