#include "AiJobs.h"
#include "MediaAnalysis.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <memory>

namespace cutlery {
namespace {
QString extension(const QString &task) {
    return task == "upscale" || task == "eyecontact" ? ".mov"
           : task == "transcribe"                    ? ".srt"
                                                     : ".mkv";
}
} // namespace
AiJobs::AiJobs(QString dir, QString ffmpeg, QString ffprobe, QString worker,
               QHash<QString, QString> files, QObject *parent)
    : QObject(parent), m_dir(std::move(dir)), m_ffmpeg(std::move(ffmpeg)),
      m_ffprobe(std::move(ffprobe)), m_worker(std::move(worker)), m_files(std::move(files)) {}
AiJobs::~AiJobs() {
    cancel();
}
QString AiJobs::missing(const QString &task) const {
    if (m_worker.isEmpty() || !QFileInfo(m_worker).isExecutable())
        return "The AI worker (cutlery-ai) is missing from this build.";
    if (task == "transcribe" && !QFileInfo(m_files.value("whisper")).isExecutable())
        return "Speech recognition (whisper-cli) is missing from this build.";
    const auto model = m_files.value(task);
    if (model.isEmpty() || !QFileInfo(model).isFile())
        return "The AI pack is not installed: models/" +
               QFileInfo(model.isEmpty() ? task : model).fileName() + " is missing.";
    // Eye contact uses three face models side by side.
    if (task == "eyecontact")
        for (const auto *name : {"face_detection_short_range.onnx", "iris_landmark.onnx"})
            if (!QFileInfo(QFileInfo(model).dir().filePath(name)).isFile())
                return QString("The AI pack is incomplete: models/%1 is missing.").arg(name);
    if (m_ffmpeg.isEmpty() || ((task == "upscale" || task == "eyecontact") && m_ffprobe.isEmpty()))
        return "FFmpeg is missing.";
    return {};
}
QSize AiJobs::upscaleSize(const Asset &a, int height) {
    const double aspect = a.width > 0 && a.height > 0 ? double(a.width) / a.height : 16. / 9;
    return {std::max(2, int(std::lround(height * aspect / 2)) * 2), std::max(2, height / 2 * 2)};
}
QString AiJobs::key(const QString &task, const Asset &a, const QString &variant) const {
    // Version 2 transcripts have one cue per word.
    return MediaAnalysis::fingerprint(a) + "-" + task + (task == "transcribe" ? "-v2" : "-v1") +
           (variant.isEmpty() ? QString() : "-" + variant);
}
MatteSource AiJobs::result(const QString &task, const Asset &a, const QString &variant) const {
    const auto base = QDir(m_dir).filePath(key(task, a, variant));
    QFile meta(base + ".json");
    if (!QFileInfo(base + extension(task)).isFile() || !meta.open(QIODevice::ReadOnly))
        return {};
    const auto o = QJsonDocument::fromJson(meta.readAll()).object();
    MatteSource m;
    m.path = base + extension(task);
    m.start = o["start"].toDouble();
    m.end = o["end"].toDouble();
    m.rate = o["rate"].toDouble(matteRate);
    if (m.rate <= 0 || m.end <= m.start)
        return {};
    return m;
}
QVariantMap AiJobs::status(const QString &task, const Asset &a, const QString &variant) const {
    const auto k = key(task, a, variant);
    if (m_process && m_job.key == k)
        return {{"status", "running"}, {"progress", m_progress}, {"device", m_device}};
    if (std::any_of(m_queue.begin(), m_queue.end(), [&](const Job &j) { return j.key == k; }))
        return {{"status", "queued"}};
    if (m_errorKey == k)
        return {{"status", "failed"}, {"error", m_error}};
    const auto r = result(task, a, variant);
    if (r.path.isEmpty())
        return {{"status", "none"}};
    return {{"status", "ready"}, {"start", r.start}, {"end", r.end}};
}
void AiJobs::cancel() {
    m_queue.clear();
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(3000);
        m_process->deleteLater();
        m_process = nullptr;
        QFile::remove(QDir(m_dir).filePath(m_job.key + ".part" + extension(m_job.task)));
    }
    emit changed();
}
void AiJobs::start(const QString &task, const Asset &a, double start, double end,
                   const QString &variant) {
    if (!available(task))
        return;
    if (task == "transcribe" ? !a.hasAudio
                             : a.kind != "video" || a.width <= 0 || a.height <= 0)
        return;
    Job job;
    job.task = task;
    job.asset = a;
    job.start = std::clamp(start, 0., a.duration);
    job.end = std::clamp(end, job.start, a.duration);
    job.variant = variant;
    job.key = key(task, a, variant);
    if (job.end - job.start < 0.05)
        return;
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                 [&](const Job &j) { return j.key == job.key; }),
                  m_queue.end());
    if (m_errorKey == job.key)
        m_errorKey.clear();
    m_queue.push_back(job);
    emit changed();
    next();
}
void AiJobs::next() {
    if (m_process || m_queue.isEmpty())
        return;
    m_job = m_queue.takeFirst();
    m_progress = 0;
    m_device.clear();
    QDir().mkpath(m_dir);
    if (m_job.task != "upscale" && m_job.task != "eyecontact")
        return run(m_job, {});
    // Upscale and eye contact keep every source frame, so they need the source frame rate.
    auto *probe = new QProcess(this);
    m_process = probe;
    connect(probe, &QProcess::finished, this, [this, probe](int code) {
        const auto rate = QString::fromUtf8(probe->readAllStandardOutput()).trimmed();
        probe->deleteLater();
        m_process = nullptr;
        const auto parts = rate.split('/');
        if (code != 0 || parts.size() != 2 || parts[0].toDouble() <= 0 || parts[1].toDouble() <= 0)
            return finish(false, "Cannot read the video frame rate");
        run(m_job, rate);
    });
    connect(probe, &QProcess::errorOccurred, this, [this, probe](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        probe->deleteLater();
        m_process = nullptr;
        finish(false, "Cannot start ffprobe");
    });
    probe->start(m_ffprobe, {"-v", "error", "-select_streams", "v:0", "-show_entries",
                             "stream=avg_frame_rate", "-of", "csv=p=0", m_job.asset.path});
    emit changed();
}
void AiJobs::run(const Job &job, const QString &rate) {
    const auto part = QDir(m_dir).filePath(job.key + ".part" + extension(job.task));
    const auto &a = job.asset;
    QStringList args{job.task,     "--ffmpeg",        m_ffmpeg,
                     "--model",    m_files.value(job.task),
                     "--input",    a.path,            "--output",
                     part,         "--start",         QString::number(job.start, 'f', 6),
                     "--duration", QString::number(job.end - job.start, 'f', 6)};
    double resultRate = matteRate;
    if (job.task == "upscale") {
        // The model works on the source picture, at most 1080p; FFmpeg's Lanczos does the rest.
        const double fit = std::min(1., std::min(1920. / a.width, 1080. / a.height));
        const QSize source(std::max(2, int(std::lround(a.width * fit / 2)) * 2),
                           std::max(2, int(std::lround(a.height * fit / 2)) * 2));
        const auto size = upscaleSize(a, job.variant.toInt());
        args << "--source" << QString("%1x%2").arg(source.width()).arg(source.height())
             << "--size" << QString("%1x%2").arg(size.width()).arg(size.height()) << "--rate"
             << rate;
        const auto parts = rate.split('/');
        resultRate = parts[0].toDouble() / parts[1].toDouble();
    } else if (job.task == "eyecontact") {
        // Processed at the source size, at most 4K, so the rest of the picture keeps its detail.
        const double fit = std::min(1., std::min(3840. / a.width, 2160. / a.height));
        args << "--source"
             << QString("%1x%2")
                    .arg(std::max(2, int(std::lround(a.width * fit / 2)) * 2))
                    .arg(std::max(2, int(std::lround(a.height * fit / 2)) * 2))
             << "--rate" << rate;
        const auto parts = rate.split('/');
        resultRate = parts[0].toDouble() / parts[1].toDouble();
    } else if (job.task == "transcribe") {
        args << "--whisper" << m_files.value("whisper") << "--language"
             << (job.variant.isEmpty() ? QString("auto") : job.variant);
        if (QFileInfo(m_files.value("vad")).isFile())
            args << "--vad" << m_files.value("vad");
    } else {
        // About 640 pixels on the long side: enough for soft edges, small on disk.
        const double fit = std::min(1., 640. / std::max(a.width, a.height));
        args << "--size"
             << QString("%1x%2")
                    .arg(std::max(2, int(std::lround(a.width * fit / 2)) * 2))
                    .arg(std::max(2, int(std::lround(a.height * fit / 2)) * 2))
             << "--rate" << QString::number(matteRate);
    }
    auto *p = new QProcess(this);
    m_process = p;
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p] {
        while (p->canReadLine()) {
            const auto parts = QString::fromUtf8(p->readLine()).trimmed().split(' ');
            if (parts.size() == 3 && parts[0] == "progress" && parts[2].toDouble() > 0)
                m_progress = std::clamp(parts[1].toDouble() / parts[2].toDouble(), 0., 1.);
            else if (parts.size() == 2 && parts[0] == "device")
                m_device = parts[1];
            else
                continue;
            emit changed();
        }
    });
    auto stderrLog = std::make_shared<QByteArray>();
    connect(p, &QProcess::readyReadStandardError, this, [p, stderrLog] {
        *stderrLog += p->readAllStandardError();
        if (stderrLog->size() > 16384)
            *stderrLog = stderrLog->right(8192);
    });
    auto complete = [this, p, part, resultRate, stderrLog](bool success) {
        *stderrLog += p->readAllStandardError();
        m_process = nullptr;
        p->deleteLater();
        const auto base = QDir(m_dir).filePath(m_job.key);
        if (success) {
            const auto file = base + extension(m_job.task);
            QFile::remove(file);
            QSaveFile meta(base + ".json");
            success = QFile::rename(part, file) && meta.open(QIODevice::WriteOnly);
            if (success) {
                meta.write(QJsonDocument(QJsonObject{
                                             {"start", m_job.start},
                                             {"end", m_job.end},
                                             {"rate", resultRate},
                                             {"model", QFileInfo(m_files.value(m_job.task))
                                                           .fileName()},
                                         })
                               .toJson());
                success = meta.commit();
            }
        }
        if (!success)
            QFile::remove(part);
        finish(success, QString::fromUtf8(*stderrLog).trimmed());
    };
    connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            complete(false);
    });
    p->start(m_worker, args);
    emit changed();
}
void AiJobs::finish(bool success, const QString &log) {
    if (!success) {
        m_errorKey = m_job.key;
        // The last lines name the problem; DirectML fallback notes are not errors.
        QStringList lines;
        for (const auto &line : log.split('\n'))
            if (!line.contains("DirectML unavailable"))
                lines << line.trimmed();
        lines.removeAll(QString());
        m_error = lines.isEmpty() ? "AI processing failed"
                                  : lines.mid(std::max<qsizetype>(0, lines.size() - 3)).join(' ');
    }
    m_job = {};
    emit finished();
    emit changed();
    QTimer::singleShot(0, this, &AiJobs::next);
}
} // namespace cutlery
