# Running a local whisper.cpp server

AudioFall sends each trimmed recording to a Whisper-compatible HTTP endpoint. A local `whisper.cpp` server is a good way to keep recordings on your own machine and avoid an external transcription service.

The app expects the whisper.cpp server's HTTP inference endpoint. The default AudioFall URL is:

```text
http://localhost:8081/inference
```

If you use the setup below, the server listens on port `8088`, so set **Whisper URL** in AudioFall's Settings to:

```text
http://localhost:8088/inference
```

The server accepts the WAV format produced by AudioFall: mono, signed 16-bit PCM, 16 kHz.

## Model used by the reference setup

The reference macOS service uses:

```text
ggml-large-v3-turbo-q5_0.bin
```

This is the quantized Whisper large-v3 Turbo model in whisper.cpp's GGML format. It is stored outside the repository, under `~/models` in the reference setup. Do not check model files into this repository; they are large and may have separate distribution terms.

Other large GGUF files that may be present in a developer's model directory are Qwen language/multimodal models, not Whisper models. They are unrelated to AudioFall transcription.

## Build whisper.cpp

Clone whisper.cpp somewhere outside the AudioFall repository, then build and install it. The following is the same general layout used by the reference macOS setup:

```bash
git clone https://github.com/ggerganov/whisper.cpp.git
cd whisper.cpp
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/whisper-bin" \
  -DCMAKE_INSTALL_RPATH="$HOME/whisper-bin/lib" \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
cmake --build build --config Release --parallel
cmake --install build
```

Download `ggml-large-v3-turbo-q5_0.bin` using the model instructions in the whisper.cpp repository and place it at:

```text
$HOME/models/ggml-large-v3-turbo-q5_0.bin
```

The exact model location can be different; pass the actual path with `-m` below.

## macOS: launchd service

The reference machine runs whisper.cpp as a per-user LaunchAgent. Save the following as:

```text
~/Library/LaunchAgents/com.example.whisper-server.plist
```

Replace `/Users/you` with your home directory, or generate the file with your own paths before loading it.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.example.whisper-server</string>
  <key>ProgramArguments</key>
  <array>
    <string>/Users/you/whisper-bin/bin/whisper-server</string>
    <string>-m</string>
    <string>/Users/you/models/ggml-large-v3-turbo-q5_0.bin</string>
    <string>--convert</string>
    <string>-t</string>
    <string>6</string>
    <string>--host</string>
    <string>127.0.0.1</string>
    <string>--port</string>
    <string>8088</string>
    <string>-dev</string>
    <string>1</string>
  </array>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>WorkingDirectory</key>
  <string>/Users/you</string>
  <key>StandardOutPath</key>
  <string>/Users/you/whisper-bin/whisper-server.log</string>
  <key>StandardErrorPath</key>
  <string>/Users/you/whisper-bin/whisper-server.log</string>
</dict>
</plist>
```

The reference service uses `--host 0.0.0.0`; use `127.0.0.1` unless another machine needs to access the server. `-dev 1` selects GPU device 1. Use `-dev 0` or omit it when appropriate for your machine, and adjust `-t` to the number of CPU threads you want to allocate. `--convert` enables input conversion and requires `ffmpeg` on the server; it is harmless when AudioFall sends WAV files directly.

Load, inspect, and stop the service with:

```bash
launchctl bootstrap "gui/$(id -u)" "$HOME/Library/LaunchAgents/com.example.whisper-server.plist"
launchctl print "gui/$(id -u)/com.example.whisper-server"
launchctl bootout "gui/$(id -u)/com.example.whisper-server"
```

If you edit the plist, boot it out and bootstrap it again. Check the log if the service exits:

```bash
tail -f "$HOME/whisper-bin/whisper-server.log"
```

## Linux: systemd user service

After installing whisper.cpp and the model as shown above, create:

```text
~/.config/systemd/user/whisper-server.service
```

```ini
[Unit]
Description=Local whisper.cpp server
After=network.target

[Service]
ExecStart=%h/whisper-bin/bin/whisper-server -m %h/models/ggml-large-v3-turbo-q5_0.bin --convert -t 6 --host 127.0.0.1 --port 8088 -dev 0
WorkingDirectory=%h
Restart=on-failure
RestartSec=2

[Install]
WantedBy=default.target
```

Enable and inspect it with:

```bash
systemctl --user daemon-reload
systemctl --user enable --now whisper-server.service
systemctl --user status whisper-server.service
journalctl --user -u whisper-server.service -f
```

Use the appropriate device option for the machine. A CPU-only server can omit `-dev`; GPU acceleration depends on how whisper.cpp was built and which backend is available on the Linux system.

## Windows: run as a service or scheduled task

Build whisper.cpp with CMake using a Visual Studio Developer Command Prompt:

```bat
cmake -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Place the model somewhere outside the repository, for example:

```text
C:\Models\ggml-large-v3-turbo-q5_0.bin
```

Test the server in PowerShell:

```powershell
.\build\bin\Release\whisper-server.exe `
  -m C:\Models\ggml-large-v3-turbo-q5_0.bin `
  --convert -t 6 --host 127.0.0.1 --port 8088 -dev 0
```

Keep that console open while using AudioFall, or install the command as a Windows service with a service manager such as NSSM, or configure it as a logon/startup task in Task Scheduler. When using a service manager, set the working directory to the whisper.cpp installation directory and configure stdout/stderr logging. The important service command is the same as the PowerShell command above.

Set AudioFall's Whisper URL to:

```text
http://localhost:8088/inference
```

## Checking the endpoint

A running server should answer on its configured port. For a real transcription test, use a short mono 16 kHz PCM WAV:

```bash
curl -f http://localhost:8088/inference \
  -F file=@sample.wav \
  -F response_format=json
```

A successful response contains a JSON `text` field. AudioFall uses the same multipart `file` upload and reads that field.

## Long recordings

AudioFall trims silence first and then sends the complete trimmed WAV to Whisper. It allows up to 30 minutes for the transcription request because a long recording can take longer than its playback duration to process. If a local server is still working after that, increase the client timeout in the application or use shorter recordings/chunks.
