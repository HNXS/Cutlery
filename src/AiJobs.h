#pragma once
#include "Project.h"
#include "RenderGraph.h"
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QSize>
#include <QVariantMap>

namespace cutlery {
// Results of the optional cutlery-ai worker, cached per media file (fingerprint) under `dir`:
//  - "matte": person matte for AI background removal (gray FFV1, 8 analysed frames/s);
//  - "upscale": the video at a higher resolution (ProRes 422 at the source frame rate).
// Jobs run one at a time in request order; each result covers a range of source seconds.
class AiJobs final : public QObject {
    Q_OBJECT
  public:
    // `models` maps a task to its model file. Worker and models may be missing: the AI pack is
    // optional.
    AiJobs(QString dir, QString ffmpeg, QString ffprobe, QString worker,
           QHash<QString, QString> models, QObject *parent = nullptr);
    ~AiJobs() override;
    // Why a task is unavailable, for the interface; empty when available.
    QString missing(const QString &task) const;
    bool available(const QString &task) const {
        return missing(task).isEmpty();
    }
    // The cached result for an asset, or an empty path. `height` selects the upscale size.
    MatteSource result(const QString &task, const Asset &, int height = 0) const;
    // {status: none|queued|running|ready|failed, progress 0..1, device gpu|cpu, error}
    QVariantMap status(const QString &task, const Asset &, int height = 0) const;
    // Queues the analysis of source seconds [start, end), replacing the cached result. Upscales
    // to `height` (and the matching width).
    void start(const QString &task, const Asset &, double start, double end, int height = 0);
    // Stops the running job and drops queued ones.
    void cancel();
    bool busy() const {
        return m_process != nullptr || !m_queue.isEmpty();
    }
    static constexpr double matteRate = 8; // analysed frames per second
    // Output size of an upscale to `height`, keeping the asset's aspect ratio (even sizes).
    static QSize upscaleSize(const Asset &, int height);

  signals:
    void changed();
    // A job ended, successfully or not.
    void finished();

  private:
    struct Job {
        QString task, key;
        Asset asset;
        double start = 0, end = 0;
        int height = 0;
    };
    QString m_dir, m_ffmpeg, m_ffprobe, m_worker;
    QHash<QString, QString> m_models;
    QList<Job> m_queue;
    QProcess *m_process = nullptr;
    Job m_job;
    QString m_device, m_errorKey, m_error;
    double m_progress = 0;
    QString key(const QString &task, const Asset &, int height) const;
    void next();
    void run(const Job &, const QString &rate);
    void finish(bool success, const QString &log);
};
} // namespace cutlery
