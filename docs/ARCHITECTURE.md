# Architecture and decision record

## ADR 001 — a testable CPU reference backend for the first alpha

The design blueprint targets a native C++/Qt shell, shared render graph, D3D11 video processing and WASAPI audio. This alpha implements the shell and project/editing contract first and uses FFmpeg child processes as the **reference** backend. It does not claim the planned GPU/audio engine is complete.

`Project` stores media references and ordered clips. Timeline coordinates are integer frames at an exact rational frame rate. Source offsets and speed are rational values. Every editor mutation copies the project, validates the result, and commits it as one undo step. JSON schema 2 stores track settings and audio-only clips and migrates schema 1 by adding neutral track settings. Both schemas store timestamps as decimal strings to avoid floating-point integer loss. Unsupported schema versions are rejected without modifying the original file. QSaveFile performs atomic saves; one debounced recovery file is separate from the user's saved project.

`compileRender()` is shared by preview stills, cached playback and export. It emits FFmpeg filter nodes and a separate argument list. Media paths are passed as process arguments, never a command shell or filter string. Titles become local transparent PNGs drawn with Qt's font shaping. No user title text is executed as a filter expression. Higher tracks composite last; same-track conflicts retain insertion order. Audio is resampled to 48 kHz stereo, adjusted, delayed, mixed and limited. Timeline length determines the output frame count.

`Editor` owns asynchronous ffprobe/FFmpeg jobs. An export compiles an immutable snapshot, writes a unique partial file in the destination folder, then renames it only on success. Existing destinations are refused. A failed/cancelled render removes its partial file. Preview jobs are debounced and replaced when seeking; revision/frame checks reject stale results. Cached playback is invalidated on every edit. Qt Multimedia handles playback of that cache, not timeline rendering.

`MediaAnalysis` streams 8 kHz mono PCM into a bounded peak accumulator (at most 6,001 bins per asset). Disk cache keys cover canonical path, size, modification time, duration and algorithm version. It processes one asset at a time with a 120-second timeout; assets over 24 hours have no waveform. Cache failures do not block editing. This is an overview, not a sample-accurate peak pyramid.

`KeyboardShortcuts` owns a validated registry and atomic per-user/portable preferences. QML disables timeline shortcuts during text entry and modal dialogs. Play requests automatically render a missing cache before starting playback.

`Main.qml` and `Timeline.qml` expose the implemented editing controls. Native file dialogs handle paths. Portable mode is selected only by an application-adjacent `portable.json`; its data folder must be writable. QLockFile prevents two instances from sharing the same recovery/cache folder.

## Known compromises

- Small projects only: whole-project snapshots (up to 60) and one decoder input per clip. No RAM budget, decoder pool or reverse-chunk strategy yet.
- PNG previews seek through a composed filtergraph. Scrubbing long timelines is not real-time.
- Preview resolution differs from export resolution; typography is rasterized at each target size. Composition semantics are shared, but pixel identity across resolutions is not promised.
- Source audio and video initially share a clip; detaching copies the audio timing to a new audio-only track and mutes the original clip. There is no pair resync yet. Edge trims preserve rational source offsets, including reverse; no roll/slip tools. Speed changes preserve source range, rounded down to full output frames.
- Crash recovery has one snapshot; no rolling journal or power-loss test qualification. A hard process kill may leave partial export/cache files. User originals are never edited.
- ffprobe runs in a child process but has no hostile-media sandbox. Network media paths are refused; FFmpeg and ffprobe input protocols are limited to file/pipe, including references inside local playlists. Media probing has a 30-second timeout. Inputs are not forensic evidence or security-isolated assets.
- H.264 Media Foundation is an optional explicit profile. Failure is surfaced; no hidden codec change or claim of certified compatibility.
- Qt Multimedia uses its deployed backend for cache playback. The future WASAPI engine will replace this path for interactive timeline audio.

## Next implementation gates

1. Windows clean-machine portable test, media corpus, large-project profiling, cache quota and render memory limits.
2. D3D11 renderer and audio clock using the same project contract; deterministic CPU/GPU frame comparisons.
3. Video thumbnails, multilevel waveforms, proxies, keyframes, advanced trims and linked-pair resync.
4. Colour management, broader codec qualification, installer and signed distribution.
5. Optional local inference packs only after ordinary editing and offline packaging gates are stable.
