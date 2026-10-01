#pragma once
#include "ExportProfiles.h"
#include "MediaAnalysis.h"
#include "Playback.h"
#include "Thumbnails.h"
#include "AiJobs.h"
#include "Project.h"
#include <QObject>
#include <QProcess>
#include <QQuickImageProvider>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>

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
    Q_PROPERTY(QVariantList trackList READ trackList NOTIFY projectChanged)
    // Live playback position; separate from `state` so the viewer clock updates cheaply.
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(qint64 playbackFrame READ playbackFrame NOTIFY playbackChanged)
  public:
    explicit Editor(FrameProvider *, QObject *parent = nullptr);
    ~Editor() override;
    QVariantMap state() const;
    QVariantList assets() const;
    QVariantList clips() const;
    QVariantList trackList() const;
    Q_INVOKABLE QVariantMap trimBounds(const QString &id) const;
    Q_INVOKABLE qint64 snap(qint64 frame, qint64 threshold, const QString &exclude,
                            qint64 length = 0) const;
    Q_INVOKABLE void trimClip(const QString &id, qint64 start, qint64 end);
    Q_INVOKABLE void addTrack();
    Q_INVOKABLE void removeTrack(int track);
    Q_INVOKABLE void setTrack(int track, const QString &key, const QVariant &value);
    Q_INVOKABLE void detachAudio();
    Q_INVOKABLE qint64 adjacentCut(bool forward) const;
    Q_INVOKABLE void newProject();
    Q_INVOKABLE bool openProject(const QUrl &);
    Q_INVOKABLE bool save(const QUrl &url = QUrl());
    Q_INVOKABLE void recover();
    Q_INVOKABLE void importMedia(const QList<QUrl> &);
    Q_INVOKABLE void dropFiles(const QList<QUrl> &, int track, qint64 frame);
    Q_INVOKABLE bool insertAsset(const QString &assetId, int track, qint64 frame);
    Q_INVOKABLE qint64 placement(int track, qint64 frame, const QString &exclude = {}) const;
    Q_INVOKABLE void relink(const QString &assetId, const QUrl &);
    Q_INVOKABLE void addAsset(const QString &assetId, int track = 0);
    Q_INVOKABLE void addTitle();
    Q_INVOKABLE void select(const QString &id);
    Q_INVOKABLE void seek(qint64 frame);
    Q_INVOKABLE void setClip(const QString &key, const QVariant &value);
    Q_INVOKABLE void setClipValues(const QVariantMap &values);
    // Picture rectangle of a clip on the canvas at the playhead: {x, y, width, height} as
    // fractions of the canvas, plus rotation and whether the playhead is inside the clip.
    Q_INVOKABLE QVariantMap clipBounds(const QString &id) const;
    // Picture-in-picture placement: "topLeft", "topRight", "bottomLeft", "bottomRight" shrink
    // a full-size clip to 30% and inset it by a margin; "full" restores a centred full frame.
    Q_INVOKABLE void placeClip(const QString &corner);
    Q_INVOKABLE void moveClip(const QString &id, qint64 frame, int track);
    Q_INVOKABLE void split();
    // Adds a keyframe at the playhead with the current value, or removes the one there.
    Q_INVOKABLE void toggleKeyframe(const QString &property);
    // Previous/next keyframe position of the selected clip (the playhead when there is none).
    Q_INVOKABLE qint64 adjacentKeyframe(bool forward) const;
    Q_INVOKABLE void remove(bool ripple = false);
    Q_INVOKABLE void duplicate();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void configure(int width, int height, int fpsN, int fpsD);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void setVideoSink(QObject *sink);
    bool playing() const {
        return m_playback->active();
    }
    qint64 playbackFrame() const {
        return m_playback->active() ? m_playback->frame() : m_playhead;
    }
    // Legacy profiles: "mpeg4", "webm", "h264".
    Q_INVOKABLE void exportVideo(const QUrl &, const QString &profile);
    // settings: {format, quality, height}; see ExportSettings.
    Q_INVOKABLE void exportWith(const QUrl &, const QVariantMap &settings);
    // Output size and file extension for export settings, for the export dialog.
    Q_INVOKABLE QVariantMap exportPreview(const QVariantMap &settings) const;
    Q_INVOKABLE void cancelJob();
    Q_INVOKABLE void importSrt(const QUrl &);
    Q_INVOKABLE bool exportSrt(const QUrl &);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE QVariantMap waveform(const QString &assetId) const {
        return m_analysis->waveform(assetId);
    }
    Q_INVOKABLE QVariantList transitionTypes() const {
        QVariantList result;
        for (const auto &[id, label] : cutlery::transitionTypes())
            result << QVariantMap{{"id", id}, {"label", label}};
        return result;
    }
    // Runs an AI task ("matte" for background removal, "upscale") on the selected clip's media,
    // covering every clip of that media using it. Results are cached per media file.
    Q_INVOKABLE void runAi(const QString &task);
    Q_INVOKABLE void cancelAi();
    // Automatic captions: transcribes every audible clip's media (language "auto", "de", "en",
    // ...) and puts the captions on the "AI captions" track, replacing earlier ones.
    Q_INVOKABLE void generateCaptions(const QString &language);
    static constexpr auto captionTrackName = "AI captions";
    Q_INVOKABLE QVariantMap thumbnails(const QString &assetId) const {
        return m_thumbnails->strip(assetId);
    }
    static QString executable(const QString &name);
    const Project &project() const {
        return m_project;
    }
  signals:
    void changed();
    void projectChanged();
    void analysisChanged();
    void playbackChanged();
    void thumbnailsChanged();

  private:
    Project m_project;
    QVector<Project> m_undo, m_redo;
    QString m_selected, m_path, m_status = "Ready", m_error, m_data, m_recovery, m_previewUrl;
    bool m_dirty = false, m_busy = false, m_importing = false, m_cancelled = false,
         m_hasRecovery = false;
    double m_progress = 0;
    qint64 m_playhead = 0, m_revision = 0, m_previewSerial = 0;
    FrameProvider *m_frames;
    MediaAnalysis *m_analysis;
    Thumbnails *m_thumbnails;
    AiJobs *m_ai;
    EncoderResolver *m_encoders;
    Playback *m_playback;
    QTimer m_previewTimer, m_saveTimer, m_resumeTimer;
    QProcess *m_preview = nullptr, *m_job = nullptr, *m_probe = nullptr;
    QString m_jobTemp;
    struct DropBatch {
        QString trackId;
        qint64 frame = 0;
        QString lastClip;
    };
    struct ImportRequest {
        QUrl url;
        std::shared_ptr<DropBatch> drop;
    };
    QList<ImportRequest> m_importQueue;
    QStringList m_importErrors;
    void fail(const QString &);
    bool mutate(const std::function<void(Project &)> &);
    void edited();
    void requestPreview();
    void probeNext();
    void probeFile(const QUrl &, const QString &replaceId, std::shared_ptr<DropBatch> drop = {});
    static QString insert(Project &, const QString &assetId, int track, qint64 frame);
    void startRender(const QString &output, QSize size, const Encoder &encoder);
    void applyClipValue(Project &, const QString &key, const QVariant &value);
    void stopPlayback();
    QSize previewSize(int longSide) const;
    // Source seconds an AI task needs for an asset: the clips using it, plus `extra`.
    std::pair<double, double> aiSpan(const QString &task, const Asset &,
                                     const Clip *extra = nullptr) const;
    bool aiCovered(const QString &task, const Asset &, const Clip *extra = nullptr) const;
    bool usesAi(const Clip &, const QString &task) const;
    QString aiVariant(const QString &task, const Asset &) const;
    bool startAi(const QString &task, const Asset &, const Clip *extra = nullptr);
    QStringList speakingAssets() const;
    void placeCaptions();
    QVariantMap captionState() const;
    QString m_captionLanguage = "auto";
    QStringList m_captionAssets; // media of a running caption request
    static int upscaleHeight(const Asset &);
    void addAiMedia(RenderOptions &) const;
    void autosave();
};
} // namespace cutlery
