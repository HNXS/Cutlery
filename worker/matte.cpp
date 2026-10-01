// cutlery-matte: computes a person matte (alpha) for a range of a video with a segmentation
// model in ONNX format. Runs as a separate process so the AI runtime never loads into the
// editor, a crash cannot take the editor down, and cancelling is a plain kill.
//
// FFmpeg decodes the range at the analysis rate and the model's input size; each matte is
// smoothed over time, resized to the picture's aspect ratio and encoded as lossless grayscale
// FFV1 (every frame a keyframe, so the renderer can seek exactly). The matte's timestamps start
// at zero for the first analysed source time.
//
//   cutlery-matte --ffmpeg F --model M --input IN --output OUT.mkv --size WxH
//                 [--start S] [--duration D] [--rate R]
//
// Prints "progress <done> <total>" lines and exits non-zero on failure.
#include <onnxruntime_cxx_api.h>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
int fail(const QString &message) {
    std::fprintf(stderr, "cutlery-matte: %s\n", qUtf8Printable(message));
    return 1;
}
QString num(double v) {
    return QString::number(v, 'f', 6);
}
bool readExactly(QProcess &p, char *data, qint64 size) {
    qint64 done = 0;
    while (done < size) {
        const auto n = p.read(data + done, size - done);
        if (n < 0)
            return false;
        done += n;
        if (done < size && !p.waitForReadyRead(120000) && p.bytesAvailable() == 0)
            return false;
    }
    return true;
}
bool writeAll(QProcess &p, const char *data, qint64 size) {
    if (p.write(data, size) != size)
        return false;
    while (p.bytesToWrite() > 0)
        if (!p.waitForBytesWritten(120000))
            return false;
    return true;
}
} // namespace

int main(int argc, char **argv) {
    QString ffmpeg, model, input, output;
    double start = 0, duration = -1, rate = 8;
    int width = 0, height = 0;
    for (int i = 1; i + 1 < argc; i += 2) {
        const QString key = QString::fromUtf8(argv[i]), value = QString::fromUtf8(argv[i + 1]);
        if (key == "--ffmpeg")
            ffmpeg = value;
        else if (key == "--model")
            model = value;
        else if (key == "--input")
            input = value;
        else if (key == "--output")
            output = value;
        else if (key == "--start")
            start = value.toDouble();
        else if (key == "--duration")
            duration = value.toDouble();
        else if (key == "--rate")
            rate = value.toDouble();
        else if (key == "--size") {
            const auto parts = value.split('x');
            if (parts.size() == 2) {
                width = parts[0].toInt();
                height = parts[1].toInt();
            }
        } else
            return fail("unknown argument " + key);
    }
    if (ffmpeg.isEmpty() || model.isEmpty() || input.isEmpty() || output.isEmpty() ||
        width < 2 || height < 2 || rate <= 0 || rate > 60)
        return fail("usage: --ffmpeg F --model M --input IN --output OUT --size WxH "
                    "[--start S] [--duration D] [--rate R]");
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "cutlery-matte");
        Ort::SessionOptions options;
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
#ifdef _WIN32
        Ort::Session session(env, model.toStdWString().c_str(), options);
#else
        Ort::Session session(env, model.toStdString().c_str(), options);
#endif
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
        QImage small(mw, mh, QImage::Format_Grayscale8);
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
                auto *line = small.scanLine(y);
                for (int x = 0; x < mw; ++x) {
                    // Firmer edges: an opaque body and a clear background without a halo.
                    const float v = (matte[size_t(y) * mw + x] - 0.15f) / 0.75f;
                    line[x] = uchar(std::lround(std::clamp(v, 0.f, 1.f) * 255));
                }
            }
            const auto frame = small.scaled(width, height, Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation)
                                   .convertToFormat(QImage::Format_Grayscale8);
            for (int y = 0; y < height; ++y)
                if (!writeAll(encoder, reinterpret_cast<const char *>(frame.constScanLine(y)),
                              width))
                    return fail("matte encoder stopped");
            ++done;
            std::printf("progress %lld %lld\n", static_cast<long long>(done),
                        static_cast<long long>(std::max(total, done)));
            std::fflush(stdout);
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
    } catch (const Ort::Exception &e) {
        return fail(QString("ONNX Runtime: ") + e.what());
    }
    return 0;
}
