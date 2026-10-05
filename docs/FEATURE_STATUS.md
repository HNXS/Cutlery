# Feature status — 0.5.0 alpha

The original 187 rows remain the design scope, not a claim of completion. “Implemented (alpha)” means a present code path, not production qualification. Partial rows list their actual boundary. Acceptance criteria in `features.json` remain the original future gates.

| ID | Feature | Alpha status | Current boundary |
|---|---|---|---|
| F001 | Create, open, recent projects and settings | Partial | Create/open/save and presets; Project → Open recent lists the last 10 projects opened or saved (kept in the data folder; missing ones are marked and dropped when chosen). No start screen or global settings dialog. |
| F002 | Media bin, folders, collections and search | Implemented (alpha) | The library tab shows all media, or only videos, audio or images, or one folder; a search box narrows it by name (every word must match, any order and case). Folders are created, renamed and deleted (their media stays) in the library; media is moved by right-click, new imports go into the folder on show, and folders are saved with the project. Unused media can be removed from the library. Titles, graphics and sounds moved to a separate Add tab so the list has room. No nested folders or saved smart collections. |
| F003 | Drag/drop and background media analysis | Partial | Library-to-track and local file drops, async probing and waveform analysis, partial-error reporting; no directory recursion or cancellable import queue. |
| F004 | Nondestructive media references | Implemented (alpha) | Relative project references; originals never edited. |
| F005 | Missing-media relink and replace | Partial | Validated source relinking; no batch relink. |
| F006 | Collect Project and portable media bundle | Implemented (alpha) | Project → "Collect project and media…" copies every media file, LUT and font added in Cutlery that the project uses into an empty folder (media/, luts/, fonts/ with clashing names numbered) and saves <folder>.cutlery there with relative paths, in the background with progress. Opening a project loads the fonts beside it. Refuses projects with missing media; no trimming of unused source ranges. |
| F007 | Project schema migrations | Implemented (alpha) | Reads schema 1/2 without moving clips; writes schema 3 with stable track IDs and snapping/magnetic modes. Older versions cannot read new saves. |
| F008 | Atomic save, autosave and rolling backup | Implemented (alpha) | Atomic save and one recovery file; each save keeps the previous project file as a version in the data folder (the newest 20 per project), and Project → Restore an earlier version… puts one back after keeping the current file as a version too. Backups are whole project files; media is not copied. |
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
| F022 | Linked A/V, unlink and resync | Implemented (alpha) | Detach audio puts the sound on its own track, linked to the picture: moving or trimming either one moves or trims both while they stay aligned, splitting both keeps two linked pairs, and a slip of one alone keeps the link. "Unlink picture and sound" frees them; linked clips show ⛓ in the timeline. No automatic resync by waveform. |
| F023 | Groups, track lock, mute, solo and hide | Implemented (alpha) | Track lock, audio mute/solo and picture hide; multiple selection (Ctrl+click, Ctrl+A) that moves and deletes together, clip groups (Ctrl+G, Ctrl+Shift+G) that are selected, moved and deleted as one, and rubber-band selection by dragging over empty timeline (Ctrl adds). Copy and paste keeps several clips' spacing, links and groups. No nested groups. |
| F024 | Snapping and optional magnetic/ripple mode | Implemented (alpha) | Per-track edge snapping and magnetic insertion/reorder/ripple edits. Master edge-snap switch is independent of Magnet; locks respected. |
| F025 | Insert, overwrite and reorder | Partial | Magnetic insert and reorder; free-position drops. No overwrite edit mode. |
| F026 | Split, trim, delete and duplicate | Implemented (alpha) | Split, drag-edge/source-aware trims, inspector trims, delete and duplicate; each committed edit is undoable. |
| F027 | Ripple trim, roll, slip and slide | Implemented (alpha) | Magnetic tracks ripple; Alt+drag a clip to slip its source, Alt+Shift+drag to slide it between its neighbours (they grow or shrink), Alt+drag an edge between two touching clips to roll the cut. Each edit is one undo step; slipping takes the clip's linked sound along. No numeric trim entry. |
| F028 | Nested sequences and compound clips | Planned | Not implemented in this alpha. |
| F029 | Copy/paste clips and selected attributes | Implemented (alpha) | Copy (Ctrl+C) and paste at the playhead (Ctrl+V) within the session and across projects (the media comes along); the copy goes to its own track when free there, otherwise the nearest free track or a new one. Paste look (colour, effects, LUT) or paste all attributes (Ctrl+Alt+V: also transform, keyframes, shape, keying, volume, fades) onto the selected clip, in one undo step. No system-clipboard exchange or multi-clip selection. |
| F030 | Markers, in/out and timeline navigation | Implemented (alpha) | Markers (M) with name and colour, shown on the ruler and across the tracks; click to jump, double-click to rename or recolour, right-click to remove; previous/next marker (Ctrl+Left/Right). In and out points (I, O, Alt+X to clear) shown as a band on the ruler; the export dialog can export just the in/out range. Frame and edit navigation as before. Markers stay at timeline positions; they do not move with ripple edits. |
| F031 | Timeline zoom and frame-accurate seeking | Partial | Timeline zoom/fit; seeking renders only the playhead frame, so latency no longer grows with position (about 0.1 s for 1080p H.264 on a 4-core development container). Not qualified on the hardware profiles. |
| F032 | JKL shuttle and reverse audition | Partial | L plays forward and doubles to 2× and 4× on each press (FFmpeg real-time pacing at that speed, sound sped up with atempo, frames mapped by the faster clock); J scrubs backward at 1×, 2× and 4× (ten preview steps a second, stopping at the start); K pauses both; the transport shows the speed. Backward is a stepping preview without sound, not reverse playback. |
| F033 | Constant speed 0.1x to 100x | Partial | 0.1–10x with pitch kept (atempo chain). No speeds beyond 10x. |
| F034 | Speed ramps and curve editor | Planned | Not implemented in this alpha. |
| F035 | Reverse and freeze frames | Partial | Reverse, and "Freeze frame here": the picture at the playhead becomes a still of 2 s with the clip's size, position and look; the clip and its detached audio are split there and the rest moves later. One undo step. No freeze of the whole composited frame or of reversed clips; the hold length is fixed in the button (any length via the API). |
| F036 | Property keyframes and graph editor | Partial | Keyframes for scale, position, rotation, opacity and volume, with smooth (ease in/out) interpolation. Set at the playhead in the inspector, shown as timeline markers, kept attached through trim, split and speed changes. No graph editor or per-key easing choice in the UI. |
| F037 | Frame-rate changes and mixed-rate sequences | Planned | Not implemented in this alpha. |
| F038 | Scale, position, rotate, anchor and opacity | Implemented (alpha) | Scale, position, rotation and opacity (keyframable) around an anchor point chosen from a 3×3 grid in the inspector (centre by default); the anchor stays in place while the picture scales or turns, also when animated, and the preview frame follows it. |
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
| F049 | Glitch, shake, VHS, retro and film effects | Implemented (alpha) | Per clip in EFFECTS: camera shake (zoom-in with an irregular moving crop), glitch (colour channels jumping apart in short bursts that are the same every render), VHS (channel offset, softness, desaturation, noise and scanlines) and old film (sepia mixed by strength, flicker, grain), each with a strength. One effect per clip; no retro presets beyond these four or effect keyframes. |
| F050 | Distortion, edge, lens and stylize effects | Planned | Not implemented in this alpha. |
| F051 | Motion blur | Implemented (alpha) | Per video clip: motion blur 0..1 mixes 2–6 consecutive frames (tmix); a little source before the frame is decoded so previews match playback and export. Frame blending, not motion-vector blur. |
| F052 | Frame blending slow motion | Implemented (alpha) | For clips slower than 1×: "Blend frames" (FFmpeg framerate, cross-fading neighbouring source frames, without blending across scene cuts) or "Optical flow" (minterpolate motion-compensated interpolation, slow to render); a little source around the range is decoded so preview frames match export. Not for reversed clips. |
| F053 | Camera-like and pseudo-3D effects | Planned | Not implemented in this alpha. |
| F054 | Dissolve, fade and dip to color | Implemented (alpha) | Two-clip dissolve and dip to black/white centred on a cut, with equal-power audio crossfades; clip fades as before. |
| F055 | Wipe, slide, push and directional transitions | Implemented (alpha) | Wipe left/right/up/down, slide left/right/up/down, smooth left/right, push in from each side (cover) and reveal left/right (FFmpeg xfade). |
| F056 | Zoom, spin, stretch and geometric transitions | Partial | Zoom in, circle open/close, radial and squeeze. No spin. |
| F057 | Blur, glitch and light transitions | Partial | Pixelize, blur, fade through grey, pixel dissolve, wind, and dips to black or white. No glitch or light-leak transitions. |
| F058 | Transition duration and curve editing | Partial | Duration 0.1–3 s in the inspector, limited by both clips. No easing curves. |
| F059 | Audio extraction and linked source streams | Partial | Detach audio reuses source media on an independent track; no extraction to a standalone audio file or resync. |
| F060 | Waveform generation and peak pyramids | Partial | Async bounded mono waveform overview cached by file fingerprint; follows trim/speed/reverse. No multilevel pyramid. |
| F061 | Volume, gain, pan, mute and fades | Implemented (alpha) | Volume and pan per clip, both keyframable (pan as balance −1 left to +1 right), mute and fades. |
| F062 | Multitrack mixing and channel layouts | Partial | 48 kHz stereo only. |
| F063 | 5.1 and configurable audio routing | Planned | Not implemented in this alpha. |
| F064 | Voice-over and audio recording | Partial | "● Voice-over" in the transport row records the default microphone to WAV (Qt Multimedia) in the data folder's recordings/ while the timeline plays from the playhead; stopping adds the recording at that frame on the lowest free non-magnetic track (or a new one). No input-device choice, level display during recording, count-in or punch-in; checked on CI only for the no-microphone path. |
| F065 | EQ, compressor, limiter and noise gate | Implemented (alpha) | Per clip: three-band EQ (bass shelf 100 Hz, presence 2.5 kHz, treble shelf 8 kHz, ±12 dB), low cut (highpass up to 300 Hz), compressor with make-up gain, noise gate and de-esser (FFmpeg filters), and six original presets (Clear voice, Warm podcast voice, Noisy room, Phone call, Music punch, Natural). The export limiter stays on the mix. No per-track effects or visual EQ curve. |
| F066 | Reverb and delay | Implemented (alpha) | Per clip in SOUND: reverb (four short room reflections) and echo (repeats 320 and 640 ms apart), 0..1 each (FFmpeg aecho). The tail ends with the clip; no room presets or tempo-synced delay. |
| F067 | Loudness measurement and normalization | Partial | Export option: two-pass loudness normalisation. FFmpeg's EBU R128 meter measures the whole mix, then one gain reaches the target (-14 YouTube/streaming, -16 podcast, -23 EBU R128) and a peak limiter holds peaks about 1.5 dB below full scale. In the editor, left/right peak meters follow playback and the export dialog measures the whole mix (integrated LUFS, true peak, gain to the target). No true-peak limiting, loudness-range display or per-clip normalisation. |
| F068 | Pitch shift and tempo preservation | Partial | Speed with pitch preservation; no independent pitch. |
| F069 | Silence detection and removal | Partial | Remove pauses on a clip: FFmpeg silencedetect with an adjustable threshold and minimum length, 0.12 s kept around speech, preview of count and total length, then one undoable ripple edit on the clip's track and on its detached audio. Other tracks keep their timing. |
| F070 | Beat detection and markers | Implemented (alpha) | "Mark the beats" analyses the selected clip's sound in the background (spectral flux onsets, autocorrelation tempo 60–200 BPM, dynamic-programming beat tracking, beats moved to the nearest onset) and adds blue markers on every 1st, 2nd or 4th beat inside the clip, in one undo step. Tested on synthetic drum tracks at 97, 120 and 174 BPM (beats within 30 ms); no downbeat/bar detection or tempo changes within a song. |
| F071 | Beat-synchronized edits | Partial | Clips snap to markers (beat markers included) while dragging and trimming. No automatic cut-to-beat or montage generation. |
| F072 | Classical noise reduction | Partial | Per-clip noise reduction with FFmpeg afftdn (spectral subtraction, 6–30 dB), part of the sound tools; reduces steady hiss and hum. No noise print or learning from a selected noise-only range. |
| F073 | Speech denoise and enhancement | Partial | Speech clean-up through the sound presets: low cut, noise reduction, gate, presence EQ, de-esser and compression ("Clear voice", "Noisy room"). No AI speech enhancement. |
| F074 | Vocal isolation and music separation | Planned | Not implemented in this alpha. |
| F075 | Dereverb | Planned | Not implemented in this alpha. |
| F076 | Voice changer | Planned | Not implemented in this alpha. |
| F077 | Voice conversion and custom voice | Planned | Not implemented in this alpha. |
| F078 | Plain and rich text; Unicode shaping | Partial | Plain Unicode Qt titles; no rich text. |
| F079 | Font browser, favorites and local font files | Partial | Font list of all installed families (each shown in its own font, type to search) and local .ttf/.otf/.ttc files added with "+ Font", copied into the data folder so they travel with a portable install and load at start. No favourites; projects store the family name only. |
| F080 | Size, weight, spacing, kerning and alignment | Implemented (alpha) | Font size, bold, italic, left/centre/right alignment, letter spacing (−0.1..0.5 of the size) and line spacing (0.7..3×) for titles and captions; kerning is the font's own. Title templates keep their fixed layout. |
| F081 | Outline, shadow, glow and text background | Partial | Outline (width and colour), drop shadow strength and a rounded box behind each line (colour and opacity) for titles; karaoke captions get outline and shadow. No text glow; templates keep their own plate. |
| F082 | Text transforms and per-character animation | Partial | Plain titles can build up from the clip's start, finished after a set time (0.1–30 s): typewriter (one character after another), word by word, or letters that rise into place, pop up (grow with a slight overshoot) or fly in from the right, each easing in and fading up, staggered over the time. Any title text can have a top-to-bottom colour gradient. Letter animations are rendered per frame (up to 60 per second) as an image sequence of just the frames the range needs. No custom per-character paths, rotation or colour animation, and none for title templates or captions. |
| F083 | Original titles, lower thirds and presets | Partial | Original title templates: lower third with plate, lower third with accent line (both slide in from the left and fade), and title card; name/role from the text lines, accent colour, scale and position from the preview frame. Three templates, no preset library, no per-template animation choice. |
| F084 | Manual captions and subtitle track | Partial | Editable title clips used as captions. |
| F085 | SRT import/export and TXT import | Partial | SRT roundtrip (WebVTT and ASS too, see F086); no TXT. |
| F086 | WebVTT and ASS/SSA interoperability | Implemented (alpha) | Captions import from SRT, WebVTT (identifiers, cue settings, tags and entities handled) and ASS/SSA (field order from the Format line, override tags dropped), and export to SRT, WebVTT and ASS with a canvas-sized default style using the first caption's font and size. Per-caption styling, positions and karaoke timing are not carried over. |
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
| F097 | Shapes, arrows, callouts and speech bubbles | Partial | Shape clips from the library: arrow, circle/ellipse, speech bubble, box and line, with fill (or none), outline colour and width, and size; moved, resized, rotated and keyframed like overlays. Bubbles and boxes hold styled text. No callout pointers that follow a target or custom paths. |
| F098 | Stickers, icons and overlays | Partial | Imported images, SVG graphics and animated GIFs as overlays; no built-in sticker or icon library. |
| F099 | PNG, SVG, GIF and image-sequence overlays | Implemented (alpha) | PNG, JPEG, WebP, TIFF and BMP stills; SVG files drawn with Qt SVG into a 2048-pixel transparent picture on import; animated GIFs as looping clips of any length; numbered image sequences (F151). |
| F100 | Collage and reusable layout templates | Planned | Not implemented in this alpha. |
| F101 | Stock music and sound effects import | Partial | Sound effects library ("♪ Sound effects…" in the media library): Cutlery's own synthesised sounds (mouse click, double click, keyboard typing of 2, 5 and 10 s, two swooshes; deterministic, written as WAV on first use, free to use) and, in the portable build, six recorded CC0 sounds (Kenney switch click; Elements mouse click, 8 s keyboard typing and three whooshes) pinned by hash. Listen, add at the playhead on a free track, or put a whoosh on every transition with its loudest moment at the cut. Licence and source are shown per sound. User audio files import as before. No stock music, search or online catalogue. |
| F102 | Commercial-use rights metadata | Planned | Not implemented in this alpha. |
| F103 | Exposure, brightness, contrast and saturation | Partial | Brightness/contrast/saturation; no exposure model. |
| F104 | Temperature, tint, vibrance and tonal controls | Partial | Per clip: temperature (light colour temperature with lightness kept), tint (green–magenta), vibrance, and shadows/highlights through a master curve, each −1..1, plus the existing brightness, contrast and saturation. Not keyframable; no exposure/whites/blacks split or auto white balance. |
| F105 | HSL, RGB curves and master curves | Planned | Not implemented in this alpha. |
| F106 | Color wheels and automatic adjustment | Planned | Not implemented in this alpha. |
| F107 | LUT import, intensity and original presets | Partial | .cube and .3dl 3D LUTs per clip (tetrahedral interpolation) with a strength mix; missing files are skipped and flagged. Eight original one-click looks built from the colour and look controls; no bundled LUT files or LUT browser. |
| F108 | Histogram, waveform and vectorscope | Implemented (alpha) | "Scopes" in the transport row shows a histogram (R, G, B, luma), a luma waveform or a vectorscope (BT.709, 75 % bar targets, skin-tone line) over the viewer, for the preview still and four times a second while playing. No RGB parade or false colour. |
| F109 | Rec.709, sRGB and range conversions | Planned | Not implemented in this alpha. |
| F110 | HDR input and SDR tone mapping | Planned | Not implemented in this alpha. |
| F111 | 10-bit and HDR export | Planned | Not implemented in this alpha. |
| F112 | SDR-to-HDR workflow | Planned | Not implemented in this alpha. |
| F113 | Display management and multi-monitor HDR | Planned | Not implemented in this alpha. |
| F114 | Point and planar tracking | Planned | Not implemented in this alpha. |
| F115 | Object tracking and editable paths | Planned | Not implemented in this alpha. |
| F116 | Face detection and face tracking | Partial | MediaPipe BlazeFace (Apache-2.0, ONNX) in the cutlery-ai worker: the "faces" task finds faces 8 times a second on the whole frame and on overlapping tiles (faces from about 4 % of the frame height), cached per media file. "Follow a face (AI)" keyframes a blur/mosaic area (or any overlay) onto the face nearest it in the video below, frame-to-frame nearest neighbour with smoothing, and sizes areas to the face. Eye contact uses the same detector with Face Mesh and Iris. No identity tracking across cuts or occlusions; rotation, crop and keyframed scale of the video below are approximated. |
| F117 | Tracked masks, blur and overlays | Planned | Not implemented in this alpha. |
| F118 | Stabilization and auto crop | Partial | Per video clip: "Stabilize" smooths camera shake with FFmpeg's deshake (edges mirrored, about 32 px search). Single-pass and local; no two-pass vid.stab, smoothing strength or automatic crop/zoom. |
| F119 | Rolling-shutter and lens correction | Planned | Not implemented in this alpha. |
| F120 | Smart crop and automatic reframing | Partial | Project → Reframe for… (9:16, 4:5, 1:1, 16:9) changes the canvas, keeping its shorter side, and zooms every video and image that filled the old frame to cover the new one (titles, graphics and pictures placed in a corner keep their place); one undo step. With the AI pack, videos are analysed for faces and get position keyframes every half second that keep the main face (the largest, then the same one) in the middle, smoothed over a second either side and never uncovering the canvas; a second undo step. No object or motion saliency, no per-shot framing choices, and reversed clips stay centred. |
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
| F131 | Automatic scene detection | Partial | "Split at scene changes" on a video clip: FFmpeg scdet on a 320-pixel copy of the clip's source range finds shot changes; the clip and its detached audio are split there in one undo step, ignoring shots under 0.5 s. Fixed sensitivity in the UI; no review list before splitting. |
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
| F151 | Numbered image sequences | Partial | Project → "Import image sequence…": pick any frame of a numbered sequence (shot_0001.png …; PNG, JPEG, TIFF, BMP, WebP, EXR, DPX); the contiguous run around it is found, and FFmpeg turns it at a chosen frame rate into a ProRes 4444 video with alpha in the data folder's sequences/, which is imported as a normal clip. No live link to the image files; no image-sequence export. |
| F152 | Variable frame rate and timestamp repair | Partial | Import detects variable frame rate (nominal and average rates differ by more than 1 %) and the inspector flags it. Playback and export already place frames by timestamp. "Convert to constant frame rate" makes a ProRes 422 + PCM editing copy at the nearest standard rate in the data folder and relinks the media, keeping clip trims. No automatic conversion on import; no repair of broken timestamps beyond the re-encode. |
| F153 | MP4/H.264/AAC export | Implemented (alpha) | H.264 via NVENC, AMF, Quick Sync or Media Foundation, whichever passes a probe; no software H.264 encoder in the LGPL build. |
| F154 | MOV and mezzanine/alpha workflow | Partial | ProRes 422 HQ/422/LT MOV export with PCM audio; no alpha-channel export. |
| F155 | WebM VP9/Opus software export | Implemented (alpha) | VP9/Opus export profile (integration coverage to expand). |
| F156 | HEVC and AV1 hardware/software export | Implemented (alpha) | HEVC via hardware or Media Foundation; AV1 via hardware or SVT-AV1 software. |
| F157 | Audio-only and image-sequence export | Partial | Audio-only export of the timeline mix: MP3 (LAME VBR), AAC in M4A, or WAV (24-bit at Maximum quality, else 16-bit), 48 kHz stereo, with the same loudness normalisation as video. No image-sequence export. |
| F158 | Resolution, fps, quality, bitrate and format controls | Partial | Presets plus format, four quality levels and output height (720p–4K); no explicit bitrate or fps override. |
| F159 | Hardware encoder selection and CPU fallback | Implemented (alpha) | Candidates are probed with a short test encode at the real size and arguments, cached per session. Software formats always remain available. Not qualified on real NVIDIA/AMD/Intel hardware yet. |
| F160 | Queue, range export, progress and cancellation | Implemented (alpha) | Export with progress and cancel, of the whole timeline or the in/out range. "Add to queue…" in the export dialog queues jobs that each keep a snapshot of the timeline as it was when added and run one after another while you keep editing; the dialog lists them with their state, waiting jobs can be removed, and cancelling an export pauses the queue until "Continue the queue". Output is published atomically. The queue is not saved with the project. |
| F161 | Interrupted render retry and recovery | Planned | Not implemented in this alpha. |
| F162 | Convert/compress and export presets | Planned | Not implemented in this alpha. |
| F163 | Proxy and optimized-media workflows | Planned | Not implemented in this alpha. |
| F164 | Frame, thumbnail and render caches | Partial | Keyframe filmstrips (up to 200 tiles per asset) cached by file fingerprint; live playback needs no render cache. No frame cache or cache quotas. |
| F165 | Async jobs, priorities and cancellation | Partial | Async import/render; no scheduler/priority system. |
| F166 | RAM/VRAM budgets and decoder pooling | Planned | Not implemented in this alpha. |
| F167 | GPU device loss and CPU fallback | Planned | Not implemented in this alpha. |
| F168 | Dark UI, dockable panels and searchable tools | Partial | Dark resizable panels and original app icon; no docking/search. |
| F169 | High DPI, scaling and multi-monitor | Partial | Qt scaling support; manual qualification outstanding. |
| F170 | Shortcuts, customization and command search | Partial | 44 customizable commands, conflict checks and portable settings; no command search or complete CapCut keymap. |
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
