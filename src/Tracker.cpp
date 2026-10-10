#include "Tracker.h"

#include <algorithm>
#include <cmath>

namespace cutlery {
namespace {
// Half-size copy of a grey frame (2 × 2 means).
QVector<uchar> halve(const uchar *frame, int width, int height) {
    const int w = width / 2, h = height / 2;
    QVector<uchar> out(w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uchar *p = frame + 2 * y * width + 2 * x;
            out[y * w + x] = uchar((p[0] + p[1] + p[width] + p[width + 1] + 2) / 4);
        }
    return out;
}
} // namespace

void PatchTracker::setTemplate(const QVector<float> &patch) {
    double mean = 0;
    for (float v : patch)
        mean += v;
    mean /= patch.size();
    m_template.resize(patch.size());
    double norm = 0;
    for (int i = 0; i < patch.size(); ++i) {
        m_template[i] = float(patch[i] - mean);
        norm += double(m_template[i]) * m_template[i];
    }
    m_templateNorm = std::sqrt(norm);
}
QVector<float> PatchTracker::sample(const uchar *frame, QPointF centre) const {
    QVector<float> out(m_tw * m_th);
    const double x0 = centre.x() - (m_tw - 1) / 2., y0 = centre.y() - (m_th - 1) / 2.;
    for (int j = 0; j < m_th; ++j) {
        const double y = std::clamp(y0 + j, 0., m_height - 1.);
        const int yi = std::min(int(y), m_height - 2);
        const double fy = y - yi;
        for (int i = 0; i < m_tw; ++i) {
            const double x = std::clamp(x0 + i, 0., m_width - 1.);
            const int xi = std::min(int(x), m_width - 2);
            const double fx = x - xi;
            const uchar *p = frame + yi * m_width + xi;
            out[j * m_tw + i] = float((p[0] * (1 - fx) + p[1] * fx) * (1 - fy) +
                                      (p[m_width] * (1 - fx) + p[m_width + 1] * fx) * fy);
        }
    }
    return out;
}
double PatchTracker::match(const uchar *frame, int stride, int width, int height,
                           const float *tmpl, int tw, int th, double tnorm, int x, int y) const {
    if (x < 0 || y < 0 || x + tw > width || y + th > height || tnorm <= 0)
        return -2;
    double sum = 0, sum2 = 0, cross = 0;
    for (int j = 0; j < th; ++j) {
        const uchar *row = frame + (y + j) * stride + x;
        const float *t = tmpl + j * tw;
        for (int i = 0; i < tw; ++i) {
            const double v = row[i];
            sum += v;
            sum2 += v * v;
            cross += v * t[i];
        }
    }
    const double variance = sum2 - sum * sum / (tw * th);
    return variance < 1e-3 ? 0 : cross / (std::sqrt(variance) * tnorm);
}
bool PatchTracker::start(const uchar *frame, int width, int height, QRectF patch) {
    m_width = width;
    m_height = height;
    if (width < 16 || height < 16)
        return false;
    m_tw = std::clamp(int(std::lround(patch.width())), 12, std::min(64, width - 2));
    m_th = std::clamp(int(std::lround(patch.height())), 12, std::min(64, height - 2));
    m_position = {std::clamp(patch.center().x(), 0., width - 1.),
                  std::clamp(patch.center().y(), 0., height - 1.)};
    m_velocity = {};
    m_score = 1;
    const auto first = sample(frame, m_position);
    setTemplate(first);
    // Too even to find again (a wall, the sky): a standard deviation under 3 grey levels.
    return m_templateNorm / std::sqrt(double(m_tw * m_th)) >= 3;
}
bool PatchTracker::next(const uchar *frame) {
    const QPointF predicted = m_position + m_velocity;
    const double speed = std::hypot(m_velocity.x(), m_velocity.y());
    const int radius = int(std::clamp(std::max(m_tw, m_th) * 0.75 + speed * 0.5, 12., 48.));
    // Coarse: half size, template averaged the same way.
    const int hw = m_width / 2, hh = m_height / 2, htw = m_tw / 2, hth = m_th / 2;
    const auto half = halve(frame, m_width, m_height);
    QVector<float> halfTemplate(htw * hth);
    for (int j = 0; j < hth; ++j)
        for (int i = 0; i < htw; ++i) {
            const float *t = m_template.constData() + 2 * j * m_tw + 2 * i;
            halfTemplate[j * htw + i] = (t[0] + t[1] + t[m_tw] + t[m_tw + 1]) / 4;
        }
    double mean = 0, norm = 0;
    for (float v : halfTemplate)
        mean += v;
    mean /= halfTemplate.size();
    for (auto &v : halfTemplate) {
        v -= float(mean);
        norm += double(v) * v;
    }
    norm = std::sqrt(norm);
    // Top-left corner of the template for a centre: centre − (size − 1) / 2.
    const int cx = int(std::lround((predicted.x() - (m_tw - 1) / 2.) / 2)),
              cy = int(std::lround((predicted.y() - (m_th - 1) / 2.) / 2)), r = radius / 2;
    double best = -2;
    int bx = 0, by = 0;
    for (int y = cy - r; y <= cy + r; ++y)
        for (int x = cx - r; x <= cx + r; ++x) {
            const double s = match(half.constData(), hw, hw, hh, halfTemplate.constData(), htw,
                                   hth, norm, x, y);
            // Ties go to the smaller move.
            if (s > best + 1e-9) {
                best = s;
                bx = x;
                by = y;
            }
        }
    if (best < -1) {
        m_score = 0;
        return false;
    }
    // Fine: full size within two pixels, then a parabola through the neighbours.
    constexpr int fine = 3;
    double scores[2 * fine + 1][2 * fine + 1];
    double top = -2;
    int fx = 0, fy = 0;
    for (int j = -fine; j <= fine; ++j)
        for (int i = -fine; i <= fine; ++i) {
            const double s = match(frame, m_width, m_width, m_height, m_template.constData(), m_tw,
                                   m_th, m_templateNorm, 2 * bx + i, 2 * by + j);
            scores[j + fine][i + fine] = s;
            if (s > top) {
                top = s;
                fx = i;
                fy = j;
            }
        }
    m_score = std::max(0., top);
    if (top < 0.5)
        return false;
    auto offset = [](double a, double b, double c) {
        const double d = a - 2 * b + c;
        return a < -1 || c < -1 || d >= 0 ? 0. : std::clamp(0.5 * (a - c) / d, -0.5, 0.5);
    };
    const int si = fx + fine, sj = fy + fine;
    const double ox = si > 0 && si < 2 * fine
                          ? offset(scores[sj][si - 1], scores[sj][si], scores[sj][si + 1])
                          : 0,
                 oy = sj > 0 && sj < 2 * fine
                          ? offset(scores[sj - 1][si], scores[sj][si], scores[sj + 1][si])
                          : 0;
    const QPointF found(2 * bx + fx + ox + (m_tw - 1) / 2., 2 * by + fy + oy + (m_th - 1) / 2.);
    m_velocity = 0.7 * (found - m_position) + 0.3 * m_velocity;
    m_position = found;
    // A good match teaches the template a little of how the patch looks now.
    if (top > 0.75) {
        const auto now = sample(frame, m_position);
        double mean = 0;
        for (float v : now)
            mean += v;
        mean /= now.size();
        QVector<float> blended(now.size());
        for (int i = 0; i < now.size(); ++i)
            blended[i] = 0.9f * m_template[i] + 0.1f * float(now[i] - mean);
        setTemplate(blended);
    }
    return true;
}

Tracker::Tracker(QString ffmpeg, QObject *parent) : QObject(parent), m_ffmpeg(std::move(ffmpeg)) {}
Tracker::~Tracker() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}
double Tracker::progress() const {
    const qint64 total = m_k1 - m_k0 + 1;
    return m_running && total > 0 ? std::clamp(double(m_done) / total, 0., 1.) : 0.;
}
void Tracker::start(const Request &request) {
    cancel();
    m_request = request;
    m_points.clear();
    m_lost = false;
    m_backward = false;
    m_done = 0;
    m_firstFrame.clear();
    const double aspect = request.width > 0 && request.height > 0
                              ? double(request.height) / request.width
                              : 9. / 16;
    m_aw = std::clamp(request.width > 0 ? request.width / 2 * 2 : 320, 32, 320);
    m_ah = std::max(32, int(std::lround(m_aw * aspect / 2)) * 2);
    m_k1 = std::max<qint64>(0, qint64(std::floor((request.to - request.start) * request.rate)));
    m_k0 = -std::max<qint64>(0, qint64(std::floor((request.start - request.from) * request.rate)));
    m_running = true;
    runChunk(0, m_k1 + 1);
}
void Tracker::cancel() {
    if (!m_running)
        return;
    m_running = false;
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1000);
        m_process->deleteLater();
    }
}
void Tracker::runChunk(qint64 first, qint64 count) {
    m_chunkFirst = first;
    m_chunkCount = count;
    m_buffer.clear();
    m_chunkFrames.clear();
    auto *p = new QProcess(this);
    m_process = p;
    const double at = std::max(0., m_request.start + first / m_request.rate);
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p] {
        m_buffer += p->readAllStandardOutput();
        const qint64 size = qint64(m_aw) * m_ah;
        while (m_running && m_process == p && m_buffer.size() >= size) {
            const auto data = m_buffer.left(size);
            m_buffer.remove(0, size);
            if (m_backward) {
                m_chunkFrames << data;
            } else {
                frame(m_done, data);
                // Lost going forwards: no need to read further.
                if (m_lost && m_process == p) {
                    p->kill();
                    break;
                }
            }
        }
    });
    connect(p, &QProcess::readyReadStandardError, this, [p] { p->readAllStandardError(); });
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus status) {
        if (m_process != p)
            return;
        p->deleteLater();
        m_process = nullptr;
        chunkDone(code == 0 && status == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_process != p)
            return;
        p->deleteLater();
        m_process = nullptr;
        finish("FFmpeg could not be started");
    });
    p->start(m_ffmpeg,
             {"-hide_banner", "-nostdin", "-v", "error", "-ss", QString::number(at, 'f', 4), "-i",
              m_request.path, "-an", "-sn", "-dn", "-frames:v", QString::number(count), "-vf",
              QString("fps=%1,scale=%2:%3:flags=area,format=gray")
                  .arg(QString::number(m_request.rate, 'f', 6))
                  .arg(m_aw)
                  .arg(m_ah),
              "-f", "rawvideo", "pipe:1"});
}
void Tracker::frame(qint64 k, const QByteArray &data) {
    const auto *pixels = reinterpret_cast<const uchar *>(data.constData());
    ++m_done;
    if (k == 0 && !m_backward) {
        m_firstFrame = data;
        const auto &r = m_request.patch;
        if (!m_patch.start(pixels, m_aw, m_ah,
                           {r.x() * m_aw - 0.5, r.y() * m_ah - 0.5, r.width() * m_aw, r.height() * m_ah})) {
            finish("There is too little detail there to follow: place the area over something "
                   "with contrast");
            return;
        }
        m_points << Point{m_request.start, (m_patch.position().x() + 0.5) / m_aw,
                          (m_patch.position().y() + 0.5) / m_ah, 1};
    } else if (!m_lost) {
        if (m_patch.next(pixels))
            m_points << Point{m_request.start + k / m_request.rate,
                              (m_patch.position().x() + 0.5) / m_aw, (m_patch.position().y() + 0.5) / m_ah,
                              m_patch.score()};
        else
            m_lost = true;
    }
    if (m_running && m_done % 8 == 0)
        emit progressChanged();
}
void Tracker::chunkDone(bool ok) {
    if (!m_running)
        return;
    if (!m_backward) {
        if (m_firstFrame.isEmpty())
            return finish(ok ? "The video has no picture there" : "Could not read the video");
        // Backwards from the start, in chunks decoded forwards and followed in reverse.
        m_backward = true;
        m_lost = false;
        const auto &r = m_request.patch;
        m_patch.start(reinterpret_cast<const uchar *>(m_firstFrame.constData()), m_aw, m_ah,
                      {r.x() * m_aw - 0.5, r.y() * m_ah - 0.5, r.width() * m_aw, r.height() * m_ah});
        m_done = m_k1 + 1;
        if (m_k0 >= 0)
            return finish({});
        const qint64 first = std::max(m_k0, qint64(-50));
        return runChunk(first, -first);
    }
    // Follow this chunk's frames from its end back to its first.
    for (qint64 i = m_chunkCount - 1; i >= 0 && !m_lost && m_running; --i) {
        if (i < m_chunkFrames.size())
            frame(m_chunkFirst + i, m_chunkFrames[i]);
        else
            ++m_done;
    }
    if (!m_running)
        return;
    if (m_lost || m_chunkFirst <= m_k0)
        return finish({});
    const qint64 first = std::max(m_k0, m_chunkFirst - 50);
    runChunk(first, m_chunkFirst - first);
}
void Tracker::finish(const QString &error) {
    if (!m_running)
        return;
    m_running = false;
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1000);
        m_process->deleteLater();
    }
    std::sort(m_points.begin(), m_points.end(),
              [](const Point &a, const Point &b) { return a.time < b.time; });
    emit finished(error.isEmpty() ? m_points : QVector<Point>{}, error);
}
} // namespace cutlery
