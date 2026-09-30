# Validation record

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
