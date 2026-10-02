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

## Model and licences

| File | Model | Licence |
|---|---|---|
| `models/u2net_human_seg.onnx` | U²-Net trained for human segmentation (Qin et al., 2020), ONNX export published by the rembg project | Apache-2.0, [source](https://github.com/xuebinqin/U-2-Net) |
| `models/realesr-general-x4v3.onnx` | Real-ESRGAN realesr-general-x4v3 (Wang et al., 2021), converted from the published weights by `tools/convert-realesrgan.py` | BSD-3-Clause, [source](https://github.com/xinntao/Real-ESRGAN) |
| `models/ggml-large-v3-turbo-q5_0.bin` | OpenAI Whisper large-v3-turbo, 5-bit, in whisper.cpp's format | MIT, [source](https://github.com/openai/whisper) |
| `models/ggml-silero-v6.2.0.bin` | Silero VAD 6.2.0 voice detector, converted by whisper.cpp | MIT, [source](https://github.com/snakers4/silero-vad) |
| `whisper-cli.exe` (application package) | whisper.cpp v1.9.4, built from the pinned commit | MIT, [source](https://github.com/ggml-org/whisper.cpp/tree/v1.9.4) |
| `onnxruntime.dll` (application package) | ONNX Runtime 1.22.0 with the DirectML provider | MIT, [source](https://github.com/microsoft/onnxruntime/tree/v1.22.0) |
| `DirectML.dll` (application package) | DirectML 1.15.4 redistributable | Microsoft DirectML licence (in `licenses/onnxruntime`) |

`tools/Get-OnnxRuntime.ps1`, `tools/Get-Whisper.ps1` and `tools/Get-Models.ps1` download everything from pinned NuGet packages and GitHub releases and verify SHA-256 hashes.
