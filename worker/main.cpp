// cutlery-ai: the optional AI worker. Runs as a separate process so the AI runtime never loads
// into the editor, a crash cannot take the editor down, and cancelling is a plain kill.
//
//   cutlery-ai matte   ... person matte for background removal (see matte.cpp)
//   cutlery-ai upscale ... super-resolution of a video range (see upscale.cpp)
//   cutlery-ai transcribe ... speech to subtitles with whisper.cpp (see transcribe.cpp)
//
// Common options: --ffmpeg F --model M --input IN --output OUT [--cpu 1]. Prints
// "device gpu|cpu" and "progress <done> <total>" lines; exits non-zero on failure.
#include "common.h"

int main(int argc, char **argv) {
    const QString task = argc > 1 ? QString::fromUtf8(argv[1]) : QString();
    const auto o = worker::options(argc, argv, 2);
    try {
        if (task == "matte")
            return worker::matte(o);
        if (task == "upscale")
            return worker::upscale(o);
        if (task == "transcribe")
            return worker::transcribe(o);
    } catch (const Ort::Exception &e) {
        return worker::fail(QString("ONNX Runtime: ") + e.what());
    }
    return worker::fail("usage: cutlery-ai matte|upscale|transcribe --ffmpeg F --model M --input IN "
                        "--output OUT ...");
}
