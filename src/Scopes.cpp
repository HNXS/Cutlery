#include "Scopes.h"
#include <QPainter>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace cutlery {
namespace {
// BT.709 luma and colour differences of 8-bit RGB, in 0..255 and -128..127.
double luma(QRgb p) {
    return 0.2126 * qRed(p) + 0.7152 * qGreen(p) + 0.0722 * qBlue(p);
}
double cb(QRgb p) {
    return -0.1146 * qRed(p) - 0.3854 * qGreen(p) + 0.5 * qBlue(p);
}
double cr(QRgb p) {
    return 0.5 * qRed(p) - 0.4542 * qGreen(p) - 0.0458 * qBlue(p);
}
// Counts to brightness, so a few pixels still show and large areas do not burn out.
double glow(double count, double full) {
    return std::min(1., std::log1p(count) / std::log1p(std::max(1., full)));
}
} // namespace
QImage renderScope(const QImage &picture, const QString &kind) {
    const bool vectors = kind == "vectorscope";
    QImage out(vectors ? QSize(192, 192) : QSize(256, 128), QImage::Format_RGB32);
    out.fill(QColor(10, 13, 17));
    if (kind != "histogram" && kind != "waveform" && !vectors)
        return out;
    // At most about 320 pixels wide, which is plenty for scopes and quick to scan.
    QImage source;
    if (!picture.isNull())
        source = (picture.width() > 320 ? picture.scaledToWidth(320, Qt::FastTransformation) : picture)
                     .convertToFormat(QImage::Format_RGB32);
    const int w = out.width(), h = out.height();
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor grid(255, 255, 255, 28);
    if (kind == "histogram") {
        for (int i = 1; i < 4; ++i)
            painter.fillRect(QRect(i * w / 4, 0, 1, h), grid);
        if (source.isNull())
            return out;
        std::array<std::array<double, 256>, 4> bins{};
        for (int y = 0; y < source.height(); ++y) {
            const auto *line = reinterpret_cast<const QRgb *>(source.constScanLine(y));
            for (int x = 0; x < source.width(); ++x) {
                ++bins[0][qRed(line[x])];
                ++bins[1][qGreen(line[x])];
                ++bins[2][qBlue(line[x])];
                ++bins[3][std::clamp(qRound(luma(line[x])), 0, 255)];
            }
        }
        // Scaled to the tallest bin away from pure black and white, which often spike.
        double top = 1;
        for (const auto &b : bins)
            for (int i = 1; i < 255; ++i)
                top = std::max(top, b[i]);
        const std::array<QColor, 4> colours{QColor(255, 70, 70, 120), QColor(70, 230, 110, 120),
                                            QColor(80, 140, 255, 120), QColor(230, 230, 230, 70)};
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
        painter.setRenderHint(QPainter::Antialiasing, false);
        for (int c = 0; c < 4; ++c)
            for (int i = 0; i < 256; ++i) {
                const double height = std::min(1., bins[c][i] / top) * (h - 4);
                if (height > 0)
                    painter.fillRect(QRectF(i * w / 256., h - height, w / 256., height), colours[c]);
            }
        return out;
    }
    if (kind == "waveform") {
        // Lines at 0, 25, 50, 75 and 100 %.
        for (int i = 0; i <= 4; ++i)
            painter.fillRect(QRect(0, qRound((h - 1) * (1 - i / 4.)), w, 1), grid);
        painter.end();
        if (source.isNull())
            return out;
        std::vector<double> counts(size_t(w) * h, 0.);
        for (int y = 0; y < source.height(); ++y) {
            const auto *line = reinterpret_cast<const QRgb *>(source.constScanLine(y));
            for (int x = 0; x < source.width(); ++x) {
                const int column = x * w / source.width(),
                          row = h - 1 - qRound(luma(line[x]) * (h - 1) / 255.);
                ++counts[size_t(row) * w + column];
            }
        }
        const double full = double(source.height()) * source.width() / w / 6;
        for (int y = 0; y < h; ++y) {
            auto *line = reinterpret_cast<QRgb *>(out.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const double v = glow(counts[size_t(y) * w + x], full);
                if (v > 0)
                    line[x] = qRgb(std::max(qRed(line[x]), int(90 * v)), std::max(qGreen(line[x]), int(255 * v)),
                                   std::max(qBlue(line[x]), int(140 * v)));
            }
        }
        return out;
    }
    // Vectorscope: Cb to the right, Cr up, the outer circle at full saturation.
    const double centre = w / 2., radius = w / 2. - 6;
    auto point = [&](double u, double v) {
        return QPointF(centre + u / 128 * radius, centre - v / 128 * radius);
    };
    painter.setPen(QPen(grid, 1));
    painter.drawEllipse(QPointF(centre, centre), radius, radius);
    painter.drawLine(point(-128, 0), point(128, 0));
    painter.drawLine(point(0, -128), point(0, 128));
    // Skin tones fall near the line from the centre towards yellow-red (about 123°).
    const double skin = 123 * std::numbers::pi / 180;
    painter.setPen(QPen(QColor(255, 200, 150, 70), 1));
    painter.drawLine(point(0, 0), point(128 * std::cos(skin), 128 * std::sin(skin)));
    // Targets for 75 % colour bars.
    for (const auto &[colour, rgb] : {std::pair{QColor("#e5534b"), qRgb(191, 0, 0)},
                                      std::pair{QColor("#e3b341"), qRgb(191, 191, 0)},
                                      std::pair{QColor("#57d18b"), qRgb(0, 191, 0)},
                                      std::pair{QColor("#56d4dd"), qRgb(0, 191, 191)},
                                      std::pair{QColor("#5b8def"), qRgb(0, 0, 191)},
                                      std::pair{QColor("#d36ad6"), qRgb(191, 0, 191)}}) {
        painter.setPen(QPen(colour, 1));
        painter.drawRect(QRectF(point(cb(rgb), cr(rgb)) - QPointF(4, 4), QSizeF(8, 8)));
    }
    painter.end();
    if (source.isNull())
        return out;
    std::vector<double> counts(size_t(w) * h, 0.);
    std::vector<std::array<double, 3>> colour(size_t(w) * h, {0, 0, 0});
    for (int y = 0; y < source.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(source.constScanLine(y));
        for (int x = 0; x < source.width(); ++x) {
            const auto p = point(cb(line[x]), cr(line[x]));
            const int px = std::clamp(int(p.x()), 0, w - 1), py = std::clamp(int(p.y()), 0, h - 1);
            const auto i = size_t(py) * w + px;
            ++counts[i];
            colour[i][0] += qRed(line[x]);
            colour[i][1] += qGreen(line[x]);
            colour[i][2] += qBlue(line[x]);
        }
    }
    const double full = double(source.height()) * source.width() / 400;
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const auto i = size_t(y) * w + x;
            if (counts[i] <= 0)
                continue;
            // The pixels' own colour, brightened so dark colours show too.
            const double v = glow(counts[i], full);
            std::array<double, 3> c{colour[i][0] / counts[i], colour[i][1] / counts[i], colour[i][2] / counts[i]};
            const double peak = std::max({c[0], c[1], c[2], 1.});
            line[x] = qRgb(std::max(qRed(line[x]), int((60 + 195 * c[0] / peak) * v)),
                           std::max(qGreen(line[x]), int((60 + 195 * c[1] / peak) * v)),
                           std::max(qBlue(line[x]), int((60 + 195 * c[2] / peak) * v)));
        }
    }
    return out;
}
} // namespace cutlery
