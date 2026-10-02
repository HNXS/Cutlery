# Validation record

## Windows portable build — 2026-10-02, eye contact, markers, collect, VFR, image sequences (0.6 development)

[GitHub Actions run 37012362029](https://github.com/HNXS/Cutlery/actions/runs/37012362029) completed successfully for commit `9bd5f21851e05af5dae09c7765f8fe9329431f72`.

Toolchain change: `Get-Models.ps1` now takes MediaPipe's face detection, Face Mesh and Iris Landmark models from the pinned mediapipe 0.10.18 wheel and converts them to ONNX on the runner with tf2onnx 1.16.1 and TensorFlow 2.17.1. This step took about 2 minutes, and the AI pack now includes the three models.

New coverage:

- **Engine test `eyeContact`**, with the converted models and a public-domain photo:
  - a face already looking into the camera is left unchanged (under 2 % of the eye width);
  - a sideways gaze is corrected in both eyes;
  - a second pass finds less than two thirds of the first correction left;
  - an editor export changes only the eye region.
- **CI worker check:** the deployed-style worker on the CPU provider applied −0.069 and −0.070 eye widths to the sideways fixture, the same as the local Linux build.
- **Other engine tests:**
  - `markersAndRange`: sorted markers, navigation, renaming, validation; an in/out export of exactly 30 frames and 1 s of audio.
  - `collectProject`: same-named media are numbered; the LUT and an added font are copied; paths are relative; the copy opens after the originals are deleted.
  - `variableFrameRate`: detection, then conversion to a constant-rate ProRes copy and relink.
  - `imageSequences`: run detection with gaps and a `%` in the folder name; a 12-frame ProRes 4444 clip with alpha.
- **Interface:** a marker on the ruler, and the export range choice.

Not verified on CI: eye contact on recorded presenter footage and on DirectML GPUs.

Downloads (expire 2026-11-01):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/37012362029/artifacts/11228648619): 168,066,306 bytes, archive SHA-256 `699d7311180066a041901fcb48e73c2c43d2a403436b1b6111bd597676ae0b39`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/37012362029/artifacts/11228932529): 706,100,616 bytes, archive SHA-256 `9c4a98349fce048cce82832de7435352bf2843e4578259b284e806516c619cfc`.

## Windows portable build — 2026-10-02, colour and look, Whisper on Vulkan, editing tools (0.6 development)

[GitHub Actions run 36999719999](https://github.com/HNXS/Cutlery/actions/runs/36999719999) completed successfully for commit `9407ad93e98961805fcabf17180e92805f670af7`. Toolchain changes:

- **whisper.cpp v1.9.4:** now built with ggml backends as DLLs (Vulkan, plus one CPU backend per x64 level, MSVC OpenMP), using the LunarG Vulkan SDK 1.4.321.1 (installer SHA-256 `baaa4f7c…68bd`) as a build tool.
- **CI cache:** the build is cached by the hash of its scripts.

New coverage:

- **Speech recognition:** the check passes with the new build and asserts the CPU fallback on the GPU-less runner. The deployed smoke test runs the packaged whisper-cli with its backend DLLs.
- **Engine tests:**
  - `colourAndLook`: each control moves rendered pixels as expected; LUTs at full and half strength from a folder with special characters; alpha survives every filter; schema 11 round trip.
  - `clipboardAndAudioExport`: paste to a free track and into a new project; paste look or all; MP3, M4A and WAV exports with no video stream.
  - `sceneDetection`: video and detached audio split at 1 s and 2 s, and a 0.2 s flash ignored.
  - `smoothSlowMotion`: blended in-between frames versus repeats; optical flow renders.
  - `textStyles`: alignment, letter spacing, outline, box and shadow in pixels; a font file added to the data folder.
  - `shapes`: arrow, rotation, outline ring and bubble text in pixels; SRT export excludes shapes; validation.
  - `voiceOverWithoutMicrophone`: the error path when no microphone is present.
- **Interface:**
  - colour and look section, look presets, LUT load and rejection;
  - paste look;
  - shape menu and section;
  - audio formats in the export dialog.
- **`Get-FFmpeg.ps1`** now also requires `colortemperature`, `vibrance`, `curves`, `lut3d`, `cas`, `vignette`, `noise`, `alphaextract`, `scdet` and `minterpolate`.

Not verified on CI: Vulkan inference on a real GPU, and microphone recording.

Downloads (expire 2026-11-01):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36999719999/artifacts/11224285667): 168,000,472 bytes, archive SHA-256 `eeed55fd0c50720406d815c12dd44520825cd73672c9c4b3fd7058b490f36ea9`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36999719999/artifacts/11223514058): 701,994,943 bytes, archive SHA-256 `ec9d7ba1d5e8461a45bdd16b0aaa757ce72c679d3f6e9b679ea4198772384df2`.

## Windows portable build — 2026-10-02, release 0.5.0 with level meters

[GitHub Actions run 36996269492](https://github.com/HNXS/Cutlery/actions/runs/36996269492) completed successfully for commit `d35326ceeceb9c13eed0397a0a1b3d84909789b4`, with the same toolchain as the lower-thirds build. The artifacts are named 0.5.0. New coverage:

- **Engine test `pausesAndLoudness`:**
  - `pcmPeaks` for 16-bit and float PCM, and `parseTruePeak`.
  - "Measure mix" end to end: a test tone measures −33 LUFS with a true peak of −31.5 dBTP, and the result reads as stale after an edit.
- **Interface:** the export dialog's measure button and result text, and the level meter, load without QML warnings.

Downloads (expire 2026-11-01):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36996269492/artifacts/11221554077): 148,512,563 bytes, archive SHA-256 `1d8595e6618487d0deb6e9facb3c24d1df068711be8c5a97b4c1f5474130eaea`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36996269492/artifacts/11222530480): 701,994,869 bytes, archive SHA-256 `c9d252d964f95d390e276250289553609a032cc063c5e95006ca4bcdaf828d0c`.

## Windows portable build — 2026-10-02, lower thirds and title cards (0.5 development)

[GitHub Actions run 36981200885](https://github.com/HNXS/Cutlery/actions/runs/36981200885) completed successfully for commit `a4e48e1eb1b0fab9ee131d79c3a7f8c476f2497c`, with the same toolchain as the blur-and-mosaic build. New coverage:

- **Engine test `titleTemplates`:**
  - Layout: the lower third is anchored in the title-safe lower left, the title card centred, plain titles have no plate, and text scales with the clip.
  - Rendered pixels: the accent bar and plate after the slide-in, the plate still off-screen on the first frame, and movement with the clip's y offset.
  - Schema 10 round trip and validation; `addTitleTemplate`; the preview frame matches the plate.
- **Interface:** "+ Lower third" adds and selects a lower third, and the style list switches it to a title card.

Downloads (expire 2026-11-01):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36981200885/artifacts/11216301264): 148,503,915 bytes, archive SHA-256 `4e5cb3fb760967fe8eb7c224d6ab9c590ae38d242122540dcd9578d1d18ae0a1`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36981200885/artifacts/11216026844): 701,994,868 bytes, archive SHA-256 `c6c90eb0bc5c17aca66630fd6922bad9aca63853a2b392cd4e8a69a2384b9162`.

## Windows portable build — 2026-10-01, blur and mosaic (0.5 development)

[GitHub Actions run 36929907077](https://github.com/HNXS/Cutlery/actions/runs/36929907077) completed successfully for commit `7f67b2047e175c92de8aebac32705f18ac99a0fb`, with the same toolchain as the karaoke-captions build. New coverage:

- **Engine test `blurAndMosaic`**, on a fine checkerboard:
  - A blur area turns its rectangle grey while the surroundings stay sharp.
  - A mosaic area produces flat blocks.
  - A keyframed area moves from the centre to the right edge.
  - Outside its clip time an area has no effect.
  - The clip-wide blur softens the whole picture.
  - Model checks: schema 9 round trip, validation, preview size, `addEffect`, exclusion from SRT export.
- **Interface:** "+ Mosaic area" creates and selects an area, the effect section switches it to blur, and presenter controls stay hidden for areas.
- `Get-FFmpeg.ps1` now also requires `gblur`, `pixelize` and `split`.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36929907077/artifacts/11195807816): 148,494,268 bytes, archive SHA-256 `a9621cedbd54b018966bbe97b6ab2b5bb37feffcba073d718c135199ef4c28fc`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36929907077/artifacts/11196220307): 701,994,875 bytes, archive SHA-256 `7957c963773a698324e6a03bc1487bc7cc9db2f2bf46c011a24719a9e187cb96`.

## Windows portable build — 2026-10-01, karaoke captions (0.5 development)

[GitHub Actions run 36927521399](https://github.com/HNXS/Cutlery/actions/runs/36927521399) completed successfully for commit `c9104b706b931458b8ffc50a96678659cbbc2542`, with the same toolchain as the automatic-captions build. New coverage:

- **Engine test `karaokeCaptions`:**
  - Word grouping: pauses, sentence ends, line length and lone punctuation.
  - Placement with clip-local word starts; splitting a timed caption between its words; the plain fallback after a word-count change; the schema 8 round trip.
  - Pixel checks on rendered frames: the karaoke highlight moves from the first word to the second; "one word at a time" shows only the current word; a caption whose word count changed renders plainly.
- **Real Whisper model on synthesized speech:**
  - One cue per word, with sensible starts: "Welcome" 0.13 s, "to" 0.75 s, "Cutlery," 0.88 s, "this" 2.32 s, "video" 2.57 s, "editor" 2.88 s, …
  - An empty first cue from whisper.cpp is skipped when grouping.
  - Transcription took 55 s on the 2-core runner.
- Tests on Windows point the offscreen platform at `%WINDIR%/Fonts` (`QT_QPA_FONTDIR`). Without it, text in test renders used a tiny fallback font, so pixel checks on captions failed only on Windows. The application uses the normal Windows platform and system fonts.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36927521399/artifacts/11195410549): 148,489,604 bytes, archive SHA-256 `71d723affcd03eedca3626b3ba8ec3497481f128284e5e33d2a6326dc501e01e`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36927521399/artifacts/11194916737): 701,994,873 bytes, archive SHA-256 `72712465bfdd9548a2e53ebfe7fa3e08b1c705f71fc4c9e9b34c38d337fbb308`.

## Windows portable build — 2026-10-01, pause removal and loudness (0.5 development)

[GitHub Actions run 36896811347](https://github.com/HNXS/Cutlery/actions/runs/36896811347) completed successfully for commit `4eb7315b14f7df539e23c5b43726a53620700cdd`, with the same toolchain as the automatic-captions build. New coverage:

- **Engine test `pausesAndLoudness`:**
  - `Project::cutRanges` with overlapping, unsorted and out-of-range ranges, a cut at the clip start, ripple on the clip's track only, and detached-audio linking.
  - Parsing the EBU R128 summary.
  - End to end: a 3.5 s clip with a 1.5 s silence. One pause of about 1.26 s is found and removed, and undo restores the clip.
  - An MPEG-4 export normalised to −14 LUFS, measured afterwards within 1 LU of the target.
- **Interface tests:**
  - The loudness choice in the export dialog: the YouTube preset sets −14 LUFS, and a manual change makes the preset "Custom".
  - The Remove pauses dialog: find, preview text and removal.
- `Get-FFmpeg.ps1` now also requires `volume`, `ebur128` and `silencedetect`.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36896811347/artifacts/11181295204): 148,476,115 bytes, archive SHA-256 `7cb5ca46f8cc361a161765e62653f83bd830caf11c2405841832123888c2f0ca`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36896811347/artifacts/11180780838): 701,994,508 bytes, archive SHA-256 `74558d1c4eb8d6bcb973e2df1da9da5af7f9f16743ea3d8370b44d44d0f8e2f8`.

## Windows portable build — 2026-10-01, automatic captions (0.5 development)

[GitHub Actions run 36875006979](https://github.com/HNXS/Cutlery/actions/runs/36875006979) completed successfully for commit `dbc9d576e6ec3ca2af75e35d76ebaa68147f79b9`. It used Windows Server 2022, MSVC 2022, Qt 6.8.3, LGPL FFmpeg 8.1, ONNX Runtime 1.22.0 with DirectML, and whisper.cpp v1.9.4. whisper.cpp was built from commit `927cfce34f31707e17f2bff35c349632fb9e2c3a` (static, AVX2, OpenMP). The run covered:

- Compilation and both CTest suites, including the new `automaticCaptions` test:
  - SRT parsing;
  - caption placement through trims, positions and speed, with duplicate and reverse handling and no overlaps;
  - the full Editor path through `cutlery-ai transcribe` and whisper-cli on whisper.cpp's test model ("No speech found", the "AI captions" track, and cache reuse on a second run).
- The interface check that the caption dialog explains a missing speech-recognition component.
- The real Whisper large-v3-turbo q5_0 model with Silero VAD through `cutlery-ai`, on speech synthesized by Windows text-to-speech: "Welcome to Cutlery. This video editor works on your own computer." (5.7 s).
  - The transcript matched word for word.
  - On the 2-core runner the transcription took 71 s, of which 69 s was the encoder for one 30-second block.
- The packaged `whisper-cli.exe` with `PATH` limited to System32, using the bundled MSVC and OpenMP runtimes.

An earlier build without OpenMP did not finish within 15 minutes on the same runner: ggml's spinning thread pool stalls when threads outnumber free cores. It was replaced before release.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36875006979/artifacts/11169526333): 148,456,184 bytes, archive SHA-256 `7e07c1791dbec230af01a6d8aeb6525abea15b205a0dbc4590cbe9279741ada9`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36875006979/artifacts/11168892705): 701,994,440 bytes, archive SHA-256 `37870c4234d9ff7a27f19340fe8adc462b76ed2b5bb5ba175704669851e3d1bb`.

## Windows portable build — 2026-10-01, AI upscale and DirectML (0.5 development)

[GitHub Actions run 36862210410](https://github.com/HNXS/Cutlery/actions/runs/36862210410) completed successfully for commit `bd98b4ce9614e712ff27dc8972eb16203232b68d` on Windows Server 2022 with MSVC 2022, Qt 6.8.3, LGPL FFmpeg 8.1 and ONNX Runtime 1.22.0 with DirectML 1.15.4. Both NuGet packages are pinned by SHA-256. The Real-ESRGAN weights were converted to ONNX in CI by `tools/convert-realesrgan.py`. The run covered:

- Compilation of `cutlery-ai` with the DirectML provider, both CTest suites and QML startup.
- Engine test `aiCutout`: a stand-in upscaled copy replaces the clip's picture at the right source time. Both worker tasks run through `AiJobs` with stand-in ONNX models: queueing, a matte, and every frame of an upscale at twice the size.
- Interface test `aiCutoutControls`: both inspector options run their task and report the result.
- The real models through the built worker: a matte, a 320×240 → 1280×960 upscale, and the same upscale forced onto the CPU provider. The packaged worker also ran with `PATH` limited to System32.
- The CI runner has no GPU. DirectML reported "Specified display adapter handle is invalid", and the worker fell back to the CPU as designed. **GPU inference has not been run on real hardware yet.**

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36862210410/artifacts/11162916313): 147,544,195 bytes, archive SHA-256 `02f4b0839b5f025be6ba9af8e9c8cc50c0428612e022fce7840607600f1475fb`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36862210410/artifacts/11163275748): 167,930,616 bytes, archive SHA-256 `c3d3c9ef65f8c894410597af7b5740d29643a30db8c245bc570dee28e70cfe8c`.

Locally (Linux, Qt 6.4, CPU) the converted model was checked against bicubic scaling on a test image and a test video, including 64-pixel tiles, with no visible seams. On 4 CPU cores a 640×360 frame takes about 2.9 s.

## Windows portable build — 2026-10-01, AI background removal (0.5 development)

[GitHub Actions run 36850705437](https://github.com/HNXS/Cutlery/actions/runs/36850705437) completed successfully for commit `e5c9937b75690c420df51a3c803d06e3e3b069f0` on Windows Server 2022 with MSVC 2022, Qt 6.8.3, LGPL FFmpeg 8.1 and ONNX Runtime 1.22.0 (CPU). It covered compilation including `cutlery-matte`, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- `aiCutout` (engine): a generated matte applied through the renderer, checked for alignment between analysed frames, with offset ranges, flip and a circle shape. The worker runs end to end with a stand-in ONNX model, and the `Mattes` job manager is exercised.
- `aiCutoutControls` (interface): the inspector checkbox starts the analysis, which finishes with the matte covering the clip.
- The real U²-Net model ran on a test video through the built worker. The deployed worker also ran with `PATH` limited to System32, so the packaged `onnxruntime.dll` was loaded. Inference took about 1 s per analysed frame on the CI runner.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36850705437/artifacts/11156230322): 136,807,908 bytes, archive SHA-256 `73ae0be90d8305100583edc6993624b7408d1f69b8d4d8823fe2bc783a40ad02`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36850705437/artifacts/11156105821): 163,395,458 bytes, archive SHA-256 `ef760bcbdf52a7f4e573d6d970db9fde4232417679d490d5933426d6cc3eeeee`.

Locally (Linux, Qt 6.4) the real model correctly cut out a drawn person figure. No photographic footage has been checked yet.

## Windows portable build — 2026-10-01, presenter overlays (0.5 development)

[GitHub Actions run 36845035491](https://github.com/HNXS/Cutlery/actions/runs/36845035491) completed successfully for commit `493f4e12d88fedb24ff1a8c65b8b450510fec478` on Windows Server 2022 with MSVC 2022, Qt 6.8.3 and LGPL FFmpeg 8.1. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- the new `overlayStyles` engine test, with pixel checks of a circle with border ring and shadow, rounded corners, green-screen removal, keying combined with a circle and animated scale, schema 6 persistence, clip bounds, one-step position edits, and display-rotation probing;
- the `presenterControls` interface test: corner placement, mouse drag to move and corner drag to resize in the viewer, undo, shape and green-screen controls.

A 1280×720 still of a green-screen speaker in a circle with border and shadow over a test pattern was inspected visually. It found and fixed a defect where keyed areas inside the border showed the border colour instead of the background.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36845035491/artifacts/11153650251) (131,913,117 bytes, expires 2026-10-31). Archive SHA-256: `360d03f5389d2de064f868c98c2ef1d15b7dd4c9283d4b21a28da0307371353e`.

## Windows portable build — 2026-10-01, export presets (0.5 development)

[GitHub Actions run 36841760041](https://github.com/HNXS/Cutlery/actions/runs/36841760041) completed successfully for commit `acbff13afe51a6b505ea037cda4bc6626b8200ee` on Windows Server 2022 with MSVC 2022, Qt 6.8.3 and LGPL FFmpeg 8.1. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- the new `exportProfiles` engine test: candidate lists, output sizes, skipping a missing encoder with a cached result, and real AV1 (upscaled), VP9, ProRes and MPEG-4 exports checked with ffprobe for codec, size and audio codec;
- the `exportDialog` interface test: the YouTube preset fills the fields, a manual change becomes "Custom", and the extension follows the format.

The H.264 case accepts either a working encoder or the "No H264 encoder" message. Passing CTest output does not record which path the runner took, so hardware-encoder behaviour still needs checking on a real PC with an NVIDIA, AMD or Intel GPU.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36841760041/artifacts/11152345050) (131,887,842 bytes, expires 2026-10-31). Archive SHA-256: `7b1c1cd100db808effe15a95a93f43a3c831ee9c823d500b33b7e61deaef0df0`.

## Windows portable build — 2026-10-01, keyframes (0.5 development)

[GitHub Actions run 36834388932](https://github.com/HNXS/Cutlery/actions/runs/36834388932) completed successfully for commit `1475f8a36ebdc141e56a0d80320741f78c7f06fd` on Windows Server 2022 with MSVC 2022, Qt 6.8.3 and the shipped LGPL FFmpeg 8.1. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- the new `keyframes` engine test: interpolation, trim/split/speed rules, schema 5 roundtrip, validation; measured animated scale, position plus rotation, and opacity; windowed frames inside animations; animated volume loudness;
- the `keyframeControls` interface test: the ◇ toggle, auto-key on value change, timeline markers, previous/next navigation, and removal keeping the last value.

The preceding run 36833107826 failed because `perspective` is GPL-only and absent from the LGPL build. Geometry now uses `rotate`, `scale=eval=frame` and `overlay`. Local tests now run against the same FFmpeg 8.1 LGPL build (21 engine tests passed).

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36834388932/artifacts/11148399888) (131,863,714 bytes, expires 2026-10-31). Archive SHA-256: `0145e3b239b1005dfddd7b8b286a23aa81cc2dd18c65dbaf411cd1dd6534d88c`.

## Windows portable build — 2026-09-30, transitions (0.5 development)

[GitHub Actions run 36785406583](https://github.com/HNXS/Cutlery/actions/runs/36785406583) completed successfully for commit `a37fd73aac105c2bd366fe758731de1d6f16eb50` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The CTest suites include:

- the new `transitions` engine test: dissolve blend at the cut, windowed frames matching the full render inside a transition, wipe halves, equal-power audio level across the crossfade, exact mix length, model rules, schema 4 roundtrip and split;
- the `transitionMarkers` interface test: clicking **+** on a cut adds a dissolve and the inspector shows it.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36785406583/artifacts/11129756210) (131,846,407 bytes, expires 2026-10-30). Archive SHA-256: `2dc8a430ff87c2f0c30167bf7591c92f2efb76424a7aaabff24c9384e17c396b`.

Local (Ubuntu, Qt 6.4.2): 20 engine tests passed. A 640×360 still at a transition cut with 1080p H.264 sources rendered in 0.6 s. Audible crossfades on real hardware still require manual checking.

## Windows portable build — 2026-09-30, filmstrip thumbnails (0.5 development)

[GitHub Actions run 36781416020](https://github.com/HNXS/Cutlery/actions/runs/36781416020) completed successfully for commit `3c4a94d22da77bcc8027fb03dec70986095a9b89` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. It covered compilation, both CTest suites (the new `thumbnailStrips` engine test, and interface checks that video and image clips show loaded filmstrip tiles), QML startup, packaging, and packaged-executable startup. The application version is still 0.4.0 until the 0.5 feature set is complete.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36781416020/artifacts/11127622752) (131,828,795 bytes, expires 2026-10-30). Archive SHA-256: `869bb9498cda9cb5072d60cd55af12a44f3d17e05b9c475fc4e47a3dc6b1737d`.

Local (Ubuntu, Qt 6.4.2): 19 engine tests passed. Keyframe-only extraction took 1.2 s for 3 minutes of 1080p H.264, against 15 s when decoding every frame. The filmstrip and library poster rendering was visually inspected in application screenshots.

## Windows portable build — 2026-09-30, version 0.4.0

[GitHub Actions run 36777216642](https://github.com/HNXS/Cutlery/actions/runs/36777216642) completed successfully for commit `6ea8a79eaaf3579028b81f1674413c654612150e` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. It covered compilation, both CTest suites (engine including windowed rendering and live playback; interface including Space/K live play and pause), QML startup, packaging, and packaged-executable startup without SDK paths. The preceding run 36774621755 failed at compilation (`QtVideo::MapMode` is not in Qt 6.8.3); that was fixed in `6ea8a79`.

[Download Cutlery-0.4.0-win64-portable](https://github.com/HNXS/Cutlery/actions/runs/36777216642/artifacts/11126345710) (131,809,976 bytes). The CI artifact expires on 2026-10-30. Archive SHA-256: `3e19f4b5fb7a683f6c4f5eeab4247c89739b847c82262ca1372e940e61ad031b`.

The CI runner has no audio device, so its playback tests use the wall clock. Audible output and A/V offset on real hardware still require manual checking.

## Local development run — 2026-09-30, version 0.4.0

The network policy of this development container blocks the Qt SDK download, so Qt 6.8.3 was unavailable. Ubuntu 24.04's Qt 6.4.2 (GCC 13.3, FFmpeg 6.1.1) was used with a local-only relaxed version check. The Windows Qt 6.8.3 CI run is the authoritative build.

- Engine suite: **18 passed, 0 failed**. The new `windowedRender` test compares one-frame preview windows with whole-timeline renders at clip starts, fades, overlay edges and a reversed 2× clip. It also checks exact raw frame/PCM sizes for streamed playback and wall-clock pacing. The job test plays live, edits during playback (automatic restart), reaches the end, and pauses at an advanced frame.
- Interface suite: not meaningful on Qt 6.4. Unmodified single-key shortcuts (Space, K, Right) do not fire in that offscreen build, and the unchanged 0.3 suite crashes there. The 0.4 UI playback flow passed on Windows CI (above).
- Benchmark on a 3-minute 1080p30 H.264 file, 4-core container, 640×360 preview:

| Operation | 0.3 | 0.4 |
|---|---|---|
| Preview frame at 0:10 | 1,377 ms | 130 ms |
| Preview frame at 1:30 | 11,890 ms | 129 ms |
| Preview frame at 2:30 | 19,629 ms | 119 ms |
| Play from 1:30 → first frame (960×540) | cache render of the full timeline first (6,815 ms per 30 s) | 164 ms |

Audible playback and A/V offset on real Windows audio hardware still require manual checking.

## Windows portable build — 2026-09-30, version 0.3.0

[GitHub Actions run 36768658353](https://github.com/HNXS/Cutlery/actions/runs/36768658353) completed successfully for commit `d80150d78c218ca3e3457ad6dd48915c876dac00` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. Compilation including the native icon resource, both engine/interface test suites, QML startup, packaging and packaged-executable startup without SDK paths passed.

[Download Cutlery-0.3.0-win64-portable](https://github.com/HNXS/Cutlery/actions/runs/36768658353/artifacts/11122917795) (131,803,167 bytes). The CI artifact expires on 2026-10-30. Archive SHA-256: `823a0e62430bf0ae7615e8b4c9c1a067d59eba9c6d76ab113060d95e676d95c0`.

## Local development run — 2026-09-30, version 0.3.0

Built with GCC 13.3, Qt 6.8.3 and CMake 4.4.3 on Ubuntu 24.04.

- Engine suite: **17 passed, 0 failed, 0 skipped**, including setup/cleanup. New coverage checks magnetic compaction/reordering/cross-track moves/trims/deletion/duplication, independent track timing, undo, lock guards, schema 2 migration and schema 3 roundtrip.
- File-drop model checks cover ordered insertion, a missing file between valid files, undo/redo, destination-track renumbering during asynchronous import, locked destinations, and removal of a destination while probing.
- Interface suite: **4 passed, 0 failed, 0 skipped**, including setup/cleanup. Actual QML gestures cover library-to-track drops for generated video/audio/images, same-track and cross-track clip moves, per-track snapping, Magnet toggle/reorder/undo, Escape cancellation for both drag types, locked-drop rejection, OS URL-MIME drops to a track and library, plus the existing trimming/keyboard/playback flow. No QML warnings were emitted.
- Visual inspection: actual demo window and the original vector app icon rendered at 256 pixels. The Windows ICO contains 16, 24, 32, 48, 64, 128 and 256 pixel variants; Windows resource compilation is part of CI.

These are automated event-level drop checks, not manual Explorer testing on a user's managed Windows desktop. Audible output, high-DPI behavior and long-session performance still require manual qualification.

## Windows portable build — 2026-09-30, version 0.2.0

[GitHub Actions run 36689916787](https://github.com/HNXS/Cutlery/actions/runs/36689916787) completed successfully for commit `4cfd7ba2aa84187f39aff8b8eeeb5496991447ae` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. Configure/compile, engine and interface tests, QML startup, portable packaging, and packaged-executable startup with SDK paths removed all passed.

[Download Cutlery-0.2.0-win64-portable](https://github.com/HNXS/Cutlery/actions/runs/36689916787/artifacts/11085730483) (131,738,605 bytes). The CI artifact expires on 2026-10-30; later successful workflow runs can provide fresh artifacts. Archive SHA-256: `eb9e3979246fb202edf113dada069025b464650aa2da633d35afd9012f6367a4`.

## Local development run — 2026-09-30, version 0.2.0

Built with GCC 13.3, Qt 6.8.3 and CMake 4.4.3 on Ubuntu 24.04. This is a development verification host, not a supported end-user distribution.

- `cutlery_tests`: **15 passed, 0 failed, 0 skipped**, including test setup/cleanup.
- `cutlery_ui_tests`: **3 passed, 0 failed, 0 skipped**, including test setup/cleanup. Actual QML mouse/keyboard checks cover drag trimming, split/undo, typing without triggering timeline commands, retained title drafts, shortcut remapping, and render-then-play with an advancing Qt Multimedia position followed by pause. No QML warnings were emitted.
- Actual-render coverage: generate source video/audio, compose a timed image overlay, render MP4, check 60 output frames, inspect foreground/background colours at multiple times, verify audio data, and compare a shared-graph preview still.
- Asynchronous job coverage: image import, timeline insertion, WebM export, refusal to overwrite an existing destination, playback-cache rendering/invalidation, cancellation and partial-file cleanup.
- Offline boundary: a local HLS playlist referencing HTTPS is rejected by the codec protocol whitelist before network access.
- Track rendering checks: picture hiding, track mute, solo suppression, and audio-only clips are checked against decoded video pixels and PCM audio. Detach-audio and undo are exercised.
- Model coverage: rational overflow, Unicode/relative-path JSON roundtrip, schema 1 migration and schema 2 roundtrip, invalid time/source bounds, split/reverse/ripple semantics, exact forward/reverse trims, edge snapping, locked-track rejection, undo/redo and SRT roundtrip.
- Waveforms: split-byte PCM peak accumulation, extraction from actual generated audio, disk cache reuse, and changed-file fingerprint invalidation. Shortcut preferences: conflict/invalid-key rejection, remapping, persistence, reset, and failed-save rollback.
- QML startup smoke test: passed. A rendered demo window was captured and visually inspected; `docs/images/editor.png` is that actual application window.

The host has no usable audio device; audible playback was not manually evaluated. The standalone Qt test executable was used because the downloaded CTest runner crashed on this host. Windows CI uses the runner-provided CTest and repeats these tests, then checks the packaged executable without SDK paths. Consult the linked GitHub Actions run for its independent result.

See BUILDING.md for qualification work that remains. No performance, HDR, clean-machine, hardware-encoder or production-readiness claims are implied by these checks.
