# AudioFall

AudioFall is a native Qt6 desktop application that records microphone audio, trims extended quiet sections, transcribes through a Whisper-compatible service, and writes Markdown summaries through an OpenAI-compatible LLM service.

It is a clean C++ implementation: no Python, virtual environment, pip packages, or AudioSumma configuration migration is required.

## Runtime requirements

- A microphone allowed by the operating system.
- A Whisper-compatible endpoint; default: `http://localhost:8081/inference`.
- An OpenAI-compatible chat-completions endpoint; default: `http://localhost:8080/v1`.

See [Running a local whisper.cpp server](docs/local-whisper.md) for the reference model, macOS `launchd` setup, and Linux/Windows service instructions. See [LLM providers and local summarization](docs/llm-providers.md) for hosted OpenAI-compatible providers, Anthropic compatibility notes, and local Ollama/llama.cpp options.

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
