#include "MediaAnalysis.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <QtEndian>
#include <algorithm>
#include <memory>

namespace cutlery {
PeakAccumulator::PeakAccumulator(qint64 samplesPerPeak)
    : m_binSamples(std::max(qint64(1), samplesPerPeak)) {}
void PeakAccumulator::append(const QByteArray &bytes) {
    QByteArray data = m_tail + bytes;
    const auto even = data.size() / 2 * 2;
    for (qsizetype i = 0; i < even; i += 2) {
        const auto sample =
            qFromLittleEndian<qint16>(reinterpret_cast<const uchar *>(data.constData() + i));
        m_peak = std::max(m_peak, float(std::abs(int(sample))) / 32768.f);
        if (++m_count == m_binSamples) {
            if (m_peaks.size() < 6001)
                m_peaks.push_back(m_peak);
            m_count = 0;
            m_peak = 0;
        }
    }
    m_tail = data.mid(even);
}
QVector<float> PeakAccumulator::finish() {
    if (m_count && m_peaks.size() < 6001)
        m_peaks.push_back(m_peak);
    m_count = 0;
    return m_peaks;
}
MediaAnalysis::MediaAnalysis(QString cache, QString ffmpeg, QObject *parent)
    : QObject(parent), m_cache(std::move(cache)), m_ffmpeg(std::move(ffmpeg)) {
    QDir().mkpath(m_cache);
}
MediaAnalysis::~MediaAnalysis() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1500);
    }
}
QString MediaAnalysis::fingerprint(const Asset &a) {
    const QFileInfo f(a.path);
    const auto data = f.canonicalFilePath().toUtf8() + '\0' + QByteArray::number(f.size()) + '\0' +
                      QByteArray::number(f.lastModified().toMSecsSinceEpoch()) + '\0' +
                      QByteArray::number(a.duration, 'g', 17) + "peak-v1";
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
bool MediaAnalysis::load(const QString &id, const QString &key) {
    QFile f(m_cache + '/' + key + ".json");
    if (!f.open(QIODevice::ReadOnly) || f.size() > 100000)
        return false;
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
    const auto a = o["peaks"].toArray();
    const auto step = o["step"].toDouble();
    if (o["version"].toInt() != 1 || a.isEmpty() || a.size() > 6001 || !std::isfinite(step) ||
        step <= 0)
        return false;
    QVector<float> peaks;
    for (const auto &v : a) {
        const auto n = v.toDouble(-1);
        if (!std::isfinite(n) || n < 0 || n > 1)
            return false;
        peaks.push_back(float(n));
    }
    m_waves[id] = {key, "ready", peaks, step};
    return true;
}
void MediaAnalysis::save(const QString &id) {
    const auto w = m_waves.value(id);
    QJsonArray peaks;
    for (float n : w.peaks)
        peaks.append(n);
    QSaveFile f(m_cache + '/' + w.key + ".json");
    if (!f.open(QIODevice::WriteOnly))
        return;
    const auto data = QJsonDocument(QJsonObject{{"version", 1}, {"step", w.step}, {"peaks", peaks}})
                          .toJson(QJsonDocument::Compact);
    if (f.write(data) == data.size())
        f.commit();
}
void MediaAnalysis::setAssets(const QVector<Asset> &assets) {
    QHash<QString, Asset> nextAssets;
    bool updated = false;
    for (const auto &a : assets) {
        if (!a.hasAudio)
            continue;
        nextAssets.insert(a.id, a);
        const auto key = fingerprint(a);
        if (m_waves.contains(a.id) && m_waves.value(a.id).key == key)
            continue;
        updated = true;
        m_queue.removeAll(a.id);
        if (!load(a.id, key)) {
            m_waves[a.id] = {key, "queued", {}, 0};
            m_queue.push_back(a.id);
        }
    }
    for (const auto &id : m_waves.keys())
        if (!nextAssets.contains(id)) {
            m_waves.remove(id);
            m_queue.removeAll(id);
            updated = true;
        }
    m_assets = std::move(nextAssets);
    if (updated)
        emit changed();
    next();
}
QVariantMap MediaAnalysis::waveform(const QString &id) const {
    const auto w = m_waves.value(id);
    QVariantList peaks;
    peaks.reserve(w.peaks.size());
    for (float n : w.peaks)
        peaks.push_back(n);
    return {{"status", w.status}, {"step", w.step}, {"peaks", peaks}};
}
void MediaAnalysis::setPaused(bool paused) {
    m_paused = paused;
    if (!paused)
        next();
}
void MediaAnalysis::next() {
    if (m_process || m_paused)
        return;
    while (!m_queue.empty() && !m_assets.contains(m_queue.first()))
        m_queue.removeFirst();
    if (m_queue.empty())
        return;
    const auto id = m_queue.takeFirst();
    const auto a = m_assets.value(id);
    const auto key = m_waves.value(id).key;
    if (a.duration <= 0 || a.duration > 86400 || !QFileInfo(a.path).isFile() ||
        m_ffmpeg.isEmpty()) {
        m_waves[id].status = "unavailable";
        emit changed();
        QTimer::singleShot(0, this, &MediaAnalysis::next);
        return;
    }
    const qint64 bin = std::max(qint64(80), qint64(std::ceil(a.duration * 8000 / 6000)));
    auto peaks = std::make_shared<PeakAccumulator>(bin);
    auto *p = new QProcess(this);
    m_process = p;
    m_current = id;
    m_waves[id].status = "reading";
    connect(p, &QProcess::readyReadStandardOutput, this,
            [p, peaks] { peaks->append(p->readAllStandardOutput()); });
    connect(p, &QProcess::readyReadStandardError, this, [p] { p->readAllStandardError(); });
    auto complete = [this, p, peaks, id, key, bin](bool success) {
        peaks->append(p->readAllStandardOutput());
        m_process = nullptr;
        m_current.clear();
        p->deleteLater();
        if (m_waves.contains(id) && m_waves.value(id).key == key) {
            auto data = peaks->finish();
            m_waves[id] = {key, success && !data.empty() ? "ready" : "unavailable",
                           success ? data : QVector<float>{}, double(bin) / 8000};
            if (success && !data.empty())
                save(id);
            emit changed();
        }
        QTimer::singleShot(0, this, &MediaAnalysis::next);
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
    p->start(m_ffmpeg, {"-hide_banner", "-nostdin", "-v", "error", "-threads", "1",
                        "-protocol_whitelist", "file,pipe", "-i", a.path, "-vn", "-ac", "1", "-ar",
                        "8000", "-f", "s16le", "pipe:1"});
    emit changed();
}
} // namespace cutlery
