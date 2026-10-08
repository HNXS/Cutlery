#include "RenderGraph.h"
#include <QRegularExpression>
#include <limits>
#include "ExportProfiles.h"
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QHash>
#include <QPainterPath>
#include <QTransform>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace cutlery {
// A file path as a filter option value inside a filter graph: escaped for the option parser,
// then for the graph parser, so any character in a path is safe.
QString filterPath(const QString &path) {
    auto escape = [](const QString &s, const QString &special) {
        QString out;
        for (const auto ch : s) {
            if (special.contains(ch) || ch == '\\' || ch == '\'')
                out += '\\';
            out += ch;
        }
        return out;
    };
    return escape(escape(QDir::fromNativeSeparators(path), ":="), ",;[]");
}
static QString num(double v) {
    return QString::number(v, 'f', 9);
}
static bool animatedGeometry(const Clip &c) {
    return c.keyframes.contains("scale") || c.keyframes.contains("x") ||
           c.keyframes.contains("y") || c.keyframes.contains("rotation");
}
// Corner pin maps for FFmpeg's remap: for each pixel of the w × h result, the source pixel it
// shows (x in the first image, y in the second), or 65535 where the warped picture does not
// reach (left transparent).
static std::pair<QImage, QImage> cornerPinMaps(const QVector<double> &k, int w, int h) {
    QImage xs(w, h, QImage::Format_Grayscale16), ys(w, h, QImage::Format_Grayscale16);
    const QPolygonF to{QPointF(k[0] * w, k[1] * h), QPointF(k[2] * w, k[3] * h),
                       QPointF(k[6] * w, k[7] * h), QPointF(k[4] * w, k[5] * h)};
    const QPolygonF from{QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)};
    QTransform back;
    const bool ok = QTransform::quadToQuad(to, from, back);
    for (int y = 0; y < h; ++y) {
        auto *lx = reinterpret_cast<quint16 *>(xs.scanLine(y));
        auto *ly = reinterpret_cast<quint16 *>(ys.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const auto s = ok ? back.map(QPointF(x + 0.5, y + 0.5)) : QPointF(-1, -1);
            const bool inside = s.x() >= 0 && s.y() >= 0 && s.x() < w && s.y() < h;
            lx[x] = inside ? quint16(s.x()) : 65535;
            ly[x] = inside ? quint16(s.y()) : 65535;
        }
    }
    return {xs, ys};
}
// Where the corners of a w × h picture go (as in Clip::cornerPin): its corner pin, then its 3D
// tilt seen in perspective from a distance of twice the larger side, shrunk to fit its box.
static QVector<double> pinCorners(const Clip &c, int w, int h) {
    QVector<double> k = c.cornerPin.size() == 8 ? c.cornerPin : QVector<double>{0, 0, 1, 0, 0, 1, 1, 1};
    if (c.tiltX == 0 && c.tiltY == 0)
        return k;
    const double ax = c.tiltX * std::numbers::pi / 180, ay = c.tiltY * std::numbers::pi / 180;
    const double f = 2. * std::max(w, h);
    double lo[2] = {1e9, 1e9}, hi[2] = {-1e9, -1e9};
    for (int i = 0; i < 8; i += 2) {
        const double x = (k[i] - 0.5) * w, y = (k[i + 1] - 0.5) * h;
        // Turn about the vertical axis, then lean about the horizontal one; z points away.
        const double x1 = x * std::cos(ay), z1 = x * std::sin(ay);
        const double y2 = y * std::cos(ax) + z1 * std::sin(ax),
                     z2 = -y * std::sin(ax) + z1 * std::cos(ax);
        const double s = f / (f + z2);
        k[i] = x1 * s / w;
        k[i + 1] = y2 * s / h;
        for (int d = 0; d < 2; ++d) {
            lo[d] = std::min(lo[d], k[i + d]);
            hi[d] = std::max(hi[d], k[i + d]);
        }
    }
    // Centred, and no larger than the box.
    const double fit = std::min({1., 1 / std::max(1e-9, hi[0] - lo[0]), 1 / std::max(1e-9, hi[1] - lo[1])});
    for (int i = 0; i < 8; i += 2) {
        k[i] = 0.5 + (k[i] - (lo[0] + hi[0]) / 2) * fit;
        k[i + 1] = 0.5 + (k[i + 1] - (lo[1] + hi[1]) / 2) * fit;
    }
    return k;
}
// Alpha mask of a styled overlay: rounded rectangle or circle, antialiased.
static QImage overlayMask(const Clip &c, int w, int h) {
    QImage mask(w, h, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    // A soft edge: the shape is drawn smaller by half the feather and blurred, so it fades from
    // fully visible to transparent over the feather width, ending at the picture's edge.
    const double feather = c.feather * std::min(w, h);
    const double inset = feather / 2;
    {
        QPainter paint(&mask);
        paint.setRenderHint(QPainter::Antialiasing);
        paint.setPen(Qt::NoPen);
        paint.setBrush(Qt::white);
        const QRectF area = QRectF(0, 0, w, h).adjusted(inset, inset, -inset, -inset);
        if (c.shape == "circle")
            paint.drawEllipse(area);
        else {
            const double r = c.shape == "rounded" ? c.radius * std::min(w, h) : 0;
            paint.drawRoundedRect(area, std::max(0., r - inset), std::max(0., r - inset));
        }
    }
    const int radius = int(std::lround(feather / 6));
    if (radius < 1)
        return mask;
    // Three box blurs of the alpha (close to a Gaussian), with running sums.
    QVector<int> alpha(w * h), tmp(w * h);
    for (int y = 0; y < h; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(mask.constScanLine(y));
        for (int x = 0; x < w; ++x)
            alpha[y * w + x] = qAlpha(line[x]);
    }
    auto pass = [radius](const int *in, int *out, int count, int stride) {
        // Edges count as transparent.
        int sum = 0;
        for (int k = 0; k <= radius && k < count; ++k)
            sum += in[k * stride];
        const int n = 2 * radius + 1;
        for (int i = 0; i < count; ++i) {
            out[i * stride] = sum / n;
            if (i + radius + 1 < count)
                sum += in[(i + radius + 1) * stride];
            if (i - radius >= 0)
                sum -= in[(i - radius) * stride];
        }
    };
    for (int round = 0; round < 3; ++round) {
        for (int y = 0; y < h; ++y)
            pass(alpha.data() + y * w, tmp.data() + y * w, w, 1);
        for (int x = 0; x < w; ++x)
            pass(tmp.data() + x, alpha.data() + x, h, w);
    }
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(mask.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const int a = alpha[y * w + x];
            line[x] = qRgba(a, a, a, a);
        }
    }
    return mask;
}
struct Decoration {
    QImage image;
    QPoint offset; // where the picture goes inside the image
};
// Border ring and soft drop shadow around a styled overlay, drawn once per render.
static Decoration overlayDecoration(const Clip &c, int w, int h, double scaleHeight) {
    const int b = int(std::lround(c.border * scaleHeight));
    const int s = c.shadow > 0 ? std::max(4, int(std::lround(0.04 * scaleHeight))) : 0;
    const int dw = w + 2 * (b + s), dh = h + 2 * (b + s);
    QImage image(dw, dh, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const QRectF ring(s, s, w + 2 * b, h + 2 * b), picture(s + b, s + b, w, h);
    auto shapePath = [&](const QRectF &rect, double extra) {
        QPainterPath path;
        if (c.shape == "circle")
            path.addEllipse(rect);
        else if (c.shape == "rounded") {
            const double r = c.radius * std::min(w, h) + extra;
            path.addRoundedRect(rect, r, r);
        } else
            path.addRect(rect);
        return path;
    };
    if (s > 0) {
        // Shadow: the shape in black, offset down, box-blurred three times on alpha.
        QImage shadow(dw, dh, QImage::Format_ARGB32);
        shadow.fill(Qt::transparent);
        QPainter paint(&shadow);
        paint.setRenderHint(QPainter::Antialiasing);
        // Only outside the picture, so keyed (transparent) areas show the background cleanly.
        paint.fillPath(shapePath(ring.translated(0, s * 0.35), b).subtracted(shapePath(picture, 0)),
                       QColor(0, 0, 0));
        paint.end();
        const int radius = std::max(1, s / 3);
        QVector<int> alpha(dw * dh), tmp(dw * dh);
        for (int y = 0; y < dh; ++y)
            for (int x = 0; x < dw; ++x)
                alpha[y * dw + x] = qAlpha(shadow.pixel(x, y));
        for (int pass = 0; pass < 3; ++pass) {
            for (int y = 0; y < dh; ++y)
                for (int x = 0; x < dw; ++x) {
                    int sum = 0, n = 0;
                    for (int k = std::max(0, x - radius); k <= std::min(dw - 1, x + radius); ++k, ++n)
                        sum += alpha[y * dw + k];
                    tmp[y * dw + x] = sum / n;
                }
            for (int y = 0; y < dh; ++y)
                for (int x = 0; x < dw; ++x) {
                    int sum = 0, n = 0;
                    for (int k = std::max(0, y - radius); k <= std::min(dh - 1, y + radius); ++k, ++n)
                        sum += tmp[k * dw + x];
                    alpha[y * dw + x] = sum / n;
                }
        }
        for (int y = 0; y < dh; ++y)
            for (int x = 0; x < dw; ++x)
                image.setPixel(x, y, qPremultiply(qRgba(0, 0, 0,
                                                        int(alpha[y * dw + x] * 0.6 * c.shadow))));
    }
    if (b > 0) {
        QPainter paint(&image);
        paint.setRenderHint(QPainter::Antialiasing);
        paint.fillPath(shapePath(ring, b).subtracted(shapePath(picture, 0)),
                       QColor(c.borderColor));
    }
    return {image, QPoint(s + b, s + b)};
}
// FFmpeg expression for a property over clip-local frame `frame`, matching Clip::valueAt.
static QString curve(const Clip &c, const QString &property, const QString &frame) {
    const auto k = c.keyframes.value(property);
    if (k.isEmpty())
        return num(c.staticValue(property));
    QString expr = num(k.last().value);
    for (auto i = k.size() - 2; i >= 0; --i) {
        const auto u = QString("((%1-%2)/%3)").arg(frame).arg(k[i].frame).arg(
            k[i + 1].frame - k[i].frame);
        // Matches eased() in Project.cpp.
        const auto easing = k[i].easing();
        const auto eased = easing == "smooth" ? QString("(%1*%1*(3-2*%1))").arg(u)
                           : easing == "in"   ? QString("(%1*%1)").arg(u)
                           : easing == "out"  ? QString("(1-(1-%1)*(1-%1))").arg(u)
                           : easing == "hold" ? QString("0")
                                              : u;
        expr = QString("if(lt(%1,%2),%3+(%4)*%5,%6)")
                   .arg(frame)
                   .arg(k[i + 1].frame)
                   .arg(num(k[i].value), num(k[i + 1].value - k[i].value), eased, expr);
    }
    return QString("if(lt(%1,%2),%3,%4)").arg(frame).arg(k.first().frame).arg(
        num(k.first().value), expr);
}
// The shadow offset and outline of a text, from the clip's style at a font pixel size.
static void paintStyledPath(QPainter &paint, const Clip &c, const QPainterPath &path, double px,
                            const QBrush &fill) {
    paint.setPen(Qt::NoPen);
    if (c.textGlow > 0) {
        // A soft halo: layers of wide, faint strokes, the narrower ones adding up near the
        // letters.
        QColor colour(c.textGlowColor);
        const double reach = c.textGlow * 0.45 * px;
        const int layers = 8;
        colour.setAlpha(qRound(255 * std::min(1., 0.35 + 0.5 * c.textGlow) / layers * 2.2));
        for (int i = layers; i >= 1; --i) {
            QPen pen(colour, 2 * reach * i / layers);
            pen.setJoinStyle(Qt::RoundJoin);
            pen.setCapStyle(Qt::RoundCap);
            paint.strokePath(path, pen);
        }
        paint.fillPath(path, colour);
    }
    if (c.textShadow > 0) {
        const double d = std::max(1., px / 24);
        paint.fillPath(path.translated(d * 0.7, d), QColor(0, 0, 0, qRound(210 * c.textShadow)));
    }
    if (c.outline > 0) {
        QPen pen(QColor(c.outlineColor), 2 * c.outline * px);
        pen.setJoinStyle(Qt::RoundJoin);
        paint.strokePath(path, pen);
    }
    paint.fillPath(path, fill);
}
// A text font with the clip's weight, slant and letter spacing at a pixel size.
static QFont textFont(const Clip &c, int pixelSize) {
    QFont font(c.fontFamily);
    font.setPixelSize(std::max(8, pixelSize));
    font.setBold(c.bold);
    font.setItalic(c.italic);
    if (c.letterSpacing != 0)
        font.setLetterSpacing(QFont::AbsoluteSpacing, c.letterSpacing * font.pixelSize());
    return font;
}
// Draws the clip's text in `area` as its style describes: wrapped at spaces, aligned, centred
// vertically, with line spacing, a rounded box behind each line, a shadow, an outline and a
// colour gradient. `visible` limits the drawing to that many characters (spaces not counted),
// for titles that build up; the layout stays that of the whole text. A `time` (clip-local
// seconds) draws the letters of a "rise", "pop", "fly", "drop", "spin" or "fade" animation at
// that moment.
// Title words with their highlight: words between asterisks (*like this*) are marked; the
// asterisks themselves are not shown.
struct MarkedWord {
    QString text;
    bool marked = false;
};
static QVector<QVector<MarkedWord>> markedParagraphs(const QString &text) {
    QVector<QVector<MarkedWord>> paragraphs;
    for (const auto &paragraph : text.split('\n')) {
        QVector<MarkedWord> words;
        bool inside = false;
        for (auto token : paragraph.split(' ', Qt::SkipEmptyParts)) {
            const bool open = token.startsWith('*'), close = token.size() > 1 && token.endsWith('*');
            while (token.startsWith('*'))
                token.remove(0, 1);
            while (token.endsWith('*'))
                token.chop(1);
            inside = inside || open;
            if (!token.isEmpty())
                words.push_back({token, inside});
            if (close || (open && token.isEmpty()))
                inside = false;
        }
        paragraphs.push_back(words);
    }
    return paragraphs;
}
// The title text as shown: the highlight asterisks removed.
QString shownTitleText(const QString &text) {
    QStringList paragraphs;
    for (const auto &words : markedParagraphs(text)) {
        QStringList plain;
        for (const auto &w : words)
            plain << w.text;
        paragraphs << plain.join(' ');
    }
    return paragraphs.join('\n');
}
static void paintText(QPainter &paint, const Clip &c, const QFont &font, const QRect &area,
                      int visible = -1, double time = -1) {
    const QFontMetricsF m(font);
    QStringList lines;
    QVector<QVector<MarkedWord>> lineWords;
    for (const auto &paragraph : markedParagraphs(c.text)) {
        QString line;
        QVector<MarkedWord> words;
        for (const auto &word : paragraph) {
            const auto candidate = line.isEmpty() ? word.text : line + ' ' + word.text;
            if (!line.isEmpty() && m.horizontalAdvance(candidate) > area.width()) {
                lines << line;
                lineWords << words;
                line = word.text;
                words = {word};
            } else {
                line = candidate;
                words << word;
            }
        }
        lines << line;
        lineWords << words;
    }
    // Whether the character at `i` of line `l` is in a marked word.
    auto markedAt = [&](int l, int i) {
        int at = 0;
        for (const auto &w : lineWords[l]) {
            if (i < at + w.text.size())
                return i >= at && w.marked;
            at += int(w.text.size()) + 1;
        }
        return false;
    };
    const double px = font.pixelSize(), step = m.height() * c.lineSpacing,
                 pad = 0.25 * px;
    const double top = area.top() + (area.height() - (step * (lines.size() - 1) + m.height())) / 2;
    double y = top;
    QPainterPath path, plain, marked;
    QVector<QRectF> boxes;
    // Letters one by one for the animations: the outline of each and its place in the text.
    QVector<QPainterPath> letters;
    QVector<bool> letterMarked;
    for (int l = 0; l < lines.size(); ++l) {
        const auto &line = lines[l];
        const double w = m.horizontalAdvance(line);
        const double x = c.align == "left"    ? area.left()
                         : c.align == "right" ? area.right() + 1 - w
                                              : area.left() + (area.width() - w) / 2;
        auto shown = line;
        if (visible >= 0) {
            int end = 0;
            for (int count = 0; end < line.size() && count < visible; ++end)
                count += !line[end].isSpace();
            visible -= int(std::count_if(line.begin(), line.begin() + end,
                                         [](QChar ch) { return !ch.isSpace(); }));
            shown = line.left(end);
        }
        if (!shown.isEmpty()) {
            path.addText(QPointF(x, y + m.ascent()), font, shown);
            // The same text split into unmarked and marked runs, for their fills.
            for (int i = 0; i < shown.size();) {
                int e = i;
                while (e < shown.size() && markedAt(l, e) == markedAt(l, i))
                    ++e;
                (markedAt(l, i) ? marked : plain)
                    .addText(QPointF(x + m.horizontalAdvance(shown.left(i)), y + m.ascent()), font,
                             shown.mid(i, e - i));
                i = e;
            }
            boxes << QRectF(x - pad, y - pad * 0.3, m.horizontalAdvance(shown) + 2 * pad,
                            m.height() + pad * 0.6);
            if (time >= 0)
                for (int i = 0; i < shown.size(); ++i)
                    if (!shown[i].isSpace()) {
                        QPainterPath letter;
                        letter.addText(QPointF(x + m.horizontalAdvance(shown.left(i)), y + m.ascent()),
                                       font, QString(shown[i]));
                        letters << letter;
                        letterMarked << markedAt(l, i);
                    }
        }
        y += step;
    }
    // The fill: one colour, or a gradient over the height of the whole text.
    QBrush fill{QColor(c.textColor)};
    if (!c.gradientColor.isEmpty()) {
        QLinearGradient gradient(0, top, 0, y - step + m.height());
        gradient.setColorAt(0, QColor(c.textColor));
        gradient.setColorAt(1, QColor(c.gradientColor));
        fill = QBrush(gradient);
    }
    // Each letter moves in over `d` seconds; the starts spread over the animation time.
    const double total = c.textAnimationTime, d = std::min(0.6, total / 2);
    auto progress = [&](int i) {
        const double start = letters.size() > 1 ? (total - d) * i / (letters.size() - 1) : 0;
        return std::clamp((time - start) / d, 0., 1.);
    };
    if (c.background > 0) {
        QColor box(c.backgroundColor);
        box.setAlphaF(c.background);
        paint.setPen(Qt::NoPen);
        paint.setBrush(box);
        // Animated text: the boxes fade in with the first letter.
        paint.setOpacity(time >= 0 && !letters.isEmpty() ? progress(0) : 1);
        for (const auto &b : boxes)
            paint.drawRoundedRect(b, pad * 0.6, pad * 0.6);
        paint.setOpacity(1);
    }
    const QBrush highlight{QColor(c.highlightColor)};
    if (time < 0 || letters.isEmpty()) {
        if (marked.isEmpty()) {
            paintStyledPath(paint, c, path, px, fill);
            return;
        }
        // Glow, shadow and outline for the whole text, then each run's own fill.
        paintStyledPath(paint, c, path, px, Qt::NoBrush);
        paint.fillPath(plain, fill);
        paint.fillPath(marked, highlight);
        return;
    }
    for (int i = 0; i < letters.size(); ++i) {
        const double p = progress(i);
        if (p <= 0)
            continue;
        const double ease = 1 - std::pow(1 - p, 3);
        const auto centre = letters[i].boundingRect().center();
        QTransform t;
        double opacity = ease;
        if (c.textAnimation == "rise") {
            t.translate(0, (1 - ease) * 0.8 * px);
        } else if (c.textAnimation == "fly") {
            t.translate((1 - ease) * area.width() * 0.5, 0);
        } else if (c.textAnimation == "drop") {
            // Falls from above and bounces twice before it rests.
            const double b = p < 1 / 2.75     ? 7.5625 * p * p
                             : p < 2 / 2.75   ? 7.5625 * (p - 1.5 / 2.75) * (p - 1.5 / 2.75) + 0.75
                             : p < 2.5 / 2.75 ? 7.5625 * (p - 2.25 / 2.75) * (p - 2.25 / 2.75) + 0.9375
                                              : 7.5625 * (p - 2.625 / 2.75) * (p - 2.625 / 2.75) + 0.984375;
            t.translate(0, -(1 - b) * 1.5 * px);
            opacity = std::min(1., p * 4);
        } else if (c.textAnimation == "spin") {
            // Turns half a round about its centre while it grows to size.
            t.translate(centre.x(), centre.y());
            t.rotate(-180 * (1 - ease));
            t.scale(std::max(0.01, ease), std::max(0.01, ease));
            t.translate(-centre.x(), -centre.y());
        } else if (c.textAnimation == "fade") {
            // Only fades in, in place.
        } else { // pop: grows from nothing, overshoots a little and settles
            const double k = 1.7, q = p - 1, grow = 1 + (k + 1) * q * q * q + k * q * q;
            t.translate(centre.x(), centre.y());
            t.scale(std::max(0.01, grow), std::max(0.01, grow));
            t.translate(-centre.x(), -centre.y());
            opacity = std::min(1., p * 3);
        }
        paint.setOpacity(opacity);
        paintStyledPath(paint, c, t.map(letters[i]), px, letterMarked[i] ? highlight : fill);
    }
    paint.setOpacity(1);
}
// A graphic clip's shape filling `box` on a canvas `height` pixels high.
static void paintGraphic(QPainter &paint, const Clip &c, const QRectF &box, int height) {
    QPainterPath path;
    const double w = box.width(), h = box.height();
    if (c.graphic == "ellipse" || c.graphic == "badge")
        path.addEllipse(box);
    else if (c.graphic == "rectangle")
        path.addRoundedRect(box, std::min(w, h) * 0.08, std::min(w, h) * 0.08);
    else if (c.graphic == "line")
        path.addRect(box);
    else if (c.graphic == "arrow") {
        // A shaft and a head pointing right; the head is as long as the arrow is thick.
        const double head = std::min(h * 1.1, w * 0.5), shaft = h * 0.36;
        const double mid = box.center().y(), tip = box.right(), neck = tip - head;
        path.moveTo(box.left(), mid - shaft / 2);
        path.lineTo(neck, mid - shaft / 2);
        path.lineTo(neck, box.top());
        path.lineTo(tip, mid);
        path.lineTo(neck, box.bottom());
        path.lineTo(neck, mid + shaft / 2);
        path.lineTo(box.left(), mid + shaft / 2);
        path.closeSubpath();
    } else if (c.graphic == "bubble") {
        // A rounded body with a tail at the lower left.
        const QRectF body(box.left(), box.top(), w, h * 0.8);
        const double r = std::min(body.width(), body.height()) * 0.25;
        path.addRoundedRect(body, r, r);
        QPainterPath tail;
        tail.moveTo(body.left() + w * 0.18, body.bottom() - 1);
        tail.lineTo(body.left() + w * 0.12, box.bottom());
        tail.lineTo(body.left() + w * 0.34, body.bottom() - 1);
        tail.closeSubpath();
        path = path.united(tail);
    }
    // Icons are drawn in the largest centred square of the box.
    const double side = std::min(w, h);
    const QRectF sq(box.center().x() - side / 2, box.center().y() - side / 2, side, side);
    auto at = [&](double x, double y) { return QPointF(sq.left() + x * side, sq.top() + y * side); };
    auto stroked = [&](const QPainterPath &line, double width) {
        QPainterPathStroker stroker;
        stroker.setWidth(width * side);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        return stroker.createStroke(line).simplified();
    };
    if (c.graphic == "check") {
        QPainterPath line(at(0.14, 0.52));
        line.lineTo(at(0.4, 0.78));
        line.lineTo(at(0.86, 0.24));
        path = stroked(line, 0.16);
    } else if (c.graphic == "cross") {
        QPainterPath line(at(0.18, 0.18));
        line.lineTo(at(0.82, 0.82));
        line.moveTo(at(0.82, 0.18));
        line.lineTo(at(0.18, 0.82));
        path = stroked(line, 0.16);
    } else if (c.graphic == "star") {
        QPolygonF star;
        for (int i = 0; i < 10; ++i) {
            const double a = -std::numbers::pi / 2 + i * std::numbers::pi / 5, r = i % 2 ? 0.2 : 0.48;
            star << at(0.5 + r * std::cos(a), 0.53 + r * std::sin(a));
        }
        path.addPolygon(star);
        path.closeSubpath();
    } else if (c.graphic == "heart") {
        path.moveTo(at(0.5, 0.88));
        path.cubicTo(at(0.1, 0.62), at(0.0, 0.32), at(0.25, 0.17));
        path.cubicTo(at(0.4, 0.09), at(0.5, 0.22), at(0.5, 0.3));
        path.cubicTo(at(0.5, 0.22), at(0.6, 0.09), at(0.75, 0.17));
        path.cubicTo(at(1.0, 0.32), at(0.9, 0.62), at(0.5, 0.88));
        path.closeSubpath();
    } else if (c.graphic == "warning") {
        // A triangle with an exclamation mark cut out.
        QPainterPath triangle(at(0.5, 0.06));
        triangle.lineTo(at(0.96, 0.9));
        triangle.lineTo(at(0.04, 0.9));
        triangle.closeSubpath();
        QPainterPath mark;
        mark.addRoundedRect(QRectF(at(0.44, 0.32), at(0.56, 0.64)), 0.04 * side, 0.04 * side);
        mark.addEllipse(QRectF(at(0.43, 0.7), at(0.57, 0.84)));
        path = triangle.subtracted(mark);
    } else if (c.graphic == "info") {
        QPainterPath disc;
        disc.addEllipse(QRectF(at(0.04, 0.04), at(0.96, 0.96)));
        QPainterPath mark;
        mark.addEllipse(QRectF(at(0.43, 0.2), at(0.57, 0.34)));
        mark.addRoundedRect(QRectF(at(0.44, 0.4), at(0.56, 0.8)), 0.04 * side, 0.04 * side);
        path = disc.subtracted(mark);
    } else if (c.graphic == "cursor" || c.graphic == "click") {
        // A mouse pointer; "click" adds short rays around its tip.
        const double s0 = c.graphic == "click" ? 0.25 : 0.1;
        auto p = [&](double x, double y) { return at(s0 + x * (1 - s0) * 0.95, s0 + y * (1 - s0) * 0.95); };
        QPolygonF pointer{p(0, 0),    p(0, 0.82),  p(0.2, 0.64), p(0.34, 0.95),
                          p(0.46, 0.9), p(0.32, 0.6), p(0.58, 0.6)};
        path.addPolygon(pointer);
        path.closeSubpath();
        if (c.graphic == "click") {
            QPainterPath rays;
            for (const double a : {-150., -100., 160.}) {
                const double r = a * std::numbers::pi / 180;
                rays.moveTo(at(0.22 + 0.08 * std::cos(r), 0.22 + 0.08 * std::sin(r)));
                rays.lineTo(at(0.22 + 0.2 * std::cos(r), 0.22 + 0.2 * std::sin(r)));
            }
            path = path.united(stroked(rays, 0.05));
        }
    } else if (c.graphic == "lightbulb") {
        QPainterPath bulb;
        bulb.addEllipse(QRectF(at(0.2, 0.04), at(0.8, 0.64)));
        QPainterPath neck;
        neck.addRect(QRectF(at(0.36, 0.5), at(0.64, 0.74)));
        QPainterPath base;
        base.addRoundedRect(QRectF(at(0.34, 0.78), at(0.66, 0.96)), 0.05 * side, 0.05 * side);
        path = bulb.united(neck).united(base);
    } else if (c.graphic == "play") {
        // A disc with a play triangle cut out of it.
        QPainterPath disc, triangle;
        disc.addEllipse(QRectF(at(0.04, 0.04), at(0.96, 0.96)));
        triangle.moveTo(at(0.4, 0.28));
        triangle.lineTo(at(0.74, 0.5));
        triangle.lineTo(at(0.4, 0.72));
        triangle.closeSubpath();
        path = disc.subtracted(triangle);
    } else if (c.graphic == "bell") {
        QPainterPath dome, body, brim, clapper, knob;
        dome.addEllipse(QRectF(at(0.24, 0.12), at(0.76, 0.6)));
        body.addRect(QRectF(at(0.24, 0.36), at(0.76, 0.72)));
        brim.addRoundedRect(QRectF(at(0.1, 0.68), at(0.9, 0.8)), 0.04 * side, 0.04 * side);
        clapper.addEllipse(QRectF(at(0.41, 0.8), at(0.59, 0.95)));
        knob.addEllipse(QRectF(at(0.44, 0.05), at(0.56, 0.17)));
        path = dome.united(body).united(brim).united(clapper).united(knob);
    } else if (c.graphic == "pin") {
        // A map pin: a round head running to a point, with a hole.
        QPainterPath head, point, hole;
        head.addEllipse(QRectF(at(0.2, 0.04), at(0.8, 0.64)));
        point.moveTo(at(0.25, 0.46));
        point.lineTo(at(0.75, 0.46));
        point.lineTo(at(0.5, 0.97));
        point.closeSubpath();
        hole.addEllipse(QRectF(at(0.38, 0.22), at(0.62, 0.46)));
        path = head.united(point).subtracted(hole);
    } else if (c.graphic == "clock") {
        // A clock face with its hands cut out, at ten past twelve.
        QPainterPath face, hands;
        face.addEllipse(QRectF(at(0.04, 0.04), at(0.96, 0.96)));
        hands.addRoundedRect(QRectF(at(0.455, 0.16), at(0.545, 0.545)), 0.03 * side, 0.03 * side);
        hands.addRoundedRect(QRectF(at(0.455, 0.455), at(0.76, 0.545)), 0.03 * side, 0.03 * side);
        path = face.subtracted(hands.simplified());
    }
    paint.setPen(Qt::NoPen);
    paint.fillPath(path, QColor(c.fillColor));
    if (c.stroke > 0) {
        QPen pen(QColor(c.strokeColor), c.stroke * height);
        pen.setJoinStyle(Qt::RoundJoin);
        paint.strokePath(path, pen);
    }
}
// Timed captions ("karaoke", "word"): one band-high variant per word, stacked vertically, so a
// single looped image serves the whole caption and a per-frame crop picks the spoken word.
struct CaptionSprite {
    QImage image;
    int band = 0, variants = 0;
};
static CaptionSprite captionSprite(const Clip &c, int width, int height, int projectHeight) {
    const auto words = captionWords(c.text);
    const bool single = c.captionStyle == "word";
    const auto font =
        textFont(c, qRound(c.fontSize * (single ? 1.5 : 1.) * double(height) / projectHeight));
    const QFontMetrics metrics(font);
    const int maxWidth = width * 13 / 15, lineHeight = metrics.height(),
              space = metrics.horizontalAdvance(' ');
    // Lines of word indexes, wrapped like the plain title.
    QVector<QVector<int>> lines{{}};
    int used = 0;
    for (int i = 0; i < words.size(); ++i) {
        const int w = metrics.horizontalAdvance(words[i]);
        if (!lines.last().isEmpty() && used + space + w > maxWidth) {
            lines.push_back({});
            used = 0;
        }
        used += (lines.last().isEmpty() ? 0 : space) + w;
        lines.last() << i;
    }
    CaptionSprite s;
    s.variants = int(words.size());
    s.band = (single ? 1 : int(lines.size())) * lineHeight + lineHeight / 2;
    if (s.variants == 0 || qint64(s.band) * s.variants > 32000)
        return {};
    s.image = QImage(width, s.band * s.variants, QImage::Format_ARGB32_Premultiplied);
    s.image.fill(Qt::transparent);
    QPainter paint(&s.image);
    paint.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    paint.setFont(font);
    auto draw = [&](const QString &text, int x, int baseline, const QColor &color) {
        QPainterPath path;
        path.addText(QPointF(x, baseline), font, text);
        paintStyledPath(paint, c, path, font.pixelSize(), color);
    };
    for (int v = 0; v < s.variants; ++v) {
        const int top = v * s.band + lineHeight / 4;
        if (single) {
            const int w = metrics.horizontalAdvance(words[v]);
            draw(words[v], (width - w) / 2, top + metrics.ascent(), QColor(c.highlightColor));
            continue;
        }
        for (int l = 0; l < lines.size(); ++l) {
            int lineWidth = 0;
            for (int i : lines[l])
                lineWidth += metrics.horizontalAdvance(words[i]) + (i == lines[l].first() ? 0 : space);
            int x = (width - lineWidth) / 2;
            for (int i : lines[l]) {
                draw(words[i], x, top + l * lineHeight + metrics.ascent(),
                     QColor(i == v ? c.highlightColor : c.textColor));
                x += metrics.horizontalAdvance(words[i]) + space;
            }
        }
    }
    return s;
}
TitlePlate titlePlate(const Clip &c, int width, int height, int projectHeight) {
    if (c.titleStyle.isEmpty() || !c.assetId.isEmpty() || !c.effect.isEmpty())
        return {};
    const auto lines = c.text.split('\n');
    const QString name = lines.value(0).trimmed();
    QStringList rest;
    for (int i = 1; i < lines.size(); ++i)
        if (!lines[i].trimmed().isEmpty())
            rest << lines[i].trimmed();
    // Centred styles: the title card, the quote and the banner across the bottom; the others
    // are lower thirds, on the left or (lowerThirdRight) on the right.
    const bool quote = c.titleStyle == "quote", banner = c.titleStyle == "banner",
               card = c.titleStyle == "titleCard" || quote || banner,
               right = c.titleStyle == "lowerThirdRight";
    const double unit = double(height) / projectHeight * c.scale;
    QFont nameFont(c.fontFamily), restFont(c.fontFamily);
    nameFont.setPixelSize(std::max(8, qRound(c.fontSize * (banner ? 0.75 : card ? (quote ? 0.9 : 1.2) : 0.75) * unit)));
    nameFont.setBold(!quote);
    nameFont.setItalic(quote);
    restFont.setPixelSize(std::max(8, qRound(c.fontSize * (card ? 0.6 : 0.45) * unit)));
    const QFontMetrics nm(nameFont), rm(restFont);
    const int pad = std::max(4, nm.height() / 3), accent = std::max(3, nm.height() / 9);
    const int maxWidth = int(width * (banner ? 0.9 : card ? 0.8 : 0.6));
    auto elide = [&](const QFontMetrics &m, const QString &t) {
        return m.elidedText(t, Qt::ElideRight, maxWidth);
    };
    int textWidth = nm.horizontalAdvance(elide(nm, name));
    for (const auto &r : rest)
        textWidth = std::max(textWidth, rm.horizontalAdvance(elide(rm, r)));
    const int textHeight = nm.height() + int(rest.size()) * rm.height();
    const bool plate = c.titleStyle != "lowerThirdLine" && !quote;
    // A quote has a large opening quotation mark above its text.
    QFont markFont(c.fontFamily);
    markFont.setPixelSize(std::max(8, nm.height() * 2));
    markFont.setBold(true);
    const int markHeight = quote ? QFontMetrics(markFont).ascent() * 2 / 3 : 0;
    const int left = card ? pad * 2 : (right ? pad : accent + pad);
    int w = left + textWidth + (right ? accent + pad : pad * 2);
    if (banner)
        w = width - 4; // the band spans the picture
    const int h = textHeight + pad * 2 + (card && !banner ? accent + pad / 2 : 0) + markHeight;
    TitlePlate t;
    t.image = QImage(w + 4, h + 4, QImage::Format_ARGB32_Premultiplied);
    t.image.fill(Qt::transparent);
    QPainter paint(&t.image);
    paint.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    if (plate) {
        paint.setPen(Qt::NoPen);
        paint.setBrush(QColor(16, 20, 26, 215));
        const double round = banner ? 0 : pad / 2.0; // the band runs to the picture's edges
        paint.drawRoundedRect(QRectF(0, 0, w, h), round, round);
    }
    paint.setPen(Qt::NoPen);
    paint.setBrush(QColor(c.accentColor));
    if (banner) // a strip along the top of the band
        paint.drawRect(QRectF(0, 0, w, accent));
    else if (quote) { // the quotation mark
        paint.setFont(markFont);
        paint.setPen(QColor(c.accentColor));
        paint.drawText(QRectF(0, 0, w, markHeight * 1.5), Qt::AlignHCenter | Qt::AlignTop,
                       QString::fromUtf8("\u201C"));
        paint.setPen(Qt::NoPen);
    } else if (card) // underline below the headline
        paint.drawRect(QRectF((w - textWidth) / 2.0, pad + nm.height() + pad / 4.0, textWidth, accent));
    else if (right) // bar along the right edge
        paint.drawRect(QRectF(w - accent, plate ? 0 : pad / 2.0, accent, plate ? h : h - pad));
    else // bar along the left edge
        paint.drawRect(QRectF(0, plate ? 0 : pad / 2.0, accent, plate ? h : h - pad));
    auto text = [&](const QFont &font, const QFontMetrics &m, const QString &s, int top,
                    const QColor &color) {
        const auto shown = elide(m, s);
        const int x = card    ? (w - m.horizontalAdvance(shown)) / 2
                      : right ? w - accent - pad - m.horizontalAdvance(shown)
                              : left;
        paint.setFont(font);
        if (!plate) { // readable on any picture without a plate
            paint.setPen(QColor(0, 0, 0, 200));
            paint.drawText(x + 2, top + m.ascent() + 2, shown);
        }
        paint.setPen(color);
        paint.drawText(x, top + m.ascent(), shown);
    };
    int top = pad + markHeight + (banner ? accent : 0);
    text(nameFont, nm, name, top, QColor(c.textColor));
    top += nm.height() + (card && !banner ? accent + pad / 2 : 0);
    for (const auto &r : rest) {
        QColor sub(c.textColor);
        sub.setAlpha(200);
        text(restFont, rm, r, top, sub);
        top += rm.height();
    }
    paint.end();
    // Lower thirds sit in the lower left, inside the title-safe area; cards in the centre.
    const int iw = t.image.width(), ih = t.image.height();
    t.position = banner  ? QPoint(0, qRound(height * 0.94) - ih)
                 : card  ? QPoint((width - iw) / 2, (height - ih) / 2)
                 : right ? QPoint(qRound(width * 0.94) - iw, qRound(height * 0.86) - ih)
                         : QPoint(qRound(width * 0.06), qRound(height * 0.86) - ih);
    return t;
}
RenderPlan compileRender(const Project &p, const QString &work, int width, int height,
                         const RenderOptions &o) {
    p.validate();
    if (p.clips.empty())
        throw std::runtime_error("The timeline is empty");
    if (width < 64 || height < 64 || width % 2 || height % 2)
        throw std::runtime_error("Invalid render dimensions");
    if (!o.video && !o.audio)
        throw std::runtime_error("Nothing to render");
    const qint64 from = o.from, to = o.to < 0 ? p.duration() : std::min(o.to, p.duration());
    if (from < 0 || from >= to)
        throw std::runtime_error("The render range is outside the timeline");
    const bool audio = o.audio;
    QDir().mkpath(work);
    RenderPlan r;
    r.frames = to - from;
    r.duration = frameTime(r.frames, p.fpsN, p.fpsD).seconds();
    r.width = width;
    r.height = height;
    const QString fps = QString::number(p.fpsN) + "/" + QString::number(p.fpsD);
    const auto secs = [&p](qint64 frames) { return frameTime(frames, p.fpsN, p.fpsD).seconds(); };
    QStringList nodes, audioLabels;
    QString visual = "base";
    if (o.video)
        nodes << QString("color=c=%6:s=%1x%2:r=%3:d=%4,trim=end_frame=%5,format=rgba[base]")
                     .arg(width)
                     .arg(height)
                     .arg(fps, num(r.duration))
                     .arg(r.frames)
                     .arg(o.transparent ? "black@0.0" : "black");
    if (audio) {
        nodes << QString("anullsrc=r=48000:cl=stereo,atrim=end_sample=%1[asilence]")
                     .arg(qRound64(r.duration * 48000));
        audioLabels << "[asilence]";
    }
    // Per-clip facts and transition handles. A transition of d frames into clip c is centred on
    // the cut: `before` frames before it, `after` frames after. The outgoing clip is extended by
    // `after` frames past its end and the incoming clip by `before` frames before its start.
    struct Info {
        const Clip *clip = nullptr;
        const Asset *asset = nullptr;
        bool title = false, image = false, video = false, audio = false;
        qint64 inLength = 0, outLength = 0;
        qint64 vPre = 0, vPost = 0, aPre = 0, aPost = 0;
        int vPrev = -1, vNext = -1;
        QString file;
    };
    auto clips = p.clips;
    std::stable_sort(clips.begin(), clips.end(),
                     [](const auto &a, const auto &b) { return a.track < b.track; });
    QVector<Info> info(clips.size());
    QHash<QString, int> index;
    for (int i = 0; i < clips.size(); ++i) {
        const auto &c = clips[i];
        auto &n = info[i];
        n.clip = &c;
        n.asset = p.asset(c.assetId);
        n.title = c.assetId.isEmpty() && c.effect.isEmpty();
        n.image = n.title || (n.asset && n.asset->kind == "image");
        n.video = o.video &&
                  (n.title || !c.effect.isEmpty() || (n.asset && n.asset->kind != "audio")) &&
                  !c.audioOnly &&
                  !p.trackSettings[c.track].hidden && !c.hidden;
        n.audio = o.audio && n.asset && n.asset->hasAudio && !c.muted && p.audioEnabled(c.track);
        index.insert(c.id, i);
    }
    for (int i = 0; i < clips.size(); ++i) {
        const auto length = p.transitionLength(clips[i]);
        if (!length)
            continue;
        const int prev = index.value(p.previousAdjacent(clips[i])->id);
        const qint64 before = length / 2, after = length - before;
        info[i].inLength = length;
        info[prev].outLength = length;
        if (info[prev].video && info[i].video) {
            info[prev].vPost = after;
            info[i].vPre = before;
            info[prev].vNext = i;
            info[i].vPrev = prev;
        }
        if (info[prev].audio && info[i].audio) {
            info[prev].aPost = after;
            info[i].aPre = before;
        }
    }
    int input = 0, serial = 0;
    const double frame = secs(1), half = frame / 2;
    auto mediaFile = [&](Info &n) {
        if (!n.file.isEmpty())
            return n.file;
        const auto &c = *n.clip;
        if (n.title) {
            n.file = QDir(work).filePath(QString("title-%1.png").arg(serial++));
            QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QPainter paint(&img);
            paint.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            const auto font = textFont(c, qRound(c.fontSize * double(height) / p.height));
            QRect rect(width / 15, height / 12, width * 13 / 15, height * 5 / 6);
            if (!c.graphic.isEmpty()) {
                // Shapes are drawn centred on the canvas at their own size; text goes inside.
                const QSizeF size(width * c.graphicWidth, height * c.graphicHeight);
                const QRectF box(QPointF(width - size.width(), height - size.height()) / 2, size);
                paintGraphic(paint, c, box, height);
                const double inset = std::min(box.width(), box.height()) * 0.12;
                rect = box.adjusted(inset, inset, -inset, -inset - (c.graphic == "bubble"
                                                                       ? box.height() * 0.18
                                                                       : 0))
                           .toRect();
            }
            if (!c.text.isEmpty() && c.graphic != "arrow" && c.graphic != "line")
                paintText(paint, c, font, rect);
            paint.end();
            if (!img.save(n.file))
                throw std::runtime_error("Cannot write title render asset");
        } else {
            n.file = n.asset->path;
            if (!QFileInfo(n.file).isFile())
                throw std::runtime_error(("Missing media: " + n.file).toStdString());
        }
        return n.file;
    };
    // Adds an input seeked to `seek` source seconds (images loop instead) and returns its index.
    auto addInput = [&](Info &n, double seek) {
        const auto file = mediaFile(n);
        r.inputs << "-protocol_whitelist" << "file,pipe";
        if (n.image)
            r.inputs << "-loop" << "1" << "-framerate" << fps;
        else if (n.asset && n.asset->loops)
            // Repeats endlessly; the start point is cut in the filter graph (see videoChain).
            r.inputs << "-stream_loop" << "-1";
        else
            r.inputs << "-ss" << num(std::max(0., seek));
        r.inputs << "-i" << QFileInfo(file).absoluteFilePath();
        return input++;
    };
    // Clip-local seconds where source media exists; images and titles never run out.
    auto available = [&](const Info &n) {
        const auto &c = *n.clip;
        if (n.image || (n.asset && n.asset->loops))
            return std::pair{-1e12, 1e12};
        const double s = c.speed.seconds(), in = c.sourceIn.seconds(), d = secs(c.duration),
                     media = n.asset->duration;
        return c.reverse ? std::pair{d - (media - in) / s, d + in / s}
                         : std::pair{-in / s, (media - in) / s};
    };
    // Picture of clip-local frames [l0, l1), which may reach into transition handles. Missing
    // source frames at either end hold the nearest frame. Output timestamps start at zero.
    // Colour and look of a clip appended to the chain `f` of a picture `w` pixels wide; filters
    // that mix with the picture go through split branches added to `nodes`.
    auto appendLook = [&](QString &f, const Clip &c, int w) {
        if (c.exposure != 0 || c.brightness != 0 || c.contrast != 1) {
            // Exposure scales linear light by 2^stops; with the 2.2 gamma of video that is a
            // gain of 2^(stops/2.2) on the coded values.
            const QString expr = QString("clip((val*%3-128)*%1+128+%2,0,255)")
                                     .arg(num(c.contrast), num(c.brightness * 255),
                                          num(std::pow(2., c.exposure / 2.2)));
            f += QString(",lutrgb=r='%1':g='%1':b='%1'").arg(expr);
        }
        if (c.saturation != 1)
            f += ",hue=s=" + num(c.saturation);
        // Colour: light colour temperature with lightness kept, a green–magenta balance, and
        // vibrance, which saturates muted colours more than saturated ones.
        if (c.temperature != 0)
            f += QString(",colortemperature=temperature=%1:pl=1")
                     .arg(num(c.temperature > 0 ? 6500 - 3500 * c.temperature
                                                : 6500 - 6500 * c.temperature));
        if (c.tint > 0)
            f += ",colorchannelmixer=gg=" + num(1 - 0.2 * c.tint);
        else if (c.tint < 0)
            f += QString(",colorchannelmixer=rr=%1:bb=%1").arg(num(1 + 0.2 * c.tint));
        if (c.vibrance != 0)
            f += ",vibrance=intensity=" + num(c.vibrance);
        if (c.liftX != 0 || c.liftY != 0 || c.gammaX != 0 || c.gammaY != 0 || c.gainX != 0 ||
            c.gainY != 0) {
            // Colour wheels, as lift, gamma and gain per channel: each point is turned into red,
            // green and blue amounts along its direction (a third of a turn apart, adding up to
            // zero so the lightness stays about the same), at most ±0.25. Then for each channel
            // out = (gain × (v + lift × (1 − v)))^(1 / gamma), as one look-up table.
            auto rgb = [](double x, double y) {
                const double a = std::atan2(y, x), r = std::min(1., std::hypot(x, y)) * 0.25;
                return std::array<double, 3>{r * std::cos(a), r * std::cos(a - 2 * std::numbers::pi / 3),
                                             r * std::cos(a + 2 * std::numbers::pi / 3)};
            };
            const auto lift = rgb(c.liftX, c.liftY), gamma = rgb(c.gammaX, c.gammaY),
                       gain = rgb(c.gainX, c.gainY);
            QStringList channels;
            for (int i = 0; i < 3; ++i)
                channels << QString("255*pow(clip(%1*(val/255+%2*(1-val/255)),0,1),%3)")
                                .arg(num(1 + gain[i]), num(lift[i]), num(1 / (1 + gamma[i])));
            f += QString(",lutrgb=r='%1':g='%2':b='%3'").arg(channels[0], channels[1], channels[2]);
        }
        if (c.shadows != 0 || c.highlights != 0 || c.whites != 0 || c.blacks != 0) {
            // Blacks and whites move the ends of the master curve (lifting blacks fades the
            // darkest tones, lowering whites softens the brightest), keeping it rising.
            const double black = std::clamp(0.15 * c.blacks, -0.15, 0.15),
                         white = std::clamp(1 + 0.15 * c.whites, 0.85, 1.15);
            const auto end = [](double x, double y) {
                // A point past 0 or 1 becomes the input level that reaches 0 or 1.
                return y < 0 ? QString("%1/0").arg(num(-y)) : y > 1 ? QString("%1/1").arg(num(2 - y)) : QString("%1/%2").arg(num(x), num(y));
            };
            f += QString(",curves=m='%1 0.25/%2 0.75/%3 %4'")
                     .arg(end(0, black), num(0.25 + 0.12 * c.shadows),
                          num(0.75 + 0.12 * c.highlights), end(1, white));
        }
        // Tone curves, and a selective change to some colours (FFmpeg's huesaturation).
        {
            QStringList curves;
            for (const auto &[name, points] : {std::pair{"m", &c.curveMaster}, std::pair{"r", &c.curveRed},
                                               std::pair{"g", &c.curveGreen}, std::pair{"b", &c.curveBlue}})
                if (!points->isEmpty() && validCurve(*points))
                    curves << QLatin1String(name) + "='" + *points + "'";
            if (!curves.isEmpty())
                f += ",curves=" + curves.join(':');
        }
        if (c.hslHue != 0 || c.hslSaturation != 0 || c.hslLightness != 0) {
            const auto colours = c.hslColors.split(' ', Qt::SkipEmptyParts);
            // Strength 5 reaches muted colours too while staying with the chosen ones.
            f += QString(",huesaturation=hue=%1:saturation=%2:intensity=%3:colors=%4:strength=5")
                     .arg(num(c.hslHue), num(c.hslSaturation), num(c.hslLightness * 0.5),
                          colours.isEmpty() ? QString("a") : colours.join('+'));
        }
        // Branches for filters that mix with the picture or would drop its alpha channel.
        auto branch = [&](const QString &a, const QString &b, const QString &join) {
            const auto id = QString::number(serial++);
            nodes << f + QString(",split[lka%1][lkb%1]").arg(id);
            nodes << QString("[lka%1]%2[lkc%1]").arg(id, a);
            nodes << QString("[lkb%1]%2[lkd%1]").arg(id, b);
            f = QString("[lkc%1][lkd%1]%2").arg(id, join);
        };
        if (!c.lut.isEmpty() && c.lutStrength > 0 && QFileInfo(c.lut).isFile()) {
            const auto lut = "lut3d=file=" + filterPath(c.lut) + ":interp=tetrahedral";
            if (c.lutStrength >= 1)
                f += "," + lut;
            else
                branch(lut, "null", "blend=all_mode=normal:all_opacity=" + num(c.lutStrength));
        }
        if (c.blur > 0)
            f += ",gblur=sigma=" + num(c.blur * 30 * w / 1920.0 + 0.5);
        if (c.sharpen > 0)
            f += ",cas=strength=" + num(c.sharpen);
        if (c.glow > 0)
            // Screen a soft copy over the picture; alpha stays the original's.
            branch("null", "gblur=sigma=" + num(std::max(1., 18. * w / 1920)),
                   QString("blend=c0_mode=screen:c1_mode=screen:c2_mode=screen:c0_opacity=%1:"
                           "c1_opacity=%1:c2_opacity=%1,format=rgba")
                       .arg(num(0.8 * c.glow)));
        if (c.vignette > 0)
            branch("vignette=angle=" + num(c.vignette * 1.1) + ":eval=init", "alphaextract",
                   "alphamerge,format=rgba");
        if (c.grain > 0)
            // Luma grain that changes every frame, on a format that keeps alpha.
            f += QString(",format=yuva444p,noise=c0s=%1:c0f=t,format=rgba").arg(num(4 + 26 * c.grain));
    };
    // `fill`, when given, receives a canvas-sized stream to put under the picture for its
    // canvas fill (blurred copy or colour), or stays empty.
    auto videoChain = [&](Info &n, qint64 l0, qint64 l1, QString *fill) {
        const auto &c = *n.clip;
        const double s = c.speed.seconds(), d = secs(c.duration);
        const auto [lo, hi] = available(n);
        const double rate = double(p.fpsN) / p.fpsD;
        const qint64 first = qint64(std::ceil(lo * rate - 1e-6)),
                     last = qint64(std::floor(hi * rate + 1e-6));
        qint64 c0 = std::max(l0, first), c1 = std::min(l1, last);
        if (c1 <= c0) {
            c0 = std::clamp(l0, first, std::max(first, last - 1));
            c1 = c0 + 1;
        }
        const qint64 begin = std::min(l0, c0), padStart = std::max<qint64>(0, c0 - l0),
                     padStop = std::max<qint64>(0, l1 - c1) + 1;
        const double seek = c.sourceIn.seconds() + (c.reverse ? d - secs(c1) : secs(c0)) * s;
        // Smooth slow motion makes in-between frames from source neighbours, so it decodes a
        // little source before and after the range, even for a single preview frame.
        const auto up = n.image || !n.asset                         ? MatteSource{}
                        : c.eyeContact && o.eyeContact.contains(n.asset->id) ? o.eyeContact.value(n.asset->id)
                        : c.aiUpscale ? o.upscaled.value(n.asset->id)
                                      : MatteSource{};
        const bool smooth = s < 1 && !c.reverse && !n.image && !c.slowMotion.isEmpty();
        // Motion blur mixes each frame with the ones before it, so it decodes a little before too.
        const bool blur = c.motionBlur > 0 && !n.image, history = smooth || (blur && !c.reverse);
        const double pre = history ? std::clamp(seek - (up.path.isEmpty() ? 0. : up.start), 0., 0.2)
                                  : 0,
                     post = smooth ? 0.2 : 0;
        const qint64 base = n.vPre;
        // Source frames to clip-local frames: seek, speed, reverse, hold at the ends, then exact
        // frame timestamps, clip-local and shifted by the handle so none are negative. The AI
        // matte takes the same path so it stays frame-aligned with the picture.
        // A matte arrives already aligned to the seek point and may end a little before the
        // picture (its last analysed frame), which it holds.
        auto timing = [&](const QString &source, bool reversible, bool matte = false) {
            const bool around = history && !matte;
            QString t = source + (matte ? "trim=start=0:duration=" : "trim=duration=") +
                        num(secs(c1 - c0) * s + (around ? pre + post : 0)) +
                        (matte ? "" : ",setpts=PTS-STARTPTS");
            if (c.reverse && reversible)
                t += ",reverse";
            // The fps filter below keeps, for each output frame, the last source frame that
            // rounds to it. Shifting the timestamps so that is the frame at exactly k × speed
            // (1/8 frame before the next output slot, measured in source frames) makes fast clips
            // and time-lapses show the right moment; at up to 1x the shift is 1/8 frame, which
            // resolves exact half-frame ties the same way wherever decoding started. Stills,
            // playback and export agree.
            const double shift = s > 1 ? 0.5 - 0.125 / s : 0.125;
            t += ",setpts=" + (around ? "(PTS-" + num(pre) + "/TB)" : QString("PTS")) + "/" +
                 num(s) + "+" + num(frame * shift) + "/TB";
            // Slow motion: blended or motion-interpolated in-between frames instead of repeats;
            // the extra source before the range is dropped afterwards.
            if (around && c.slowMotion == "blend")
                t += ",framerate=fps=" + fps;
            else if (around && c.slowMotion == "flow")
                t += ",minterpolate=fps=" + fps + ":mi_mode=mci:mc_mode=aobmc:me_mode=bidir:vsbmc=1";
            t += ",fps=" + fps;
            if (blur && !matte)
                t += ",tmix=frames=" + QString::number(2 + qRound(4 * c.motionBlur));
            if (around)
                t += ",trim=start=0,setpts=PTS-STARTPTS";
            t += QString(",trim=end_frame=%1,tpad=start=%2:stop=%3:start_mode=clone:stop_mode=clone")
                     .arg(c1 - c0)
                     .arg(padStart)
                     .arg(matte ? -1 : padStop);
            t += QString(",trim=start_frame=%1:end_frame=%2,settb=%3/%4,setpts=N+%5")
                     .arg(l0 - begin)
                     .arg(l0 - begin + (l1 - l0))
                     .arg(p.fpsD)
                     .arg(p.fpsN)
                     .arg(l0 + base);
            if (c.cropLeft > 0 || c.cropRight > 0 || c.cropTop > 0 || c.cropBottom > 0)
                t += QString(",crop=iw*%1:ih*%2:iw*%3:ih*%4")
                         .arg(num(1 - c.cropLeft - c.cropRight), num(1 - c.cropTop - c.cropBottom),
                              num(c.cropLeft), num(c.cropTop));
            if (c.crop > 0)
                t += QString(",crop=iw*(1-2*%1):ih*(1-2*%1)").arg(num(c.crop));
            if (c.shape == "circle")
                t += ",crop='min(iw,ih)':'min(iw,ih)'";
            return t;
        };
        int in = 0;
        if (!up.path.isEmpty()) {
            r.inputs << "-protocol_whitelist" << "file,pipe" << "-ss"
                     << num(std::max(0., seek - pre - up.start)) << "-i"
                     << QFileInfo(up.path).absoluteFilePath();
            in = input++;
        } else
            in = addInput(n, seek - pre);
        auto source = QString("[%1:v:0]").arg(in);
        if (up.path.isEmpty() && n.asset && n.asset->loops && n.asset->duration > 0)
            // A looping animation starts a whole number of loops before the seek point. Frames
            // are made regular first, so the cut keeps the picture shown at that moment (an
            // input seek would drop a long-held frame that began before it).
            source += QString("fps=%1,trim=start=%2,")
                          .arg(fps, num(std::fmod(std::max(0., seek - pre), n.asset->duration)));
        QString f = timing(source, !n.image);
        // Camera shake is measured on the source picture, before scaling.
        if (c.stabilize && !n.image) {
            // The search range in source pixels (FFmpeg allows up to 64); zooming in by it on
            // every side hides the edges the correction uncovers.
            const int range = std::clamp(int(std::lround(16 + 48 * c.stabilizeStrength)), 16, 64);
            f += QString(",deshake=rx=%1:ry=%1:edge=mirror").arg(range);
            if (c.stabilizeZoom)
                f += QString(",crop=w='iw-2*%1':h='ih-2*%1*ih/iw'").arg(range);
        }
        const bool moving = animatedGeometry(c);
        // Animated geometry first fits the canvas at scale 1 and is resized per frame below;
        // otherwise the clip is scaled once.
        const double boxScale = moving ? 1 : c.scale;
        int w = moving ? width : std::max(2, int(width * c.scale) / 2 * 2),
            h = moving ? height : std::max(2, int(height * c.scale) / 2 * 2);
        if (c.styled()) {
            // Styled overlays need the exact picture size for their mask and decoration.
            const auto size = p.pictureSize(c, w, h);
            w = std::max(2, int(std::lround(size.width() / 2)) * 2);
            h = std::max(2, int(std::lround(size.height() / 2)) * 2);
            f += QString(",scale=%1:%2,setsar=1,format=rgba").arg(w).arg(h);
        } else
            f += QString(",scale=%1:%2:force_original_aspect_ratio=decrease,setsar=1,format=rgba")
                     .arg(w)
                     .arg(h);
        if (c.flip)
            f += ",hflip";
        if (c.flipVertical)
            f += ",vflip";
        appendLook(f, c, w);
        if (fill && !c.canvasFill.isEmpty()) {
            // Canvas fill: the picture enlarged to cover the canvas and blurred, or a colour,
            // fading in and out with the clip.
            const auto id = QString::number(serial++);
            nodes << f + QString(",split[cfp%1][cfb%1]").arg(id);
            QString b = QString("[cfb%1]scale=%2:%3:force_original_aspect_ratio=increase,"
                                "crop=%2:%3,setsar=1")
                            .arg(id)
                            .arg(width)
                            .arg(height);
            if (c.canvasFill == "blur")
                b += QString(",gblur=sigma=%1:steps=2").arg(num(height / 25.));
            else
                b += QString(",drawbox=c=%1:t=fill").arg(c.canvasFill);
            b += ",format=rgb24,format=rgba";
            if (c.opacity != 1 && !c.keyframes.contains("opacity"))
                b += ",colorchannelmixer=aa=" + num(c.opacity);
            const double k = secs(base);
            if (c.fadeIn > 0)
                b += QString(",fade=t=in:st=%1:d=%2:alpha=1").arg(num(k), num(std::min(c.fadeIn, d)));
            if (c.fadeOut > 0) {
                const auto fd = std::min(c.fadeOut, d);
                b += QString(",fade=t=out:st=%1:d=%2:alpha=1").arg(num(k + d - fd), num(fd));
            }
            nodes << b + QString(",setpts=PTS-STARTPTS[cfo%1]").arg(id);
            *fill = QString("[cfo%1]null").arg(id);
            f = QString("[cfp%1]null").arg(id);
        }
        // Style effects. Times in the chain are clip-local frames plus the handle.
        const double fxk = c.fxStrength;
        if (c.fx == "shake" && fxk > 0) {
            // Zoom in a little and move the window along an irregular path.
            const double a = 0.01 + 0.05 * fxk;
            f += QString(",scale=trunc(iw*%1/2)*2:trunc(ih*%1/2)*2,"
                         "crop=trunc(iw/%1/2)*2:trunc(ih/%1/2)*2:"
                         "x='(iw-ow)/2*(1+sin(t*23)*cos(t*7.3))':"
                         "y='(ih-oh)/2*(1+sin(t*17.7+1)*cos(t*5.1))'")
                     .arg(num(1 + 2 * a));
        } else if (c.fx == "glitch" && fxk > 0) {
            // Colour channels jump apart in short, irregular bursts (the same each render).
            const auto name = QString("rgbashift@glitch%1").arg(serial++);
            QStringList commands;
            const double step = 0.1;
            const auto seed = qHash(c.id);
            // Only the bursts in the rendered part of the clip.
            const int firstStep = std::max(0, int(secs(l0) / step) - 2);
            for (int i = firstStep; i * step < std::min(secs(c.duration), secs(l1) + step); ++i) {
                const auto h = qHash(i, seed);
                if (h % 100 >= quint32(15 + 35 * fxk))
                    continue;
                const int shift = int(4 + h % 17 * fxk * 1.5) * ((h >> 8) % 2 ? 1 : -1);
                const double t0 = secs(base) + i * step, t1 = t0 + step * (1 + (h >> 4) % 2);
                commands << QString("%1-%2 [enter] %3 rh %4, [enter] %3 bh %5, [leave] %3 rh 0, "
                                    "[leave] %3 bh 0")
                                .arg(num(t0), num(t1), name)
                                .arg(shift)
                                .arg(-shift);
            }
            if (!commands.isEmpty())
                f += QString(",sendcmd=c='%1',%2").arg(commands.join(";"), name);
        } else if (c.fx == "vhs" && fxk > 0) {
            const int shift = qRound(1 + 4 * fxk);
            f += QString(",rgbashift=rh=%1:bh=%2,gblur=sigma=%3,hue=s=%4,format=yuva444p,"
                         "noise=c0s=%5:c0f=t,format=rgba,"
                         "drawgrid=w=iw:h=3:t=1:c=black@%6")
                     .arg(shift)
                     .arg(-shift)
                     .arg(num(0.3 + 0.8 * fxk), num(1 - 0.3 * fxk), num(4 + 14 * fxk),
                          num(0.08 + 0.2 * fxk));
        } else if (c.fx == "film" && fxk > 0) {
            // Sepia tone mixed in by strength, a light flicker and grain.
            auto mix = [&](double sepia, double identity) { return num(fxk * sepia + (1 - fxk) * identity); };
            f += QString(",colorchannelmixer=rr=%1:rg=%2:rb=%3:gr=%4:gg=%5:gb=%6:br=%7:bg=%8:bb=%9")
                     .arg(mix(.393, 1), mix(.769, 0), mix(.189, 0), mix(.349, 0), mix(.686, 1),
                          mix(.168, 0), mix(.272, 0), mix(.534, 0), mix(.131, 1));
            f += QString(",hue=b='%1*sin(t*41)',format=yuva444p,noise=c0s=%2:c0f=t,format=rgba")
                     .arg(num(0.06 * fxk), num(6 + 16 * fxk));
        } else if (c.fx == "sketch" && fxk > 0) {
            // Dark outlines on white, like a pencil drawing, mixed over the picture by strength.
            // The edge filters have no alpha, so the picture's alpha is put back afterwards.
            const auto id = QString::number(serial++);
            nodes << f + QString(",split[sa%1][sb%1]").arg(id);
            nodes << QString("[sa%1]alphaextract[sm%1]").arg(id);
            nodes << QString("[sb%1]format=gbrp,split[so%1][se%1]").arg(id);
            nodes << QString("[se%1]format=gray,gblur=sigma=1,sobel=scale=3,negate,format=gbrp[sn%1]")
                         .arg(id);
            nodes << QString("[sn%1][so%1]blend=all_mode=normal:all_opacity=%2[sc%1]")
                         .arg(id, num(fxk));
            f = QString("[sc%1][sm%1]alphamerge,format=rgba").arg(id);
        } else if (c.fx == "poster" && fxk > 0) {
            // Each colour channel rounded to a few levels: 8 at the lowest strength, 2 at full.
            const int levels = std::max(2, int(std::lround(8 - 6 * fxk)));
            const auto q = QString("floor(val*%1/256)*255/%2").arg(levels).arg(levels - 1);
            f += QString(",lutrgb=r='%1':g='%1':b='%1'").arg(q);
        } else if (c.fx == "fisheye" && fxk > 0) {
            // Barrel distortion, then zoomed in so the bent edges are cut away.
            const double k1 = 0.5 * fxk, zoom = 1 + 0.7 * k1;
            const auto id = QString::number(serial++);
            nodes << f + QString(",split[la%1][lb%1]").arg(id);
            nodes << QString("[la%1]alphaextract,lenscorrection=k1=%2:k2=0:i=bilinear[lm%1]")
                         .arg(id, num(k1));
            nodes << QString("[lb%1]format=gbrp,lenscorrection=k1=%2:k2=0:i=bilinear[lc%1]")
                         .arg(id, num(k1));
            const auto size = c.styled() ? QString("%1:%2").arg(w).arg(h)
                                         : QString("trunc(iw*%1/2)*2:trunc(ih*%1/2)*2").arg(num(zoom));
            f = QString("[lc%1][lm%1]alphamerge,format=rgba,crop=trunc(iw/%2/2)*2:trunc(ih/%2/2)*2,"
                        "scale=%3")
                    .arg(id, num(zoom), size);
        } else if (c.fx == "mirror" && fxk > 0) {
            // The left half, flipped, replaces the right half.
            const auto id = QString::number(serial++);
            nodes << f + QString(",split[ma%1][mb%1]").arg(id);
            nodes << QString("[mb%1]crop=trunc(iw/2):ih:0:0,hflip[mh%1]").arg(id);
            f = QString("[ma%1][mh%1]overlay=x=W-w:y=0:format=auto").arg(id);
        }
        const auto matte = c.aiCutout && !n.image && n.asset ? o.mattes.value(n.asset->id)
                                                             : MatteSource{};
        if (!matte.path.isEmpty()) {
            // The matte (a few analysed frames per second) is interpolated to the project rate,
            // timed, cropped and sized like the picture, then becomes its alpha channel.
            // Decoding starts at the analysed frame at or before the seek point (every matte
            // frame is a keyframe); the remainder is shifted out after interpolation.
            const double offset = std::max(0., seek - matte.start);
            const double key = std::floor(offset * matte.rate + 1e-6) / matte.rate;
            r.inputs << "-protocol_whitelist" << "file,pipe" << "-ss"
                     << num(std::max(0., key - 0.0005)) << "-i"
                     << QFileInfo(matte.path).absoluteFilePath();
            const auto id = QString::number(serial++);
            nodes << f + QString("[pic%1]").arg(id);
            nodes << timing(QString("[%1:v:0]format=gray,setpts=PTS-STARTPTS,"
                                    "framerate=fps=%2:scene=100,setpts=PTS-%3/TB,")
                                .arg(input++)
                                .arg(fps, num(offset - key)),
                            true, true) +
                         QString(",scale=%1:%2,setsar=1%3,format=gray[matte%4]")
                             .arg(w)
                             .arg(h)
                             .arg(QString(c.flip ? ",hflip" : "") + (c.flipVertical ? ",vflip" : ""))
                             .arg(id);
            f = QString("[pic%1][matte%1]alphamerge").arg(id);
        } else if (c.chromaKey) {
            const QColor key(c.keyColor);
            f += QString(",colorkey=0x%1:%2:%3")
                     .arg(key.name().mid(1), num(c.keySimilarity), num(c.keyBlend));
            // Remove the green or blue light reflected onto the subject.
            if (key.green() > key.red() && key.green() > key.blue())
                f += ",despill=type=green,format=rgba";
            else if (key.blue() > key.red() && key.blue() > key.green())
                f += ",despill=type=blue,format=rgba";
        }
        if (matte.path.isEmpty() && !c.lumaKey.isEmpty()) {
            // Keys out pixels whose brightness is within the tolerance of black or white. The
            // key multiplies the alpha the picture already has (transparent images, colour key).
            const auto id = QString::number(serial++);
            nodes << f + QString(",format=yuva444p,split=3[lk%1a][lk%1b][lk%1c]").arg(id);
            nodes << QString("[lk%1a]lumakey=threshold=%2:tolerance=%3:softness=%4,"
                             "alphaextract[lk%1k]")
                         .arg(id)
                         .arg(c.lumaKey == "light" ? 1 : 0)
                         .arg(num(c.lumaTolerance), num(c.lumaSoftness));
            nodes << QString("[lk%1b]alphaextract[lk%1o]").arg(id);
            nodes << QString("[lk%1o][lk%1k]blend=all_mode=multiply[lk%1m]").arg(id);
            f = QString("[lk%1c][lk%1m]alphamerge,format=rgba").arg(id);
        }
        // Each still layer (mask, decoration) is a looped image retimed to the clip's frames.
        auto stillInput = [&](const QImage &image, const QString &kind) {
            const auto file = QDir(work).filePath(QString("%1-%2.png").arg(kind).arg(serial++));
            if (!image.save(file))
                throw std::runtime_error("Cannot write overlay style asset");
            r.inputs << "-loop" << "1" << "-framerate" << fps << "-i" << file;
            return QString("[%1:v:0]settb=%2/%3,setpts=N+%4")
                .arg(input++)
                .arg(p.fpsD)
                .arg(p.fpsN)
                .arg(l0 + base);
        };
        if (!c.cornerPin.isEmpty() || c.tiltX != 0 || c.tiltY != 0) {
            // Corner pin and tilt: every pixel takes the source pixel its inverse perspective
            // lands on.
            const auto [xs, ys] = cornerPinMaps(pinCorners(c, w, h), w, h);
            const auto id = QString::number(serial++);
            nodes << f + QString("[pin%1]").arg(id);
            nodes << stillInput(xs, "pinx") + QString(",format=gray16[pinx%1]").arg(id);
            nodes << stillInput(ys, "piny") + QString(",format=gray16[piny%1]").arg(id);
            f = QString("[pin%1][pinx%1][piny%1]remap=fill=black@0,format=rgba").arg(id);
        }
        if (c.shape != "rect" || c.feather > 0) {
            // Multiply the picture's alpha by the shape: keeps chroma-key transparency.
            const auto id = QString::number(serial++);
            nodes << f + QString(",format=gbrap[pic%1]").arg(id);
            nodes << stillInput(overlayMask(c, w, h), "mask") +
                         QString(",format=gbrap[mask%1]").arg(id);
            f = QString("[pic%1][mask%1]blend=c0_mode=normal:c1_mode=normal:c2_mode=normal:"
                        "c3_mode=multiply:shortest=1,format=rgba")
                    .arg(id);
        }
        if (c.border > 0 || c.shadow > 0) {
            // Border and shadow are drawn once into a larger image the picture sits on.
            const auto deco = overlayDecoration(c, w, h, height * boxScale);
            const auto id = QString::number(serial++);
            nodes << f + QString("[pic%1]").arg(id);
            nodes << stillInput(deco.image, "deco") + QString(",format=rgba[deco%1]").arg(id);
            f = QString("[deco%1][pic%1]overlay=x=%2:y=%3:format=auto:eof_action=endall")
                    .arg(id)
                    .arg(deco.offset.x())
                    .arg(deco.offset.y());
        }
        if (!moving && c.rotation != 0)
            f += QString(",rotate=%1*PI/180:ow=rotw(%1*PI/180):oh=roth(%1*PI/180):c=black@0")
                     .arg(num(c.rotation));
        if (c.keyframes.contains("opacity")) {
            // Opacity changes per frame through runtime commands, one per run of equal values.
            const auto name = QString("colorchannelmixer@op%1").arg(serial++);
            QStringList commands;
            auto value = [&](qint64 f) { return qRound(c.valueAt("opacity", f) * 1000) / 1000.; };
            for (qint64 f = l0; f < l1;) {
                qint64 e = f + 1;
                while (e < l1 && value(e) == value(f))
                    ++e;
                commands << QString("%1-%2 %3 aa %4")
                                .arg(num(std::max(0., secs(f + base) - half)),
                                     num(secs(e + base) - half), name,
                                     num(value(f)));
                f = e;
            }
            f += QString(",sendcmd=c='%1',%2=aa=%3")
                     .arg(commands.join(";"), name, num(value(l0)));
        } else if (c.opacity != 1)
            f += ",colorchannelmixer=aa=" + num(c.opacity);
        const double k = secs(base);
        if (c.fadeIn > 0)
            f += QString(",fade=t=in:st=%1:d=%2:alpha=1").arg(num(k), num(std::min(c.fadeIn, d)));
        if (c.fadeOut > 0) {
            const auto fd = std::min(c.fadeOut, d);
            f += QString(",fade=t=out:st=%1:d=%2:alpha=1").arg(num(k + d - fd), num(fd));
        }
        if (moving) {
            // Animated geometry: rotate within a square that fits any angle, then resize per
            // frame. Overlay positions the changing frame (see overlayPosition). All three are
            // LGPL filters evaluated per frame from the clip-local frame t*fps - base.
            const auto local = QString("(t*%1/%2-%3)").arg(p.fpsN).arg(p.fpsD).arg(base);
            if (c.rotation != 0 || c.keyframes.contains("rotation"))
                f += QString(",rotate=a='(%1)*PI/180':ow='hypot(iw,ih)':oh=ow:c=black@0")
                         .arg(curve(c, "rotation", local));
            const auto size = curve(c, "scale", local);
            f += QString(",scale=w='max(2,trunc(iw*(%1)/2)*2)':h='max(2,trunc(ih*(%1)/2)*2)':"
                         "eval=frame")
                     .arg(size);
        }
        return f + ",setpts=PTS-STARTPTS";
    };
    // `offset` is the clip-local frame shown at overlay time zero, for animated positions.
    // An anchor away from the centre moves the centre as the picture scales and turns, so the
    // anchor point stays put (see Project::anchorShift).
    auto overlayPosition = [&](const Clip &c, qint64 offset) {
        const bool anchored = (c.anchorX != 0.5 || c.anchorY != 0.5) && c.titleStyle.isEmpty();
        const auto base = p.pictureSize(c, width, height);
        if (animatedGeometry(c)) {
            const auto local = QString("(t*%1/%2+%3)").arg(p.fpsN).arg(p.fpsD).arg(offset);
            QString sx, sy;
            if (anchored) {
                const auto dx = num((c.anchorX - 0.5) * base.width()),
                           dy = num((c.anchorY - 0.5) * base.height()),
                           s = "(" + curve(c, "scale", local) + ")",
                           a = "((" + curve(c, "rotation", local) + ")*PI/180)";
                sx = QString("+%1-%3*(cos(%4)*%1-sin(%4)*%2)").arg(dx, dy, s, a);
                sy = QString("+%2-%3*(sin(%4)*%1+cos(%4)*%2)").arg(dx, dy, s, a);
            }
            return QString("x='(W-w)/2+(%1)*W%3':y='(H-h)/2+(%2)*H%4'")
                .arg(curve(c, "x", local), curve(c, "y", local), sx, sy);
        }
        if (!anchored)
            return QString("x=(W-w)/2+%1*W:y=(H-h)/2+%2*H").arg(num(c.x), num(c.y));
        const auto shift = Project::anchorShift(c, base, c.scale, c.rotation);
        return QString("x=(W-w)/2+%1*W+(%3):y=(H-h)/2+%2*H+(%4)")
            .arg(num(c.x), num(c.y), num(shift.x()), num(shift.y()));
    };
    // Composites a zero-based stream onto the picture for window frames [place, place + length),
    // normally or with a blend mode (see blendModes()).
    auto composite = [&](const QString &stream, const QString &position, qint64 place,
                         qint64 length, const QString &blend = {}) {
        const auto id = QString::number(serial++);
        nodes << stream + QString(",setpts=PTS+%1[v%2]").arg(place).arg(id);
        const QString next = "mix" + id;
        const auto enable = QString("enable='gte(t,%1)*lt(t,%2)'")
                                .arg(num(secs(place) - half), num(secs(place + length) - half));
        if (blend.isEmpty()) {
            nodes << QString("[%1][v%2]overlay=%3:eof_action=pass:repeatlast=0:format=auto:%4[%5]")
                         .arg(visual, id, position, enable, next);
            visual = next;
            return;
        }
        // The picture is placed on a transparent canvas; its colours are blended with the
        // picture below by the mode, and its alpha (shape, keys, opacity, fades) decides where
        // and how much of the blended result shows.
        nodes << QString("color=c=black@0.0:s=%1x%2:r=%3,trim=end_frame=%4,format=rgba[bc%5]")
                     .arg(width)
                     .arg(height)
                     .arg(fps)
                     .arg(r.frames)
                     .arg(id);
        nodes << QString("[bc%1][v%1]overlay=%2:eof_action=pass:repeatlast=0:format=auto:%3,"
                         "format=gbrap,split[bl%1][ba%1]")
                     .arg(id, position, enable);
        nodes << QString("[%1]split[bd%2][bk%2]").arg(visual, id);
        nodes << QString("[bd%1]format=gbrap[bg%1]").arg(id);
        nodes << QString("[bg%1][bl%1]blend=all_mode=%2:%3[bx%1]").arg(id, blend, enable);
        nodes << QString("[ba%1]alphaextract[bm%1]").arg(id);
        nodes << QString("[bx%1][bm%1]alphamerge[by%1]").arg(id);
        nodes << QString("[bk%1][by%1]overlay=0:0:eof_action=pass:repeatlast=0:format=auto:%2[%3]")
                     .arg(id, enable, next);
        visual = next;
    };
    QVector<bool> emitted(clips.size());
    for (int i = 0; i < clips.size(); ++i) {
        if (!info[i].video || emitted[i])
            continue;
        int head = i;
        while (info[head].vPrev >= 0)
            head = info[head].vPrev;
        QVector<int> group;
        for (int m = head; m >= 0; m = info[m].vNext) {
            group << m;
            emitted[m] = true;
        }
        const auto startOf = [&](int m) { return clips[m].start; };
        const auto endOf = [&](int m) { return clips[m].start + clips[m].duration; };
        // Render from the start of any transition the window begins inside, because xfade
        // cannot start part-way through; likewise finish any transition the window ends inside.
        qint64 gs = std::max(from, startOf(group.first())),
               ge = std::min(to, endOf(group.last()));
        if (gs >= ge)
            continue;
        for (int g = 1; g < group.size(); ++g) {
            const auto m = group[g];
            const qint64 ws = startOf(m) - info[m].vPre, we = startOf(m) + info[m].inLength -
                                                              info[m].vPre;
            if (gs > ws && gs < we)
                gs = ws;
            if (ge > ws && ge < we)
                ge = we;
        }
        QVector<int> members;
        for (int m : group)
            if (std::max(gs, startOf(m) - info[m].vPre) <
                std::min(ge, endOf(m) + info[m].vPost))
                members << m;
        const qint64 visibleStart = std::max(gs, from), visibleEnd = std::min(ge, to);
        if (members.size() == 1) {
            // Inside one clip's own time: an ordinary clip, positioned on the canvas directly.
            auto &n = info[members.first()];
            const auto &c = *n.clip;
            if (c.effect == "adjust") {
                // Adjustment layer: the clip's colour and look on the picture composited so
                // far, mixed in by its opacity while the clip runs.
                const auto id = QString::number(serial++);
                QString look = QString("[adj%1]format=rgba").arg(id);
                appendLook(look, c, width);
                if (look == QString("[adj%1]format=rgba").arg(id) || c.opacity <= 0)
                    continue; // nothing to change
                if (c.opacity < 1)
                    look += ",colorchannelmixer=aa=" + num(c.opacity);
                nodes << QString("[%1]split[base%2][adj%2]").arg(visual, id);
                nodes << look + QString("[look%1]").arg(id);
                nodes << QString("[base%1][look%1]overlay=eof_action=pass:repeatlast=0:format=auto:"
                                 "enable='gte(t,%2)*lt(t,%3)'[adjusted%1]")
                             .arg(id, num(secs(visibleStart - from) - half),
                                  num(secs(visibleEnd - from) - half));
                visual = "adjusted" + id;
                continue;
            }
            if (!c.effect.isEmpty()) {
                // Effect area: blur or pixelate the picture composited so far, inside a
                // rectangle that follows the clip's (possibly animated) position.
                const double scale = c.valueAt("scale", 0);
                const int rw = std::clamp(int(std::lround(width * c.effectWidth * scale / 2)) * 2, 2,
                                          width / 2 * 2),
                          rh = std::clamp(int(std::lround(height * c.effectHeight * scale / 2)) * 2,
                                          2, height / 2 * 2);
                const auto local =
                    QString("(t*%1/%2+%3)").arg(p.fpsN).arg(p.fpsD).arg(from - c.start);
                const auto x = QString("clip((%1-%2)/2+(%3)*%1,0,%1-%2)")
                                   .arg(width)
                                   .arg(rw)
                                   .arg(curve(c, "x", local));
                const auto y = QString("clip((%1-%2)/2+(%3)*%1,0,%1-%2)")
                                   .arg(height)
                                   .arg(rh)
                                   .arg(curve(c, "y", local));
                // Strength scales with the output size so previews look like the export.
                const double unit = height / 1080.0;
                const QString effect =
                    c.effect == "pixelate"
                        ? QString("pixelize=w=%1:h=%1").arg(std::max(2, int(std::lround((6 + c.effectStrength * 54) * unit))))
                        : QString("gblur=sigma=%1:steps=3").arg(num((3 + c.effectStrength * 37) * unit));
                const auto id = QString::number(serial++);
                nodes << QString("[%1]split[base%2][src%2]").arg(visual, id);
                nodes << QString("[src%1]crop=w=%2:h=%3:x='%4':y='%5',%6[fx%1]")
                             .arg(id)
                             .arg(rw)
                             .arg(rh)
                             .arg(x, y, effect);
                QString area = "fx" + id;
                if (c.effectShape == "ellipse" || c.feather > 0) {
                    // Ellipse or soft edge: the effect's alpha is multiplied by a still mask,
                    // so the untouched picture shows through outside the shape.
                    Clip shape;
                    shape.shape = c.effectShape == "ellipse" ? "circle" : "rect";
                    shape.feather = c.feather;
                    const auto file =
                        QDir(work).filePath(QString("area-%1.png").arg(serial++));
                    if (!overlayMask(shape, rw, rh).save(file))
                        throw std::runtime_error("Cannot write effect area mask");
                    r.inputs << "-loop" << "1" << "-framerate" << fps << "-i" << file;
                    nodes << QString("[%1:v:0]settb=%2/%3,setpts=N,format=gbrap[amask%4]")
                                 .arg(input++)
                                 .arg(p.fpsD)
                                 .arg(p.fpsN)
                                 .arg(id);
                    nodes << QString("[fx%1]format=gbrap[fxp%1]").arg(id);
                    nodes << QString("[fxp%1][amask%1]blend=c0_mode=normal:c1_mode=normal:"
                                     "c2_mode=normal:c3_mode=multiply:shortest=1,format=rgba[fxm%1]")
                                 .arg(id);
                    area = "fxm" + id;
                }
                nodes << QString("[base%1][%6]overlay=x='%2':y='%3':eof_action=pass:"
                                 "repeatlast=0:format=auto:enable='gte(t,%4)*lt(t,%5)'[area%1]")
                             .arg(id, x, y, num(secs(visibleStart - from) - half),
                                  num(secs(visibleEnd - from) - half))
                             .arg(area);
                visual = "area" + id;
                continue;
            }
            if (n.title && !c.titleStyle.isEmpty() && !animatedGeometry(c) && c.rotation == 0 &&
                n.vPre == 0) {
                // Title template: the plate image, faded, and for lower thirds slid in from the
                // left over the first 0.45 s with an ease-out curve.
                const auto plate = titlePlate(c, width, height, p.height);
                const auto file = QDir(work).filePath(QString("plate-%1.png").arg(serial++));
                if (plate.image.isNull() || !plate.image.save(file))
                    throw std::runtime_error("Cannot write title render asset");
                r.inputs << "-loop" << "1" << "-framerate" << fps << "-i" << file;
                const qint64 l0 = visibleStart - c.start, l1 = visibleEnd - c.start;
                QString f = QString("[%1:v:0]trim=end_frame=%2,settb=%3/%4,setpts=N+%5,format=rgba")
                                .arg(input++)
                                .arg(l1 - l0)
                                .arg(p.fpsD)
                                .arg(p.fpsN)
                                .arg(l0);
                if (c.opacity != 1)
                    f += ",colorchannelmixer=aa=" + num(c.opacity);
                const double d = secs(c.duration);
                if (c.fadeIn > 0)
                    f += QString(",fade=t=in:st=0:d=%1:alpha=1").arg(num(std::min(c.fadeIn, d)));
                if (c.fadeOut > 0) {
                    const auto fd = std::min(c.fadeOut, d);
                    f += QString(",fade=t=out:st=%1:d=%2:alpha=1").arg(num(d - fd), num(fd));
                }
                const auto px = plate.position.x() + qRound(c.x * width),
                           py = plate.position.y() + qRound(c.y * height);
                QString x = QString::number(px);
                // Lower thirds slide in from their side: the left, or the right for the right one.
                if (QStringList{"lowerThird", "lowerThirdLine", "lowerThirdRight"}.contains(c.titleStyle) &&
                    c.titleSlide) {
                    const auto local = QString("(t+%1)").arg(num(secs(from - c.start)));
                    x = c.titleStyle == "lowerThirdRight"
                            ? QString("'%1+(W-%1)*pow(max(0,1-%2/0.45),3)'").arg(px).arg(local)
                            : QString("'%1-(%1+w)*pow(max(0,1-%2/0.45),3)'").arg(px).arg(local);
                }
                composite(f + ",setpts=PTS-STARTPTS", QString("x=%1:y=%2").arg(x).arg(py),
                          visibleStart - from, visibleEnd - visibleStart, c.blendMode);
                continue;
            }
            if (n.title && QStringList{"rise", "pop", "fly", "drop", "spin", "fade"}.contains(c.textAnimation) &&
                c.graphic.isEmpty() && c.titleStyle.isEmpty() && c.captionStyle.isEmpty() &&
                !c.text.isEmpty() && !animatedGeometry(c) && c.rotation == 0 && c.scale == 1 &&
                n.vPre == 0) {
                // Letters that move in: a picture per frame of the animation (only those the
                // rendered range needs), as an image sequence; after the animation the last
                // picture, the finished text, is held.
                const auto font = textFont(c, qRound(c.fontSize * double(height) / p.height));
                const QRect rect(width / 15, height / 12, width * 13 / 15, height * 5 / 6);
                const double px = font.pixelSize();
                // Rows the finished text covers, with room below for rising letters.
                QImage full(width, height, QImage::Format_ARGB32_Premultiplied);
                full.fill(Qt::transparent);
                {
                    QPainter paint(&full);
                    paint.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
                    paintText(paint, c, font, rect);
                }
                int top = height, bottom = -1;
                for (int y = 0; y < height; ++y) {
                    const auto *line = reinterpret_cast<const QRgb *>(full.constScanLine(y));
                    for (int x = 0; x < width; ++x)
                        if (qAlpha(line[x]) > 0) {
                            top = std::min(top, y);
                            bottom = y;
                            break;
                        }
                }
                if (bottom >= 0) {
                    top = std::max(0, top - int(std::ceil(0.15 * px)));
                    bottom = std::min(height - 1, bottom + int(std::ceil(0.9 * px)));
                    const int band = bottom - top + 1;
                    const double rate = std::min(double(p.fpsN) / p.fpsD, 60.);
                    const int last = int(std::ceil(c.textAnimationTime * rate));
                    const qint64 l0 = visibleStart - c.start, l1 = visibleEnd - c.start;
                    const int k0 = std::min(last, int(std::floor(secs(l0) * rate))),
                              k1 = std::min(last, int(std::ceil(secs(l1) * rate)));
                    const auto folder = QDir(work).filePath(QString("letters-%1").arg(serial++));
                    QDir(folder).removeRecursively(); // a sequence takes every numbered file
                    if (!QDir().mkpath(folder))
                        throw std::runtime_error("Cannot write title render asset");
                    for (int k = k0; k <= k1; ++k) {
                        QImage frame(width, band, QImage::Format_ARGB32_Premultiplied);
                        frame.fill(Qt::transparent);
                        {
                            QPainter paint(&frame);
                            paint.setRenderHints(QPainter::Antialiasing |
                                                 QPainter::TextAntialiasing);
                            paint.translate(0, -top);
                            paintText(paint, c, font, rect, -1, k == last ? -1 : k / rate);
                        }
                        if (!frame.save(QString("%1/%2.png").arg(folder).arg(k - k0, 5, 10,
                                                                               QChar('0'))))
                            throw std::runtime_error("Cannot write title render asset");
                    }
                    r.inputs << "-framerate" << num(rate) << "-i" << folder + "/%05d.png";
                    // Clip-local timestamps: the first picture is frame k0 of the animation.
                    QString f = QString("[%1:v:0]setpts='PTS+%2/TB',fps=%3/%4,"
                                        "tpad=stop_mode=clone:stop_duration=%5,"
                                        "trim=start=%6:end=%7,format=rgba")
                                    .arg(input++)
                                    .arg(num(k0 / rate))
                                    .arg(p.fpsN)
                                    .arg(p.fpsD)
                                    .arg(num(secs(l1) + 1))
                                    .arg(num(secs(l0)), num(secs(l1)));
                    if (c.opacity != 1)
                        f += ",colorchannelmixer=aa=" + num(c.opacity);
                    const double d = secs(c.duration);
                    if (c.fadeIn > 0)
                        f += QString(",fade=t=in:st=0:d=%1:alpha=1").arg(num(std::min(c.fadeIn, d)));
                    if (c.fadeOut > 0) {
                        const auto fd = std::min(c.fadeOut, d);
                        f += QString(",fade=t=out:st=%1:d=%2:alpha=1").arg(num(d - fd), num(fd));
                    }
                    composite(f + ",setpts=PTS-STARTPTS",
                              QString("x=%1:y=%2")
                                  .arg(qRound(c.x * width))
                                  .arg(top + qRound(c.y * height)),
                              visibleStart - from, visibleEnd - visibleStart, c.blendMode);
                    continue;
                }
            }
            if (n.title && (c.textAnimation == "typewriter" || c.textAnimation == "words") &&
                c.graphic.isEmpty() &&
                c.titleStyle.isEmpty() && c.captionStyle.isEmpty() && !c.text.isEmpty() &&
                !animatedGeometry(c) && c.rotation == 0 && c.scale == 1 && n.vPre == 0) {
                // A title that builds up: one band per step (a character or a word), stacked
                // in one picture; a crop shows the band for the clip-local time.
                const auto font = textFont(c, qRound(c.fontSize * double(height) / p.height));
                const QRect rect(width / 15, height / 12, width * 13 / 15, height * 5 / 6);
                auto drawn = [&](int visible) {
                    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
                    img.fill(Qt::transparent);
                    QPainter paint(&img);
                    paint.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
                    paintText(paint, c, font, rect, visible);
                    return img;
                };
                // Rows the whole text covers, outline and shadow included.
                const auto full = drawn(-1);
                int top = height, bottom = -1;
                for (int y = 0; y < height; ++y) {
                    const auto *line = reinterpret_cast<const QRgb *>(full.constScanLine(y));
                    for (int x = 0; x < width; ++x)
                        if (qAlpha(line[x]) > 0) {
                            top = std::min(top, y);
                            bottom = y;
                            break;
                        }
                }
                // Characters shown at each step: one more character, or the next whole word.
                QVector<int> counts;
                if (c.textAnimation == "words") {
                    int total = 0;
                    for (const auto &word : shownTitleText(c.text).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts))
                        counts << (total += int(word.size()));
                } else {
                    const auto text = shownTitleText(c.text);
                    const int total = int(std::count_if(text.begin(), text.end(),
                                                        [](QChar ch) { return !ch.isSpace(); }));
                    for (int i = 1; i <= total; ++i)
                        counts << i;
                }
                const int band = bottom - top + 1;
                // At most 32000 rows: long texts reveal a few characters per step.
                const int stride =
                    counts.isEmpty() || bottom < 0
                        ? 1
                        : int(std::ceil(double(band) * counts.size() / 32000));
                QVector<int> steps;
                for (int i = stride - 1; i < counts.size(); i += stride)
                    steps << counts[i];
                if (!counts.isEmpty() && (steps.isEmpty() || steps.last() != counts.last()))
                    steps << counts.last();
                if (bottom >= 0 && !steps.isEmpty()) {
                    QImage strip(width, band * int(steps.size()), QImage::Format_ARGB32_Premultiplied);
                    strip.fill(Qt::transparent);
                    {
                        QPainter paint(&strip);
                        for (int i = 0; i < steps.size(); ++i)
                            paint.drawImage(QPoint(0, i * band), drawn(steps[i]),
                                            QRect(0, top, width, band));
                    }
                    const auto file = QDir(work).filePath(QString("build-%1.png").arg(serial++));
                    if (!strip.save(file))
                        throw std::runtime_error("Cannot write title render asset");
                    r.inputs << "-loop" << "1" << "-framerate" << fps << "-i" << file;
                    const qint64 l0 = visibleStart - c.start, l1 = visibleEnd - c.start;
                    const int count = int(steps.size());
                    QString f = QString("[%1:v:0]trim=end_frame=%2,settb=%3/%4,setpts=N+%5,"
                                        "crop=w=%6:h=%7:x=0:y='%7*min(%8,floor(t*%9))',format=rgba")
                                    .arg(input++)
                                    .arg(l1 - l0)
                                    .arg(p.fpsD)
                                    .arg(p.fpsN)
                                    .arg(l0)
                                    .arg(width)
                                    .arg(band)
                                    .arg(count - 1)
                                    .arg(num(count / c.textAnimationTime));
                    if (c.opacity != 1)
                        f += ",colorchannelmixer=aa=" + num(c.opacity);
                    const double d = secs(c.duration);
                    if (c.fadeIn > 0)
                        f += QString(",fade=t=in:st=0:d=%1:alpha=1").arg(num(std::min(c.fadeIn, d)));
                    if (c.fadeOut > 0) {
                        const auto fd = std::min(c.fadeOut, d);
                        f += QString(",fade=t=out:st=%1:d=%2:alpha=1").arg(num(d - fd), num(fd));
                    }
                    composite(f + ",setpts=PTS-STARTPTS",
                              QString("x=%1:y=%2")
                                  .arg(qRound(c.x * width))
                                  .arg(top + qRound(c.y * height)),
                              visibleStart - from, visibleEnd - visibleStart, c.blendMode);
                    continue;
                }
            }
            if (n.title && !c.captionStyle.isEmpty() && c.timedWords() && !animatedGeometry(c) &&
                c.rotation == 0 && c.scale == 1 && n.vPre == 0) {
                const auto sprite = captionSprite(c, width, height, p.height);
                if (!sprite.image.isNull()) {
                    const auto file = QDir(work).filePath(QString("caption-%1.png").arg(serial++));
                    if (!sprite.image.save(file))
                        throw std::runtime_error("Cannot write caption render asset");
                    r.inputs << "-loop" << "1" << "-framerate" << fps << "-i" << file;
                    const qint64 l0 = visibleStart - c.start, l1 = visibleEnd - c.start;
                    // t is clip-local time; each word start adds one band to the crop offset.
                    QStringList steps{"0"};
                    for (int i = 1; i < c.wordStarts.size(); ++i)
                        steps << QString("gte(t,%1)").arg(num((c.wordStarts[i] - 0.5) / (double(p.fpsN) / p.fpsD)));
                    QString f = QString("[%1:v:0]trim=end_frame=%2,settb=%3/%4,setpts=N+%5,"
                                        "crop=w=%6:h=%7:x=0:y='%7*(%8)',format=rgba")
                                    .arg(input++)
                                    .arg(l1 - l0)
                                    .arg(p.fpsD)
                                    .arg(p.fpsN)
                                    .arg(l0)
                                    .arg(width)
                                    .arg(sprite.band)
                                    .arg(steps.join('+'));
                    if (c.opacity != 1)
                        f += ",colorchannelmixer=aa=" + num(c.opacity);
                    const double d = secs(c.duration);
                    if (c.fadeIn > 0)
                        f += QString(",fade=t=in:st=0:d=%1:alpha=1").arg(num(std::min(c.fadeIn, d)));
                    if (c.fadeOut > 0) {
                        const auto fd = std::min(c.fadeOut, d);
                        f += QString(",fade=t=out:st=%1:d=%2:alpha=1").arg(num(d - fd), num(fd));
                    }
                    composite(f + ",setpts=PTS-STARTPTS", overlayPosition(c, l0),
                              visibleStart - from, visibleEnd - visibleStart, c.blendMode);
                    continue;
                }
            }
            QString fill;
            const auto chain = videoChain(n, visibleStart - c.start, visibleEnd - c.start, &fill);
            if (!fill.isEmpty())
                composite(fill, "x=0:y=0", visibleStart - from, visibleEnd - visibleStart);
            composite(chain, overlayPosition(c, from - c.start), visibleStart - from,
                      visibleEnd - visibleStart, c.blendMode);
            continue;
        }
        // Each member is placed on its own transparent canvas and joined with xfade.
        QString accumulated;
        for (int g = 0; g < members.size(); ++g) {
            auto &n = info[members[g]];
            const auto &c = *n.clip;
            const qint64 r0 = std::max(gs, c.start - n.vPre),
                         r1 = std::min(ge, c.start + c.duration + n.vPost);
            const auto id = QString::number(serial++);
            nodes << QString("color=c=black@0.0:s=%1x%2:r=%3,trim=end_frame=%4,format=rgba[cv%5]")
                         .arg(width)
                         .arg(height)
                         .arg(fps)
                         .arg(r1 - r0)
                         .arg(id);
            QString fill;
            nodes << videoChain(n, r0 - c.start, r1 - c.start, &fill) + QString("[m%1]").arg(id);
            if (!fill.isEmpty()) {
                nodes << fill + QString("[cf%1]").arg(id);
                nodes << QString("[cv%1][cf%1]overlay=0:0:eof_action=pass:format=auto[cvf%1]").arg(id);
            }
            nodes << QString("[%5][m%1]overlay=%2:eof_action=pass:format=auto,settb=%3/%4,"
                             "setpts=PTS-STARTPTS[mc%1]")
                         .arg(id, overlayPosition(c, r0 - c.start))
                         .arg(p.fpsD)
                         .arg(p.fpsN)
                         .arg((fill.isEmpty() ? "cv" : "cvf") + id);
            if (g == 0) {
                accumulated = "mc" + id;
                continue;
            }
            const auto joined = "x" + id;
            // Cutlery's own transitions are a dissolve with an effect over the transition time.
            const bool own = QStringList{"spin", "glitch", "lightleak"}.contains(c.transition);
            const double off = secs(r0 - gs), length = secs(n.inLength);
            const auto during = QString("between(t,%1,%2)").arg(num(off), num(off + length));
            // Progress 0..1 through the transition, eased in and out.
            const auto u = QString("clip((t-%1)/%2,0,1)").arg(num(off), num(length));
            const auto eased = QString("(%1)*(%1)*(3-2*(%1))").arg(u);
            QString effect;
            if (c.transition == "spin")
                // One full turn, the dissolve happening on the way.
                effect = QString(",rotate=a='2*PI*%1':c=black@0:enable='%2'").arg(eased, during);
            else if (c.transition == "glitch") {
                // Colour channels jump apart and noise flickers, strongest at the cut.
                const int shift = std::max(4, width / 60);
                effect = QString(",format=gbrap,rgbashift=rh=%1:bh=-%1:gv=%2:enable='%3*lt(mod(t*12,1),0.6)',"
                                 "noise=alls=40:allf=t+u:enable='%3*lt(abs(%4-0.5),0.3)',format=rgba")
                             .arg(shift)
                             .arg(shift / 2)
                             .arg(during, u);
            }
            nodes << QString("[%1][mc%2]xfade=transition=%3:duration=%4:offset=%5%7[%6]")
                         .arg(accumulated, id, own ? QString("fade") : c.transition,
                              num(length), num(off), (c.transition == "lightleak" ? "a" : "") + joined,
                              effect);
            if (c.transition == "lightleak") {
                // A warm, slowly turning glow that flares up to the cut and fades away.
                nodes << QString("gradients=s=%1x%2:r=%3:c0=0xffd27a:c1=0xff6a2a:n=2:x0=0:y0=%2:"
                                 "x1=%1:y1=0:speed=0.03:seed=1:d=%4,format=rgba,"
                                 "colorchannelmixer=aa=0.85,fade=t=in:st=0:d=%5:alpha=1,"
                                 "fade=t=out:st=%5:d=%5:alpha=1,setpts=PTS+%6/TB[lk%7]")
                             .arg(width)
                             .arg(height)
                             .arg(fps, num(length), num(length / 2), num(off))
                             .arg(id);
                nodes << QString("[a%1][lk%2]overlay=eof_action=pass:repeatlast=0:format=auto[%1]")
                             .arg(joined, id);
            }
            accumulated = joined;
        }
        composite(QString("[%1]trim=start_frame=%2:end_frame=%3,setpts=PTS-STARTPTS")
                      .arg(accumulated)
                      .arg(visibleStart - gs)
                      .arg(visibleEnd - gs),
                  "x=0:y=0", visibleStart - from, visibleEnd - visibleStart);
    }
    for (int i = 0; i < clips.size(); ++i) {
        auto &n = info[i];
        if (!n.audio)
            continue;
        const auto &c = *n.clip;
        const qint64 v0 = std::max(from, c.start - n.aPre),
                     v1 = std::min(to, c.start + c.duration + n.aPost);
        if (v0 >= v1)
            continue;
        const double l0 = secs(v0 - c.start), l1 = secs(v1 - c.start);
        const auto [lo, hi] = available(n);
        const double t0 = std::max(l0, lo), t1 = std::min(l1, hi);
        if (t1 <= t0)
            continue; // Handles beyond the source are silent.
        const double s = c.speed.seconds(), d = secs(c.duration), k = secs(n.aPre);
        const int in =
            addInput(n, c.sourceIn.seconds() + (c.reverse ? d - t1 : t0) * s);
        QString a = QString("[%1:a:0]atrim=duration=%2,asetpts=PTS-STARTPTS")
                        .arg(in)
                        .arg(num((t1 - t0) * s));
        if (c.reverse)
            a += ",areverse";
        double tempo = s;
        while (tempo > 2) {
            a += ",atempo=2";
            tempo /= 2;
        }
        while (tempo < 0.5) {
            a += ",atempo=0.5";
            tempo *= 2;
        }
        a += ",atempo=" + num(tempo);
        a += ",aresample=48000";
        // The chipmunk and monster voices are a pitch shift on top of the clip's own.
        const double pitch = c.pitch + (c.voice == "chipmunk" ? 7 : c.voice == "monster" ? -7 : 0);
        if (pitch != 0) {
            // Played faster or slower at the same sample rate, which moves the pitch, then
            // brought back to the original tempo.
            const int rate = qRound(48000 * std::pow(2., pitch / 12));
            a += QString(",asetrate=%1,aresample=48000,atempo=%2").arg(rate).arg(num(48000. / rate));
        }
        if (c.voice == "robot")
            // Every frequency keeps its strength but loses its phase: a flat, buzzing voice,
            // brought back to about its former loudness.
            a += ",afftfilt=real='hypot(re,im)':imag='0':win_size=512:overlap=0.75,volume=6,"
                 "asoftclip=type=atan";
        else if (c.voice == "telephone")
            a += ",highpass=f=300:poles=2,highpass=f=300:poles=2,lowpass=f=3400:poles=2,"
                 "lowpass=f=3400:poles=2";
        else if (c.voice == "megaphone")
            a += ",highpass=f=500:poles=2,lowpass=f=4000:poles=2,volume=4,asoftclip=type=atan,"
                 "volume=0.5";
        else if (c.voice == "alien")
            a += ",vibrato=f=7:d=0.6,aphaser=type=t:speed=1.5:in_gain=0.9:out_gain=0.9";
        else if (c.voice == "monster")
            a += ",bass=g=6:f=120";
        a += ",aformat=sample_fmts=fltp:channel_layouts=stereo,asetpts=PTS+" + num(t0 + k) + "/TB";
        if (c.keyframes.contains("volume"))
            // Audio timestamps here are clip-local seconds plus the handle.
            a += QString(",volume=eval=frame:volume='%1'")
                     .arg(curve(c, "volume", QString("((t-%1)*%2/%3)")
                                                 .arg(num(k))
                                                 .arg(p.fpsN)
                                                 .arg(p.fpsD)));
        else
            a += ",volume=" + num(c.volume);
        if (c.keyframes.contains("pan")) {
            // Animated balance, evaluated per sample from the clip-local frame.
            const auto p0 = "(" + curve(c, "pan", QString("((t-%1)*%2/%3)").arg(num(k)).arg(p.fpsN).arg(p.fpsD)) + ")";
            a += QString(",aeval=exprs='val(0)*if(gt(%1,0),1-%1,1)|val(1)*if(lt(%1,0),1+%1,1)':c=same")
                     .arg(p0);
        } else if (c.pan != 0)
            // Balance: the far side gets quieter, the near side keeps its level.
            a += QString(",pan=stereo|c0=%1*c0|c1=%2*c1")
                     .arg(num(c.pan > 0 ? 1 - c.pan : 1), num(c.pan < 0 ? 1 + c.pan : 1));
        // Sound: clean-up first (low cut, noise reduction, gate), then tone, then dynamics.
        if (c.lowCut > 0)
            a += ",highpass=f=" + num(c.lowCut) + ":poles=2";
        if (c.denoise > 0)
            a += QString(",afftdn=nr=%1:nf=-50").arg(num(6 + 24 * c.denoise));
        if (c.gate > 0)
            // Opens above a threshold from −60 dB (gentle) to −30 dB (strong).
            a += QString(",agate=threshold=%1:ratio=4:attack=5:release=150:range=%2")
                     .arg(num(std::pow(10, (-60 + 30 * c.gate) / 20)), num(std::pow(10, -24 * c.gate / 20)));
        if (c.eqLow != 0)
            a += ",bass=g=" + num(c.eqLow) + ":f=100:w=0.7";
        if (c.eqMid != 0)
            a += ",equalizer=f=2500:t=q:w=1:g=" + num(c.eqMid);
        if (c.eqHigh != 0)
            a += ",treble=g=" + num(c.eqHigh) + ":f=8000:w=0.7";
        if (c.deess > 0)
            a += ",deesser=i=" + num(0.2 + 0.6 * c.deess) + ":m=0.5:f=0.5";
        if (c.compressor > 0)
            // Lower threshold and higher ratio with the amount; make-up gain restores level.
            a += QString(",acompressor=threshold=%1:ratio=%2:attack=10:release=200:makeup=%3")
                     .arg(num(std::pow(10, (-12 - 18 * c.compressor) / 20)), num(2 + 6 * c.compressor),
                          num(std::pow(10, 9 * c.compressor / 20)));
        if (c.reverb > 0)
            // A small room: several short reflections that die away.
            a += QString(",aecho=in_gain=1:out_gain=%1:delays=31|47|71|113:decays=%2|%3|%4|%5")
                     .arg(num(1 - 0.15 * c.reverb), num(0.5 * c.reverb), num(0.42 * c.reverb),
                          num(0.33 * c.reverb), num(0.25 * c.reverb));
        if (c.echo > 0)
            a += QString(",aecho=in_gain=1:out_gain=%1:delays=320|640:decays=%2|%3")
                     .arg(num(1 - 0.2 * c.echo), num(0.5 * c.echo), num(0.25 * c.echo));
        if (c.fadeIn > 0)
            a += ",afade=t=in:st=" + num(k) + ":d=" + num(std::min(c.fadeIn, d));
        if (c.fadeOut > 0) {
            auto fd = std::min(c.fadeOut, d);
            a += ",afade=t=out:st=" + num(k + d - fd) + ":d=" + num(fd);
        }
        // Equal-power crossfades across transitions.
        if (n.aPre > 0)
            a += ",afade=t=in:st=0:d=" + num(secs(n.inLength)) + ":curve=qsin";
        if (n.aPost > 0) {
            const qint64 before = n.outLength - n.aPost;
            a += ",afade=t=out:st=" + num(k + d - secs(before)) +
                 ":d=" + num(secs(n.outLength)) + ":curve=qsin";
        }
        const auto id = QString::number(serial++);
        a += QString(",asetpts=PTS-STARTPTS,apad,atrim=duration=%1,adelay=%2S:all=1[a%3]")
                 .arg(num(l1 - t0))
                 .arg(qRound64((secs(v0 - from) + t0 - l0) * 48000))
                 .arg(id);
        nodes << a;
        audioLabels << "[a" + id + "]";
    }
    const QString pace = !o.realtime ? QString()
                         : o.rate != 1 ? ",realtime=speed=" + num(o.rate)
                                       : QString(",realtime");
    QString tempo;
    for (double r = o.rate; r > 1.0001; r /= 2)
        tempo += ",atempo=" + num(std::min(2., r));
    if (o.video)
        nodes << QString("[%1]trim=end_frame=%2,setpts=PTS-STARTPTS,format=%3%4%5[vout]")
                     .arg(visual)
                     .arg(r.frames)
                     .arg(o.pixelFormat, pace, o.videoTail.isEmpty() ? QString() : "," + o.videoTail);
    if (audio) {
        // Mix, master gain, then a peak limiter so no gain can clip; or the loudness meter.
        QString master;
        if (o.gainDb != 0)
            master += ",volume=" + num(o.gainDb) + "dB";
        master += o.measureLoudness
                      ? QString(",ebur128=peak=true:framelog=quiet")
                      : QString(",alimiter=limit=%1:level=0:latency=1").arg(num(o.limit));
        nodes << audioLabels.join("") +
                     QString("amix=inputs=%1:duration=longest:normalize=0%2,atrim=end_sample=%3%4%5"
                             "[aout]")
                         .arg(audioLabels.size())
                         .arg(master)
                         .arg(qRound64(r.duration * 48000))
                         .arg(tempo)
                         .arg(o.realtime ? ",arealtime" : "");
    }
    r.graph = nodes.join(";\n");
    if (o.highQuality)
        r.graph = "sws_flags=lanczos+accurate_rnd+full_chroma_int;\n" + r.graph;
    return r;
}
QStringList renderArguments(const RenderPlan &r, const QString &graph, const QString &output,
                            const QString &profile, double seek) {
    QStringList a{"-hide_banner", "-nostdin", "-y", "-loglevel", "error"};
    // A single still is cheap; keep it from competing with playback or export for every core.
    if (seek >= 0)
        a << "-filter_complex_threads" << "1";
    a += r.inputs;
    a << "-filter_complex_script" << graph << "-map" << "[vout]";
    if (seek >= 0) {
        a << "-ss" << num(seek) << "-frames:v" << "1" << "-c:v" << "png" << "-f" << "image2pipe"
          << "pipe:1";
        return a;
    }
    a << "-map" << "[aout]" << "-frames:v" << QString::number(r.frames) << "-t" << num(r.duration);
    if (profile == "webm")
        a << "-c:v" << "libvpx-vp9" << "-crf" << "30" << "-b:v" << "0" << "-deadline" << "realtime"
          << "-cpu-used" << "6" << "-c:a" << "libopus" << "-b:a" << "160k";
    else if (profile == "h264")
        a << "-c:v" << "h264_mf" << "-b:v" << "12000k" << "-c:a" << "aac" << "-b:a" << "192k"
          << "-movflags" << "+faststart";
    else
        a << "-c:v" << "mpeg4" << "-q:v" << "3" << "-c:a" << "aac" << "-b:a" << "192k"
          << "-movflags" << "+faststart";
    a << "-pix_fmt" << "yuv420p" << "-progress" << "pipe:1" << output;
    return a;
}
QStringList exportArguments(const RenderPlan &r, const QString &graph, const QString &output,
                            const Encoder &e) {
    QStringList a{"-hide_banner", "-nostdin", "-y", "-loglevel", "error"};
    a += r.inputs;
    a << "-filter_complex_script" << graph;
    if (e.audioOnly)
        a << "-map" << "[aout]" << "-t" << num(r.duration) << "-vn";
    else {
        a << "-map" << "[vout]";
        if (!e.noAudio)
            a << "-map" << "[aout]";
        // Another frame rate: FFmpeg repeats or drops frames to reach it.
        const auto frames = e.frameRate.isEmpty()
                                ? r.frames
                                : qint64(std::ceil(r.duration * e.frameRateValue - 1e-6));
        if (!e.frameRate.isEmpty())
            a << "-r" << e.frameRate;
        a << "-frames:v" << QString::number(frames) << "-t" << num(r.duration);
        a += e.videoArguments;
        // A tail of filters (the GIF palette) already gives the encoder its pixel format.
        if (e.videoTail.isEmpty())
            a << "-pix_fmt" << e.pixelFormat;
    }
    if (e.noAudio)
        a << "-an";
    a += e.audioArguments;
    if (QStringList{"mp4", "mov", "m4a"}.contains(e.extension))
        a << "-movflags" << "+faststart";
    a << "-progress" << "pipe:1" << output;
    return a;
}
double parseIntegratedLoudness(const QString &log) {
    // The summary comes last: "Integrated loudness:\n    I:  -16.2 LUFS".
    static const QRegularExpression integrated("Integrated loudness:\\s*I:\\s*(-?[0-9.]+|-inf)\\s*LUFS");
    double value = std::numeric_limits<double>::quiet_NaN();
    for (auto it = integrated.globalMatch(log); it.hasNext();) {
        const auto m = it.next().captured(1);
        value = m == "-inf" ? -std::numeric_limits<double>::infinity() : m.toDouble();
    }
    return value;
}
double parseTruePeak(const QString &log) {
    static const QRegularExpression peak("True peak:\\s*Peak:\\s*(-?[0-9.]+|-inf)\\s*dBFS");
    double value = std::numeric_limits<double>::quiet_NaN();
    for (auto it = peak.globalMatch(log); it.hasNext();) {
        const auto m = it.next().captured(1);
        value = m == "-inf" ? -std::numeric_limits<double>::infinity() : m.toDouble();
    }
    return value;
}
QStringList measureArguments(const RenderPlan &r, const QString &graph) {
    QStringList a{"-hide_banner", "-nostdin", "-loglevel", "info", "-nostats"};
    a += r.inputs;
    a << "-filter_complex_script" << graph << "-map" << "[aout]" << "-t" << num(r.duration)
      << "-progress" << "pipe:1" << "-f" << "null" << "-";
    return a;
}
QStringList streamArguments(const RenderPlan &r, const QString &graph, bool video,
                            bool floatAudio) {
    QStringList a{"-hide_banner", "-nostdin", "-loglevel", "error"};
    a += r.inputs;
    a << "-filter_complex_script" << graph;
    if (video)
        a << "-map" << "[vout]" << "-frames:v" << QString::number(r.frames) << "-f" << "rawvideo"
          << "-pix_fmt" << "yuv420p";
    else
        a << "-map" << "[aout]" << "-f" << (floatAudio ? "f32le" : "s16le") << "-ar" << "48000" << "-ac" << "2";
    a << "pipe:1";
    return a;
}
} // namespace cutlery
