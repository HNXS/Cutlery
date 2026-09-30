# Validation record

## Local development run — 2026-09-30, version 0.4.0

The network policy of this development container blocks the Qt SDK download, so Qt 6.8.3 was unavailable. Ubuntu 24.04's Qt 6.4.2 (GCC 13.3, FFmpeg 6.1.1) was used with a local-only relaxed version check. The Windows Qt 6.8.3 CI run is the authoritative build.

- Engine suite: **18 passed, 0 failed**. The new `windowedRender` test compares one-frame preview windows with whole-timeline renders at clip starts, fades, overlay edges and a reversed 2× clip. It also checks exact raw frame/PCM sizes for streamed playback and wall-clock pacing. The job test plays live, edits during playback (automatic restart), reaches the end, and pauses at an advanced frame.
- Interface suite: not meaningful on Qt 6.4. Unmodified single-key shortcuts (Space, K, Right) do not fire in that offscreen build, and the unchanged 0.3 suite crashes there. The 0.4 UI playback flow is verified by Windows CI.
- Benchmark on a 3-minute 1080p30 H.264 file, 4-core container, 640×360 preview:

| Operation | 0.3 | 0.4 |
|---|---|---|
| Preview frame at 0:10 | 1,377 ms | 130 ms |
| Preview frame at 1:30 | 11,890 ms | 129 ms |
| Preview frame at 2:30 | 19,629 ms | 119 ms |
| Play from 1:30 → first frame (960×540) | cache render of the full timeline first (6,815 ms per 30 s) | 164 ms |

Audible playback and A/V offset on real Windows audio hardware still require manual checking.

## Windows portable build — 2026-09-30, version 0.3.0

[GitHub Actions run 36768658353](https://github.com/HNXS/Cutlery/actions/runs/36768658353) completed successfully for commit `d80150d78c218ca3e3457ad6dd48915c876dac00` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. Compilation including the native icon resource, both engine/interface test suites, QML startup, packaging and packaged-executable startup without SDK paths passed.

[Download Cutlery-0.3.0-win64-portable](https://github.com/HNXS/Cutlery/actions/runs/36768658353/artifacts/11122917795) (131,803,167 bytes). The CI artifact expires on 2026-10-30. Archive SHA-256: `823a0e62430bf0ae7615e8b4c9c1a067d59eba9c6d76ab113060d95e676d95c0`.

## Local development run — 2026-09-30, version 0.3.0

Built with GCC 13.3, Qt 6.8.3 and CMake 4.4.3 on Ubuntu 24.04.

- Engine suite: **17 passed, 0 failed, 0 skipped**, including setup/cleanup. New coverage checks magnetic compaction/reordering/cross-track moves/trims/deletion/duplication, independent track timing, undo, lock guards, schema 2 migration and schema 3 roundtrip.
- File-drop model checks cover ordered insertion, a missing file between valid files, undo/redo, destination-track renumbering during asynchronous import, locked destinations, and removal of a destination while probing.
- Interface suite: **4 passed, 0 failed, 0 skipped**, including setup/cleanup. Actual QML gestures cover library-to-track drops for generated video/audio/images, same-track and cross-track clip moves, per-track snapping, Magnet toggle/reorder/undo, Escape cancellation for both drag types, locked-drop rejection, OS URL-MIME drops to a track and library, plus the existing trimming/keyboard/playback flow. No QML warnings were emitted.
- Visual inspection: actual demo window and the original vector app icon rendered at 256 pixels. The Windows ICO contains 16, 24, 32, 48, 64, 128 and 256 pixel variants; Windows resource compilation is part of CI.

These are automated event-level drop checks, not manual Explorer testing on a user's managed Windows desktop. Audible output, high-DPI behavior and long-session performance still require manual qualification.

## Windows portable build — 2026-09-30, version 0.2.0

[GitHub Actions run 36689916787](https://github.com/HNXS/Cutlery/actions/runs/36689916787) completed successfully for commit `4cfd7ba2aa84187f39aff8b8eeeb5496991447ae` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. Configure/compile, engine and interface tests, QML startup, portable packaging, and packaged-executable startup with SDK paths removed all passed.

[Download Cutlery-0.2.0-win64-portable](https://github.com/HNXS/Cutlery/actions/runs/36689916787/artifacts/11085730483) (131,738,605 bytes). The CI artifact expires on 2026-10-30; later successful workflow runs can provide fresh artifacts. Archive SHA-256: `eb9e3979246fb202edf113dada069025b464650aa2da633d35afd9012f6367a4`.

## Local development run — 2026-09-30, version 0.2.0

Built with GCC 13.3, Qt 6.8.3 and CMake 4.4.3 on Ubuntu 24.04. This is a development verification host, not a supported end-user distribution.

- `cutlery_tests`: **15 passed, 0 failed, 0 skipped**, including test setup/cleanup.
- `cutlery_ui_tests`: **3 passed, 0 failed, 0 skipped**, including test setup/cleanup. Actual QML mouse/keyboard checks cover drag trimming, split/undo, typing without triggering timeline commands, retained title drafts, shortcut remapping, and render-then-play with an advancing Qt Multimedia position followed by pause. No QML warnings were emitted.
- Actual-render coverage: generate source video/audio, compose a timed image overlay, render MP4, check 60 output frames, inspect foreground/background colours at multiple times, verify audio data, and compare a shared-graph preview still.
- Asynchronous job coverage: image import, timeline insertion, WebM export, refusal to overwrite an existing destination, playback-cache rendering/invalidation, cancellation and partial-file cleanup.
- Offline boundary: a local HLS playlist referencing HTTPS is rejected by the codec protocol whitelist before network access.
- Track rendering checks: picture hiding, track mute, solo suppression, and audio-only clips are checked against decoded video pixels and PCM audio. Detach-audio and undo are exercised.
- Model coverage: rational overflow, Unicode/relative-path JSON roundtrip, schema 1 migration and schema 2 roundtrip, invalid time/source bounds, split/reverse/ripple semantics, exact forward/reverse trims, edge snapping, locked-track rejection, undo/redo and SRT roundtrip.
- Waveforms: split-byte PCM peak accumulation, extraction from actual generated audio, disk cache reuse, and changed-file fingerprint invalidation. Shortcut preferences: conflict/invalid-key rejection, remapping, persistence, reset, and failed-save rollback.
- QML startup smoke test: passed. A rendered demo window was captured and visually inspected; `docs/images/editor.png` is that actual application window.

The host has no usable audio device; audible playback was not manually evaluated. The standalone Qt test executable was used because the downloaded CTest runner crashed on this host. Windows CI uses the runner-provided CTest and repeats these tests, then checks the packaged executable without SDK paths. Consult the linked GitHub Actions run for its independent result.

See BUILDING.md for qualification work that remains. No performance, HDR, clean-machine, hardware-encoder or production-readiness claims are implied by these checks.
