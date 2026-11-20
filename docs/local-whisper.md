# Embedded offline transcription

AudioFall links whisper.cpp directly. It does not start or connect to a transcription
server, bind a localhost port, run external commands, or download models at runtime.
The LLM used for summaries remains independently configurable.

## Engine and model

- whisper.cpp 1.9.5, pinned commit `d1be6fde11ac6e0407606b4e42fe72d34add8037`.
- Model: `ggml-large-v3-turbo-q5_0.bin`, the existing multilingual Turbo Q5 quantization.
- Model SHA-256: `394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2`.
- Model repository revision: `5359861c739e955e79d9a303bcbc70fb988958b1`.
- Engine and original Whisper weights: MIT; notices are bundled under `licenses/`.

CMake caches downloaded weights under `models/`. To reuse an existing copy, configure
with `-DAUDIOFALL_MODEL_FILE=/absolute/path/to/ggml-large-v3-turbo-q5_0.bin`.
Checksum mismatches stop the build. Do not commit weights to Git.

## Hardware and audio

macOS builds embed the Metal shaders, use Metal acceleration, and retry CPU model
initialization if GPU initialization fails. Windows/Linux builds use a portable CPU
baseline without requiring a GPU toolkit, vendor runtime, or CPU-specific CI flags.
Performance varies with hardware; long meetings can take significant time on slower CPUs.

The WAV reader validates mono PCM16 at 16 kHz and converts it to normalized float
samples. Existing RMS silence trimming is retained. Language defaults to automatic
detection; Settings offers English, German, and French. Other supported Whisper
language codes can be set in `transcription_language` in settings.json. The task is
always transcription, not translation. One model context is reused for each processing
batch; inference runs on the existing worker thread and reports progress.

## Installed assets

Paths are resolved relative to the executable, never the process working directory:

| Package | Model path |
| --- | --- |
| macOS app | `Contents/Resources/models/ggml-large-v3-turbo-q5_0.bin` |
| Windows ZIP | `models/ggml-large-v3-turbo-q5_0.bin` beside AudioFall.exe |
| Linux AppImage / Debian | `usr/share/audiofall/models/ggml-large-v3-turbo-q5_0.bin` |
| Linux source build | `models/ggml-large-v3-turbo-q5_0.bin` beside the binary |

Missing model errors tell the user to reinstall AudioFall; there is no hosted fallback.
Old `whisper_url` settings have no effect. Existing independently managed Whisper
servers may still be used by other programs; AudioFall neither requires nor stops them.

## Smoke verification

The same packaged executable can transcribe a WAV without starting the GUI or LLM:

```sh
/path/to/AudioFall --transcribe-file /absolute/path/to/sample.wav
# macOS: /Applications/AudioFall.app/Contents/MacOS/AudioFall --transcribe-file ...
```

The language comes from the normal AudioFall settings. This emits transcript text on
stdout, diagnostics on stderr, and a nonzero exit status on failure; it does not write
transcripts or summaries. No LLM call is made in this mode. Unit tests validate PCM
conversion, language/settings migration, path resolution, and the pipeline via mocks.
