# Release notes

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
- **Not yet available:** loudness meters in the editor, masks other than rectangles, face tracking, and an installer.

See [what remains](ROADMAP.md) and the [feature status](FEATURE_STATUS.md).
