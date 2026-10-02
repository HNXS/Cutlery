# Release notes

## 0.6.0 (in development)

Projects are saved as schema 11; 0.5 cannot open them.

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
