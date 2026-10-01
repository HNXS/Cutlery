# Validation record

## Windows portable build — 2026-10-01, presenter overlays (0.5 development)

[GitHub Actions run 36845035491](https://github.com/HNXS/Cutlery/actions/runs/36845035491) completed successfully for commit `493f4e12d88fedb24ff1a8c65b8b450510fec478` on Windows Server 2022 with MSVC 2022, Qt 6.8.3 and LGPL FFmpeg 8.1. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- the new `overlayStyles` engine test, with pixel checks of a circle with border ring and shadow, rounded corners, green-screen removal, keying combined with a circle and animated scale, schema 6 persistence, clip bounds, one-step position edits, and display-rotation probing;
- the `presenterControls` interface test: corner placement, mouse drag to move and corner drag to resize in the viewer, undo, shape and green-screen controls.

A 1280×720 still of a green-screen speaker in a circle with border and shadow over a test pattern was inspected visually. It found and fixed a defect where keyed areas inside the border showed the border colour instead of the background.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36845035491/artifacts/11153650251) (131,913,117 bytes, expires 2026-10-31). Archive SHA-256: `360d03f5389d2de064f868c98c2ef1d15b7dd4c9283d4b21a28da0307371353e`.

## Windows portable build — 2026-10-01, export presets (0.5 development)

[GitHub Actions run 36841760041](https://github.com/HNXS/Cutlery/actions/runs/36841760041) completed successfully for commit `acbff13afe51a6b505ea037cda4bc6626b8200ee` on Windows Server 2022 with MSVC 2022, Qt 6.8.3 and LGPL FFmpeg 8.1. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- the new `exportProfiles` engine test: candidate lists, output sizes, skipping a missing encoder with a cached result, and real AV1 (upscaled), VP9, ProRes and MPEG-4 exports checked with ffprobe for codec, size and audio codec;
- the `exportDialog` interface test: the YouTube preset fills the fields, a manual change becomes "Custom", and the extension follows the format.

The H.264 case accepts either a working encoder or the "No H264 encoder" message. Passing CTest output does not record which path the runner took, so hardware-encoder behaviour still needs checking on a real PC with an NVIDIA, AMD or Intel GPU.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36841760041/artifacts/11152345050) (131,887,842 bytes, expires 2026-10-31). Archive SHA-256: `7b1c1cd100db808effe15a95a93f43a3c831ee9c823d500b33b7e61deaef0df0`.

## Windows portable build — 2026-10-01, keyframes (0.5 development)

[GitHub Actions run 36834388932](https://github.com/HNXS/Cutlery/actions/runs/36834388932) completed successfully for commit `1475f8a36ebdc141e56a0d80320741f78c7f06fd` on Windows Server 2022 with MSVC 2022, Qt 6.8.3 and the shipped LGPL FFmpeg 8.1. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- the new `keyframes` engine test: interpolation, trim/split/speed rules, schema 5 roundtrip, validation; measured animated scale, position plus rotation, and opacity; windowed frames inside animations; animated volume loudness;
- the `keyframeControls` interface test: the ◇ toggle, auto-key on value change, timeline markers, previous/next navigation, and removal keeping the last value.

The preceding run 36833107826 failed because `perspective` is GPL-only and absent from the LGPL build. Geometry now uses `rotate`, `scale=eval=frame` and `overlay`. Local tests now run against the same FFmpeg 8.1 LGPL build (21 engine tests passed).

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36834388932/artifacts/11148399888) (131,863,714 bytes, expires 2026-10-31). Archive SHA-256: `0145e3b239b1005dfddd7b8b286a23aa81cc2dd18c65dbaf411cd1dd6534d88c`.

## Windows portable build — 2026-09-30, transitions (0.5 development)

[GitHub Actions run 36785406583](https://github.com/HNXS/Cutlery/actions/runs/36785406583) completed successfully for commit `a37fd73aac105c2bd366fe758731de1d6f16eb50` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The CTest suites include:

- the new `transitions` engine test: dissolve blend at the cut, windowed frames matching the full render inside a transition, wipe halves, equal-power audio level across the crossfade, exact mix length, model rules, schema 4 roundtrip and split;
- the `transitionMarkers` interface test: clicking **+** on a cut adds a dissolve and the inspector shows it.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36785406583/artifacts/11129756210) (131,846,407 bytes, expires 2026-10-30). Archive SHA-256: `2dc8a430ff87c2f0c30167bf7591c92f2efb76424a7aaabff24c9384e17c396b`.

Local (Ubuntu, Qt 6.4.2): 20 engine tests passed. A 640×360 still at a transition cut with 1080p H.264 sources rendered in 0.6 s. Audible crossfades on real hardware still require manual checking.

## Windows portable build — 2026-09-30, filmstrip thumbnails (0.5 development)

[GitHub Actions run 36781416020](https://github.com/HNXS/Cutlery/actions/runs/36781416020) completed successfully for commit `3c4a94d22da77bcc8027fb03dec70986095a9b89` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. It covered compilation, both CTest suites (the new `thumbnailStrips` engine test, and interface checks that video and image clips show loaded filmstrip tiles), QML startup, packaging, and packaged-executable startup. The application version is still 0.4.0 until the 0.5 feature set is complete.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36781416020/artifacts/11127622752) (131,828,795 bytes, expires 2026-10-30). Archive SHA-256: `869bb9498cda9cb5072d60cd55af12a44f3d17e05b9c475fc4e47a3dc6b1737d`.

Local (Ubuntu, Qt 6.4.2): 19 engine tests passed. Keyframe-only extraction took 1.2 s for 3 minutes of 1080p H.264, against 15 s when decoding every frame. The filmstrip and library poster rendering was visually inspected in application screenshots.

## Windows portable build — 2026-09-30, version 0.4.0

[GitHub Actions run 36777216642](https://github.com/HNXS/Cutlery/actions/runs/36777216642) completed successfully for commit `6ea8a79eaaf3579028b81f1674413c654612150e` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. It covered compilation, both CTest suites (engine including windowed rendering and live playback; interface including Space/K live play and pause), QML startup, packaging, and packaged-executable startup without SDK paths. The preceding run 36774621755 failed at compilation (`QtVideo::MapMode` is not in Qt 6.8.3); that was fixed in `6ea8a79`.

[Download Cutlery-0.4.0-win64-portable](https://github.com/HNXS/Cutlery/actions/runs/36777216642/artifacts/11126345710) (131,809,976 bytes). The CI artifact expires on 2026-10-30. Archive SHA-256: `3e19f4b5fb7a683f6c4f5eeab4247c89739b847c82262ca1372e940e61ad031b`.

The CI runner has no audio device, so its playback tests use the wall clock. Audible output and A/V offset on real hardware still require manual checking.

## Local development run — 2026-09-30, version 0.4.0

The network policy of this development container blocks the Qt SDK download, so Qt 6.8.3 was unavailable. Ubuntu 24.04's Qt 6.4.2 (GCC 13.3, FFmpeg 6.1.1) was used with a local-only relaxed version check. The Windows Qt 6.8.3 CI run is the authoritative build.

- Engine suite: **18 passed, 0 failed**. The new `windowedRender` test compares one-frame preview windows with whole-timeline renders at clip starts, fades, overlay edges and a reversed 2× clip. It also checks exact raw frame/PCM sizes for streamed playback and wall-clock pacing. The job test plays live, edits during playback (automatic restart), reaches the end, and pauses at an advanced frame.
- Interface suite: not meaningful on Qt 6.4. Unmodified single-key shortcuts (Space, K, Right) do not fire in that offscreen build, and the unchanged 0.3 suite crashes there. The 0.4 UI playback flow passed on Windows CI (above).
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
