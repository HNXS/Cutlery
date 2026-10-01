#pragma once
#include "Project.h"
#include "RenderGraph.h"
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QVariantMap>

namespace cutlery {
// AI person mattes for background removal, analysed by the optional cutlery-matte worker and
// cached per media file (fingerprint) under `dir`. One analysis runs at a time.
class Mattes final : public QObject {
    Q_OBJECT
  public:
    // `worker` and `model` may be empty or missing: the AI pack is optional.
    Mattes(QString dir, QString ffmpeg, QString worker, QString model, QObject *parent = nullptr);
    ~Mattes() override;
    bool available() const;
    // Why analysis is unavailable, for the interface; empty when available.
    QString missing() const;
    // The cached matte of an asset, or an empty path.
    MatteSource matte(const Asset &) const;
    // {status: none|analyzing|ready|failed, progress 0..1, error}
    QVariantMap status(const Asset &) const;
    // Analyses source seconds [start, end) of a video asset, replacing its cached matte.
    void analyze(const Asset &, double start, double end);
    void cancel();
    bool busy() const {
        return m_process != nullptr;
    }
    static constexpr double rate = 8; // analysed frames per second

  signals:
    void changed();

  private:
    QString m_dir, m_ffmpeg, m_worker, m_model;
    QProcess *m_process = nullptr;
    QString m_key, m_error, m_errorKey;
    double m_progress = 0;
    QString key(const Asset &) const;
};
} // namespace cutlery
