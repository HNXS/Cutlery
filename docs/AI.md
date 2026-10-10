# Offline AI features

Cutlery's AI features run on your computer without accounts, uploads or network access. The model files are a separate download, the **AI pack**, because they are large (about 720 MB) and optional.

## Installing the AI pack

1. Download `Cutlery-0.5.0-AI-pack` from the same build as `Cutlery-0.5.0-win64-portable`.
2. Extract it into the Cutlery folder, so that the `models` folder sits next to `Cutlery.exe`.
3. Restart Cutlery.

The application package already contains the worker `cutlery-ai.exe`, the ONNX Runtime and DirectML libraries it uses, and `whisper-cli.exe` for speech recognition. Without the AI pack, the AI options are disabled and say what is missing.

## GPU or CPU

The AI features run on the graphics card through DirectML when Windows offers a DirectX 12 GPU (NVIDIA, AMD or Intel, including integrated graphics). Otherwise they use the CPU. The progress line shows which one is working. On the CPU, upscaling is very slow; see below.

## Background removal (speaker cutout)

Select a video clip and turn on **Remove background (AI)** in the *Presenter overlay* section of the inspector.

- The first time, Cutlery analyses the clip's media in the background and shows the progress. You can keep editing while it runs; **Stop** cancels it.
- The analysis finds people in 8 frames per second of video and smooths the result over time. Rendering blends between those frames, so the cutout follows normal speaker movement.
- Results are cached per media file in `data/ai`. Other clips from the same file reuse the analysis. When you trim a clip beyond the analysed range, the inspector offers **Run** again.
- The cutout combines with shapes, borders, shadows, keyframes and transitions. It takes the place of the green-screen key on that clip.

### Speed

On a 4-core CPU, one second of video takes about 5 seconds to analyse, so a 10-minute recording takes roughly 50 minutes. A GPU is much faster. Only the parts of the media your clips use, plus one second either side, are analysed.

### Limits

- The model looks for people. Objects a person holds are often kept; other subjects (pets, products) are not.
- Edges are soft. Fine hair and fast movement such as waving hands can lose detail or show a thin trace of the old background.
- Variable-frame-rate phone recordings can drift by a frame against the matte.

## AI upscaling (sharper low-resolution video)

Select a video clip below 4K and turn on **Enhance resolution (AI)** under *Picture & sound*. Cutlery creates a copy of the clip's media at four times the resolution, at most 2160p (4K). It uses Real-ESRGAN, which restores edges and texture instead of just enlarging pixels. Export, preview and playback then use the sharper copy.

- Use it for old or low-resolution footage (480p, 720p, phone clips) that you want to export at 1080p or 4K. It does not add anything to footage that is already sharp at the export size.
- Every frame of the used range is processed, plus one second either side. Results are cached per media file in `data/ai`.
- The copy is stored as ProRes 422 so no quality is lost before the final export. It needs about 0.9 GB per minute at 1080p and 3.5 GB per minute at 4K.

### Speed

Upscaling needs far more computation than background removal. On a 4-core CPU, a 360p frame takes about 3 seconds and a 720p frame about 11 seconds, so one minute of 720p video takes hours. With a GPU, expect seconds per second of video, depending on the card. Try a short clip first.

### Limits

- Real-ESRGAN invents plausible detail. Small text can come out as wrong letters, and faces can look smoothed. Check the result before you rely on it.
- Video clips only; images are not upscaled yet.

## Automatic captions

Choose **Captions → Generate captions (AI)…**, pick the spoken language (or *Detect automatically*) and a style, then click **Generate**.

- **Karaoke** shows each line and colours the word being spoken. This is the default.
- **Plain** shows lines as ordinary captions.
- **One word at a time** shows each word large on its own, as in short-form videos.

You can change the style and highlight colour of any caption later in the inspector.

- Cutlery transcribes the media of every audible clip: clips that are not muted, on tracks you can hear. It shows each caption exactly where its words play on the timeline, following trims and speed changes.
- The captions go on a new track, **AI captions**. Running it again replaces that track, so edit the captions after the last run, or rename the track to keep a version.
- Each media file is transcribed once per language and cached in `data/ai`. Later runs after edits are almost instant.
- A voice detector skips silence and music first. Without it, Whisper tends to invent sentences in quiet passages.
- Whisper times every word. Cutlery groups the words into lines of at most 42 characters, starting a new line after a sentence or a pause of more than 0.6 s, and keeps each word's start for highlighting.
- If you edit a caption's text and keep the same number of words, the timing stays. If you change the word count, the caption shows plainly; generate the captions again to get timing back. Splitting a caption gives each half its own words.
- Export them with **Captions → Export titles/captions as SRT…** to upload to YouTube, or style them like any other title.

### Speed and limits

- Speech recognition uses the graphics card through Vulkan (NVIDIA, AMD or Intel, with a current driver) and otherwise the CPU, picking the fastest code for the processor (any x64 CPU works; AVX2 and AVX-512 are faster). If the GPU run fails, it repeats on the CPU. On the CPU, Whisper works through the audio in 30-second blocks. On a 2-core CI machine, one block took about 70 seconds, so 10 minutes of speech would take about 25 minutes there. A modern 8-core PC should be roughly four times faster: about 6 minutes for 10 minutes of speech. Silence and music skipped by the voice detector cost almost nothing.
- Reversed clips are not captioned. When the same speech plays on two tracks, it is captioned once.
- Names, brands and technical terms can come out wrong. Read the captions before publishing.
- Word times are accurate to about a tenth of a second. Highlights can lead or trail fast speech slightly.
- Karaoke captions cannot be animated, rotated or scaled; they keep their position and fades. Turn the style to *Plain caption* to animate one.
- There is no speaker labelling yet.

## Eye contact (look into the camera)

For presenters who read from a script or notes beside the lens: select the video clip and turn on **Eye contact (AI)** under *Picture & sound*. Cutlery makes a copy of the clip's media in which the eyes look toward the camera, and preview, playback and export use it. Untick the option to compare with the original.

- **What it changes:** only the eyes. The iris moves toward the middle of the eye opening, and for an upward correction the upper lid moves with it, as it does in reality. The correction is at most 15 % of the eye width, about the difference between reading a teleprompter-like script and looking into the lens.
- **When it holds back:** it eases off while the eyes close, when the head turns away, when the face is small, and when the gaze is far off. A presenter who already looks into the camera is left alone.
- **Stability:** both eyes are corrected together, by the gaze they share, and the correction is smoothed over time so the eyes do not shimmer.
- **Storage:** the copy is ProRes 422 at the source size (at most 4K), about 0.9 GB per minute at 1080p, cached per media file in `data/ai`.

### How it works

Three small Google MediaPipe models (Apache-2.0) find the face, 468 face landmarks and, for each eye, the iris and the lid contour. Cutlery's own code warps only the eye regions. Nothing generative is involved: the result is the original picture, slightly redrawn. On a 4-core CPU, a 1080p frame takes about a tenth of a second.

### Limits

- **Small corrections only:** larger corrections would look unnatural and are reduced.
- **Difficult pictures:** glasses with strong reflections, heavy side light and very small faces can leave the eyes uncorrected.
- **One person:** only the most prominent face is corrected.
- **Not yet validated on real recordings:** tests use a photo with the gaze moved synthetically. Check your result before you publish.

## Follow a face (blur and mosaic areas)

Select a blur or mosaic area that lies over a video clip and click **Follow a face (AI)**.

- **Finding faces:** the first time, Cutlery finds the faces in that part of the video, 8 times a second, using the face detector from the eye-contact models. Results are cached per media file.
- **Following:** the area follows the face nearest to where you placed it, frame to frame, and is sized to cover it with some room.
- **Editing the result:** it is ordinary X/Y keyframes, so you can correct any frame. One undo step removes it.
- **Limits:**
  - Faces smaller than about 4 % of the picture height are missed.
  - A face that disappears behind something or leaves the picture is not picked up again if another face is closer.
  - Rotated or cropped videos below are only approximated.

## Translate captions

**Captions › Translate captions (AI)…** translates German captions into English or English ones into German.

- **What is translated:** the selected titles, or, with none selected, every caption on the "AI captions" track. Their text is replaced, keeping one or two lines as before; one undo step brings the original back.
- **On this computer:** the Opus-MT models from the AI pack run on the processor; a few hundred captions take a minute or so.
- **Limits:**
  - Each caption is translated on its own, so a sentence split over two captions may read less smoothly.
  - Karaoke and word-by-word captions show the translation as plain text (the word timing belongs to the spoken words).
  - Only German ↔ English for now.

## Voice and music apart

Select a clip with sound and choose **Audio › Clean-up › Voice and music**: **Voice only** keeps the speech or singing, **Without the voice** keeps the music and background (karaoke).

- **The first time**, Cutlery separates the part of the media file the clips use, on the graphics card when there is one (otherwise on the processor, about two to three times as long as the sound). The result is kept per file, so further clips of it are instant. Until then the clip plays as recorded.
- **Exact:** voice and music add up to the original sound, so nothing is lost between them.
- **Limits:**
  - Two parts only: drums, bass or single instruments cannot be picked out.
  - Backing vocals count as voice; very quiet speech under loud music may keep traces of the music.

## Text to speech (German voice)

Under **Audio › Text to speech**, type what should be said, choose the voice and speed, and click **Add speech at the playhead**. For a title, **Read aloud** in its Text settings speaks it (several selected titles each at their own start).

- **On this computer:** speech is made by Piper with the Thorsten voice (German, from the AI pack), a few seconds of work for a paragraph. Nothing is sent anywhere.
- **The result:** an ordinary audio clip on a free track, named after its first words. One undo step removes it. The same text, voice and speed are spoken only once and kept in the cache.
- **Other voices:** a Piper voice (`.onnx` with its `.onnx.json`) copied into the `models` folder appears in the voice list.
- **Limits:**
  - German pronunciation comes from eSpeak NG rules; unusual names and abbreviations may need writing out ("Doktor" for "Dr.").
  - There is no emphasis or emotion control, and one voice per text.

## Model and licences

| File | Model | Licence |
|---|---|---|
| `models/u2net_human_seg.onnx` | U²-Net trained for human segmentation (Qin et al., 2020), ONNX export published by the rembg project | Apache-2.0, [source](https://github.com/xuebinqin/U-2-Net) |
| `models/realesr-general-x4v3.onnx` | Real-ESRGAN realesr-general-x4v3 (Wang et al., 2021), converted from the published weights by `tools/convert-realesrgan.py` | BSD-3-Clause, [source](https://github.com/xinntao/Real-ESRGAN) |
| `models/ggml-large-v3-turbo-q5_0.bin` | OpenAI Whisper large-v3-turbo, 5-bit, in whisper.cpp's format | MIT, [source](https://github.com/openai/whisper) |
| `models/ggml-silero-v6.2.0.bin` | Silero VAD 6.2.0 voice detector, converted by whisper.cpp | MIT, [source](https://github.com/snakers4/silero-vad) |
| `models/face_detection_short_range.onnx`, `models/face_landmark.onnx`, `models/iris_landmark.onnx` | Google MediaPipe face detection (BlazeFace), Face Mesh and Iris Landmark, taken from the pinned mediapipe 0.10.18 wheel and converted from TFLite with tf2onnx | Apache-2.0, [source](https://github.com/google-ai-edge/mediapipe) |
| `models/opus-mt-de-en/`, `models/opus-mt-en-de/` | Opus-MT German–English and English–German (Marian) by the Helsinki NLP group: int8 ONNX encoder and decoder (Xenova's export) with the original SentencePiece model, vocabulary and settings | CC-BY 4.0, [source](https://github.com/Helsinki-NLP/Opus-MT) |
| `models/UVR-MDX-NET-Inst_HQ_3.onnx` | MDX-Net model by the Ultimate Vocal Remover developers (Anjok07, aufr33), from the pinned public UVR model release | MIT with credit to UVR and its developers, [source](https://github.com/Anjok07/ultimatevocalremovergui) |
| `models/de_DE-thorsten-medium.onnx` (with `.onnx.json` and its model card) | Piper voice Thorsten (medium), German, from the pinned piper-voices v1.0.0 release | Dataset CC0 ([Thorsten-Voice](https://github.com/thorstenMueller/Thorsten-Voice)); per its model card fine-tuned from Piper's U.S. English lessac voice, whose training data has its own terms |
| `tts/piper.exe` with its DLLs and `espeak-ng-data` | Piper 2023.11.14-2 (text to speech) with piper-phonemize, eSpeak NG and ONNX Runtime 1.14.1, the pinned Windows release | Piper MIT ([source](https://github.com/rhasspy/piper/tree/2023.11.14-2)); eSpeak NG GPL-3.0-or-later ([source](https://github.com/rhasspy/espeak-ng)), a separate program |
| `whisper-cli.exe` (application package) | whisper.cpp v1.9.4, built from the pinned commit | MIT, [source](https://github.com/ggml-org/whisper.cpp/tree/v1.9.4) |
| `onnxruntime.dll` (application package) | ONNX Runtime 1.22.0 with the DirectML provider | MIT, [source](https://github.com/microsoft/onnxruntime/tree/v1.22.0) |
| `DirectML.dll` (application package) | DirectML 1.15.4 redistributable | Microsoft DirectML licence (in `licenses/onnxruntime`) |

`tools/Get-OnnxRuntime.ps1`, `tools/Get-Whisper.ps1`, `tools/Get-Piper.ps1` and `tools/Get-Models.ps1` download everything from pinned NuGet packages, GitHub releases, Hugging Face and PyPI and verify SHA-256 hashes. The face models' TFLite originals are pinned; the ONNX conversion is not byte-reproducible, so its hashes are recorded in `models/manifest.json`.
