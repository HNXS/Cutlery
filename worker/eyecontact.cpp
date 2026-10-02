// Task "eyecontact": makes a presenter who reads from a script look into the camera.
//
// Per frame, with MediaPipe's models (Apache-2.0) converted to ONNX:
//   1. BlazeFace (face_detection_short_range) finds the face and the eye line;
//   2. Face Mesh (face_landmark, 468 points) on the face rotated level gives the eye corners and
//      the head direction;
//   3. Iris Landmark on each eye gives the iris centre and radius and the lid contour.
// The iris should sit at the middle of the eye opening, slightly above the corner line. The
// offset both eyes share, limited to 15 % of the eye width, is undone by warping only the eyes:
// vertically the iris and the upper lid move together (looking down lowers the upper lid) while
// the lower lid stays, fading out toward the brow; horizontally only the inside of the opening
// moves. The correction fades out when an eye closes, the head turns, the face is small, the
// gaze is far off (where any correction looks wrong) or already nearly on target (landmark
// noise). Shifts are smoothed over time with One-Euro filters so the eyes do not shimmer.
// Frames without a face pass through unchanged.
//
// The result is ProRes 422 at the processing size, like the upscale task, and replaces the
// source in the renderer. --model names face_landmark.onnx; the other two models sit beside it.
//
//   cutlery-ai eyecontact --ffmpeg F --model M --input IN --output OUT.mov --source WxH
//                         --rate R [--start S] [--duration D] [--strength 0..1] [--cpu 1]
//                         [--report FILE]
// --report writes one line per frame: "frame face eye1_dx eye1_dy eye2_dx eye2_dy", the applied
// iris shifts as fractions of the eye width (for tests).
#include "face.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace {
using namespace worker::face;
// One-Euro filter (Casiez et al. 2012): smooths jitter while following fast moves.
class OneEuro {
  public:
    double filter(double x, double dt, double minCutoff = 1.0, double beta = 0.05) {
        if (!m_ready) {
            m_x = x;
            m_dx = 0;
            m_ready = true;
            return x;
        }
        auto alpha = [dt](double cutoff) {
            const double tau = 1 / (2 * kPi * cutoff);
            return 1 / (1 + tau / dt);
        };
        const double dx = (x - m_x) / dt;
        m_dx += alpha(1.0) * (dx - m_dx);
        m_x += alpha(minCutoff + beta * std::abs(m_dx)) * (x - m_x);
        return m_x;
    }
    void reset() { m_ready = false; }

  private:
    bool m_ready = false;
    double m_x = 0, m_dx = 0;
};
struct Eye {
    Point inner, outer, iris;
    double radius = 0;
    std::array<Point, 16> lid; // the eye opening's contour
};
// The face in a frame: both eyes and how much the head is turned (0 = facing the camera).
struct Face {
    std::array<Eye, 2> eyes;
    double turn = 0;
};
class EyeContact {
  public:
    EyeContact(Ort::Env &env, const QString &meshPath, bool cpu)
        : m_detector(env, QFileInfo(meshPath).dir().filePath("face_detection_short_range.onnx"), cpu),
          m_mesh(env, meshPath, cpu),
          m_iris(env, QFileInfo(meshPath).dir().filePath("iris_landmark.onnx"), cpu) {}
    std::optional<Face> find(const Image &img) {
        // 1. Face detection on the whole frame, letterboxed to a square; the most confident face.
        const auto faces = m_detector.find(img, {img.width / 2., img.height / 2.},
                                           std::max(img.width, img.height));
        if (faces.empty())
            return std::nullopt;
        const auto &best = faces.front();
        const Point centre = best.centre;
        const double boxSize = std::max(best.width, best.height);
        const Point eyeA = best.keypoints[0], eyeB = best.keypoints[1];
        // 2. Face mesh on the face turned level.
        Crop face{centre, boxSize * 1.5, std::atan2(eyeB.y - eyeA.y, eyeB.x - eyeA.x), false};
        auto input = face.sample(img, 192, 0, 1 / 255.);
        const auto mesh = m_mesh.run(input, 192);
        if (mesh[1].empty() || 1 / (1 + std::exp(-mesh[1][0])) < 0.5)
            return std::nullopt;
        auto landmark = [&](int i) {
            return face.toFrame(mesh[0][size_t(i) * 3], mesh[0][size_t(i) * 3 + 1], 192);
        };
        Face result;
        // Head turn: the nose tip's position between the outer eye corners, along the eye line.
        {
            const Point a = landmark(33), b = landmark(263), nose = landmark(1);
            const Point axis = (b - a) * (1 / std::max(1e-6, (b - a).length()));
            result.turn = ((nose - (a + b) * 0.5).dot(axis)) / std::max(1e-6, (b - a).length());
        }
        // 3. Iris and lids for each eye; the model expects the image's left eye, so the other is
        // mirrored.
        const std::array<std::pair<int, int>, 2> corners{{{33, 133}, {263, 362}}};
        for (int e = 0; e < 2; ++e) {
            const Point outer = landmark(corners[e].first), inner = landmark(corners[e].second);
            Crop crop{(outer + inner) * 0.5, (outer - inner).length() * 2.3, face.angle, e == 1};
            input = crop.sample(img, 64, 0, 1 / 255.);
            const auto out = m_iris.run(input, 64);
            auto &eye = result.eyes[size_t(e)];
            for (int i = 0; i < 16; ++i)
                eye.lid[size_t(i)] = crop.toFrame(out[0][size_t(i) * 3], out[0][size_t(i) * 3 + 1], 64);
            // The corners of the iris model's own lid contour agree with its iris far better
            // than the face mesh's corners, which shift by pixels from frame to frame.
            eye.outer = eye.lid[0];
            eye.inner = eye.lid[8];
            eye.iris = crop.toFrame(out[1][0], out[1][1], 64);
            const Point r1 = crop.toFrame(out[1][3], out[1][4], 64),
                        r3 = crop.toFrame(out[1][9], out[1][10], 64);
            eye.radius = (r1 - r3).length() / 2;
        }
        return result;
    }

  private:
    Detector m_detector;
    Model m_mesh, m_iris;
};
// Moves the iris (and, vertically, the upper lid) of one eye by `shift` pixels in a copy of the
// eye region of `src` into `dst`.
void warpEye(const Image &src, Image &dst, const Eye &eye, Point shift) {
    Point a = eye.outer, b = eye.inner;
    if (a.x > b.x)
        std::swap(a, b);
    const double width = (b - a).length();
    if (width < 4 || shift.length() < 0.05)
        return;
    const Point ax = (b - a) * (1 / width), ay{-ax.y, ax.x}, o = (a + b) * 0.5;
    const double du = shift.dot(ax), dv = shift.dot(ay);
    // Lids in eye coordinates (u along the eye, v down), as lower and upper curves over u.
    std::vector<std::pair<double, double>> lower{{-width / 2, 0}, {width / 2, 0}},
        upper{{-width / 2, 0}, {width / 2, 0}};
    for (const auto &p : eye.lid) {
        const double u = (p - o).dot(ax), v = (p - o).dot(ay);
        (v >= 0 ? lower : upper).push_back({u, v});
    }
    std::sort(lower.begin(), lower.end());
    std::sort(upper.begin(), upper.end());
    auto curve = [](const std::vector<std::pair<double, double>> &c, double u) {
        if (u <= c.front().first)
            return c.front().second;
        for (size_t i = 1; i < c.size(); ++i)
            if (u <= c[i].first) {
                const double t = (u - c[i - 1].first) / std::max(1e-9, c[i].first - c[i - 1].first);
                return c[i - 1].second + t * (c[i].second - c[i - 1].second);
            }
        return c.back().second;
    };
    double lowest = 0;
    for (const auto &p : lower)
        lowest = std::max(lowest, p.second);
    const double iu = (eye.iris - o).dot(ax), iv = (eye.iris - o).dot(ay);
    const int reach = int(std::ceil(width * 1.1));
    const int x0 = std::max(0, int(o.x) - reach), x1 = std::min(src.width, int(o.x) + reach),
              y0 = std::max(0, int(o.y) - reach), y1 = std::min(src.height, int(o.y) + reach);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const Point p = Point{x + 0.5, y + 0.5} - o;
            const double u = p.dot(ax), v = p.dot(ay);
            const double vL = curve(lower, u), vU = curve(upper, u);
            // Vertical weight: 0 at and below the lower lid, rising to the iris centre, 1
            // through the upper lid, fading out above it toward the brow.
            double kv;
            if (v >= vL)
                kv = 0;
            else if (v >= iv)
                kv = smoothstep((vL - v) / std::max(lowest - iv, 1e-3));
            else if (v > vU)
                kv = 1;
            else
                kv = 1 - smoothstep((vU - v) / (0.45 * width));
            kv *= (1 - smoothstep((std::abs(u - iu) - 0.2 * width) / (0.45 * width))) *
                  (1 - smoothstep((std::abs(u) - 0.5 * width) / (0.15 * width)));
            // Horizontal weight: inside the opening only, fading toward lids and corners.
            double kh = 0;
            if (v < vL && v > vU)
                kh = smoothstep(std::min(v - vU, vL - v) / (0.35 * std::max(vL - vU, 1e-3))) *
                     (1 - smoothstep((std::abs(u) - 0.25 * width) / (0.25 * width)));
            if (kv <= 0 && kh <= 0)
                continue;
            const Point from = Point{x + 0.5, y + 0.5} - ax * (du * kh) - ay * (dv * kv);
            auto *px = &dst.rgb[(size_t(y) * dst.width + x) * 3];
            for (int c = 0; c < 3; ++c)
                px[c] = static_cast<unsigned char>(
                    std::lround(std::clamp(src.at(from.x - 0.5, from.y - 0.5, c), 0., 255.)));
        }
}
} // namespace

int worker::eyecontact(const QHash<QString, QString> &o) {
    const QString ffmpeg = o.value("ffmpeg"), model = o.value("model"), input = o.value("input"),
                  output = o.value("output"), rate = o.value("rate"), reportPath = o.value("report");
    const double start = o.value("start", "0").toDouble(),
                 duration = o.value("duration", "-1").toDouble(),
                 strength = std::clamp(o.value("strength", "1").toDouble(), 0., 1.);
    int width = 0, height = 0;
    double fps = 0;
    if (const auto parts = rate.split('/'); !parts.isEmpty() && parts[0].toDouble() > 0)
        fps = parts[0].toDouble() / (parts.size() == 2 ? parts[1].toDouble() : 1);
    if (ffmpeg.isEmpty() || model.isEmpty() || input.isEmpty() || output.isEmpty() ||
        !size(o.value("source"), width, height) || !(fps > 0 && fps <= 240))
        return fail("usage: eyecontact --ffmpeg F --model M --input IN --output OUT --source WxH "
                    "--rate R [--start S] [--duration D] [--strength K]");
    for (const auto *name : {"face_detection_short_range.onnx", "iris_landmark.onnx"})
        if (!QFileInfo(QFileInfo(model).dir().filePath(name)).isFile())
            return fail(QString("%1 is missing beside the face model").arg(name));
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "cutlery-ai");
    EyeContact models(env, model, o.value("cpu") == "1");

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
    QFile report(reportPath);
    if (!reportPath.isEmpty() && !report.open(QIODevice::WriteOnly | QIODevice::Text))
        return fail("cannot write the report");

    const qint64 total = duration > 0 ? qint64(std::ceil(duration * fps - 1e-6)) : 0;
    Image frame{width, height, std::vector<unsigned char>(size_t(width) * height * 3)};
    Image result = frame;
    // Smoothed per eye: iris shift (x, y) and the fade, so corrections ease in and out.
    // Smoothed gaze shift (in eye widths, along and across the eye) and the fade.
    std::array<OneEuro, 3> filters;
    const double dt = 1 / fps;
    qint64 done = 0;
    while (readExactly(decoder, reinterpret_cast<char *>(frame.rgb.data()),
                       qint64(frame.rgb.size()))) {
        result.rgb = frame.rgb;
        const auto face = models.find(frame);
        QStringList line{QString::number(done), face ? "1" : "0"};
        // Both eyes look the same way, but each has its own small asymmetry (convergence on a
        // near camera, anatomy, landmark noise). Correct the gaze they share, weighted by how
        // open each eye is, and keep each eye's own offset.
        struct Measure {
            double width = 0, weight = 0;
            Point axisU, axisV, offset; // offset in eye widths, image axes
        };
        std::array<Measure, 2> eyes;
        double commonU = 0, commonV = 0, weights = 0, fade = 0;
        if (face) {
            for (int e = 0; e < 2; ++e) {
                const auto &eye = face->eyes[size_t(e)];
                auto &m = eyes[size_t(e)];
                Point a = eye.outer, b = eye.inner;
                if (a.x > b.x)
                    std::swap(a, b);
                m.width = (b - a).length();
                if (m.width < 1)
                    continue;
                m.axisU = (b - a) * (1 / m.width);
                m.axisV = {-m.axisU.y, m.axisU.x};
                // Looking into the lens, the iris sits mid-way between the corners and a
                // little above their line.
                const Point target = (a + b) * 0.5 - m.axisV * (0.05 * m.width);
                m.offset = (target - eye.iris) * (1 / m.width);
                double top = 1e9, bottom = -1e9;
                for (const auto &p : eye.lid) {
                    top = std::min(top, (p - a).dot(m.axisV));
                    bottom = std::max(bottom, (p - a).dot(m.axisV));
                }
                const double open = (bottom - top) / m.width;
                // A closing eye or a small one says little about the gaze.
                m.weight = smoothstep((open - 0.12) / 0.08) * smoothstep((m.width - 10) / 8);
                commonU += m.weight * m.offset.dot(m.axisU);
                commonV += m.weight * m.offset.dot(m.axisV);
                weights += m.weight;
                if (qEnvironmentVariableIsSet("CUTLERY_EYE_DEBUG"))
                    std::fprintf(stderr, "eye %d open %.3f width %.1f offset %.3f %.3f\n", e,
                                 open, m.width, m.offset.dot(m.axisU), m.offset.dot(m.axisV));
            }
            if (weights > 0) {
                commonU /= weights;
                commonV /= weights;
                const double offset = std::hypot(commonU, commonV);
                // Fade out for closed eyes, turned heads and far-off gazes; leave gazes that are
                // nearly on target rather than chase landmark noise.
                fade = std::min(1., weights) * (1 - smoothstep((std::abs(face->turn) - 0.12) / 0.1)) *
                       (1 - smoothstep((offset - 0.3) / 0.1)) * smoothstep((offset - 0.02) / 0.04);
                // At most 15 % of the eye width.
                if (offset > 0.15) {
                    commonU *= 0.15 / offset;
                    commonV *= 0.15 / offset;
                }
            }
        }
        if (!face || weights <= 0) {
            for (auto &f : filters)
                f.reset();
            line << "0" << "0" << "0" << "0";
        } else {
            const double u = filters[0].filter(commonU, dt), v = filters[1].filter(commonV, dt),
                         k = filters[2].filter(fade, dt, 2.0, 0) * strength;
            for (int e = 0; e < 2; ++e) {
                const auto &m = eyes[size_t(e)];
                if (m.width < 1) {
                    line << "0" << "0";
                    continue;
                }
                const Point applied = (m.axisU * u + m.axisV * v) * (k * m.width);
                warpEye(frame, result, face->eyes[size_t(e)], applied);
                line << num(applied.x / m.width) << num(applied.y / m.width);
            }
        }
        if (report.isOpen())
            report.write((line.join(' ') + "\n").toUtf8());
        if (!writeAll(encoder, reinterpret_cast<const char *>(result.rgb.data()),
                      qint64(result.rgb.size())))
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
