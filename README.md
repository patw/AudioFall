# AudioFall

A native Qt6 desktop application for recording microphone audio, trimming extended silence, transcribing against a local Whisper-compatible server, and creating Markdown summaries through an OpenAI-compatible LLM endpoint.

## macOS development

```bash
brew install qt cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/audiofall
```

## Distribution bundle

```bash
./build_macos.sh arm64
```

This produces `AudioFall.app` and `AudioFall-macOS-arm64.dmg`. The build uses ad-hoc signing for local testing. Public distribution should use Developer ID signing and notarization.

## Requirements

- A microphone available to macOS and approved in Privacy & Security.
- A Whisper-compatible endpoint, default `http://localhost:8081/inference`.
- An OpenAI-compatible LLM endpoint, default `http://localhost:8080/v1`.

Settings are stored independently as JSON in the platform app-config directory. No AudioSumma/Python settings are imported.
