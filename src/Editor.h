#pragma once
#include "Project.h"
#include <QObject>
#include <QProcess>
#include <QQuickImageProvider>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

namespace cutlery {
class FrameProvider final : public QQuickImageProvider {
  public:
    FrameProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage frame;
    QImage requestImage(const QString &, QSize *size, const QSize &) override {
        if (size)
            *size = frame.size();
        return frame;
    }
};
class Editor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantList assets READ assets NOTIFY projectChanged)
    Q_PROPERTY(QVariantList clips READ clips NOTIFY projectChanged)
  public:
    explicit Editor(FrameProvider *, QObject *parent = nullptr);
    ~Editor() override;
    QVariantMap state() const;
    QVariantList assets() const;
    QVariantList clips() const;
    Q_INVOKABLE void newProject();
    Q_INVOKABLE bool openProject(const QUrl &);
    Q_INVOKABLE bool save(const QUrl &url = QUrl());
    Q_INVOKABLE void recover();
    Q_INVOKABLE void importMedia(const QList<QUrl> &);
    Q_INVOKABLE void relink(const QString &assetId, const QUrl &);
    Q_INVOKABLE void addAsset(const QString &assetId, int track = 0);
    Q_INVOKABLE void addTitle();
    Q_INVOKABLE void select(const QString &id);
    Q_INVOKABLE void seek(qint64 frame);
    Q_INVOKABLE void setClip(const QString &key, const QVariant &value);
    Q_INVOKABLE void moveClip(const QString &id, qint64 frame, int track);
    Q_INVOKABLE void split();
    Q_INVOKABLE void remove(bool ripple = false);
    Q_INVOKABLE void duplicate();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void configure(int width, int height, int fpsN, int fpsD);
    Q_INVOKABLE void renderPlayback();
    Q_INVOKABLE void exportVideo(const QUrl &, const QString &profile);
    Q_INVOKABLE void cancelJob();
    Q_INVOKABLE void importSrt(const QUrl &);
    Q_INVOKABLE bool exportSrt(const QUrl &);
    Q_INVOKABLE void clearError();
    static QString executable(const QString &name);
    const Project &project() const {
        return m_project;
    }
  signals:
    void changed();
    void projectChanged();

  private:
    Project m_project;
    QVector<Project> m_undo, m_redo;
    QString m_selected, m_path, m_status = "Ready", m_error, m_data, m_recovery, m_previewUrl,
                                m_playbackUrl;
    bool m_dirty = false, m_busy = false, m_importing = false, m_cancelled = false,
         m_hasRecovery = false;
    double m_progress = 0;
    qint64 m_playhead = 0, m_revision = 0, m_previewSerial = 0;
    FrameProvider *m_frames;
    QTimer m_previewTimer, m_saveTimer;
    QProcess *m_preview = nullptr, *m_job = nullptr, *m_probe = nullptr;
    QString m_jobTemp;
    QList<QUrl> m_importQueue;
    void fail(const QString &);
    void mutate(const std::function<void(Project &)> &);
    void edited();
    void requestPreview();
    void probeNext();
    void probeFile(const QUrl &, const QString &replaceId);
    void startRender(const QString &output, const QString &profile, bool playback);
    void autosave();
};
} // namespace cutlery
