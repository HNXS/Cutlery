# Research: eye-contact correction

Status: research note (October 2026), not implemented. This note decides how Cutlery could make a presenter who reads from a script look into the camera, offline and with a licence Cutlery can ship.

## What the feature has to do

- **Find the gaze:** find the eyes in each frame and estimate where they look.
- **Redraw the eyes:** redraw the eye regions so the gaze points at the lens, keeping blinks, lids and lighting.
- **Know its limits:** fade out when the head turns too far, the eyes close, or the face is small. Above roughly 20–25° of head or gaze angle every known method looks uncanny.
- **Stay stable:** without flicker between frames, since editors look at the result frame by frame.

## Options

| Option | Quality | Runs on | Licence for a shipped app | Verdict |
|---|---|---|---|---|
| **NVIDIA Maxine Eye Contact** (AR SDK, Windows) | Best available; production-grade, handles blinks and limits itself | NVIDIA RTX GPUs only | NVIDIA SDK licence (the evaluation and EA builds are evaluation licences). The runtime and models are installed from NVIDIA, not redistributed by us. | Optional backend: use it when the user has installed the Maxine AR runtime |
| **LivePortrait** (eye retargeting) | Good still frames, but it re-renders the whole face, with temporal jitter | GPU (PyTorch; ONNX ports exist) | Code and weights MIT. The default face detector/landmarks (InsightFace buffalo_l) are **non-commercial only**. A MediaPipe variant avoids that. | Too heavy and too risky as the main path |
| **Classical warp: landmarks + eye-region flow** (GazeDirector-style; *Eye Contact Correction using Deep Neural Networks*, 2019) | Good for small corrections (reading a script a few degrees off-axis), the common case | CPU or DirectML; small models | Our own code; MediaPipe Face Landmarker with iris is **Apache-2.0** | **Recommended own path** |
| **Small research models** (LiteGaze, gaze-correction-cam, DIM in-painting) | Uneven; mostly webcam-resolution demos | CPU/GPU | Mixed (BSD-3 for gaze-correction-cam; others unclear or research-only) | Reference only; no weights to ship without checking each |

## Recommendation

1. **Own path, AI pack:** an `eyecontact` task in `cutlery-ai`, like the matte and upscale tasks.
   - **Landmarks:** MediaPipe Face Landmarker (with iris), converted to ONNX and run through the existing ONNX Runtime/DirectML stack. Licence: Apache-2.0, to be added to THIRD_PARTY.
   - **Per frame:** estimate the head pose and the iris offset from the eye centre. The target is the iris position that faces the camera, limited to about 15° of correction.
   - **Redraw:** warp only the eye regions with a smooth flow field (moving iris and upper lid, fixed eye corners), then blend with a feathered mask.
   - **Fade out:** the correction fades with blink detection (eye aspect ratio), large head turns and small faces.
   - **Stability:** smooth over time with a One-Euro filter on landmarks and target.
   - **Caching:** results are cached per media fingerprint like the other AI outputs (`…-eyecontact-v1`). The renderer then uses the corrected video instead of the source, like AI upscaling.
   - **Learned refinement (optional, later):** a small learned model (a DeepWarp-style flow correction trained on synthetic data). Only with weights whose licence we have checked.
2. **Maxine backend, optional:** when the Maxine AR runtime with the Eye Contact feature is installed on an RTX machine, offer it as the higher-quality engine. Cutlery would call the user's installed runtime and ship no NVIDIA files. Check the then-current SDK licence before release.
3. **Not LivePortrait with InsightFace:** the non-commercial landmark weights are incompatible with selling Cutlery or a paid AI pack.

## Effort and risks

- **Effort:** the own path is about 2–3 weeks:
  - landmark model conversion and tests;
  - warp and blend;
  - temporal smoothing;
  - UI (an "Eye contact" option on video clips, with a strength slider and a before/after toggle).
- **Main risk:** quality on glasses with reflections, strong side light, and low-resolution webcam footage. The fade-out rules keep failures subtle rather than uncanny.
- **Tests:** synthetic faces would not be convincing. Validate on a small set of recorded presenter clips with known gaze offsets, and measure the iris offset after correction.

## Sources

- [NVIDIA Maxine Eye Contact, developer blog](https://developer.nvidia.com/blog/improve-human-connection-in-video-conferences-with-nvidia-maxine-eye-contact)
- [Maxine Eye Contact model card](https://build.nvidia.com/nvidia/eyecontact/modelcard)
- [Maxine Windows AR SDK (NGC)](https://catalog.ngc.nvidia.com/orgs/nvidia/maxine/collections/maxine_windows_ar_sdk_collection_ga/-)
- [ComfyUI LivePortraitKJ: licence notes on InsightFace and MediaPipe](https://github.com/kijai/ComfyUI-LivePortraitKJ)
- [Eye Contact Correction using Deep Neural Networks (arXiv:1906.05378)](https://arxiv.org/pdf/1906.05378)
- [GazeDirector (arXiv:1704.08763)](https://arxiv.org/pdf/1704.08763)
- [gaze-correction-cam (BSD-3-Clause)](https://github.com/WangWilly/gaze-correction-cam)
- [LiteGaze](https://ece.utexas.edu/capstone/2024-spring/litegaze-lightweight-gaze-correction-model-and-conferencing-application)
