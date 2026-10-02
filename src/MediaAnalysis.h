#pragma once
#include "Project.h"
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QVariantMap>

namespace cutlery {
// Streaming peak extraction keeps decoded PCM out of the project/undo history.
class PeakAccumulator {
  public:
    explicit PeakAccumulator(qint64 samplesPerPeak);
    void append(const QByteArray &bytes);
    QVector<float> finish();

  private:
    qint64 m_binSamples, m_count = 0;
    float m_peak = 0;
    QByteArray m_tail;
    QVector<float> m_peaks;
};
// Beat times in seconds from the start of mono samples at `rate` Hz: an onset envelope from
// spectral flux, the tempo from its autocorrelation, then dynamic-programming beat tracking.
// `bpm` receives the tempo when given. Fewer than about two seconds give no beats.
QVector<double> detectBeats(const QVector<float> &samples, int rate, double *bpm = nullptr);
class MediaAnalysis final : public QObject {
    Q_OBJECT
  public:
    MediaAnalysis(QString cacheDir, QString ffmpeg, QObject *parent = nullptr);
    ~MediaAnalysis() override;
    void setAssets(const QVector<Asset> &assets);
    void setPaused(bool paused);
    QVariantMap waveform(const QString &assetId) const;
    bool busy() const {
        return m_process || !m_queue.empty();
    }
    static QString fingerprint(const Asset &asset);
  signals:
    void changed();

  private:
    struct Waveform {
        QString key, status;
        QVector<float> peaks;
        double step = 0;
    };
    QHash<QString, Asset> m_assets;
    QHash<QString, Waveform> m_waves;
    QList<QString> m_queue;
    QString m_cache, m_ffmpeg, m_current;
    QProcess *m_process = nullptr;
    bool m_paused = false;
    void next();
    bool load(const QString &id, const QString &key);
    void save(const QString &id);
};
} // namespace cutlery
