#pragma once
// Shared pieces of the face tasks (eyecontact, faces): frames, rotated crops, ONNX models and
// MediaPipe's short-range face detector (BlazeFace, Apache-2.0).
#include "common.h"
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace worker::face {
constexpr double kPi = 3.14159265358979323846;
struct Point {
    double x = 0, y = 0;
    Point operator+(Point o) const { return {x + o.x, y + o.y}; }
    Point operator-(Point o) const { return {x - o.x, y - o.y}; }
    Point operator*(double k) const { return {x * k, y * k}; }
    double dot(Point o) const { return x * o.x + y * o.y; }
    double length() const { return std::hypot(x, y); }
};
inline double smoothstep(double t) {
    t = std::clamp(t, 0., 1.);
    return t * t * (3 - 2 * t);
}
// An RGB frame, 8 bits per channel.
struct Image {
    int width = 0, height = 0;
    std::vector<unsigned char> rgb;
    // Bilinear sample of channel c at (x, y), clamped to the frame.
    double at(double x, double y, int c) const {
        x = std::clamp(x, 0., width - 1.001);
        y = std::clamp(y, 0., height - 1.001);
        const int x0 = int(x), y0 = int(y);
        const double fx = x - x0, fy = y - y0;
        auto px = [&](int xx, int yy) { return double(rgb[(size_t(yy) * width + xx) * 3 + c]); };
        return (px(x0, y0) * (1 - fx) + px(x0 + 1, y0) * fx) * (1 - fy) +
               (px(x0, y0 + 1) * (1 - fx) + px(x0 + 1, y0 + 1) * fx) * fy;
    }
};
// A square region of the frame, `size` pixels wide, centred at `centre` and rotated by `angle`,
// sampled to n × n model pixels (optionally mirrored), and the way back to frame coordinates.
struct Crop {
    Point centre;
    double size = 0, angle = 0;
    bool mirror = false;
    Point toFrame(double u, double v, int n) const {
        double a = u / n - 0.5, b = v / n - 0.5;
        if (mirror)
            a = -a;
        const double c = std::cos(angle), s = std::sin(angle);
        return {centre.x + (a * c - b * s) * size, centre.y + (a * s + b * c) * size};
    }
    // NHWC float tensor, values scaled by `scale` after subtracting `offset`.
    std::vector<float> sample(const Image &img, int n, double offset, double scale) const {
        std::vector<float> t(size_t(n) * n * 3);
        for (int v = 0; v < n; ++v)
            for (int u = 0; u < n; ++u) {
                const auto p = toFrame(u + 0.5, v + 0.5, n);
                for (int c = 0; c < 3; ++c)
                    t[(size_t(v) * n + u) * 3 + c] = float((img.at(p.x, p.y, c) - offset) * scale);
            }
        return t;
    }
};
class Model {
  public:
    Model(Ort::Env &env, const QString &path, bool cpu) : m_session(worker::openModel(env, path, cpu)) {
        Ort::AllocatorWithDefaultOptions allocator;
        m_input = m_session->GetInputNameAllocated(0, allocator).get();
        for (size_t i = 0; i < m_session->GetOutputCount(); ++i)
            m_outputs.push_back(m_session->GetOutputNameAllocated(i, allocator).get());
    }
    std::vector<std::vector<float>> run(std::vector<float> &input, int n) {
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<int64_t> dims{1, n, n, 3};
        auto value = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(),
                                                     dims.data(), dims.size());
        const char *in[] = {m_input.c_str()};
        std::vector<const char *> out;
        for (const auto &o : m_outputs)
            out.push_back(o.c_str());
        auto results = m_session->Run(Ort::RunOptions{nullptr}, in, &value, 1, out.data(),
                                      out.size());
        std::vector<std::vector<float>> data;
        for (auto &r : results) {
            const auto count = r.GetTensorTypeAndShapeInfo().GetElementCount();
            const float *p = r.GetTensorData<float>();
            data.emplace_back(p, p + count);
        }
        return data;
    }

  private:
    std::unique_ptr<Ort::Session> m_session;
    std::string m_input;
    std::vector<std::string> m_outputs;
};
// One face found by BlazeFace: centre and size in frame pixels, the six keypoints (eyes, nose,
// mouth, ears) and the score.
struct Detection {
    Point centre;
    double width = 0, height = 0, score = 0;
    std::array<Point, 6> keypoints;
};
class Detector {
  public:
    Detector(Ort::Env &env, const QString &path, bool cpu) : m_model(env, path, cpu) {
        // Short-range anchors: 2 per cell on a 16 × 16 grid, 6 per cell on 8 × 8.
        for (const auto [cells, perCell] : {std::pair{16, 2}, std::pair{8, 6}})
            for (int y = 0; y < cells; ++y)
                for (int x = 0; x < cells; ++x)
                    for (int k = 0; k < perCell; ++k)
                        m_anchors.push_back({(x + 0.5) / cells, (y + 0.5) / cells});
    }
    // Faces in a square region of the frame (letterboxed when it reaches past the edges),
    // after non-maximum suppression.
    std::vector<Detection> find(const Image &img, Point centre, double side, double threshold = 0.5) {
        Crop region{centre, side, 0, false};
        auto input = region.sample(img, 128, 127.5, 1 / 127.5);
        const auto out = m_model.run(input, 128);
        const auto &boxes = out[0], &scores = out[1];
        std::vector<Detection> found;
        for (size_t i = 0; i < m_anchors.size(); ++i) {
            const double s = 1 / (1 + std::exp(-std::clamp(double(scores[i]), -100., 100.)));
            if (s < threshold)
                continue;
            const float *raw = &boxes[i * 16];
            const auto a = m_anchors[i];
            auto at = [&](double u, double v) { return region.toFrame(u * 128, v * 128, 128); };
            Detection d;
            d.score = s;
            d.centre = at(raw[0] / 128 + a.x, raw[1] / 128 + a.y);
            d.width = raw[2] / 128 * side;
            d.height = raw[3] / 128 * side;
            for (int k = 0; k < 6; ++k)
                d.keypoints[size_t(k)] = at(raw[4 + 2 * k] / 128 + a.x, raw[5 + 2 * k] / 128 + a.y);
            found.push_back(d);
        }
        return suppress(std::move(found));
    }
    // Keeps the best of overlapping detections.
    static std::vector<Detection> suppress(std::vector<Detection> found) {
        std::sort(found.begin(), found.end(),
                  [](const Detection &a, const Detection &b) { return a.score > b.score; });
        std::vector<Detection> kept;
        for (const auto &d : found) {
            const bool overlaps = std::any_of(kept.begin(), kept.end(), [&](const Detection &k) {
                const double ix = std::max(0., std::min(d.centre.x + d.width / 2, k.centre.x + k.width / 2) -
                                                   std::max(d.centre.x - d.width / 2, k.centre.x - k.width / 2)),
                             iy = std::max(0., std::min(d.centre.y + d.height / 2, k.centre.y + k.height / 2) -
                                                   std::max(d.centre.y - d.height / 2, k.centre.y - k.height / 2));
                const double inter = ix * iy, uni = d.width * d.height + k.width * k.height - inter;
                return uni > 0 && inter / uni > 0.3;
            });
            if (!overlaps)
                kept.push_back(d);
        }
        return kept;
    }

  private:
    Model m_model;
    std::vector<Point> m_anchors;
};
} // namespace worker::face
