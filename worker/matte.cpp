// Task "matte": a person matte (alpha) for a range of a video, from a segmentation model with a
// [1,3,H,W] input and a [1,1,H,W] output.
//
// FFmpeg decodes the range at the analysis rate and the model's input size; each matte is
// smoothed over time, resized to the picture's aspect ratio and encoded as lossless grayscale
// FFV1 (every frame a keyframe, so the renderer can seek exactly). The matte's timestamps start
// at zero for the first analysed source time.
//
//   cutlery-ai matte --ffmpeg F --model M --input IN --output OUT.mkv --size WxH
//                    [--start S] [--duration D] [--rate R] [--cpu 1]
#include "common.h"
#include <QImage>
#include <cmath>
#include <vector>

int worker::matte(const QHash<QString, QString> &o) {
    const QString ffmpeg = o.value("ffmpeg"), model = o.value("model"), input = o.value("input"),
                  output = o.value("output");
    const double start = o.value("start", "0").toDouble(),
                 duration = o.value("duration", "-1").toDouble(),
                 rate = o.value("rate", "8").toDouble();
    int width = 0, height = 0;
    if (ffmpeg.isEmpty() || model.isEmpty() || input.isEmpty() || output.isEmpty() ||
        !size(o.value("size"), width, height) || rate <= 0 || rate > 60)
        return fail("usage: matte --ffmpeg F --model M --input IN --output OUT --size WxH "
                    "[--start S] [--duration D] [--rate R]");
    {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "cutlery-ai");
        auto owned = openModel(env, model, o.value("cpu") == "1");
        auto &session = *owned;
        Ort::AllocatorWithDefaultOptions allocator;
        const auto inputName = session.GetInputNameAllocated(0, allocator);
        const auto outputName = session.GetOutputNameAllocated(0, allocator);
        const auto shape = session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() != 4 || shape[1] != 3 || shape[2] <= 0 || shape[3] <= 0)
            return fail("model input must be [1,3,H,W]");
        const int mw = int(shape[3]), mh = int(shape[2]);
        std::vector<int64_t> dims{1, 3, mh, mw};
        const size_t plane = size_t(mw) * mh;

        QProcess decoder;
        decoder.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        QStringList args{"-hide_banner", "-nostdin", "-v", "error", "-ss", num(start)};
        if (duration > 0)
            args << "-t" << num(duration);
        args << "-i" << input << "-map" << "0:v:0" << "-an" << "-vf"
             << QString("fps=%1,scale=%2:%3:flags=bicubic,format=rgb24")
                    .arg(num(rate))
                    .arg(mw)
                    .arg(mh)
             << "-f" << "rawvideo" << "pipe:1";
        decoder.start(ffmpeg, args);
        if (!decoder.waitForStarted())
            return fail("cannot start FFmpeg");
        QProcess encoder;
        encoder.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        encoder.start(ffmpeg, {"-hide_banner", "-nostdin", "-v", "error", "-y", "-f", "rawvideo",
                               "-pix_fmt", "gray", "-s", QString("%1x%2").arg(width).arg(height),
                               "-framerate", num(rate), "-i", "pipe:0", "-c:v", "ffv1", "-g", "1",
                               "-f", "matroska", output});
        if (!encoder.waitForStarted())
            return fail("cannot start FFmpeg encoder");

        const qint64 total = duration > 0 ? qint64(std::ceil(duration * rate - 1e-6)) : 0;
        std::vector<char> rgb(plane * 3);
        std::vector<float> tensor(plane * 3), previous;
        const float mean[] = {0.485f, 0.456f, 0.406f}, deviation[] = {0.229f, 0.224f, 0.225f};
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const char *inputs[] = {inputName.get()}, *outputs[] = {outputName.get()};
        QImage lowRes(mw, mh, QImage::Format_Grayscale8);
        qint64 done = 0;
        while (readExactly(decoder, rgb.data(), qint64(rgb.size()))) {
            // Normalised like the model's training data: scaled by the frame maximum, then
            // ImageNet mean and deviation, planar RGB.
            const auto *px = reinterpret_cast<const unsigned char *>(rgb.data());
            const float peak = std::max<float>(1, *std::max_element(px, px + plane * 3));
            for (size_t i = 0; i < plane; ++i)
                for (int ch = 0; ch < 3; ++ch)
                    tensor[ch * plane + i] = (px[i * 3 + ch] / peak - mean[ch]) / deviation[ch];
            auto value = Ort::Value::CreateTensor<float>(memory, tensor.data(), tensor.size(),
                                                         dims.data(), dims.size());
            auto result = session.Run(Ort::RunOptions{nullptr}, inputs, &value, 1, outputs, 1);
            const float *out = result[0].GetTensorData<float>();
            if (result[0].GetTensorTypeAndShapeInfo().GetElementCount() < plane)
                return fail("unexpected model output size");
            std::vector<float> matte(out, out + plane);
            const auto [lo, hi] = std::minmax_element(matte.begin(), matte.end());
            const float low = *lo, range = std::max(1e-6f, *hi - *lo);
            for (auto &v : matte)
                v = (v - low) / range;
            // Temporal smoothing against flicker between analysed frames.
            if (!previous.empty())
                for (size_t i = 0; i < plane; ++i)
                    matte[i] = 0.65f * matte[i] + 0.35f * previous[i];
            previous = matte;
            for (int y = 0; y < mh; ++y) {
                auto *line = lowRes.scanLine(y);
                for (int x = 0; x < mw; ++x) {
                    // Firmer edges: an opaque body and a clear background without a halo.
                    const float v = (matte[size_t(y) * mw + x] - 0.15f) / 0.75f;
                    line[x] = uchar(std::lround(std::clamp(v, 0.f, 1.f) * 255));
                }
            }
            const auto frame = lowRes.scaled(width, height, Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation)
                                   .convertToFormat(QImage::Format_Grayscale8);
            for (int y = 0; y < height; ++y)
                if (!writeAll(encoder, reinterpret_cast<const char *>(frame.constScanLine(y)),
                              width))
                    return fail("matte encoder stopped");
            ++done;
            progress(done, total);
        }
        decoder.waitForFinished(-1);
        if (decoder.exitStatus() != QProcess::NormalExit || decoder.exitCode() != 0)
            return fail("decoding failed");
        if (done == 0)
            return fail("no video frames in range");
        encoder.closeWriteChannel();
        if (!encoder.waitForFinished(-1) || encoder.exitStatus() != QProcess::NormalExit ||
            encoder.exitCode() != 0)
            return fail("encoding failed");
    }
    return 0;
}
