# Release notes

## 0.6.0 (in development)

Projects are saved as schema 11; 0.5 cannot open them.

- **Nested sequences:** Edit → Nest selected clips packs several clips into one clip that moves, trims and takes effects as a whole. Double-click it to edit what is inside, and use Back to return; Take apart puts the clips back.
- **Find missing media at once:** when files have moved, "Find in a folder…" in the library relinks every missing file found by name in that folder or below.
- **Start screen and preferences:** Cutlery opens with a choice of video shapes (16:9, 4K, 9:16, 4:5, 1:1), your recent projects and the autosave. Project → Preferences… sets the format of new projects, how long pictures last and how many earlier versions are kept.
- **Media folders and search:** sort media into folders (right-click a file), show only videos, audio or images, and search by name. New imports go into the folder on show. Titles, shapes and sounds are now in the "Add" tab beside "Media", which leaves the library more room.
- **Reframe for Shorts:** Project → Reframe for… turns a 16:9 edit into 9:16 (or 4:5, 1:1): the pictures zoom to fill the tall frame, and with the AI pack the picture pans to keep the speaker's face in the middle. Save As first to keep the wide version. Clips can now be zoomed up to 5×.
- **Export queue:** "Add to queue…" in the export dialog lines up several exports (say a 4K master, a small 1080p and an MP3) that run one after another; each renders the timeline as it was when you queued it, so you can keep editing.
- **Several clips at once:** Ctrl+click to select more clips, or drag a frame over empty timeline; they move, delete, copy and paste together (pasting keeps their spacing). Ctrl+G groups them, Ctrl+Shift+G ungroups, Ctrl+A selects all.
- **Titles that build up:** typewriter, word by word, or letters that rise into place, pop up or fly in. **Gradient text:** a second colour fades the letters from top to bottom.
- **Pan keyframes:** move the sound from left to right over time.
- **Slip, slide and roll:** Alt+drag a clip to slip, Alt+Shift+drag to slide, Alt+drag the edge between two clips to roll the cut.
- **Linked picture and sound:** detached audio moves and trims with its video until you unlink it.
- **Anchor point:** zoom and rotate around a corner or edge, also with keyframes.
- **GIF and SVG:** animated GIFs loop for as long as the clip runs; SVG graphics import sharp and transparent.
- **Speed 0.1× to 10×.**
- **Freeze frame:** holds the picture at the playhead for 2 seconds; the rest of the clip and its sound continue afterwards.
- **29 transitions:** new push, reveal, blur, fade through grey, squeeze, pixel dissolve, wind and more.
- **Pan:** move a clip's sound left or right.
- **Open recent** and **Restore an earlier version:** every save keeps the version before it (the last 20).
- **Sound effects:** mouse click, double click, keyboard typing and swooshes made by Cutlery, plus recorded CC0 clicks, typing and whooshes; listen, add at the playhead, or a whoosh on every transition (loudest at the cut).
- **Effects:** camera shake, glitch, VHS and old film with a strength slider; motion blur; stabilizing for shaky hand-held footage.
- **Reverb and echo** per clip in the sound tools.
- **Beat markers:** "Mark the beats" finds the beats of a music clip and puts a marker on every beat, every 2nd or every 4th; clips snap to markers.
- **WebVTT and ASS captions:** import and export besides SRT.
- **Scopes:** histogram, waveform and vectorscope over the viewer, live while playing.
- **Eye contact (AI pack):** turns a presenter's eyes toward the camera when they read from a script beside the lens. It redraws only the eyes, by up to 15 %, eases off for blinks and turned heads, and leaves people who already look into the camera alone.
- **JKL shuttle:** L plays at 1×, 2× and 4×, J scrubs backward, K stops.
- **Follow a face (AI pack):** a blur or mosaic area follows a face in the video below and is sized to cover it.
- **Sound tools:** EQ, low cut, noise reduction, noise gate, de-esser and compressor per clip, with presets such as "Clear voice" and "Noisy room".
- **Colour and look:** temperature, tint, vibrance, shadows and highlights; sharpen, glow, vignette and film grain; .cube/.3dl LUTs with a strength slider; eight one-click looks.
- **Image sequences:** numbered frames (e.g. rendered animations) import as one clip, transparency included.
- **Variable frame rate:** phone and screen recordings with irregular frame timing are flagged, and can be converted to a constant-rate editing copy.
- **Collect project:** copies the project with all media, LUTs and added fonts into one folder, ready to archive or move.
- **Markers and in/out:** markers (M) with names and colours; in and out points (I, O) and export of just that range.
- **Text styling:** any installed font or your own .ttf/.otf files, bold and italic, alignment, letter and line spacing, outline, shadow and a background box.
- **Voice-over:** record narration from the microphone while the timeline plays; the recording lands at the playhead.
- **Shapes:** arrows, circles, speech bubbles, boxes and lines for tutorials, with colours, outline and text inside bubbles and boxes.
- **Copy and paste:** clips (Ctrl+C / Ctrl+V, also into another project) and their attributes: "Paste look" or "Paste all" (Ctrl+Alt+V) onto the selected clip.
- **Smooth slow motion:** blended or optical-flow in-between frames for clips slower than 1×.
- **Split at scene changes:** cuts a video clip (and its detached audio) into its shots.
- **Audio-only export:** the mix as MP3, AAC (M4A) or WAV, with loudness normalisation.
- **Faster captions:** speech recognition uses the graphics card through Vulkan (NVIDIA, AMD, Intel) and falls back to the CPU. Any x64 processor works; AVX2 is no longer required.

## 0.5.0 alpha

0.5 adds the tools for talking-head and presentation videos: speakers over slides, automatic captions, clean sound, and titles. AI features run offline from an optional AI pack. Projects are saved as schema 10. 0.4 cannot open them; use **Save As** to keep an older copy. 0.5 opens every earlier project.

### Editing and playback
- **Playback and previews:**
  - Live playback starts from the playhead in a fraction of a second, with an audio clock and frame skipping.
  - Preview frames render only the playhead frame.
  - Filmstrip thumbnails appear on video and image clips.
- **Transitions:** 14 two-clip transitions with equal-power audio crossfades.
- **Keyframes:** scale, position, rotation, opacity and volume can be keyframed, with ease in/out.

### Presenter videos
- **Picture-in-picture:** place clips in a corner in one click, drag and resize them in the preview, and use rounded or circular shapes with borders and soft shadows.
- **Green/blue screen** removal with spill suppression.
- **AI background removal** (AI pack): cut out a speaker without a green screen.

### Captions and titles
- **Automatic captions** (AI pack): whisper.cpp with Whisper large-v3-turbo transcribes offline in 10 languages or with automatic detection.
- **Caption styles:** karaoke captions highlight the spoken word, or show one word at a time, with word timing from Whisper.
- **Lower thirds and title cards:** a name and role on a plate or beside an accent line, sliding in; large chapter headings.

### Picture
- **AI upscaling** (AI pack): Real-ESRGAN, up to 4K, for low-resolution footage.
- **Blur and mosaic areas:** hide private data or faces; the area can follow movement with keyframes.
- **Clip blur:** a blur slider for a clip's own picture.

### Sound
- **Remove pauses:** finds quiet moments in a clip and cuts them out in one undoable step, including detached audio.
- **Loudness normalisation on export:** −14 LUFS (YouTube and streaming), −16 (podcasts) or −23 (EBU R128), with a peak limiter.
- **Level meters:** left and right peak meters next to the playback clock; **Measure mix** in the export dialog shows the integrated loudness, true peak and the gain the export will apply.

### Export
- **Formats:** H.264, HEVC, AV1, VP9 and ProRes 422 HQ, with four quality levels and up to 4K.
- **Hardware encoders:** NVIDIA, AMD and Intel, each proven with a test encode, with Windows and software fallbacks.

### AI pack and GPU
- **Separate download:** the AI pack (`models` folder next to `Cutlery.exe`) is optional. Without it, the AI options say what is missing.
- **GPU:** background removal and upscaling use the GPU through DirectML when one is available, otherwise the CPU.

### Known limits
- **Unverified on real hardware:** GPU inference and hardware encoders are verified on CI without a GPU (fallback paths only).
- **Speed:** CPU upscaling is slow (hours per minute of 720p video).
- **Captions:** speech recognition runs on the CPU and needs AVX2.
- **Not yet available:** masks other than rectangles, face tracking, and an installer.

See [what remains](ROADMAP.md) and the [feature status](FEATURE_STATUS.md).
