# Cutlery

An original, local desktop video editor for Windows. **Version 0.1.0 is a functional alpha, not the completed 187-feature roadmap.** Built with C++20, Qt Quick 6.8 and a shared FFmpeg reference renderer.

![Cutlery alpha rendering a synthetic demonstration project](docs/images/editor.png)

## Try it on Windows

Open [Actions → Windows portable build](https://github.com/HNXS/Cutlery/actions/workflows/windows.yml), select a successful run, and download **Cutlery-0.1.0-win64-portable** from its artifacts. Extract the entire ZIP into a writable folder and run `Cutlery.exe`. The package is unsigned. No account, service, telemetry or model download is required by the application.

1. Import local video, audio or images. Double-click a media item to append it to the chosen track.
2. Drag clips to move them between tracks. Higher tracks appear on top. Overlapping clips on the same track follow insertion order.
3. Double-click a clip to seek its start; click the timeline to seek elsewhere. Split at the playhead, duplicate, delete, or adjust exact frame ranges in the inspector.
4. Add titles, edit their text and click **Apply text**. Import SRT to create editable, burned-in captions.
5. Adjust crop, position, scale, rotation, opacity, colour, speed, reverse, volume and fades. Changes are nondestructive.
6. Click **Render playback**, then **Play**. Re-render after edits. Still previews and exported files use the same composition compiler.
7. Save a `.cutlery` project. **Export video** writes a new MP4 or WebM. Keep your source media; the project references those files.

## What works

- Three visible timeline tracks; frame-based edit coordinates and rational source time; mixed media; linked source audio.
- Media probing/import and relinking; project presets; atomic JSON saves; single recovery snapshot; 60-step undo/redo.
- Split, inspector trims, move, duplicate, delete, track-local ripple delete; snapping to the playhead.
- Static transforms, equal-edge crop, horizontal flip, opacity, brightness/contrast/saturation, clip fades.
- Constant speed **0.25–4×**, reverse, volume/mute, stereo mixing with an output limiter.
- Rasterized Unicode titles, manual captions, SRT import/export and subtitle burn-in.
- Asynchronous rendered still previews, cached audio/video playback, export progress and cancellation.
- MP4 MPEG-4/AAC default; WebM VP9/Opus; optional Windows Media Foundation H.264/AAC (availability depends on the machine).
- Windows portable packaging, pinned codec download with checksum, dependency notices, and render regression tests.

## Boundaries of this alpha

This is a CPU reference implementation. It does **not** yet implement the planned D3D11/WASAPI real-time engine, proxies, waveform thumbnails, keyframes, transition library, masks, tracking, HDR colour management, hardware qualification, offline AI, transcription, an installer, or project-media collection. Long timelines and reversed clips can be slow and memory-intensive. The UI exposes three tracks even though the file format reserves more. The UI is not yet an accessibility-qualified release.

Only finite local media files are supported. Source audio/video remain linked. Timing is rounded to sequence frames; SRT timing is quantized on import. Colour operations are simple SDR adjustments, not a colour-managed grading pipeline. Output filenames must be new; exports never overwrite an existing file. Playback caches are disk files; remove the `cache` folder while Cutlery is closed if space is needed.

See [BUILDING](docs/BUILDING.md), [PORTABLE](docs/PORTABLE.md), [architecture](docs/ARCHITECTURE.md), [feature status](docs/FEATURE_STATUS.md), [the original blueprint](docs/BLUEPRINT.md), and [third-party notices](docs/THIRD_PARTY.md).

## Development

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Qt 6.8.3 (Core, Gui, Quick, QuickControls2, Multimedia, Test), MSVC 2022, CMake ≥3.24, FFmpeg and ffprobe are required. See the build guide for environment setup and packaging. The project has no application-wide open-source licence selected yet; dependency licences remain separate.
