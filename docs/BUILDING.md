# Build and test

## Windows (supported distribution target)

### One command

Install:

- **Visual Studio 2022** (Community or Build Tools) with the workload "Desktop development with C++". This includes CMake.
- **Python 3.9–3.12** from python.org, with "Add python.exe to PATH" ticked.
- **Git for Windows.**

Allow about 20 GB of disk space. Then, in PowerShell:

```powershell
git clone https://github.com/HNXS/Cutlery.git
cd Cutlery
./tools/Build-Local.ps1
```

If PowerShell refuses to run scripts, start it with `powershell -ExecutionPolicy Bypass -File tools/Build-Local.ps1`.

The script does what CI does. It downloads the pinned dependencies into `.deps/` and checks each one's hash:

- Qt 6.8.3 (via aqtinstall, no Qt account needed);
- the LGPL FFmpeg build;
- the recorded sound effects;
- ONNX Runtime;
- the AI models, converted in a private Python environment in `.deps/venv`;
- whisper.cpp, compiled once (it needs the Vulkan SDK, which the script fetches).

It then compiles, runs the tests and packages:

- `dist/Cutlery-0.5.0-win64-portable` is the application. Copy the whole folder, not just the EXE.
- `dist/Cutlery-0.5.0-AI-pack` is the optional AI pack. Copy its contents next to `Cutlery.exe`.

The first run takes 30–60 minutes, mostly downloads and the one-time whisper.cpp build. Later runs reuse `.deps` and `build`, so they take a few minutes. Delete `.deps` to fetch everything again.

Options:

- `-SkipAi` builds without the AI worker, models and speech recognition. This is much faster, and the result is just the portable build.
- `-SkipTests` packages without running the tests.
- `-QtRoot <path>` uses a Qt 6.8.3 msvc2022_64 installation you already have.

### By hand

These are the same steps the script runs, for a build without the AI pack:

```powershell
python -m pip install aqtinstall==3.3.0
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir C:/Qt --modules qtmultimedia
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
$ff = ./tools/Get-FFmpeg.ps1
$env:PATH = "$ff;" + $env:PATH
$sounds = ./tools/Get-Sounds.ps1
cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./tools/Package-Windows.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64 -FFmpegBin $ff -Sounds $sounds
```

For the AI pack, see the order in `tools/Build-Local.ps1`: `Get-OnnxRuntime.ps1`, `Get-Models.ps1`, `Get-VulkanSdk.ps1` and `Get-Whisper.ps1`, plus the matching CMake and packaging options.

### CI

The GitHub Actions workflow repeats the build, tests, QML startup, deployment and a deployed-executable startup check without SDK paths. It also runs `Build-Local.ps1`. It uploads the packages only from a manual run with "upload" ticked: each run is about 870 MB, more than the free Actions storage of a private repository.

## Linux (development verification only)

Install a C++20 compiler, CMake, Ninja, Qt 6.8.3 with Multimedia, OpenGL/EGL/XKB development dependencies, and FFmpeg/ffprobe on PATH. The Qt binary SDK also requires its matching ICU libraries. Configure with `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64`, build with CMake, and run `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`. Linux distribution is not a product target for this alpha.

## Tests and developer switches

`cutlery_tests` exercises rational overflow, project round trips (including Unicode/apostrophe paths), invalid timestamp ranges, source bounds, forward/reverse splits, ripple delete, source-aware trimming, magnetic insertion/reordering/ripple edits, queued file-drop placement, snapping, schema migration, track controls, undo/redo, SRT timing, shortcut preferences, waveform caching, actual rendering and audio presence. Integration fixtures are generated locally with FFmpeg. Tests fail if FFmpeg is unavailable; they do not silently skip the renderer.

`cutlery_ui_tests` loads the actual QML interface and checks library drags for video/audio/images, clip moves across tracks, per-track snap/magnetic controls, native URL drops, Escape cancellation, locked-track rejection, mouse edge trimming, keyboard split/undo, text-entry focus protection, shortcut remapping, and automatic render-then-play using Qt Multimedia. It requires the same Qt modules and FFmpeg runtime as the application.

`Cutlery --smoke-test` loads the QML interface and exits after 1.5 seconds. `Cutlery --demo --screenshot output.png` creates a temporary demonstration project, waits for a rendered frame, captures the window and exits. `Cutlery path/to/project.cutlery` opens a project. The demo is synthetic test data and is not loaded for ordinary starts.

`CUTLERY_FFMPEG` and `CUTLERY_FFPROBE` may point at explicit executable files for development. Otherwise the app looks in its `codecs` subdirectory, then PATH. Never point these at untrusted programs.

## Verification still required before a public release

Manual long-session edits on Windows; a clean offline Windows machine; optional H.264 encoder availability and output compatibility; high-DPI/multi-monitor/accessibility checks; larger/variable-rate/corrupt media; abrupt termination and low-disk recovery; performance and memory budgets; dependency redistribution review. CI green is a build/integration signal, not that full qualification.
