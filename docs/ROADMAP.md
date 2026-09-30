# What remains after 0.4

The [187-row feature matrix](FEATURE_STATUS.md) is the detailed status record. The [original blueprint](BLUEPRINT.md) remains the product/engineering scope. Existing code paths are alpha implementations, not production qualification or full CapCut parity.

## Requested creative features

| Feature | In scope | Current implementation |
|---|---|---|
| Übergänge / transitions | F054–F058 | 14 two-clip transitions (dissolve, dips, wipes, slides, zoom, circle, radial, pixelize) with duration control and equal-power audio crossfades. Easing curves, push/spin and custom presets remain open. |
| Zoom in/out | F036, F038, F056 | Static clip scaling and timeline zoom work. Animated clip zoom needs property keyframes; zoom transitions are separate planned effects. |
| Fade in/out | F054, F061 | Clip video/audio fade controls work. Two-clip transitions work; advanced fade curves remain open. |
| Track loudness | F061–F067 | Clip volume/fades and track mute/solo work. Track gain sliders, meters, pan, EQ/dynamics and LUFS normalization remain open. |

## Main development areas still open

1. **Responsive playback:** 0.4 streams playback live from the playhead with an audio clock. Remaining: GPU (D3D11) compositing for heavy multi-layer timelines, proxies for 4K sources, smooth scrubbing while dragging, and large-project performance.
2. **Editing tools:** multi-selection, groups, clip/attribute copy-paste, markers, in/out ranges, roll/slip/slide, overwrite, nested sequences and detached-pair resync.
3. **Motion and transitions:** keyframes and easing, animated zoom/pan, more transitions, masks, chroma key, broader effects and original presets.
4. **Audio:** per-track gain and meters, pan, recording, crossfades, EQ/compressor, loudness analysis, noise reduction and beat tools.
5. **Text and captions:** richer typography, title presets, text animation, automatic local transcription, word highlighting and transcript editing.
6. **Colour and AI:** LUTs/curves/scopes, colour-managed HDR, tracking, stabilization, background removal, denoise/upscale, and optional offline model packs.
7. **Media and export:** collection/relink workflows, media search, broader codec qualification, hardware encoders, export queue/ranges and more quality controls.
8. **Release qualification:** clean offline Windows testing, long sessions, high DPI/accessibility, crash/low-disk recovery, installer/signing and managed-device testing.

Cloud accounts/storage/collaboration, mobile/browser applications, CapCut proprietary assets and exact proprietary model output remain outside the local Windows scope.
