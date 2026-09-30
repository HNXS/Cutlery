#pragma once
#include "Project.h"
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QSize>
#include <QVariantMap>

namespace cutlery {
// Filmstrips for the timeline and media library. One background FFmpeg job per video/image asset
// decodes keyframes only, samples them at a fixed source interval and stores all tiles side by side
// in one cached JPEG. Cache keys cover the file fingerprint, so edits never re-extract.
class Thumbnails final : public QObject {
    Q_OBJECT
  public:
    // Layout of an asset's strip; derived from the asset alone so cached files need no metadata.
    struct Layout {
        int count = 0;
        double interval = 0; // source seconds between tiles
        QSize tile;
    };
    static Layout layout(const Asset &);
    Thumbnails(QString cacheDir, QString ffmpeg, QObject *parent = nullptr);
    ~Thumbnails() override;
    void setAssets(const QVector<Asset> &assets);
    void setPaused(bool paused);
    QVariantMap strip(const QString &assetId) const;
    bool busy() const {
        return m_process || !m_queue.empty();
    }
  signals:
    void changed();

  private:
    struct Strip {
        QString key, status;
    };
    QHash<QString, Asset> m_assets;
    QHash<QString, Strip> m_strips;
    QList<QString> m_queue;
    QString m_cache, m_ffmpeg;
    QProcess *m_process = nullptr;
    bool m_paused = false;
    QString path(const QString &key) const;
    void next();
};
} // namespace cutlery
