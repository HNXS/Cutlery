# Feature status — 0.5.0 alpha

The original 187 rows remain the design scope, not a claim of completion. “Implemented (alpha)” means a present code path, not production qualification. Partial rows list their actual boundary. Acceptance criteria in `features.json` remain the original future gates.

| ID | Feature | Alpha status | Current boundary |
|---|---|---|---|
| F001 | Create, open, recent projects and settings | Partial | Create/open/save and presets; no recent list. |
| F002 | Media bin, folders, collections and search | Partial | Flat media bin; no folders/search. |
| F003 | Drag/drop and background media analysis | Partial | Library-to-track and local file drops, async probing and waveform analysis, partial-error reporting; no directory recursion or cancellable import queue. |
| F004 | Nondestructive media references | Implemented (alpha) | Relative project references; originals never edited. |
| F005 | Missing-media relink and replace | Partial | Validated source relinking; no batch relink. |
| F006 | Collect Project and portable media bundle | Planned | Not implemented in this alpha. |
| F007 | Project schema migrations | Implemented (alpha) | Reads schema 1/2 without moving clips; writes schema 3 with stable track IDs and snapping/magnetic modes. Older versions cannot read new saves. |
| F008 | Atomic save, autosave and rolling backup | Partial | Atomic save and one recovery file; no rolling backups. |
| F009 | Undo/redo and deep edit history | Partial | 60 whole-project snapshots; no deep history persistence. |
| F010 | Crash and corrupted-cache recovery | Partial | Recovery snapshot only. |
| F011 | Multiple open projects and project versions | Planned | Not implemented in this alpha. |
| F012 | Local search index for metadata and dialogue | Planned | Not implemented in this alpha. |
| F013 | Semantic media search and object search | Planned | Not implemented in this alpha. |
| F014 | Person search with user labels | Planned | Not implemented in this alpha. |
| F015 | Local templates and replaceable media slots | Planned | Not implemented in this alpha. |
| F016 | Local brand styles and reusable assets | Planned | Not implemented in this alpha. |
| F017 | Interchange OTIO, EDL and loss report | Planned | Not implemented in this alpha. |
| F018 | FCPXML and AAF interchange | Planned | Not implemented in this alpha. |
| F019 | CapCut project import | Planned | Not implemented in this alpha. |
| F020 | Multitrack video, audio, images and text | Implemented (alpha) | Up to 64 named tracks with video/audio/images/titles; large-project performance not qualified. |
| F021 | Layer order, overlays and adjustment tracks | Partial | Overlays and layer order; no adjustment tracks. |
| F022 | Linked A/V, unlink and resync | Partial | Video and audio initially share timing; detach creates an independent audio-only clip. No pair resync. |
| F023 | Groups, track lock, mute, solo and hide | Partial | Track lock, audio mute/solo and picture hide; no clip groups. |
| F024 | Snapping and optional magnetic/ripple mode | Implemented (alpha) | Per-track edge snapping and magnetic insertion/reorder/ripple edits. Master edge-snap switch is independent of Magnet; locks respected. |
| F025 | Insert, overwrite and reorder | Partial | Magnetic insert and reorder; free-position drops. No overwrite edit mode. |
| F026 | Split, trim, delete and duplicate | Implemented (alpha) | Split, drag-edge/source-aware trims, inspector trims, delete and duplicate; each committed edit is undoable. |
| F027 | Ripple trim, roll, slip and slide | Partial | Magnetic track trims ripple subsequent clips; no roll, slip, slide or linked-pair propagation. |
| F028 | Nested sequences and compound clips | Planned | Not implemented in this alpha. |
| F029 | Copy/paste clips and selected attributes | Implemented (alpha) | Copy (Ctrl+C) and paste at the playhead (Ctrl+V) within the session and across projects (the media comes along); the copy goes to its own track when free there, otherwise the nearest free track or a new one. Paste look (colour, effects, LUT) or paste all attributes (Ctrl+Alt+V: also transform, keyframes, shape, keying, volume, fades) onto the selected clip, in one undo step. No system-clipboard exchange or multi-clip selection. |
| F030 | Markers, in/out and timeline navigation | Partial | Frame and previous/next edit navigation; start/end shortcuts. No markers or in/out ranges. |
| F031 | Timeline zoom and frame-accurate seeking | Partial | Timeline zoom/fit; seeking renders only the playhead frame, so latency no longer grows with position (about 0.1 s for 1080p H.264 on a 4-core development container). Not qualified on the hardware profiles. |
| F032 | JKL shuttle and reverse audition | Planned | Not implemented in this alpha. |
| F033 | Constant speed 0.1x to 100x | Partial | 0.25–4x only, preserves pitch via atempo. |
| F034 | Speed ramps and curve editor | Planned | Not implemented in this alpha. |
| F035 | Reverse and freeze frames | Partial | Reverse only. |
| F036 | Property keyframes and graph editor | Partial | Keyframes for scale, position, rotation, opacity and volume, with smooth (ease in/out) interpolation. Set at the playhead in the inspector, shown as timeline markers, kept attached through trim, split and speed changes. No graph editor or per-key easing choice in the UI. |
| F037 | Frame-rate changes and mixed-rate sequences | Planned | Not implemented in this alpha. |
| F038 | Scale, position, rotate, anchor and opacity | Partial | Scale/position/rotation/opacity; fixed center anchor. |
| F039 | Crop, mirror and aspect ratios | Partial | Equal-edge crop, horizontal mirror, canvas presets. |
| F040 | Perspective and corner pin | Planned | Not implemented in this alpha. |
| F041 | Blend modes and alpha compositing | Partial | Normal alpha compositing only. |
| F042 | Rectangle and ellipse masks | Partial | Overlay shapes: rounded rectangle (adjustable radius) and centre-square circle, antialiased; no feather or free positioning of the mask. |
| F043 | Polygon, freeform and Bezier masks | Planned | Not implemented in this alpha. |
| F044 | Luma key, chroma key and spill suppression | Partial | Colour key (green/blue presets) with tolerance and edge softness, plus automatic green/blue spill suppression; no luma key or colour picker. |
| F045 | Background replacement and canvas | Planned | Not implemented in this alpha. |
| F046 | Adjustment layers and filter ordering | Planned | Not implemented in this alpha. |
| F047 | Blur variants, mosaic and pixelation | Partial | Gaussian blur of a clip's picture (inspector slider), and blur or mosaic (pixelate) areas: rectangles on an upper track that affect everything below, placed and resized in the preview, position keyframable, strength 0–100 %. Rectangles only; no ellipse/feathered shape, animated size or face tracking. |
| F048 | Glow, grain, vignette and sharpen | Implemented (alpha) | Per clip: contrast-adaptive sharpening, glow (screened soft copy), vignette and moving luma film grain, 0..1 each; alpha is preserved for overlays and cutouts. |
| F049 | Glitch, shake, VHS, retro and film effects | Planned | Not implemented in this alpha. |
| F050 | Distortion, edge, lens and stylize effects | Planned | Not implemented in this alpha. |
| F051 | Motion blur | Planned | Not implemented in this alpha. |
| F052 | Frame blending slow motion | Planned | Not implemented in this alpha. |
| F053 | Camera-like and pseudo-3D effects | Planned | Not implemented in this alpha. |
| F054 | Dissolve, fade and dip to color | Implemented (alpha) | Two-clip dissolve and dip to black/white centred on a cut, with equal-power audio crossfades; clip fades as before. |
| F055 | Wipe, slide, push and directional transitions | Partial | Wipe left/right, slide left/right/up/down, smooth left. No push. |
| F056 | Zoom, spin, stretch and geometric transitions | Partial | Zoom in, circle open and radial. No spin or stretch. |
| F057 | Blur, glitch and light transitions | Partial | Pixelize only. |
| F058 | Transition duration and curve editing | Partial | Duration 0.1–3 s in the inspector, limited by both clips. No easing curves. |
| F059 | Audio extraction and linked source streams | Partial | Detach audio reuses source media on an independent track; no extraction to a standalone audio file or resync. |
| F060 | Waveform generation and peak pyramids | Partial | Async bounded mono waveform overview cached by file fingerprint; follows trim/speed/reverse. No multilevel pyramid. |
| F061 | Volume, gain, pan, mute and fades | Partial | Volume/mute/fades, no pan. |
| F062 | Multitrack mixing and channel layouts | Partial | 48 kHz stereo only. |
| F063 | 5.1 and configurable audio routing | Planned | Not implemented in this alpha. |
| F064 | Voice-over and audio recording | Planned | Not implemented in this alpha. |
| F065 | EQ, compressor, limiter and noise gate | Partial | Fixed output limiter only. |
| F066 | Reverb and delay | Planned | Not implemented in this alpha. |
| F067 | Loudness measurement and normalization | Partial | Export option: two-pass loudness normalisation. FFmpeg's EBU R128 meter measures the whole mix, then one gain reaches the target (-14 YouTube/streaming, -16 podcast, -23 EBU R128) and a peak limiter holds peaks about 1.5 dB below full scale. In the editor, left/right peak meters follow playback and the export dialog measures the whole mix (integrated LUFS, true peak, gain to the target). No true-peak limiting, loudness-range display or per-clip normalisation. |
| F068 | Pitch shift and tempo preservation | Partial | Speed with pitch preservation; no independent pitch. |
| F069 | Silence detection and removal | Partial | Remove pauses on a clip: FFmpeg silencedetect with an adjustable threshold and minimum length, 0.12 s kept around speech, preview of count and total length, then one undoable ripple edit on the clip's track and on its detached audio. Other tracks keep their timing. |
| F070 | Beat detection and markers | Planned | Not implemented in this alpha. |
| F071 | Beat-synchronized edits | Planned | Not implemented in this alpha. |
| F072 | Classical noise reduction | Planned | Not implemented in this alpha. |
| F073 | Speech denoise and enhancement | Planned | Not implemented in this alpha. |
| F074 | Vocal isolation and music separation | Planned | Not implemented in this alpha. |
| F075 | Dereverb | Planned | Not implemented in this alpha. |
| F076 | Voice changer | Planned | Not implemented in this alpha. |
| F077 | Voice conversion and custom voice | Planned | Not implemented in this alpha. |
| F078 | Plain and rich text; Unicode shaping | Partial | Plain Unicode Qt titles; no rich text. |
| F079 | Font browser, favorites and local font files | Planned | Not implemented in this alpha. |
| F080 | Size, weight, spacing, kerning and alignment | Partial | Size, fixed bold/center/wrap; no typography editor. |
| F081 | Outline, shadow, glow and text background | Partial | Fixed shadow only. |
| F082 | Text transforms and per-character animation | Planned | Not implemented in this alpha. |
| F083 | Original titles, lower thirds and presets | Partial | Original title templates: lower third with plate, lower third with accent line (both slide in from the left and fade), and title card; name/role from the text lines, accent colour, scale and position from the preview frame. Three templates, no preset library, no per-template animation choice. |
| F084 | Manual captions and subtitle track | Partial | Editable title clips used as captions. |
| F085 | SRT import/export and TXT import | Partial | SRT roundtrip; no TXT. |
| F086 | WebVTT and ASS/SSA interoperability | Planned | Not implemented in this alpha. |
| F087 | Subtitle burn-in and sidecar export | Partial | Burn-in and SRT. |
| F088 | Local transcription and automatic captions | Partial | whisper.cpp v1.9.4 (whisper-cli with Vulkan GPU and per-CPU-level backends, built from the pinned commit) with Whisper large-v3-turbo q5_0 and Silero VAD, via the cutlery-ai transcribe task; language auto or chosen; word-level transcripts cached per media file and language, grouped into lines of at most 42 characters; cues mapped through each audible clip's trim, position and speed onto an 'AI captions' track, replaced on every run. GPU through Vulkan with CPU fallback (GPU unverified on CI, which has none); no speaker labels. |
| F089 | Word timing and karaoke highlights | Partial | Whisper word timestamps (one cue per word) grouped into lines; captions keep each word's start (schema 8). Styles: karaoke (spoken word in a highlight colour), one word at a time, plain; changeable per caption in the inspector. Rendered from one sprite per caption with a per-frame crop. No animation of sprite captions, no per-word styling beyond the highlight colour. |
| F090 | Sentence segmentation and caption layout | Planned | Not implemented in this alpha. |
| F091 | Transcript-based editing | Planned | Not implemented in this alpha. |
| F092 | Filler-word detection | Planned | Not implemented in this alpha. |
| F093 | Optional subtitle translation | Planned | Not implemented in this alpha. |
| F094 | Offline text-to-speech | Planned | Not implemented in this alpha. |
| F095 | German text-to-speech | Planned | Not implemented in this alpha. |
| F096 | Local dubbing and duration fitting | Planned | Not implemented in this alpha. |
| F097 | Shapes, arrows, callouts and speech bubbles | Planned | Not implemented in this alpha. |
| F098 | Stickers, icons and overlays | Partial | Imported images as overlays only. |
| F099 | PNG, SVG, GIF and image-sequence overlays | Partial | Raster still images only. |
| F100 | Collage and reusable layout templates | Planned | Not implemented in this alpha. |
| F101 | Stock music and sound effects import | Partial | User audio file import only. |
| F102 | Commercial-use rights metadata | Planned | Not implemented in this alpha. |
| F103 | Exposure, brightness, contrast and saturation | Partial | Brightness/contrast/saturation; no exposure model. |
| F104 | Temperature, tint, vibrance and tonal controls | Partial | Per clip: temperature (light colour temperature with lightness kept), tint (green–magenta), vibrance, and shadows/highlights through a master curve, each −1..1, plus the existing brightness, contrast and saturation. Not keyframable; no exposure/whites/blacks split or auto white balance. |
| F105 | HSL, RGB curves and master curves | Planned | Not implemented in this alpha. |
| F106 | Color wheels and automatic adjustment | Planned | Not implemented in this alpha. |
| F107 | LUT import, intensity and original presets | Partial | .cube and .3dl 3D LUTs per clip (tetrahedral interpolation) with a strength mix; missing files are skipped and flagged. Eight original one-click looks built from the colour and look controls; no bundled LUT files or LUT browser. |
| F108 | Histogram, waveform and vectorscope | Planned | Not implemented in this alpha. |
| F109 | Rec.709, sRGB and range conversions | Planned | Not implemented in this alpha. |
| F110 | HDR input and SDR tone mapping | Planned | Not implemented in this alpha. |
| F111 | 10-bit and HDR export | Planned | Not implemented in this alpha. |
| F112 | SDR-to-HDR workflow | Planned | Not implemented in this alpha. |
| F113 | Display management and multi-monitor HDR | Planned | Not implemented in this alpha. |
| F114 | Point and planar tracking | Planned | Not implemented in this alpha. |
| F115 | Object tracking and editable paths | Planned | Not implemented in this alpha. |
| F116 | Face detection and face tracking | Planned | Not implemented in this alpha. |
| F117 | Tracked masks, blur and overlays | Planned | Not implemented in this alpha. |
| F118 | Stabilization and auto crop | Planned | Not implemented in this alpha. |
| F119 | Rolling-shutter and lens correction | Planned | Not implemented in this alpha. |
| F120 | Smart crop and automatic reframing | Planned | Not implemented in this alpha. |
| F121 | Portrait background removal | Partial | U²-Net human segmentation (ONNX Runtime, DirectML GPU or CPU) in the separate cutlery-ai worker; 8 fps analysis with temporal smoothing, cached grayscale FFV1 matte per media file, interpolated and applied as alpha in preview, playback and export. Model ships in the optional AI pack. No hair refinement. |
| F122 | Interactive object segmentation | Planned | Not implemented in this alpha. |
| F123 | Portrait retouch and enhancement | Planned | Not implemented in this alpha. |
| F124 | Temporal denoise and flicker reduction | Planned | Not implemented in this alpha. |
| F125 | Image and video upscaling | Partial | Real-ESRGAN realesr-general-x4v3 (converted to ONNX by tools/convert-realesrgan.py) in the cutlery-ai worker; DirectML GPU inference with CPU fallback; every frame of the used range on overlapping tiles, Lanczos to 4x the source (at most 2160p), cached as ProRes 422 per media file and size; clips render from the copy. Video only; no images, denoise strength or face restoration. |
| F126 | Optical flow and frame interpolation | Planned | Not implemented in this alpha. |
| F127 | Deblur | Planned | Not implemented in this alpha. |
| F128 | Video relighting | Planned | Not implemented in this alpha. |
| F129 | Object/text/people removal and inpainting | Planned | Not implemented in this alpha. |
| F130 | Image merge and photo editing | Planned | Not implemented in this alpha. |
| F131 | Automatic scene detection | Planned | Not implemented in this alpha. |
| F132 | Long video to shorts and highlight proposals | Planned | Not implemented in this alpha. |
| F133 | Intelligent proxy generation | Planned | Not implemented in this alpha. |
| F134 | Local AI image generator | Planned | Not implemented in this alpha. |
| F135 | Local AI video generator | Planned | Not implemented in this alpha. |
| F136 | Conversational editing assistant | Planned | Not implemented in this alpha. |
| F137 | AI writer and script-to-video | Planned | Not implemented in this alpha. |
| F138 | Local avatar library and animation | Planned | Not implemented in this alpha. |
| F139 | Custom avatar clone | Planned | Not implemented in this alpha. |
| F140 | AI design and automatic layouts | Planned | Not implemented in this alpha. |
| F141 | Meme, logo, art and sticker generation | Planned | Not implemented in this alpha. |
| F142 | AI dialogue scene assembly | Planned | Not implemented in this alpha. |
| F143 | MP4, MOV, MKV, AVI and WebM import | Partial | FFmpeg-based import; limited corpus qualification. |
| F144 | MPEG, MTS/M2TS and MXF import | Planned | Not implemented in this alpha. |
| F145 | H.264 and HEVC decoding | Partial | Decoder availability follows bundled FFmpeg; no hardware qualification. |
| F146 | AV1, VP9, ProRes and DNx decoding | Partial | Decoder availability follows bundled FFmpeg; limited corpus. |
| F147 | Other FFmpeg-supported codecs | Partial | Local finite files accepted through FFmpeg; not all codecs tested. |
| F148 | WAV, MP3, AAC, FLAC, OGG, Opus and M4A | Partial | FFmpeg-based audio import; limited corpus. |
| F149 | PNG, JPEG, WebP, BMP, TIFF and GIF | Partial | Still images; animated formats not qualified. |
| F150 | HEIF/HEIC import | Planned | Not implemented in this alpha. |
| F151 | Numbered image sequences | Planned | Not implemented in this alpha. |
| F152 | Variable frame rate and timestamp repair | Planned | Not implemented in this alpha. |
| F153 | MP4/H.264/AAC export | Implemented (alpha) | H.264 via NVENC, AMF, Quick Sync or Media Foundation, whichever passes a probe; no software H.264 encoder in the LGPL build. |
| F154 | MOV and mezzanine/alpha workflow | Partial | ProRes 422 HQ/422/LT MOV export with PCM audio; no alpha-channel export. |
| F155 | WebM VP9/Opus software export | Implemented (alpha) | VP9/Opus export profile (integration coverage to expand). |
| F156 | HEVC and AV1 hardware/software export | Implemented (alpha) | HEVC via hardware or Media Foundation; AV1 via hardware or SVT-AV1 software. |
| F157 | Audio-only and image-sequence export | Partial | Audio-only export of the timeline mix: MP3 (LAME VBR), AAC in M4A, or WAV (24-bit at Maximum quality, else 16-bit), 48 kHz stereo, with the same loudness normalisation as video. No image-sequence export. |
| F158 | Resolution, fps, quality, bitrate and format controls | Partial | Presets plus format, four quality levels and output height (720p–4K); no explicit bitrate or fps override. |
| F159 | Hardware encoder selection and CPU fallback | Implemented (alpha) | Candidates are probed with a short test encode at the real size and arguments, cached per session. Software formats always remain available. Not qualified on real NVIDIA/AMD/Intel hardware yet. |
| F160 | Queue, range export, progress and cancellation | Partial | One full-sequence job with progress/cancel; no queue/range. |
| F161 | Interrupted render retry and recovery | Planned | Not implemented in this alpha. |
| F162 | Convert/compress and export presets | Planned | Not implemented in this alpha. |
| F163 | Proxy and optimized-media workflows | Planned | Not implemented in this alpha. |
| F164 | Frame, thumbnail and render caches | Partial | Keyframe filmstrips (up to 200 tiles per asset) cached by file fingerprint; live playback needs no render cache. No frame cache or cache quotas. |
| F165 | Async jobs, priorities and cancellation | Partial | Async import/render; no scheduler/priority system. |
| F166 | RAM/VRAM budgets and decoder pooling | Planned | Not implemented in this alpha. |
| F167 | GPU device loss and CPU fallback | Planned | Not implemented in this alpha. |
| F168 | Dark UI, dockable panels and searchable tools | Partial | Dark resizable panels and original app icon; no docking/search. |
| F169 | High DPI, scaling and multi-monitor | Partial | Qt scaling support; manual qualification outstanding. |
| F170 | Shortcuts, customization and command search | Partial | 31 customizable commands, conflict checks and portable settings; no command search or complete CapCut keymap. |
| F171 | Accessible focus, labels and errors | Planned | Not implemented in this alpha. |
| F172 | Background jobs, progress and tooltips | Partial | Job state/progress/cancel; tooltips incomplete. |
| F173 | Per-user Windows installer | Planned | Not implemented in this alpha. |
| F174 | Portable folder and path resolver | Partial | Portable marker/path resolver and package workflow; clean-machine gate outstanding. |
| F175 | Optional signing and enterprise distribution | Planned | Not implemented in this alpha. |
| F176 | Local model pack import and validation | Planned | Not implemented in this alpha. |
| F177 | Internal effect API and future plugin SDK | Planned | Not implemented in this alpha. |
| F178 | Offline operation and no telemetry | Implemented (alpha) | Application has no network/telemetry client. |
| F179 | Regression tests and hardware benchmark suite | Partial | Model and actual rendering tests; no hardware benchmark suite. |
| F180 | Documentation and feature status | Implemented (alpha) | Build/user/architecture/status documentation included. |
| F181 | Cloud projects, storage, sync and rendering | Excluded | Not implemented in this alpha. |
| F182 | Accounts, billing, subscriptions and entitlements | Excluded | Not implemented in this alpha. |
| F183 | Online collaboration, review, comments and teams | Excluded | Not implemented in this alpha. |
| F184 | Direct social publishing and account integration | Excluded | Not implemented in this alpha. |
| F185 | Online marketplaces and CapCut proprietary libraries | Excluded | Not implemented in this alpha. |
| F186 | Mobile, macOS and browser editor | Excluded | Not implemented in this alpha. |
| F187 | Cloud-only proprietary model endpoints and exact model output parity | Excluded | Not implemented in this alpha. |
