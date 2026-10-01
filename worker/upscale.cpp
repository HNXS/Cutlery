// Task "upscale": super-resolution of a video range with a fully convolutional model that maps
// RGB 0..1 [1,3,H,W] to [1,3,kH,kW] (Real-ESRGAN realesr-general-x4v3: k = 4).
//
// FFmpeg decodes every frame of the range at the source size and rate. The model runs on
// overlapping tiles, so memory stays bounded for large frames; each tile's margin is discarded.
// FFmpeg resizes the result to the requested size (Lanczos) and stores it as ProRes 422, an
// intra-only format the renderer can seek exactly. Timestamps start at zero for the first
// source time.
//
//   cutlery-ai upscale --ffmpeg F --model M --input IN --output OUT.mov --source WxH
//                      --size WxH --rate R [--start S] [--duration D] [--tile T] [--cpu 1]
// R is the source frame rate, e.g. 30000/1001.
#include "common.h"
#include <cmath>
#include <vector>

int worker::upscale(const QHash<QString, QString> &o) {
    const QString ffmpeg = o.value("ffmpeg"), model = o.value("model"), input = o.value("input"),
                  output = o.value("output"), rate = o.value("rate");
    const double start = o.value("start", "0").toDouble(),
                 duration = o.value("duration", "-1").toDouble();
    const int tile = std::max(64, o.value("tile", "384").toInt()), margin = 16;
    int sw = 0, sh = 0, width = 0, height = 0;
    double fps = 0;
    if (const auto parts = rate.split('/'); !parts.isEmpty() && parts[0].toDouble() > 0)
        fps = parts[0].toDouble() / (parts.size() == 2 ? parts[1].toDouble() : 1);
    if (ffmpeg.isEmpty() || model.isEmpty() || input.isEmpty() || output.isEmpty() ||
        !size(o.value("source"), sw, sh) || !size(o.value("size"), width, height) ||
        !(fps > 0 && fps <= 240))
        return fail("usage: upscale --ffmpeg F --model M --input IN --output OUT --source WxH "
                    "--size WxH --rate R [--start S] [--duration D] [--tile T]");
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "cutlery-ai");
    auto session = openModel(env, model, o.value("cpu") == "1");
    Ort::AllocatorWithDefaultOptions allocator;
    const auto inputName = session->GetInputNameAllocated(0, allocator);
    const auto outputName = session->GetOutputNameAllocated(0, allocator);
    const char *inputs[] = {inputName.get()}, *outputs[] = {outputName.get()};
    const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    // Model scale from a probe run on a small tile.
    int k = 0;
    {
        std::vector<float> probe(3 * 16 * 16, 0.5f);
        std::vector<int64_t> dims{1, 3, 16, 16};
        auto value = Ort::Value::CreateTensor<float>(memory, probe.data(), probe.size(),
                                                     dims.data(), dims.size());
        auto out = session->Run(Ort::RunOptions{nullptr}, inputs, &value, 1, outputs, 1);
        const auto shape = out[0].GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() != 4 || shape[1] != 3 || shape[2] % 16 != 0 || shape[2] < 16)
            return fail("model output must be [1,3,kH,kW]");
        k = int(shape[2] / 16);
    }
    const int ow = sw * k, oh = sh * k;

    QProcess decoder;
    decoder.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QStringList args{"-hide_banner", "-nostdin", "-v", "error", "-ss", num(start)};
    if (duration > 0)
        args << "-t" << num(duration);
    args << "-i" << input << "-map" << "0:v:0" << "-an" << "-vf"
         << QString("fps=%1,scale=%2:%3:flags=bicubic,format=rgb24").arg(rate).arg(sw).arg(sh)
         << "-f" << "rawvideo" << "pipe:1";
    decoder.start(ffmpeg, args);
    if (!decoder.waitForStarted())
        return fail("cannot start FFmpeg");
    QProcess encoder;
    encoder.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    encoder.start(ffmpeg,
                  {"-hide_banner", "-nostdin", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt",
                   "rgb24", "-s", QString("%1x%2").arg(ow).arg(oh), "-framerate", rate, "-i",
                   "pipe:0", "-vf",
                   QString("scale=%1:%2:flags=lanczos+accurate_rnd+full_chroma_int,setsar=1,"
                           "format=yuv422p10le")
                       .arg(width)
                       .arg(height),
                   "-c:v", "prores_ks", "-profile:v", "2", "-vendor", "apl0", "-f", "mov",
                   output});
    if (!encoder.waitForStarted())
        return fail("cannot start FFmpeg encoder");

    const qint64 total = duration > 0 ? qint64(std::ceil(duration * fps - 1e-6)) : 0;
    std::vector<unsigned char> frame(size_t(sw) * sh * 3), result(size_t(ow) * oh * 3);
    std::vector<float> tensor;
    qint64 done = 0;
    while (readExactly(decoder, reinterpret_cast<char *>(frame.data()), qint64(frame.size()))) {
        for (int ty = 0; ty < sh; ty += tile)
            for (int tx = 0; tx < sw; tx += tile) {
                // The tile plus a margin of real neighbouring pixels on each side.
                const int x0 = std::max(0, tx - margin), y0 = std::max(0, ty - margin),
                          x1 = std::min(sw, tx + tile + margin),
                          y1 = std::min(sh, ty + tile + margin);
                const int w = x1 - x0, h = y1 - y0;
                const size_t plane = size_t(w) * h;
                tensor.resize(plane * 3);
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x) {
                        const auto *px = &frame[(size_t(y0 + y) * sw + x0 + x) * 3];
                        for (int c = 0; c < 3; ++c)
                            tensor[c * plane + size_t(y) * w + x] = px[c] / 255.f;
                    }
                std::vector<int64_t> dims{1, 3, h, w};
                auto value = Ort::Value::CreateTensor<float>(memory, tensor.data(), tensor.size(),
                                                             dims.data(), dims.size());
                auto out = session->Run(Ort::RunOptions{nullptr}, inputs, &value, 1, outputs, 1);
                const float *up = out[0].GetTensorData<float>();
                const size_t upPlane = size_t(w) * k * h * k;
                const int cx0 = std::min(tx + tile, sw), cy0 = std::min(ty + tile, sh);
                for (int y = ty * k; y < cy0 * k; ++y)
                    for (int x = tx * k; x < cx0 * k; ++x) {
                        const size_t at = size_t(y - y0 * k) * w * k + (x - x0 * k);
                        auto *dst = &result[(size_t(y) * ow + x) * 3];
                        for (int c = 0; c < 3; ++c)
                            dst[c] = static_cast<unsigned char>(
                                std::lround(std::clamp(up[c * upPlane + at], 0.f, 1.f) * 255));
                    }
            }
        if (!writeAll(encoder, reinterpret_cast<const char *>(result.data()),
                      qint64(result.size())))
            return fail("encoder stopped");
        progress(++done, total);
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
    return 0;
}
