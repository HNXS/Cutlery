# Feature status — 0.1.0 alpha

The original 187 rows remain the design scope, not a claim of completion. “Implemented (alpha)” means a present code path, not production qualification. Partial rows list their actual boundary. Acceptance criteria in `features.json` remain the original future gates.

| ID | Feature | Alpha status | Current boundary |
|---|---|---|---|
| F001 | Create, open, recent projects and settings | Partial | Create/open/save and presets; no recent list. |
| F002 | Media bin, folders, collections and search | Partial | Flat media bin; no folders/search. |
| F003 | Drag/drop and background media analysis | Partial | Async analysis; no file drop. |
| F004 | Nondestructive media references | Implemented (alpha) | Relative project references; originals never edited. |
| F005 | Missing-media relink and replace | Partial | Validated source relinking; no batch relink. |
| F006 | Collect Project and portable media bundle | Planned | Not implemented in 0.1.0. |
| F007 | Project schema migrations | Planned | Not implemented in 0.1.0. |
| F008 | Atomic save, autosave and rolling backup | Partial | Atomic save and one recovery file; no rolling backups. |
| F009 | Undo/redo and deep edit history | Partial | 60 whole-project snapshots; no deep history persistence. |
| F010 | Crash and corrupted-cache recovery | Partial | Recovery snapshot only. |
| F011 | Multiple open projects and project versions | Planned | Not implemented in 0.1.0. |
| F012 | Local search index for metadata and dialogue | Planned | Not implemented in 0.1.0. |
| F013 | Semantic media search and object search | Planned | Not implemented in 0.1.0. |
| F014 | Person search with user labels | Planned | Not implemented in 0.1.0. |
| F015 | Local templates and replaceable media slots | Planned | Not implemented in 0.1.0. |
| F016 | Local brand styles and reusable assets | Planned | Not implemented in 0.1.0. |
| F017 | Interchange OTIO, EDL and loss report | Planned | Not implemented in 0.1.0. |
| F018 | FCPXML and AAF interchange | Planned | Not implemented in 0.1.0. |
| F019 | CapCut project import | Planned | Not implemented in 0.1.0. |
| F020 | Multitrack video, audio, images and text | Partial | Three visible tracks with video/audio/images/titles. |
| F021 | Layer order, overlays and adjustment tracks | Partial | Overlays and layer order; no adjustment tracks. |
| F022 | Linked A/V, unlink and resync | Planned | Not implemented in 0.1.0. |
| F023 | Groups, track lock, mute, solo and hide | Planned | Not implemented in 0.1.0. |
| F024 | Snapping and optional magnetic/ripple mode | Partial | Playhead snap and track-local ripple delete. |
| F025 | Insert, overwrite and reorder | Planned | Not implemented in 0.1.0. |
| F026 | Split, trim, delete and duplicate | Partial | Split/delete/duplicate; trims via inspector. |
| F027 | Ripple trim, roll, slip and slide | Planned | Not implemented in 0.1.0. |
| F028 | Nested sequences and compound clips | Planned | Not implemented in 0.1.0. |
| F029 | Copy/paste clips and selected attributes | Planned | Not implemented in 0.1.0. |
| F030 | Markers, in/out and timeline navigation | Partial | Frame navigation only. |
| F031 | Timeline zoom and frame-accurate seeking | Partial | Zoom and rendered frame seeking; latency not qualified. |
| F032 | JKL shuttle and reverse audition | Planned | Not implemented in 0.1.0. |
| F033 | Constant speed 0.1x to 100x | Partial | 0.25–4x only, preserves pitch via atempo. |
| F034 | Speed ramps and curve editor | Planned | Not implemented in 0.1.0. |
| F035 | Reverse and freeze frames | Partial | Reverse only. |
| F036 | Property keyframes and graph editor | Planned | Not implemented in 0.1.0. |
| F037 | Frame-rate changes and mixed-rate sequences | Planned | Not implemented in 0.1.0. |
| F038 | Scale, position, rotate, anchor and opacity | Partial | Scale/position/rotation/opacity; fixed center anchor. |
| F039 | Crop, mirror and aspect ratios | Partial | Equal-edge crop, horizontal mirror, canvas presets. |
| F040 | Perspective and corner pin | Planned | Not implemented in 0.1.0. |
| F041 | Blend modes and alpha compositing | Partial | Normal alpha compositing only. |
| F042 | Rectangle and ellipse masks | Planned | Not implemented in 0.1.0. |
| F043 | Polygon, freeform and Bezier masks | Planned | Not implemented in 0.1.0. |
| F044 | Luma key, chroma key and spill suppression | Planned | Not implemented in 0.1.0. |
| F045 | Background replacement and canvas | Planned | Not implemented in 0.1.0. |
| F046 | Adjustment layers and filter ordering | Planned | Not implemented in 0.1.0. |
| F047 | Blur variants, mosaic and pixelation | Planned | Not implemented in 0.1.0. |
| F048 | Glow, grain, vignette and sharpen | Planned | Not implemented in 0.1.0. |
| F049 | Glitch, shake, VHS, retro and film effects | Planned | Not implemented in 0.1.0. |
| F050 | Distortion, edge, lens and stylize effects | Planned | Not implemented in 0.1.0. |
| F051 | Motion blur | Planned | Not implemented in 0.1.0. |
| F052 | Frame blending slow motion | Planned | Not implemented in 0.1.0. |
| F053 | Camera-like and pseudo-3D effects | Planned | Not implemented in 0.1.0. |
| F054 | Dissolve, fade and dip to color | Partial | Clip video/audio fades only. |
| F055 | Wipe, slide, push and directional transitions | Planned | Not implemented in 0.1.0. |
| F056 | Zoom, spin, stretch and geometric transitions | Planned | Not implemented in 0.1.0. |
| F057 | Blur, glitch and light transitions | Planned | Not implemented in 0.1.0. |
| F058 | Transition duration and curve editing | Planned | Not implemented in 0.1.0. |
| F059 | Audio extraction and linked source streams | Planned | Not implemented in 0.1.0. |
| F060 | Waveform generation and peak pyramids | Planned | Not implemented in 0.1.0. |
| F061 | Volume, gain, pan, mute and fades | Partial | Volume/mute/fades, no pan. |
| F062 | Multitrack mixing and channel layouts | Partial | 48 kHz stereo only. |
| F063 | 5.1 and configurable audio routing | Planned | Not implemented in 0.1.0. |
| F064 | Voice-over and audio recording | Planned | Not implemented in 0.1.0. |
| F065 | EQ, compressor, limiter and noise gate | Partial | Fixed output limiter only. |
| F066 | Reverb and delay | Planned | Not implemented in 0.1.0. |
| F067 | Loudness measurement and normalization | Planned | Not implemented in 0.1.0. |
| F068 | Pitch shift and tempo preservation | Partial | Speed with pitch preservation; no independent pitch. |
| F069 | Silence detection and removal | Planned | Not implemented in 0.1.0. |
| F070 | Beat detection and markers | Planned | Not implemented in 0.1.0. |
| F071 | Beat-synchronized edits | Planned | Not implemented in 0.1.0. |
| F072 | Classical noise reduction | Planned | Not implemented in 0.1.0. |
| F073 | Speech denoise and enhancement | Planned | Not implemented in 0.1.0. |
| F074 | Vocal isolation and music separation | Planned | Not implemented in 0.1.0. |
| F075 | Dereverb | Planned | Not implemented in 0.1.0. |
| F076 | Voice changer | Planned | Not implemented in 0.1.0. |
| F077 | Voice conversion and custom voice | Planned | Not implemented in 0.1.0. |
| F078 | Plain and rich text; Unicode shaping | Partial | Plain Unicode Qt titles; no rich text. |
| F079 | Font browser, favorites and local font files | Planned | Not implemented in 0.1.0. |
| F080 | Size, weight, spacing, kerning and alignment | Partial | Size, fixed bold/center/wrap; no typography editor. |
| F081 | Outline, shadow, glow and text background | Partial | Fixed shadow only. |
| F082 | Text transforms and per-character animation | Planned | Not implemented in 0.1.0. |
| F083 | Original titles, lower thirds and presets | Planned | Not implemented in 0.1.0. |
| F084 | Manual captions and subtitle track | Partial | Editable title clips used as captions. |
| F085 | SRT import/export and TXT import | Partial | SRT roundtrip; no TXT. |
| F086 | WebVTT and ASS/SSA interoperability | Planned | Not implemented in 0.1.0. |
| F087 | Subtitle burn-in and sidecar export | Partial | Burn-in and SRT. |
| F088 | Local transcription and automatic captions | Planned | Not implemented in 0.1.0. |
| F089 | Word timing and karaoke highlights | Planned | Not implemented in 0.1.0. |
| F090 | Sentence segmentation and caption layout | Planned | Not implemented in 0.1.0. |
| F091 | Transcript-based editing | Planned | Not implemented in 0.1.0. |
| F092 | Filler-word detection | Planned | Not implemented in 0.1.0. |
| F093 | Optional subtitle translation | Planned | Not implemented in 0.1.0. |
| F094 | Offline text-to-speech | Planned | Not implemented in 0.1.0. |
| F095 | German text-to-speech | Planned | Not implemented in 0.1.0. |
| F096 | Local dubbing and duration fitting | Planned | Not implemented in 0.1.0. |
| F097 | Shapes, arrows, callouts and speech bubbles | Planned | Not implemented in 0.1.0. |
| F098 | Stickers, icons and overlays | Partial | Imported images as overlays only. |
| F099 | PNG, SVG, GIF and image-sequence overlays | Partial | Raster still images only. |
| F100 | Collage and reusable layout templates | Planned | Not implemented in 0.1.0. |
| F101 | Stock music and sound effects import | Partial | User audio file import only. |
| F102 | Commercial-use rights metadata | Planned | Not implemented in 0.1.0. |
| F103 | Exposure, brightness, contrast and saturation | Partial | Brightness/contrast/saturation; no exposure model. |
| F104 | Temperature, tint, vibrance and tonal controls | Planned | Not implemented in 0.1.0. |
| F105 | HSL, RGB curves and master curves | Planned | Not implemented in 0.1.0. |
| F106 | Color wheels and automatic adjustment | Planned | Not implemented in 0.1.0. |
| F107 | LUT import, intensity and original presets | Planned | Not implemented in 0.1.0. |
| F108 | Histogram, waveform and vectorscope | Planned | Not implemented in 0.1.0. |
| F109 | Rec.709, sRGB and range conversions | Planned | Not implemented in 0.1.0. |
| F110 | HDR input and SDR tone mapping | Planned | Not implemented in 0.1.0. |
| F111 | 10-bit and HDR export | Planned | Not implemented in 0.1.0. |
| F112 | SDR-to-HDR workflow | Planned | Not implemented in 0.1.0. |
| F113 | Display management and multi-monitor HDR | Planned | Not implemented in 0.1.0. |
| F114 | Point and planar tracking | Planned | Not implemented in 0.1.0. |
| F115 | Object tracking and editable paths | Planned | Not implemented in 0.1.0. |
| F116 | Face detection and face tracking | Planned | Not implemented in 0.1.0. |
| F117 | Tracked masks, blur and overlays | Planned | Not implemented in 0.1.0. |
| F118 | Stabilization and auto crop | Planned | Not implemented in 0.1.0. |
| F119 | Rolling-shutter and lens correction | Planned | Not implemented in 0.1.0. |
| F120 | Smart crop and automatic reframing | Planned | Not implemented in 0.1.0. |
| F121 | Portrait background removal | Planned | Not implemented in 0.1.0. |
| F122 | Interactive object segmentation | Planned | Not implemented in 0.1.0. |
| F123 | Portrait retouch and enhancement | Planned | Not implemented in 0.1.0. |
| F124 | Temporal denoise and flicker reduction | Planned | Not implemented in 0.1.0. |
| F125 | Image and video upscaling | Planned | Not implemented in 0.1.0. |
| F126 | Optical flow and frame interpolation | Planned | Not implemented in 0.1.0. |
| F127 | Deblur | Planned | Not implemented in 0.1.0. |
| F128 | Video relighting | Planned | Not implemented in 0.1.0. |
| F129 | Object/text/people removal and inpainting | Planned | Not implemented in 0.1.0. |
| F130 | Image merge and photo editing | Planned | Not implemented in 0.1.0. |
| F131 | Automatic scene detection | Planned | Not implemented in 0.1.0. |
| F132 | Long video to shorts and highlight proposals | Planned | Not implemented in 0.1.0. |
| F133 | Intelligent proxy generation | Planned | Not implemented in 0.1.0. |
| F134 | Local AI image generator | Planned | Not implemented in 0.1.0. |
| F135 | Local AI video generator | Planned | Not implemented in 0.1.0. |
| F136 | Conversational editing assistant | Planned | Not implemented in 0.1.0. |
| F137 | AI writer and script-to-video | Planned | Not implemented in 0.1.0. |
| F138 | Local avatar library and animation | Planned | Not implemented in 0.1.0. |
| F139 | Custom avatar clone | Planned | Not implemented in 0.1.0. |
| F140 | AI design and automatic layouts | Planned | Not implemented in 0.1.0. |
| F141 | Meme, logo, art and sticker generation | Planned | Not implemented in 0.1.0. |
| F142 | AI dialogue scene assembly | Planned | Not implemented in 0.1.0. |
| F143 | MP4, MOV, MKV, AVI and WebM import | Partial | FFmpeg-based import; limited corpus qualification. |
| F144 | MPEG, MTS/M2TS and MXF import | Planned | Not implemented in 0.1.0. |
| F145 | H.264 and HEVC decoding | Partial | Decoder availability follows bundled FFmpeg; no hardware qualification. |
| F146 | AV1, VP9, ProRes and DNx decoding | Partial | Decoder availability follows bundled FFmpeg; limited corpus. |
| F147 | Other FFmpeg-supported codecs | Partial | Local finite files accepted through FFmpeg; not all codecs tested. |
| F148 | WAV, MP3, AAC, FLAC, OGG, Opus and M4A | Partial | FFmpeg-based audio import; limited corpus. |
| F149 | PNG, JPEG, WebP, BMP, TIFF and GIF | Partial | Still images; animated formats not qualified. |
| F150 | HEIF/HEIC import | Planned | Not implemented in 0.1.0. |
| F151 | Numbered image sequences | Planned | Not implemented in 0.1.0. |
| F152 | Variable frame rate and timestamp repair | Planned | Not implemented in 0.1.0. |
| F153 | MP4/H.264/AAC export | Partial | Optional H.264 Media Foundation; MPEG-4 default. |
| F154 | MOV and mezzanine/alpha workflow | Planned | Not implemented in 0.1.0. |
| F155 | WebM VP9/Opus software export | Implemented (alpha) | VP9/Opus export profile (integration coverage to expand). |
| F156 | HEVC and AV1 hardware/software export | Planned | Not implemented in 0.1.0. |
| F157 | Audio-only and image-sequence export | Planned | Not implemented in 0.1.0. |
| F158 | Resolution, fps, quality, bitrate and format controls | Partial | Canvas/fps and three fixed codec profiles; no bitrate control. |
| F159 | Hardware encoder selection and CPU fallback | Planned | Not implemented in 0.1.0. |
| F160 | Queue, range export, progress and cancellation | Partial | One full-sequence job with progress/cancel; no queue/range. |
| F161 | Interrupted render retry and recovery | Planned | Not implemented in 0.1.0. |
| F162 | Convert/compress and export presets | Planned | Not implemented in 0.1.0. |
| F163 | Proxy and optimized-media workflows | Planned | Not implemented in 0.1.0. |
| F164 | Frame, thumbnail and render caches | Partial | Still and playback cache; no quotas or thumbnails. |
| F165 | Async jobs, priorities and cancellation | Partial | Async import/render; no scheduler/priority system. |
| F166 | RAM/VRAM budgets and decoder pooling | Planned | Not implemented in 0.1.0. |
| F167 | GPU device loss and CPU fallback | Planned | Not implemented in 0.1.0. |
| F168 | Dark UI, dockable panels and searchable tools | Partial | Dark resizable panels; no docking/search. |
| F169 | High DPI, scaling and multi-monitor | Partial | Qt scaling support; manual qualification outstanding. |
| F170 | Shortcuts, customization and command search | Partial | Save/open/undo/redo shortcuts only. |
| F171 | Accessible focus, labels and errors | Planned | Not implemented in 0.1.0. |
| F172 | Background jobs, progress and tooltips | Partial | Job state/progress/cancel; tooltips incomplete. |
| F173 | Per-user Windows installer | Planned | Not implemented in 0.1.0. |
| F174 | Portable folder and path resolver | Partial | Portable marker/path resolver and package workflow; clean-machine gate outstanding. |
| F175 | Optional signing and enterprise distribution | Planned | Not implemented in 0.1.0. |
| F176 | Local model pack import and validation | Planned | Not implemented in 0.1.0. |
| F177 | Internal effect API and future plugin SDK | Planned | Not implemented in 0.1.0. |
| F178 | Offline operation and no telemetry | Implemented (alpha) | Application has no network/telemetry client. |
| F179 | Regression tests and hardware benchmark suite | Partial | Model and actual rendering tests; no hardware benchmark suite. |
| F180 | Documentation and feature status | Implemented (alpha) | Build/user/architecture/status documentation included. |
| F181 | Cloud projects, storage, sync and rendering | Excluded | Not implemented in 0.1.0. |
| F182 | Accounts, billing, subscriptions and entitlements | Excluded | Not implemented in 0.1.0. |
| F183 | Online collaboration, review, comments and teams | Excluded | Not implemented in 0.1.0. |
| F184 | Direct social publishing and account integration | Excluded | Not implemented in 0.1.0. |
| F185 | Online marketplaces and CapCut proprietary libraries | Excluded | Not implemented in 0.1.0. |
| F186 | Mobile, macOS and browser editor | Excluded | Not implemented in 0.1.0. |
| F187 | Cloud-only proprietary model endpoints and exact model output parity | Excluded | Not implemented in 0.1.0. |
