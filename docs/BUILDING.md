# Build and test

## Windows (supported distribution target)

Use Visual Studio 2022 Desktop development with C++, CMake 3.24+, Python and Qt **6.8.3 msvc2022_64**, including Qt Multimedia. The CI workflow downloads that SDK with aqtinstall 3.3.0. No Qt account is required for those public SDK archives.

```powershell
python -m pip install aqtinstall==3.3.0
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir C:/Qt --modules qtmultimedia
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
$ff = ./tools/Get-FFmpeg.ps1
$env:PATH = "$ff;" + $env:PATH
cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
./tools/Package-Windows.ps1 -QtRoot C:/Qt/6.8.3/msvc2022_64 -FFmpegBin $ff
```

The package goes to `dist/Cutlery-0.3.0-win64-portable`. Copy the entire directory. Do not copy only the EXE. The GitHub Actions workflow repeats the build, tests, QML startup, deployment and a deployed-executable startup check without SDK paths.

## Linux (development verification only)

Install a C++20 compiler, CMake, Ninja, Qt 6.8.3 with Multimedia, OpenGL/EGL/XKB development dependencies, and FFmpeg/ffprobe on PATH. The Qt binary SDK also requires its matching ICU libraries. Configure with `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64`, build with CMake, and run `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`. Linux distribution is not a product target for this alpha.

## Tests and developer switches

`cutlery_tests` exercises rational overflow, project round trips (including Unicode/apostrophe paths), invalid timestamp ranges, source bounds, forward/reverse splits, ripple delete, source-aware trimming, magnetic insertion/reordering/ripple edits, queued file-drop placement, snapping, schema migration, track controls, undo/redo, SRT timing, shortcut preferences, waveform caching, actual rendering and audio presence. Integration fixtures are generated locally with FFmpeg. Tests fail if FFmpeg is unavailable; they do not silently skip the renderer.

`cutlery_ui_tests` loads the actual QML interface and checks library drags for video/audio/images, clip moves across tracks, per-track snap/magnetic controls, native URL drops, Escape cancellation, locked-track rejection, mouse edge trimming, keyboard split/undo, text-entry focus protection, shortcut remapping, and automatic render-then-play using Qt Multimedia. It requires the same Qt modules and FFmpeg runtime as the application.

`Cutlery --smoke-test` loads the QML interface and exits after 1.5 seconds. `Cutlery --demo --screenshot output.png` creates a temporary demonstration project, waits for a rendered frame, captures the window and exits. `Cutlery path/to/project.cutlery` opens a project. The demo is synthetic test data and is not loaded for ordinary starts.

`CUTLERY_FFMPEG` and `CUTLERY_FFPROBE` may point at explicit executable files for development. Otherwise the app looks in its `codecs` subdirectory, then PATH. Never point these at untrusted programs.

## Verification still required before a public release

Manual long-session edits on Windows; a clean offline Windows machine; optional H.264 encoder availability and output compatibility; high-DPI/multi-monitor/accessibility checks; larger/variable-rate/corrupt media; abrupt termination and low-disk recovery; performance and memory budgets; dependency redistribution review. CI green is a build/integration signal, not that full qualification.
