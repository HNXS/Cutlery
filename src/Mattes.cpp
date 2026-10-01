#include "Mattes.h"
#include "MediaAnalysis.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

namespace cutlery {
Mattes::Mattes(QString dir, QString ffmpeg, QString worker, QString model, QObject *parent)
    : QObject(parent), m_dir(std::move(dir)), m_ffmpeg(std::move(ffmpeg)),
      m_worker(std::move(worker)), m_model(std::move(model)) {}
Mattes::~Mattes() {
    cancel();
}
QString Mattes::missing() const {
    if (m_worker.isEmpty() || !QFileInfo(m_worker).isExecutable())
        return "The AI pack is not installed: cutlery-matte is missing.";
    if (m_model.isEmpty() || !QFileInfo(m_model).isFile())
        return "The AI pack is not installed: models/u2net_human_seg.onnx is missing.";
    if (m_ffmpeg.isEmpty())
        return "FFmpeg is missing.";
    return {};
}
bool Mattes::available() const {
    return missing().isEmpty();
}
QString Mattes::key(const Asset &a) const {
    return MediaAnalysis::fingerprint(a) + "-matte-v1";
}
MatteSource Mattes::matte(const Asset &a) const {
    const auto base = QDir(m_dir).filePath(key(a));
    QFile meta(base + ".json");
    if (!QFileInfo(base + ".mkv").isFile() || !meta.open(QIODevice::ReadOnly))
        return {};
    const auto o = QJsonDocument::fromJson(meta.readAll()).object();
    MatteSource m;
    m.path = base + ".mkv";
    m.start = o["start"].toDouble();
    m.end = o["end"].toDouble();
    m.rate = o["rate"].toDouble(rate);
    if (m.rate <= 0 || m.end <= m.start)
        return {};
    return m;
}
QVariantMap Mattes::status(const Asset &a) const {
    const auto k = key(a);
    if (m_process && m_key == k)
        return {{"status", "analyzing"}, {"progress", m_progress}};
    if (m_errorKey == k)
        return {{"status", "failed"}, {"error", m_error}};
    const auto m = matte(a);
    if (m.path.isEmpty())
        return {{"status", "none"}};
    return {{"status", "ready"}, {"start", m.start}, {"end", m.end}};
}
void Mattes::cancel() {
    if (!m_process)
        return;
    m_process->disconnect(this);
    m_process->kill();
    m_process->waitForFinished(3000);
    m_process->deleteLater();
    m_process = nullptr;
    QFile::remove(QDir(m_dir).filePath(m_key + ".part.mkv"));
    m_key.clear();
    emit changed();
}
void Mattes::analyze(const Asset &a, double start, double end) {
    cancel();
    if (!available() || a.kind != "video" || a.width <= 0 || a.height <= 0)
        return;
    QDir().mkpath(m_dir);
    start = std::clamp(start, 0., a.duration);
    end = std::clamp(end, start, a.duration);
    if (end - start < 1 / rate)
        return;
    m_key = key(a);
    m_errorKey.clear();
    m_progress = 0;
    const auto base = QDir(m_dir).filePath(m_key);
    const auto part = base + ".part.mkv";
    // About 640 pixels on the long side: enough for soft edges, small on disk.
    const double fit = 640. / std::max(a.width, a.height);
    const int w = std::max(2, int(std::lround(a.width * std::min(1., fit) / 2)) * 2),
              h = std::max(2, int(std::lround(a.height * std::min(1., fit) / 2)) * 2);
    auto *p = new QProcess(this);
    m_process = p;
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p] {
        while (p->canReadLine()) {
            const auto parts = QString::fromUtf8(p->readLine()).trimmed().split(' ');
            if (parts.size() == 3 && parts[0] == "progress" && parts[2].toDouble() > 0) {
                m_progress = std::clamp(parts[1].toDouble() / parts[2].toDouble(), 0., 1.);
                emit changed();
            }
        }
    });
    auto complete = [this, p, base, part, start, end](bool success) {
        const auto log = QString::fromUtf8(p->readAllStandardError()).trimmed();
        m_process = nullptr;
        p->deleteLater();
        if (success) {
            QFile::remove(base + ".mkv");
            QSaveFile meta(base + ".json");
            success = QFile::rename(part, base + ".mkv") && meta.open(QIODevice::WriteOnly);
            if (success) {
                meta.write(QJsonDocument(QJsonObject{{"start", start},
                                                     {"end", end},
                                                     {"rate", rate},
                                                     {"model", QFileInfo(m_model).fileName()}})
                               .toJson());
                success = meta.commit();
            }
        }
        if (!success) {
            QFile::remove(part);
            m_errorKey = m_key;
            m_error = log.isEmpty() ? "AI analysis failed" : log.section('\n', -3);
        }
        m_key.clear();
        emit changed();
    };
    connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            complete(false);
    });
    p->start(m_worker, {"--ffmpeg", m_ffmpeg, "--model", m_model, "--input", a.path, "--output",
                        part, "--size", QString("%1x%2").arg(w).arg(h), "--start",
                        QString::number(start, 'f', 6), "--duration",
                        QString::number(end - start, 'f', 6), "--rate",
                        QString::number(rate)});
    emit changed();
}
} // namespace cutlery
