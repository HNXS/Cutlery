# Offline AI features

Cutlery's AI features run on your computer without accounts, uploads or network access. The model files are a separate download, the **AI pack**, because they are large (about 170 MB) and optional.

## Installing the AI pack

1. Download `Cutlery-0.4.0-AI-pack` from the same build as `Cutlery-0.4.0-win64-portable`.
2. Extract it into the Cutlery folder, so that `models/u2net_human_seg.onnx` sits next to `Cutlery.exe`.
3. Restart Cutlery.

The application package already contains the worker `cutlery-matte.exe` and the ONNX Runtime library it uses. Without the AI pack, the AI options are disabled and say what is missing.

## Background removal (speaker cutout)

Select a video clip and turn on **Remove background (AI)** in the *Presenter overlay* section of the inspector.

- The first time, Cutlery analyses the clip's media in the background and shows the progress. You can keep editing while it runs; **Stop** cancels it.
- The analysis finds people in 8 frames per second of video and smooths the result over time. Rendering blends between those frames, so the cutout follows normal speaker movement.
- Results are cached per media file in `data/mattes`. Other clips from the same file reuse the analysis. When you trim a clip beyond the analysed range, the inspector offers **Analyse** again.
- The cutout combines with shapes, borders, shadows, keyframes and transitions. It takes the place of the green-screen key on that clip.

### Speed

The analysis uses the CPU. On a 4-core laptop, one second of video takes about 5 seconds to analyse, so a 10-minute recording takes roughly 50 minutes. A desktop with more cores is faster. Only the parts of the media your clips use, plus one second either side, are analysed.

### Limits

- The model looks for people. Objects a person holds are often kept; other subjects (pets, products) are not.
- Edges are soft. Fine hair and fast movement such as waving hands can lose detail or show a thin trace of the old background.
- Variable-frame-rate phone recordings can drift by a frame against the matte.
- GPU acceleration (DirectML) is not implemented yet.

## Model and licences

| File | Model | Licence |
|---|---|---|
| `models/u2net_human_seg.onnx` | U²-Net trained for human segmentation (Qin et al., 2020), ONNX export published by the rembg project | Apache-2.0, [source](https://github.com/xuebinqin/U-2-Net) |
| `onnxruntime.dll` (application package) | ONNX Runtime 1.22.0, CPU build | MIT, [source](https://github.com/microsoft/onnxruntime/tree/v1.22.0) |

`tools/Get-OnnxRuntime.ps1` and `tools/Get-Models.ps1` download both from pinned GitHub releases and verify their SHA-256 hashes.
