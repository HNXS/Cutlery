#include "Thumbnails.h"
#include "MediaAnalysis.h"
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace cutlery {
namespace {
constexpr int tileHeight = 54, maxTiles = 200;
QString num(double v) {
    return QString::number(v, 'f', 9);
}
} // namespace
Thumbnails::Layout Thumbnails::layout(const Asset &a) {
    Layout l;
    const double aspect = a.width > 0 && a.height > 0 ? double(a.width) / a.height : 16. / 9;
    l.tile = {std::clamp(int(std::lround(tileHeight * aspect / 2)) * 2, 30, 128), tileHeight};
    if (a.kind == "image") {
        l.count = 1;
        l.interval = std::max(a.duration, 1.);
    } else {
        // One tile per second, spread wider for long media so a strip stays small.
        l.interval = std::max(1., a.duration / maxTiles);
        l.count = std::clamp(int(std::ceil(a.duration / l.interval)), 1, maxTiles);
    }
    return l;
}
Thumbnails::Thumbnails(QString cache, QString ffmpeg, QObject *parent)
    : QObject(parent), m_cache(std::move(cache)), m_ffmpeg(std::move(ffmpeg)) {
    QDir().mkpath(m_cache);
}
Thumbnails::~Thumbnails() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1500);
    }
}
QString Thumbnails::path(const QString &key) const {
    return m_cache + '/' + key + ".jpg";
}
void Thumbnails::setAssets(const QVector<Asset> &assets) {
    QHash<QString, Asset> nextAssets;
    bool updated = false;
    for (const auto &a : assets) {
        if (a.kind != "video" && a.kind != "image")
            continue;
        nextAssets.insert(a.id, a);
        const auto key = MediaAnalysis::fingerprint(a) + "-thumb-v1";
        if (m_strips.contains(a.id) && m_strips.value(a.id).key == key)
            continue;
        updated = true;
        m_queue.removeAll(a.id);
        if (QFileInfo(path(key)).isFile())
            m_strips[a.id] = {key, "ready"};
        else {
            m_strips[a.id] = {key, "queued"};
            m_queue.push_back(a.id);
        }
    }
    for (const auto &id : m_strips.keys())
        if (!nextAssets.contains(id)) {
            m_strips.remove(id);
            m_queue.removeAll(id);
            updated = true;
        }
    m_assets = std::move(nextAssets);
    if (updated)
        emit changed();
    next();
}
QVariantMap Thumbnails::strip(const QString &id) const {
    const auto s = m_strips.value(id);
    if (s.status != "ready")
        return {{"status", s.status.isEmpty() ? "none" : s.status}};
    const auto l = layout(m_assets.value(id));
    return {{"status", s.status},
            {"url", QUrl::fromLocalFile(path(s.key)).toString()},
            {"count", l.count},
            {"interval", l.interval},
            {"tileWidth", l.tile.width()},
            {"tileHeight", l.tile.height()}};
}
void Thumbnails::setPaused(bool paused) {
    m_paused = paused;
    if (!paused)
        next();
}
void Thumbnails::next() {
    if (m_process || m_paused)
        return;
    while (!m_queue.empty() && !m_assets.contains(m_queue.first()))
        m_queue.removeFirst();
    if (m_queue.empty())
        return;
    const auto id = m_queue.takeFirst();
    const auto a = m_assets.value(id);
    const auto key = m_strips.value(id).key;
    if (a.duration <= 0 || a.duration > 86400 || !QFileInfo(a.path).isFile() ||
        m_ffmpeg.isEmpty()) {
        m_strips[id].status = "unavailable";
        emit changed();
        QTimer::singleShot(0, this, &Thumbnails::next);
        return;
    }
    const auto l = layout(a);
    const int w = l.tile.width(), h = l.tile.height();
    auto data = std::make_shared<QByteArray>();
    auto *p = new QProcess(this);
    m_process = p;
    m_strips[id].status = "reading";
    connect(p, &QProcess::readyReadStandardOutput, this, [p, data, l, w, h] {
        *data += p->readAllStandardOutput();
        // Bound memory against misbehaving decoders; the frame count is also capped below.
        if (data->size() > qsizetype(l.count) * w * h * 3)
            data->truncate(qsizetype(l.count) * w * h * 3);
    });
    connect(p, &QProcess::readyReadStandardError, this, [p] { p->readAllStandardError(); });
    auto complete = [this, p, data, id, key, l, w, h](bool success) {
        *data += p->readAllStandardOutput();
        m_process = nullptr;
        p->deleteLater();
        const qsizetype frameBytes = qsizetype(w) * h * 3;
        const int frames = int(std::min<qsizetype>(data->size() / frameBytes, l.count));
        if (m_strips.contains(id) && m_strips.value(id).key == key) {
            bool saved = false;
            if (success && frames > 0) {
                // Tiles missing at the end (short final GOP) repeat the last decoded frame.
                QImage strip(w * l.count, h, QImage::Format_RGB888);
                for (int i = 0; i < l.count; ++i) {
                    const auto *tile = data->constData() + std::min(i, frames - 1) * frameBytes;
                    for (int y = 0; y < h; ++y)
                        std::memcpy(strip.scanLine(y) + i * w * 3, tile + y * w * 3, size_t(w) * 3);
                }
                QSaveFile file(path(key));
                if (file.open(QIODevice::WriteOnly)) {
                    QImageWriter writer(&file, "jpg");
                    writer.setQuality(82);
                    saved = writer.write(strip) && file.commit();
                }
            }
            m_strips[id].status = saved ? "ready" : "unavailable";
            emit changed();
        }
        QTimer::singleShot(0, this, &Thumbnails::next);
    };
    connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            complete(false);
    });
    QTimer::singleShot(120000, p, [p] {
        if (p->state() != QProcess::NotRunning)
            p->kill();
    });
    QStringList args{"-hide_banner", "-nostdin", "-v", "error", "-threads", "2"};
    // Keyframes are enough for a thumbnail and make extraction many times faster than decoding
    // every frame. Images have only one frame anyway.
    if (a.kind == "video")
        args << "-skip_frame" << "nokey";
    args << "-protocol_whitelist" << "file,pipe" << "-i" << a.path << "-map" << "0:v:0" << "-an"
         << "-vf"
         << QString("fps=%1:round=down:eof_action=pass,scale=%2:%3:force_original_aspect_ratio=decrease,"
                    "pad=%2:%3:(ow-iw)/2:(oh-ih)/2,format=rgb24")
                .arg(num(1 / l.interval))
                .arg(w)
                .arg(h)
         << "-frames:v" << QString::number(l.count) << "-f" << "rawvideo" << "pipe:1";
    p->start(m_ffmpeg, args);
    emit changed();
}
} // namespace cutlery
