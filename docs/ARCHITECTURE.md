# Architecture and decision record

## ADR 001 — a testable CPU reference backend for the first alpha

The design blueprint targets a native C++/Qt shell, shared render graph, D3D11 video processing and WASAPI audio. This alpha implements the shell and project/editing contract first and uses FFmpeg child processes as the **reference** backend. It does not claim the planned GPU/audio engine is complete.

`Project` stores media references and ordered clips. Timeline coordinates are integer frames at an exact rational frame rate. Source offsets and speed are rational values. Every editor mutation copies the project, validates the result, and commits it as one undo step. JSON schema 1 stores timestamps as decimal strings to avoid floating-point integer loss. Unsupported schema versions are rejected without modifying the original file. QSaveFile performs atomic saves; one debounced recovery file is separate from the user's saved project.

`compileRender()` is shared by preview stills, cached playback and export. It emits FFmpeg filter nodes and a separate argument list. Media paths are passed as process arguments, never a command shell or filter string. Titles become local transparent PNGs drawn with Qt's font shaping. No user title text is executed as a filter expression. Higher tracks composite last; same-track conflicts retain insertion order. Audio is resampled to 48 kHz stereo, adjusted, delayed, mixed and limited. Timeline length determines the output frame count.

`Editor` owns asynchronous ffprobe/FFmpeg jobs. An export compiles an immutable snapshot, writes a unique partial file in the destination folder, then renames it only on success. Existing destinations are refused. A failed/cancelled render removes its partial file. Preview jobs are debounced and replaced when seeking; revision/frame checks reject stale results. Cached playback is invalidated on every edit. Qt Multimedia handles playback of that cache, not timeline rendering.

`Main.qml` exposes the implemented editing controls. Native file dialogs handle paths. Portable mode is selected only by an application-adjacent `portable.json`; its data folder must be writable. QLockFile prevents two instances from sharing the same recovery/cache folder.

## Known compromises

- Small projects only: whole-project snapshots (up to 60) and one decoder input per clip. No RAM budget, decoder pool or reverse-chunk strategy yet.
- PNG previews seek through a composed filtergraph. Scrubbing long timelines is not real-time.
- Preview resolution differs from export resolution; typography is rasterized at each target size. Composition semantics are shared, but pixel identity across resolutions is not promised.
- Source audio and video are linked. Trims are inspector edits; no roll/slip tools. Speed changes preserve source range, rounded down to full output frames.
- Crash recovery has one snapshot; no rolling journal or power-loss test qualification. A hard process kill may leave partial export/cache files. User originals are never edited.
- ffprobe runs in a child process but has no hostile-media sandbox. Network media paths are refused; only local files are imported. Inputs are not forensic evidence or security-isolated assets.
- H.264 Media Foundation is an optional explicit profile. Failure is surfaced; no hidden codec change or claim of certified compatibility.
- Qt Multimedia uses its deployed backend for cache playback. The future WASAPI engine will replace this path for interactive timeline audio.

## Next implementation gates

1. Windows clean-machine portable test, media corpus, large-project profiling, cache quota and render memory limits.
2. D3D11 renderer and audio clock using the same project contract; deterministic CPU/GPU frame comparisons.
3. Track controls, interactive trim handles, waveform/thumbnail indexing, proxies and keyframes.
4. Colour management, broader codec qualification, installer and signed distribution.
5. Optional local inference packs only after ordinary editing and offline packaging gates are stable.
