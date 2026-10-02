# What remains after 0.5

The [187-row feature matrix](FEATURE_STATUS.md) is the detailed status record. The [original blueprint](BLUEPRINT.md) remains the product/engineering scope. Existing code paths are alpha implementations, not production qualification or full CapCut parity.

## Requested creative features

| Feature | In scope | Current implementation |
|---|---|---|
| Übergänge / transitions | F054–F058 | 14 two-clip transitions (dissolve, dips, wipes, slides, zoom, circle, radial, pixelize) with duration control and equal-power audio crossfades. Easing curves, push/spin and custom presets remain open. |
| Zoom in/out | F036, F038, F056 | Animated zoom and pan through scale/position keyframes; zoom-in transition. A graph editor and one-click presets (Ken Burns) remain open. |
| Fade in/out | F054, F061 | Clip video/audio fade controls work. Two-clip transitions work; advanced fade curves remain open. |
| Track loudness | F061–F067 | Clip volume/fades and track mute/solo work. Track gain sliders, meters, pan, EQ/dynamics and LUFS normalization remain open. |

## Main development areas still open

1. **Responsive playback:** 0.4 streams playback live from the playhead with an audio clock. Remaining: GPU (D3D11) compositing for heavy multi-layer timelines, proxies for 4K sources, smooth scrubbing while dragging, and large-project performance.
2. **Editing tools:** multi-selection, groups, clip/attribute copy-paste, markers, in/out ranges, roll/slip/slide, overwrite, nested sequences and detached-pair resync.
3. **Motion and transitions:** keyframe graph editor and easing presets, more transitions, masks, chroma key, broader effects and original presets.
4. **Audio:** per-track gain and meters, pan, recording, EQ/compressor, noise reduction and beat tools. Pause removal and export loudness normalisation exist; loudness meters in the editor and true-peak limiting are open.
5. **Text and captions:** richer typography, title presets, text animation, word highlighting and transcript editing. Automatic captions work offline (AI pack).
6. **Colour and AI:** LUTs/curves/scopes, colour-managed HDR, tracking, stabilization, denoise. Person background removal and AI upscaling work offline with the optional AI pack (DirectML GPU or CPU); automatic captions use whisper.cpp offline (CPU). Open are sharper hair edges, object segmentation, image upscaling, word-level caption highlighting, GPU speech recognition and eye contact.
7. **Media and export:** collection/relink workflows, media search, broader codec qualification, hardware-encoder qualification on real GPUs, export queue/ranges, explicit bitrate/fps controls.
8. **Release qualification:** clean offline Windows testing, long sessions, high DPI/accessibility, crash/low-disk recovery, installer/signing and managed-device testing.

Cloud accounts/storage/collaboration, mobile/browser applications, CapCut proprietary assets and exact proprietary model output remain outside the local Windows scope.
