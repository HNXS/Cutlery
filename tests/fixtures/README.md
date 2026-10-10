# Test fixtures

- `face-straight.jpg`, `face-side.jpg`: NASA photograph of astronaut Eileen Collins (public domain; "no known copyright restrictions", NASA Great Images), taken from scikit-image 0.24.0's `skimage/data/astronaut.png`. It was enlarged three times with Lanczos and cropped to 960 × 540. In `face-side.jpg`, the gaze has been moved sideways with Cutlery's eye-contact warp, for testing the correction.
- `whisper-for-tests-tiny.bin`: whisper.cpp's `models/for-tests-ggml-tiny.bin` (MIT, v1.9.4), a loadable model without trained weights.
- `red-matte.onnx`, `nearest-x2.onnx`, `half-spectrum.onnx`: stand-in models from `make-test-models.py`, for testing the cutlery-ai worker without the real models.
- `translate-copy/`: a Marian-shaped stand-in translator from `make-test-models.py` that gives its input back; `source.spm` is a small SentencePiece unigram model trained on Cutlery's documentation.
