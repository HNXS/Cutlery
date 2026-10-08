# Release notes

## 0.6.0 (in development)

Projects are saved as schema 11; 0.5 cannot open them.

- **Colour wheels:** push a colour into the shadows, midtones or highlights.
- **Speed ramps:** montage, hero, bullet, jump cut, flash in and flash out make a clip speed up and slow down in one step.
- **Own layouts:** save where the selected clips sit (e.g. an interview split) and use it again in any project.
- **Undo after reopening:** saved projects keep their undo steps; open the project again and Undo goes on where you left off.
- **Freeze frames** of any length, also in reversed clips.
- **3D tilt:** lean a picture back or turn it sideways, in perspective.
- **Perspective:** move a picture's corners to place it into a screen, a sign or a frame in another picture.
- **Find media by what is said:** the library search also finds words spoken in media that were transcribed (for captions or text editing), with the moment they are said; it also searches sizes, folders and rights.
- **Usage rights:** record for each medium whether it is your own, free, needs a credit, licensed or for personal use only; the export warns about media you may not publish commercially and saves the credits as a text file.
- **Background for pictures that do not fill the frame:** a portrait video in a landscape project (or a picture in picture) can have its own picture blurred behind it, or a colour, instead of black bars.
- **New effects:** sketch, poster, fisheye and mirror.
- **Voice changer:** robot, telephone, megaphone, alien, chipmunk and monster voices in the sound tools.
- **Round and soft blur areas:** blur and mosaic areas can be ellipses (e.g. over a face) and have a soft edge.
- **Brand kit:** keep your brand colours and logo for every project (Add tab). The colours appear next to the text and shape colours; "+ Logo" puts the logo small in a corner for the whole video.
- **LUT library:** add .cube/.3dl LUTs once and pick them from a list in the colour tools of any project.
- **Motions:** "Add a motion…" in the inspector makes a picture, title or icon pop in, pop out, slide in, pulse or wiggle, as keyframes you can still change.
- **Whites and blacks** sliders in the colour tools.
- **Convert or compress:** right-click a video or sound in the library to save it in another format, size or quality without putting it on the timeline.
- **Lower thirds** can fade in without sliding.
- **Keyframe easing:** each keyframe can ease in and out, run linearly, start or end slowly, or hold until the next one.
- **More formats:** MXF, MTS/M2TS/TS, MPEG/VOB and DV video; AVIF and HEIC/HEIF pictures.
- **Collage fill:** "Fill each area (crop)" crops the arranged pictures to their areas, without empty edges.
- **New transitions:** spin, glitch and light leak.
- **Undo steps:** choose in Preferences how far Undo goes back (10–500 steps).
- **Overwrite:** right-click media in the library → "Overwrite at the playhead" replaces what is on the track there without moving the rest.
- **Captions:** "+ Caption" adds one at the playhead; automatic captions take a line length and one or two lines; the export can save the captions as an SRT or WebVTT file beside the video.
- **Pick the screen colour:** "Pick" beside the green-screen colours, then click the screen in the viewer, for screens that are not pure green or blue.
- **Cut on the beat:** "Split at markers" cuts a clip on every beat marker; "Cut on the beat" lines up the selected clips so each one ends on a beat.
- **Stronger stabilizing:** a strength slider and "Zoom in" so no filled-in edge shows.
- **Mono and 44.1 kHz:** export the sound in mono or stereo, at 48 or 44.1 kHz.
- **Pitch:** make a voice higher or lower (up to an octave) without changing its speed, in the sound tools.
- **Exposure:** brighten or darken a clip in photographic stops, like a camera.
- **Export with transparency:** ProRes 4444 MOV or a PNG picture sequence keep everything transparent that is not on the timeline, e.g. animated titles or lower thirds to use in other editors.
- **Soft edges:** fade a picture-in-picture out at its border (any shape) with the new "Soft edge" slider.
- **Frame rate and bitrate in the export:** export at 24, 25, 30, 50 or 60 fps (and the 23.976/29.97/59.94 rates) and with a fixed bitrate when a platform asks for one.
- **Try export again:** a failed export can be repeated with one click; when a graphics-card encoder fails part-way, Cutlery switches to the next encoder by itself.
- **Cache limit:** Preferences shows how much space the cache takes, keeps it under a limit (20 GB by default) and can empty it.
- **Search commands:** press Ctrl+K and type a few letters to find and run any command, from the menus or the keyboard list.
- **Import whole folders:** drop a folder (or import one) and its photos, videos and sounds come in, folders inside included, into a library folder of the same name. A Stop button skips the rest of a long import.
- **Save a clip's sound:** "Save sound as file…" writes the selected clip's sound as you hear it to WAV, MP3 or M4A.
- **Glowing text:** a soft glow around titles in a colour of your choice, also in text styles.
- **Blend modes:** multiply, screen, overlay, soft light, darken, lighten, add and difference per clip, e.g. for light leaks, textures or a logo that takes on the colour below.
- **Remove by brightness:** makes the black (or white) parts of a clip transparent, for fire, smoke, sparks and other effects filmed on black.
- **Crop each edge:** crop left, right, top and bottom separately, and turn a clip upside down.
- **Captions from a text file:** Import captions also takes a .txt script: one caption per line from the playhead, each on screen long enough to read.
- **GIF and picture export:** animated GIFs from the export dialog, and the frame at the playhead as PNG or JPEG (Project → Export current frame).
- **Hand over to other editors:** Project → Export timeline writes OpenTimelineIO (.otio, for DaVinci Resolve, Premiere, Kdenlive) or a CMX 3600 EDL, and lists what could not be carried.
- **Templates:** save a project as a template (e.g. your intro with logo and title) and start new projects from it, on the start screen or under Project; swap its media with right-click → Replace with another file.
- **Layouts:** select two to four clips and arrange them side by side, stacked, as a grid, picture in picture or in the presenter layout (screen large, you round in the corner) with one click.
- **Time-lapse up to 100×:** speed now goes from 0.1× to 100×, and fast clips show exactly the right moment of the source.
- **Icons:** check mark, cross, warning, info, star, heart, light bulb, mouse pointer and mouse click in the Add tab, sharp at any size and in any colour.
- **Text styles:** save a title's look (font, colours, outline, animation …) under a name and apply it to other titles in any project.
- **Adjustment layers:** one colour grade or look for everything below it, e.g. a whole scene: Add → “+ Adjustment layer”, then set the look in the inspector.
- **Curves and selective colour:** drag the tone curve (all, red, green, blue) in the inspector; change hue, saturation and lightness of chosen colours only. **Auto colour** corrects dull, dark or tinted clips in one click.
- **Edit by text (AI pack):** the inspector shows what is said in a clip; click words and cut them, or remove every “äh” and “ähm” at once. Double-click a word to jump there.
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
- **Freeze frame:** holds the picture at the playhead for 2 seconds; the rest of the clip and its sound continue afterwards.
- **29 transitions (now 32):** new push, reveal, blur, fade through grey, squeeze, pixel dissolve, wind and more.
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
