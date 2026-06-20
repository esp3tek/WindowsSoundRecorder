# Windows Sound Recorder

Minimal recorder that captures **the sound Windows plays** (the system audio output),
**not the microphone**, and saves it as `.wav` or `.mp3`.

Pure C + Win32, **no external dependencies**. Uses WASAPI loopback for capture and
Windows Media Foundation for MP3 encoding.

## Usage

1. Run `WindowsSoundRecorder.exe`.
2. The destination folder defaults to the Desktop; change it with **Browse...**.
3. Pick the **Format**: WAV or MP3 320 kbps.
4. Press **Record**; press **Stop** to finish. Files are named
   `Recording_YYYY-MM-DD_HHMMSS.wav` / `.mp3`.

**Autostart:** tick the *Autostart* box to arm the recorder. It starts recording when it
detects sound on the output and stops after 5 seconds of silence (the trailing silence is
not written), then re-arms for the next sound. Untick the box to stop/disarm.

Minimize the window to send it to the system tray. Left-click the tray icon to restore;
right-click for **Show / Exit**.

## Formats

- **WAV** — PCM 16-bit stereo at the system mixer rate (usually 48 kHz).
- **MP3** — 320 kbps CBR via Media Foundation.

## Build

Requires MSVC (`cl.exe`) or MinGW-w64 (`gcc.exe`) on PATH.

```bat
generate_icon.ps1   :: generates icon.ico (first time only)
build.bat
```

Run the unit tests (WAV header, float→PCM16, peak meter, filename builder):

```bat
build_test.bat
```

## License

MIT — see `LICENSE`.
