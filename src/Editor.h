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
#include <optional>
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
    // Playback level meter: [left, right] in dBFS.
    Q_PROPERTY(QVariantList levels READ levels NOTIFY playbackChanged)
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
    // A title template ("lowerThird", "lowerThirdLine", "titleCard") at the playhead.
    Q_INVOKABLE void addTitleTemplate(const QString &style);
    // A blur ("blur") or mosaic ("pixelate") area over the lower tracks, at the playhead.
    Q_INVOKABLE void addEffect(const QString &effect);
    // Adds a shape (see graphicKinds()) at the playhead on the top track.
    Q_INVOKABLE void addGraphic(const QString &kind);
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
    // Installed font families, including fonts added to Cutlery.
    Q_INVOKABLE QStringList fontFamilies() const;
    // Copies a font file into the data folder's fonts/ and returns its family ("" on failure).
    Q_INVOKABLE QString addFont(const QUrl &file);
    // Clipboard for clips within the session, also across projects (the media comes along).
    Q_INVOKABLE void copy();
    // Inserts the copied clip at the playhead on its track.
    Q_INVOKABLE void paste();
    // Applies the copied clip's settings to the selected clip: "look" (colour, effects, LUT) or
    // "all" (also transform, keyframes, shape and border, keying, volume and fades).
    Q_INVOKABLE void pasteAttributes(const QString &group = "all");
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void configure(int width, int height, int fpsN, int fpsD);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void setVideoSink(QObject *sink);
    QVariantList levels() const {
        const auto [l, r] = m_playback->levels();
        return {l, r};
    }
    // Measures the whole mix (EBU R128): state "loudness" {status, integrated LUFS, peak dBTP}.
    Q_INVOKABLE void analyzeLoudness();
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
    // Pauses in the selected clip's sound: finds stretches quieter than `thresholdDb` for at
    // least `minPause` seconds (asynchronously; see state "pauses"), then removePauses() cuts
    // them out, keeping `padding` seconds of room around speech, and closes the gaps on the
    // clip's track and on tracks with its detached audio. One undo step.
    Q_INVOKABLE void findPauses(double thresholdDb, double minPause);
    Q_INVOKABLE void removePauses();
    // Finds the shot changes in the selected video clip and splits it there, with any detached
    // audio, in one undo step. Sensitivity 0..1: higher finds subtler cuts.
    Q_INVOKABLE void splitAtScenes(double sensitivity = 0.5);
    // Automatic captions: transcribes every audible clip's media (language "auto", "de", "en",
    // ...) and puts the captions on the "AI captions" track, replacing earlier ones.
    // `style`: "" plain lines, "karaoke" (spoken word highlighted), "word" (one word at a time).
    Q_INVOKABLE void generateCaptions(const QString &language, const QString &style = {});
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
    QVariantMap m_loudness; // measurement of the running export, when normalising
    QVariantMap m_mixLoudness; // last analyzeLoudness() result
    std::optional<Clip> m_clipboard;
    std::optional<Asset> m_clipboardAsset;
    QProcess *m_loudnessProcess = nullptr;
    struct Pauses {
        QString clipId, status; // status: idle, finding, ready, failed
        QVector<QPair<qint64, qint64>> ranges; // clip-local frames
        qint64 revision = -1;
    } m_pauses;
    QProcess *m_pauseProcess = nullptr;
    QProcess *m_sceneProcess = nullptr;
    QVariantMap m_scenes; // splitAtScenes(): status finding|done|failed, count
    QVariantMap pauseState() const;
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
    void startRender(const QString &output, QSize size, const Encoder &encoder,
                     double gainDb = 0);
    // First export pass for loudness normalisation: measures the mix, then starts the render
    // with the gain that reaches `target` LUFS.
    void measureLoudness(const QString &output, QSize size, const Encoder &encoder,
                         double target);
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
    QString m_captionLanguage = "auto", m_captionStyle;
    QStringList m_captionAssets; // media of a running caption request
    static int upscaleHeight(const Asset &);
    void addAiMedia(RenderOptions &) const;
    void autosave();
};
} // namespace cutlery
