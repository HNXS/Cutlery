// Task "inpaint": removes something from a video by painting over an area of every frame with
// LaMa (Suvorov et al. 2021, Apache-2.0), an image inpainting model, exported to ONNX.
//
// The area is a polygon in fractions of the source picture ("x,y x,y ..."), the same in every
// frame. It is widened by a few pixels (edges of the object), a square around it with plenty of
// surroundings is cut out and scaled to the model's 512 × 512 input, and the model fills the
// area from what lies around it. The result is scaled back and blended in with a soft edge;
// pixels outside the area stay as decoded. While the surroundings hardly change from one frame
// to the next (a still camera), the filled area is averaged with the previous frame's so it does
// not shimmer.
//
// The result is ProRes 422 at the processing size, like the eye-contact task, and replaces the
// source in the renderer.
//
//   cutlery-ai inpaint --ffmpeg F --model M --input IN --output OUT.mov --source WxH --rate R
//                      --region "x,y x,y ..." [--start S] [--duration D] [--cpu 1]
#include "common.h"
#include <QFile>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr int kSide = 512;

struct Outline {
    std::vector<double> x, y;
    bool contains(double px, double py) const {
        bool inside = false;
        for (size_t i = 0, j = x.size() - 1; i < x.size(); j = i++)
            if ((y[i] > py) != (y[j] > py) &&
                px < (x[j] - x[i]) * (py - y[i]) / (y[j] - y[i]) + x[i])
                inside = !inside;
        return inside;
    }
};

// Running maximum (dilation) or mean (blur) over a square of the given radius, separably.
std::vector<float> boxFilter(const std::vector<float> &in, int w, int h, int radius, bool maximum) {
    if (radius <= 0)
        return in;
    std::vector<float> tmp(in.size()), out(in.size());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float v = 0.f;
            int n = 0;
            for (int k = std::max(0, x - radius); k <= std::min(w - 1, x + radius); ++k, ++n)
                v = maximum ? std::max(v, in[size_t(y) * w + k]) : v + in[size_t(y) * w + k];
            tmp[size_t(y) * w + x] = maximum ? v : v / n;
        }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float v = 0.f;
            int n = 0;
            for (int k = std::max(0, y - radius); k <= std::min(h - 1, y + radius); ++k, ++n)
                v = maximum ? std::max(v, tmp[size_t(k) * w + x]) : v + tmp[size_t(k) * w + x];
            out[size_t(y) * w + x] = maximum ? v : v / n;
        }
    return out;
}

// Bilinear sample of an interleaved RGB byte image at (fx, fy), channel c.
float sample(const unsigned char *rgb, int w, int h, double fx, double fy, int c) {
    fx = std::clamp(fx, 0., w - 1.);
    fy = std::clamp(fy, 0., h - 1.);
    const int x0 = int(fx), y0 = int(fy), x1 = std::min(w - 1, x0 + 1), y1 = std::min(h - 1, y0 + 1);
    const double ax = fx - x0, ay = fy - y0;
    auto at = [&](int x, int y) { return double(rgb[(size_t(y) * w + x) * 3 + c]); };
    return float((at(x0, y0) * (1 - ax) + at(x1, y0) * ax) * (1 - ay) +
                 (at(x0, y1) * (1 - ax) + at(x1, y1) * ax) * ay);
}
} // namespace

int worker::inpaint(const QHash<QString, QString> &o) {
    const QString ffmpeg = o.value("ffmpeg"), model = o.value("model"), input = o.value("input"),
                  output = o.value("output"), rate = o.value("rate");
    const double start = o.value("start", "0").toDouble(),
                 duration = o.value("duration", "-1").toDouble();
    int width = 0, height = 0;
    double fps = 0;
    if (const auto parts = rate.split('/'); !parts.isEmpty() && parts[0].toDouble() > 0)
        fps = parts[0].toDouble() / (parts.size() == 2 ? parts[1].toDouble() : 1);
    Outline area;
    for (const auto &point : o.value("region").split(' ', Qt::SkipEmptyParts)) {
        const auto xy = point.split(',');
        if (xy.size() != 2)
            return fail("the region is \"x,y x,y ...\" in fractions of the picture");
        area.x.push_back(xy[0].toDouble());
        area.y.push_back(xy[1].toDouble());
    }
    if (ffmpeg.isEmpty() || model.isEmpty() || input.isEmpty() || output.isEmpty() ||
        !size(o.value("source"), width, height) || !(fps > 0 && fps <= 240) || area.x.size() < 3)
        return fail("usage: inpaint --ffmpeg F --model M --input IN --output OUT --source WxH "
                    "--rate R --region \"x,y x,y x,y ...\" [--start S] [--duration D]");

    // The area in pixels, widened a little so the object's edges go too.
    for (auto &x : area.x)
        x *= width;
    for (auto &y : area.y)
        y *= height;
    const int grow = std::max(2, int(std::lround(std::min(width, height) * 0.006)));
    std::vector<float> full(size_t(width) * height, 0.f);
    int x0 = width, y0 = height, x1 = -1, y1 = -1;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (area.contains(x + 0.5, y + 0.5)) {
                full[size_t(y) * width + x] = 1.f;
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
    if (x1 < 0)
        return fail("the region covers no pixels");
    // The square cut out around it: the area with plenty of surroundings, at least 512 pixels
    // where the picture allows, and inside the picture.
    const int bw = x1 - x0 + 1 + 2 * grow, bh = y1 - y0 + 1 + 2 * grow;
    const int least = std::min({kSide, width, height});
    const int cw = std::min(width, std::max({int(std::max(bw, bh) * 2.5), least, bw})),
              ch = std::min(height, std::max({int(std::max(bw, bh) * 2.5), least, bh}));
    const int cx = std::clamp((x0 + x1) / 2 - cw / 2, 0, width - cw),
              cy = std::clamp((y0 + y1) / 2 - ch / 2, 0, height - ch);
    std::vector<float> cropMask(size_t(cw) * ch);
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x)
            cropMask[size_t(y) * cw + x] = full[size_t(y + cy) * width + x + cx];
    const auto grown = boxFilter(cropMask, cw, ch, grow, true);
    // Blended in with a soft edge just outside the widened area.
    auto alpha = boxFilter(boxFilter(grown, cw, ch, grow, true), cw, ch, grow, false);
    // The model's mask at 512 × 512: 1 where it paints.
    std::vector<float> maskIn(size_t(kSide) * kSide);
    for (int y = 0; y < kSide; ++y)
        for (int x = 0; x < kSide; ++x) {
            const int sx = std::min(cw - 1, int((x + 0.5) * cw / kSide)),
                      sy = std::min(ch - 1, int((y + 0.5) * ch / kSide));
            maskIn[size_t(y) * kSide + x] = grown[size_t(sy) * cw + sx] > 0.5f ? 1.f : 0.f;
        }

    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "cutlery-ai");
    auto session = openModel(env, model, o.value("cpu") == "1");
    Ort::AllocatorWithDefaultOptions allocator;
    if (session->GetInputCount() != 2 || session->GetOutputCount() < 1)
        return fail("the model needs an image and a mask input");
    std::array<std::string, 2> names{session->GetInputNameAllocated(0, allocator).get(),
                                     session->GetInputNameAllocated(1, allocator).get()};
    // The image comes first unless the names say otherwise.
    const bool maskFirst = names[0].find("mask") != std::string::npos;
    const std::string imageName = names[maskFirst ? 1 : 0], maskName = names[maskFirst ? 0 : 1];
    const std::string outName = session->GetOutputNameAllocated(0, allocator).get();
    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    QProcess decoder;
    decoder.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QStringList args{"-hide_banner", "-nostdin", "-v", "error", "-ss", num(start)};
    if (duration > 0)
        args << "-t" << num(duration);
    args << "-i" << input << "-map" << "0:v:0" << "-an" << "-vf"
         << QString("fps=%1,scale=%2:%3:flags=bicubic,format=rgb24").arg(rate).arg(width).arg(height)
         << "-f" << "rawvideo" << "pipe:1";
    decoder.start(ffmpeg, args);
    if (!decoder.waitForStarted())
        return fail("cannot start FFmpeg");
    QProcess encoder;
    encoder.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    encoder.start(ffmpeg, {"-hide_banner", "-nostdin", "-v", "error", "-y", "-f", "rawvideo",
                           "-pix_fmt", "rgb24", "-s", QString("%1x%2").arg(width).arg(height),
                           "-framerate", rate, "-i", "pipe:0", "-vf", "setsar=1,format=yuv422p10le",
                           "-c:v", "prores_ks", "-profile:v", "2", "-vendor", "apl0", "-f", "mov",
                           output});
    if (!encoder.waitForStarted())
        return fail("cannot start FFmpeg encoder");

    const qint64 total = duration > 0 ? qint64(std::ceil(duration * fps - 1e-6)) : 0;
    std::vector<unsigned char> frame(size_t(width) * height * 3), previousCrop;
    std::vector<float> image(size_t(3) * kSide * kSide), filled, previousFill;
    const std::array<int64_t, 4> imageShape{1, 3, kSide, kSide}, maskShape{1, 1, kSide, kSide};
    qint64 done = 0;
    while (readExactly(decoder, reinterpret_cast<char *>(frame.data()), qint64(frame.size()))) {
        const unsigned char *crop = frame.data();
        // The cut-out at 512 × 512, 0..1, channels apart; the area itself is given as grey.
        for (int c = 0; c < 3; ++c)
            for (int y = 0; y < kSide; ++y)
                for (int x = 0; x < kSide; ++x) {
                    const size_t i = size_t(y) * kSide + x;
                    image[size_t(c) * kSide * kSide + i] =
                        maskIn[i] > 0 ? 0.f
                                      : sample(crop, width, height, cx + (x + 0.5) * cw / kSide - 0.5,
                                               cy + (y + 0.5) * ch / kSide - 0.5, c) /
                                            255.f;
                }
        std::array<Ort::Value, 2> inputs{
            Ort::Value::CreateTensor<float>(memory, image.data(), image.size(), imageShape.data(), 4),
            Ort::Value::CreateTensor<float>(memory, maskIn.data(), maskIn.size(), maskShape.data(), 4)};
        if (maskFirst)
            std::swap(inputs[0], inputs[1]);
        const std::array<const char *, 2> inNames{names[0].c_str(), names[1].c_str()};
        const char *outNames[] = {outName.c_str()};
        auto result = session->Run(Ort::RunOptions{nullptr}, inNames.data(), inputs.data(), 2,
                                   outNames, 1);
        const auto shape = result[0].GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() != 4 || shape[1] != 3)
            return fail("the model gave no RGB picture");
        const int oh = int(shape[2]), ow = int(shape[3]);
        const float *out = result[0].GetTensorData<float>();
        // Exports differ: 0..1 or 0..255.
        float top = 0;
        for (size_t i = 0; i < size_t(3) * oh * ow; ++i)
            top = std::max(top, out[i]);
        const float scale = top > 1.5f ? 1.f : 255.f;
        // Back to the cut-out's size.
        filled.assign(size_t(cw) * ch * 3, 0.f);
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x) {
                if (alpha[size_t(y) * cw + x] <= 0)
                    continue;
                const double fx = std::clamp((x + 0.5) * ow / cw - 0.5, 0., ow - 1.),
                             fy = std::clamp((y + 0.5) * oh / ch - 0.5, 0., oh - 1.);
                const int ix = int(fx), iy = int(fy), jx = std::min(ow - 1, ix + 1),
                          jy = std::min(oh - 1, iy + 1);
                const double ax = fx - ix, ay = fy - iy;
                for (int c = 0; c < 3; ++c) {
                    const float *plane = out + size_t(c) * oh * ow;
                    const double v = (plane[size_t(iy) * ow + ix] * (1 - ax) + plane[size_t(iy) * ow + jx] * ax) * (1 - ay) +
                                     (plane[size_t(jy) * ow + ix] * (1 - ax) + plane[size_t(jy) * ow + jx] * ax) * ay;
                    filled[(size_t(y) * cw + x) * 3 + c] = float(v) * scale;
                }
            }
        // A still camera: the surroundings barely change, so the fill is steadied over time.
        std::vector<unsigned char> cropNow(size_t(cw) * ch * 3);
        for (int y = 0; y < ch; ++y)
            std::copy_n(frame.data() + (size_t(y + cy) * width + cx) * 3, size_t(cw) * 3,
                        cropNow.data() + size_t(y) * cw * 3);
        bool steady = false;
        if (!previousCrop.empty()) {
            double difference = 0;
            qint64 n = 0;
            for (size_t i = 0; i < size_t(cw) * ch; i += 7)
                if (alpha[i] <= 0) {
                    for (int c = 0; c < 3; ++c)
                        difference += std::abs(int(cropNow[i * 3 + c]) - int(previousCrop[i * 3 + c]));
                    n += 3;
                }
            steady = n > 0 && difference / n < 4;
        }
        if (steady)
            for (size_t i = 0; i < filled.size(); ++i)
                filled[i] = 0.5f * filled[i] + 0.5f * previousFill[i];
        previousCrop = cropNow;
        previousFill = filled;
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x) {
                const float a = alpha[size_t(y) * cw + x];
                if (a <= 0)
                    continue;
                unsigned char *px = frame.data() + (size_t(y + cy) * width + x + cx) * 3;
                for (int c = 0; c < 3; ++c)
                    px[c] = static_cast<unsigned char>(std::clamp(
                        std::lround(px[c] * (1 - a) + filled[(size_t(y) * cw + x) * 3 + c] * a), 0L, 255L));
            }
        if (!writeAll(encoder, reinterpret_cast<const char *>(frame.data()), qint64(frame.size())))
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
