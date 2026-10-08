#pragma once
#include "ExportProfiles.h"
#include "MediaAnalysis.h"
#include "Playback.h"
#include "Thumbnails.h"
#include "AiJobs.h"
#include "Project.h"
#include "Scopes.h"
#include "SoundLibrary.h"
#include <QObject>
#include <QProcess>
#include <QQuickImageProvider>
#include <QElapsedTimer>
#include <QTimer>
class QAudioInput;
class QThread;
class QMediaCaptureSession;
class QMediaRecorder;
#include <optional>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>

namespace cutlery {
// Draws an SVG file at `longest` pixels on its longer side into a transparent PNG in `folder`
// and returns the PNG's path. Throws when the file cannot be read or written.
QString rasterizeSvg(const QString &svg, const QString &folder, int longest = 2048);
class FrameProvider final : public QQuickImageProvider {
  public:
    FrameProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    // `frame`: the preview still; `live`: the last playback frame captured for the scopes.
    QImage frame, live;
    // "scope/<kind>/<still|live>/<serial>" gives a scope of that picture (see renderScope).
    QImage requestImage(const QString &id, QSize *size, const QSize &) override {
        auto image = frame;
        if (id.startsWith("scope/"))
            image = renderScope(id.section('/', 2, 2) == "live" && !live.isNull() ? live : frame,
                                id.section('/', 1, 1));
        if (size)
            *size = image.size();
        return image;
    }
};
// Brightness, contrast, temperature and tint that correct measured levels: luma at the 10th
// and 90th percentile and average chroma (0..255).
QVariantMap autoColourCorrection(double low, double high, double u, double v);
class Editor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QVariantList assets READ assets NOTIFY projectChanged)
    Q_PROPERTY(QVariantList clips READ clips NOTIFY projectChanged)
    Q_PROPERTY(QVariantList trackList READ trackList NOTIFY projectChanged)
    // Live playback position; separate from `state` so the viewer clock updates cheaply.
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(qint64 playbackFrame READ playbackFrame NOTIFY playbackChanged)
    Q_PROPERTY(double playbackRate READ playbackRate NOTIFY playbackChanged)
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
    // Slip, roll and slide by a number of frames; see Project. One undo step each.
    Q_INVOKABLE void slipClip(const QString &id, qint64 frames);
    Q_INVOKABLE void rollCut(const QString &id, qint64 frames);
    Q_INVOKABLE void slideClip(const QString &id, qint64 frames);
    Q_INVOKABLE void addTrack();
    Q_INVOKABLE void removeTrack(int track);
    Q_INVOKABLE void setTrack(int track, const QString &key, const QVariant &value);
    Q_INVOKABLE void detachAudio();
    Q_INVOKABLE qint64 adjacentCut(bool forward) const;
    Q_INVOKABLE void newProject();
    // Templates (data folder, templates/): the project as it is, saved under a name, to start
    // new projects from; its media can then be replaced (relink) or stays as it is.
    Q_INVOKABLE QVariantList templates() const;
    Q_INVOKABLE void saveTemplate(const QString &name);
    Q_INVOKABLE bool newFromTemplate(const QString &name);
    Q_INVOKABLE void removeTemplate(const QString &name);
    // App-wide settings (state "preferences"): width, height, fpsN, fpsD of new projects,
    // stillSeconds of imported pictures, backups kept per project, startScreen, cacheGB (the
    // cache limit). Saved at once.
    Q_INVOKABLE void setPreferences(const QVariantMap &values);
    // Size of the waveform, thumbnail and nested-sequence caches: bytes, files.
    Q_INVOKABLE QVariantMap cacheUsage() const;
    // Empties those caches when Cutlery next starts (files in use now stay until then).
    Q_INVOKABLE void clearCacheAtStart();
    Q_INVOKABLE bool openProject(const QUrl &);
    Q_INVOKABLE bool save(const QUrl &url = QUrl());
    // Recently opened or saved projects, newest first (state "recent": path, name, exists).
    Q_INVOKABLE bool openRecent(const QString &path);
    // Earlier versions of the open project: each save keeps the previous file (the newest 20)
    // in the data folder. Entries: file, time (ISO), bytes. Restoring backs up the current file
    // first, puts the version back at the project's path and opens it.
    Q_INVOKABLE QVariantList backups() const;
    Q_INVOKABLE bool restoreBackup(const QString &file);
    Q_INVOKABLE void recover();
    // Files and folders: a folder adds the media files inside it and the folders below it,
    // into a library folder of its name.
    Q_INVOKABLE void importMedia(const QList<QUrl> &);
    // Stops the import: the file being read and those waiting are skipped.
    Q_INVOKABLE void cancelImport();
    // Media library folders. New imports go into the import folder (the one on show).
    Q_INVOKABLE void addFolder(const QString &name);
    Q_INVOKABLE void renameFolder(const QString &from, const QString &to);
    Q_INVOKABLE void removeFolder(const QString &name);
    Q_INVOKABLE void moveToFolder(const QStringList &assetIds, const QString &folder);
    Q_INVOKABLE void setImportFolder(const QString &folder);
    // Takes media no clip uses out of the library (the files stay on disk).
    Q_INVOKABLE void removeAssets(const QStringList &assetIds);
    Q_INVOKABLE void dropFiles(const QList<QUrl> &, int track, qint64 frame);
    Q_INVOKABLE bool insertAsset(const QString &assetId, int track, qint64 frame);
    Q_INVOKABLE qint64 placement(int track, qint64 frame, const QString &exclude = {}) const;
    Q_INVOKABLE void relink(const QString &assetId, const QUrl &);
    // Finds every missing media file by name in a folder and those below it, and relinks them
    // all in one undo step; where several match, the one in the most similar folders wins.
    Q_INVOKABLE void relinkFolder(const QUrl &folder);
    Q_INVOKABLE void addAsset(const QString &assetId, int track = 0);
    Q_INVOKABLE void addTitle();
    // A title template ("lowerThird", "lowerThirdLine", "titleCard") at the playhead.
    Q_INVOKABLE void addTitleTemplate(const QString &style);
    // A blur ("blur") or mosaic ("pixelate") area over the lower tracks, at the playhead.
    Q_INVOKABLE void addEffect(const QString &effect);
    // Adds a shape (see graphicKinds()) at the playhead on the top track.
    Q_INVOKABLE void addGraphic(const QString &kind);
    // A ready-made motion for the selected clips, as keyframes that replace those of the
    // properties it moves: "popIn", "popOut", "slideLeft", "slideUp", "pulse", "wiggle", or
    // "none" to remove scale, position and rotation keyframes.
    Q_INVOKABLE void applyMotion(const QString &preset);
    // Arranges the selected pictures (lowest track first) on the canvas, in one undo step:
    // "side" (side by side), "stack" (one above the other), "grid" (2 × 2), "pip-tl", "pip-tr",
    // "pip-bl", "pip-br" (the lowest full, the others small in that corner), "presenter" (the
    // lowest large on the left, the next round in the lower right) or "full" (all full size).
    // With `fill`, each picture is cropped (equally on two opposite edges) to the shape of its
    // area and fills it; otherwise it fits inside. Arranging resets the single-edge crops.
    Q_INVOKABLE void arrange(const QString &layout, bool fill = false);
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
    // Lets the selected clip and its linked picture or sound move and trim on their own.
    Q_INVOKABLE void unlinkClip();
    // Selection of several clips (state "selectedIds", the selected clip first): Ctrl+click
    // toggles a clip, a grouped clip brings its group. Moving the selected clip moves them all;
    // deleting deletes them all.
    Q_INVOKABLE void toggleSelect(const QString &id);
    Q_INVOKABLE void selectAll();
    // The clips in a timeline rectangle: frames [from, to) on tracks [low, high]. `add` keeps
    // the current selection.
    Q_INVOKABLE void selectArea(qint64 from, qint64 to, int low, int high, bool add);
    Q_INVOKABLE void groupSelection();
    // Nested sequences: the selected clips become one clip holding them as a sequence of their
    // own; it can be opened to edit (and closed again), or taken apart.
    Q_INVOKABLE void nestSelection();
    Q_INVOKABLE void openNested(const QString &clipId = {});
    Q_INVOKABLE void closeNested();
    Q_INVOKABLE void unnest();
    Q_INVOKABLE void ungroupSelection();
    QStringList selection() const;
    Q_INVOKABLE void split();
    // Adds a keyframe at the playhead with the current value, or removes the one there.
    Q_INVOKABLE void toggleKeyframe(const QString &property);
    // How the selected clip's keyframe of `property` at the playhead moves on to the next one:
    // one of keyframeEasings().
    Q_INVOKABLE void setKeyframeEasing(const QString &property, const QString &easing);
    // Previous/next keyframe position of the selected clip (the playhead when there is none).
    Q_INVOKABLE qint64 adjacentKeyframe(bool forward) const;
    // Markers: add one at the playhead or remove the one there; rename or recolour by index.
    Q_INVOKABLE void toggleMarker();
    Q_INVOKABLE void setMarker(int index, const QString &key, const QVariant &value);
    Q_INVOKABLE void removeMarker(int index);
    // The nearest marker frame before or after the playhead, or -1.
    Q_INVOKABLE qint64 adjacentMarker(bool forward) const;
    // In/out range at the playhead (the out point is after the playhead frame); clear removes it.
    Q_INVOKABLE void setInPoint();
    Q_INVOKABLE void setOutPoint();
    Q_INVOKABLE void clearInOut();
    Q_INVOKABLE void remove(bool ripple = false);
    Q_INVOKABLE void duplicate();
    // Installed font families, including fonts added to Cutlery.
    Q_INVOKABLE QStringList fontFamilies() const;
    // Copies a font file into the data folder's fonts/ and returns its family ("" on failure).
    Q_INVOKABLE QString addFont(const QUrl &file);
    // Copies the project with all its media, LUTs and the fonts added in Cutlery that it uses
    // into an empty folder (media/, luts/, fonts/ and <folder name>.cutlery), in the background.
    // State "collect": {status: copying|done|failed, progress 0..1, path, error}.
    Q_INVOKABLE void collectProject(const QUrl &folder);
    // Re-encodes the selected clip's variable-frame-rate video to a constant rate (ProRes 422 and
    // PCM in the data folder's conformed/) and relinks the media to it. State "conform":
    // {status: converting|done|failed, progress, assetId}.
    Q_INVOKABLE void conformFrameRate();
    // Makes the selected clip (a blur or mosaic area, or any overlay) follow a face in the video
    // below it: keyframes its position through its length and, for areas, sizes it to the face.
    // Analyses the faces first when needed (AI pack). State "follow": {status:
    // analysing|done|failed, keyframes, clipId}.
    Q_INVOKABLE void followFace();
    // Changes the canvas to width × height (e.g. 9:16 for Shorts) and zooms every full-frame
    // video and image to fill it; with the AI pack, videos then pan to keep the main face in
    // the picture.
    Q_INVOKABLE void reframe(int width, int height);
    // Imports the numbered image sequence that `firstImage` belongs to (e.g. shot_0001.png …)
    // at `fps`: FFmpeg turns it into a ProRes 4444 video (alpha kept) in the data folder's
    // sequences/, which is then imported like any video.
    Q_INVOKABLE void importImageSequence(const QUrl &firstImage, double fps);
    // The sequence `file` belongs to: {pattern (FFmpeg %0Nd), start, count}; empty if none.
    static QVariantMap imageSequence(const QString &file);
    Q_INVOKABLE QVariantMap imageSequenceAt(const QUrl &file) const {
        return file.isLocalFile() ? imageSequence(file.toLocalFile()) : QVariantMap{};
    }
    // Clipboard for clips within the session, also across projects (the media comes along).
    Q_INVOKABLE void copy();
    // Inserts the copied clip at the playhead on its track.
    Q_INVOKABLE void paste();
    // Applies the copied clip's settings to the selected clip: "look" (colour, effects, LUT) or
    // "all" (also transform, keyframes, shape and border, keying, volume and fades).
    Q_INVOKABLE void pasteAttributes(const QString &group = "all");
    // Voice-over: records the default microphone to a WAV file in the data folder's recordings/
    // while the timeline plays from the playhead; stopping adds the recording there on a free
    // track. State "voiceOver": {available, recording, seconds}.
    Q_INVOKABLE void startVoiceOver();
    Q_INVOKABLE void stopVoiceOver();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void configure(int width, int height, int fpsN, int fpsD);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayback();
    // JKL shuttle: L plays forward and doubles the speed up to 4× on each press; J scrubs
    // backward the same way; K (pause) stops both.
    Q_INVOKABLE void shuttle(bool forward);
    // Shuttle speed: 1, 2 or 4 forward, negative while scrubbing backward.
    double playbackRate() const {
        return m_reverseTimer.isActive() ? -m_shuttleRate : m_playRate;
    }
    Q_INVOKABLE void setVideoSink(QObject *sink);
    // Keeps the frame on screen for the scopes while playing; false when there is none.
    Q_INVOKABLE bool captureScopeFrame();
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
    // The picture at the playhead at the project's size, as PNG or JPEG (by the file's
    // extension), rendered in the background like a preview but at full quality.
    Q_INVOKABLE void exportFrame(const QUrl &);
    // Runs the last export again after it failed (state "canRetryExport").
    Q_INVOKABLE void retryExport();
    // Saves the sound of the selected clip as heard on the timeline (trim, speed, volume, sound
    // tools, fades; other clips left out) as .wav, .mp3 or .m4a, like an export.
    Q_INVOKABLE void extractAudio(const QUrl &);
    // Converts or compresses a video or sound from the library on its own, without the
    // timeline: the whole file at its own size and frame rate, with the export settings.
    Q_INVOKABLE void convertAsset(const QString &assetId, const QUrl &, const QVariantMap &settings);
    // The timeline for other editors: OpenTimelineIO (.otio) or a CMX 3600 EDL (.edl), by the
    // file's extension. The status names what the format could not carry.
    Q_INVOKABLE void exportTimeline(const QUrl &);
    // Export queue: exports run one after another (state "exportQueue": file, label, status
    // waiting|exporting|done|failed|cancelled). Cancelling the running export pauses the queue
    // until startQueue().
    Q_INVOKABLE void queueExport(const QUrl &, const QVariantMap &settings);
    Q_INVOKABLE void removeQueued(int index);
    Q_INVOKABLE void startQueue();
    // Output size and file extension for export settings, for the export dialog.
    Q_INVOKABLE QVariantMap exportPreview(const QVariantMap &settings) const;
    Q_INVOKABLE void cancelJob();
    // Captions from and to subtitle files; the format follows the suffix: .srt, .vtt, .ass
    // (and .ssa for import).
    Q_INVOKABLE void importSrt(const QUrl &);
    Q_INVOKABLE bool exportSrt(const QUrl &);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE QVariantMap waveform(const QString &assetId) const {
        return m_analysis->waveform(assetId);
    }
    Q_INVOKABLE QVariantList blendModes() const {
        QVariantList result{QVariantMap{{"id", QString()}, {"label", "Normal"}}};
        for (const auto &[id, label] : cutlery::blendModes())
            result << QVariantMap{{"id", id}, {"label", label}};
        return result;
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
    // Text-based editing: transcribes the selected clip's speech (language as for captions; see
    // state "transcript": status none|running|ready|failed|unavailable, words with timeline
    // start/end frames and a filler flag). cutWords cuts the chosen words (indices into those
    // words) out of the clip and its detached audio, closing the gaps; removeFillers cuts every
    // hesitation sound ("äh", "ähm", "um"). Each is one undo step.
    Q_INVOKABLE void transcribeClip(const QString &language);
    Q_INVOKABLE void cutWords(const QVariantList &indices);
    Q_INVOKABLE void removeFillers();
    // Automatic correction of the selected video or image clip: measures its levels and colour
    // cast (FFmpeg signalstats, in the background) and sets brightness, contrast, temperature
    // and tint so the picture spans the usual range with neutral greys. One undo step; state
    // "autoColour": status measuring|done|failed.
    Q_INVOKABLE void autoColour();
    // Text styles kept for every project (data folder, styles.json): font, size, colours,
    // outline, shadow, box, alignment, spacing and animation of a title. saveTextStyle takes
    // them from the selected title (replacing a style of the same name); applyTextStyle sets
    // them on every selected clip with text, in one undo step.
    Q_INVOKABLE QVariantList textStyles() const;
    Q_INVOKABLE void saveTextStyle(const QString &name);
    Q_INVOKABLE void applyTextStyle(const QString &name);
    Q_INVOKABLE void removeTextStyle(const QString &name);
    // Brand kit for every project (data folder, brand.json and brand/): up to 24 colours
    // (#rrggbb, state "brandColors") offered next to the colour settings, and one logo picture
    // (state "brandLogo", copied into the data folder). addBrandLogo puts the logo on a free
    // top track over the whole project, small, in a corner (topLeft, topRight, bottomLeft,
    // bottomRight), in one undo step.
    Q_INVOKABLE void addBrandColor(const QString &color);
    Q_INVOKABLE void removeBrandColor(const QString &color);
    Q_INVOKABLE void setBrandLogo(const QUrl &file);
    Q_INVOKABLE void addBrandLogo(const QString &corner);
    // LUT library: the .cube and .3dl files in the data folder's luts/ (state "lutLibrary",
    // {name, path}, sorted by name). addLutToLibrary copies a file there and returns its path.
    Q_INVOKABLE QString addLutToLibrary(const QUrl &file);
    // Finds the shot changes in the selected video clip and splits it there, with any detached
    // audio, in one undo step. Sensitivity 0..1: higher finds subtler cuts.
    Q_INVOKABLE void splitAtScenes(double sensitivity = 0.5);
    // Splits the selected clip, with any detached audio, at every timeline marker inside it.
    Q_INVOKABLE void splitAtMarkers();
    // Puts the selected clips (one track, in timeline order) one after another from the first,
    // each ending on the next marker it can reach with its media: a cut on every beat.
    Q_INVOKABLE void fitToMarkers();
    // Sets the selected clip's key colour from its own picture (keys off) at a point of the
    // canvas (fractions of its width and height) at the playhead, and turns the colour key on.
    Q_INVOKABLE void pickKeyColor(double x, double y);
    // Finds the beats in the selected clip's sound and puts a timeline marker on every
    // `every`-th one (1, 2 or 4), asynchronously (state "beats": status finding|done|failed,
    // count, bpm). Existing markers stay; one undo step.
    Q_INVOKABLE void markBeats(int every = 1);
    // Freeze frame: holds the selected video clip's picture at the playhead for `seconds`. The
    // clip (and its detached audio) is split there and the rest moves later; the still keeps the
    // clip's size, position and look. Runs FFmpeg in the background; one undo step.
    Q_INVOKABLE void freezeFrame(double seconds = 2);
    // Sound effects: Cutlery's own (clicks, typing, swooshes) and those of an installed pack.
    // Each entry: id, name, category, seconds, licence, source, builtIn.
    Q_INVOKABLE QVariantList sounds() const;
    // The sound's file, written first for built-in sounds; empty on failure.
    Q_INVOKABLE QUrl soundFile(const QString &id);
    // Adds the sound at the playhead on the first track with room (or a new one). One undo step.
    Q_INVOKABLE void addSound(const QString &id);
    // Adds the sound at every transition, loudest at the cut, skipping cuts that have it already.
    Q_INVOKABLE void addSoundAtTransitions(const QString &id);
    // Automatic captions: transcribes every audible clip's media (language "auto", "de", "en",
    // ...) and puts the captions on the "AI captions" track, replacing earlier ones.
    // `style`: "" plain lines, "karaoke" (spoken word highlighted), "word" (one word at a time).
    // `maxChars` (20–80) per line and `lines` (1–2) per caption; two lines only for the plain
    // style, where a caption holds up to twice the characters, broken as evenly as possible.
    Q_INVOKABLE void generateCaptions(const QString &language, const QString &style = {},
                                      int maxChars = 42, int lines = 1);
    // A caption (a title on the caption track) at the playhead, 2 seconds or up to the next
    // caption, ready to type into.
    Q_INVOKABLE void addCaption();
    // Puts the asset at `frame` on `track` over whatever is there: clips (and their detached
    // sound) in that time are cut away, the rest of the track stays where it is.
    Q_INVOKABLE bool overwriteAsset(const QString &assetId, int track, qint64 frame);
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
    qint64 m_exportFrom = 0, m_exportTo = -1; // frame range of the running export
    // The last export, for "Try again"; encoders that failed in its render are left out when it
    // is retried automatically with the next one.
    QUrl m_lastExportUrl;
    QVariantMap m_lastExportSettings;
    QStringList m_failedEncoders;
    bool m_exportFailed = false;
    Project m_exportProject; // the timeline being exported, as it was when the export started
    std::optional<Clip> m_clipboard;
    void loadFonts(const QString &folder);
    QHash<QString, QString> m_fontFiles; // family → file, for fonts added in Cutlery
    QThread *m_collectThread = nullptr;
    QVariantMap m_collect;
    QVariantMap m_conform;
    QVariantMap m_follow;
    QString m_importFolder;
    QVariantMap m_reframe;
    QVariantMap m_autoColour;
    QVariantList m_textStyles, m_templates;
    QStringList m_brandColors;
    QString m_brandLogo;
    QVariantList m_lutLibrary;
    void saveBrand();
    void listLuts();
    void listTemplates();
    void saveTextStyles();
    QProcess *m_autoColourProcess = nullptr, *m_frameProcess = nullptr, *m_pickProcess = nullptr;
    // The selected clip's words in clip-local frames, cached per clip, transcript and revision.
    // Words of the run of back-to-back pieces of the selected clip's recording on its track (as
    // cuts leave them), in timeline order; start and end are local to the word's piece.
    struct ClipWord {
        QString clipId, text;
        qint64 start = 0, end = 0;
        bool filler = false;
    };
    mutable struct {
        QString clipId, path;
        qint64 revision = -1;
        QVector<ClipWord> words;
    } m_words;
    QStringList wordRun(const Clip &) const;
    QVector<ClipWord> clipWords(const Clip &) const;
    QVariantMap transcriptState() const;
    void cutClipWords(const QList<int> &indices, const QString &what);
    // The timelines above the open nested sequence, outermost first, with their undo history.
    struct NestFrame {
        Project parent;
        QString assetId;
        QVector<Project> undo, redo;
        QString clipId;
        qint64 playhead = 0;
    };
    QVector<NestFrame> m_nest;
    QProcess *m_nestedProcess = nullptr;
    QStringList m_nestedFailed; // cache files that could not be rendered this session
    QString nestedPath(const QJsonObject &content) const;
    void storeNested(Project &parent, const QString &assetId, const Project &child) const;
    Project wholeProject() const;
    Project viewable() const;
    void renderNested(); // {status: analysing|done|failed, clips: [ids], faces: count}
    void applyReframe();
    double m_playRate = 1, m_shuttleRate = 1;
    QTimer m_reverseTimer;
    void applyFollowFace();
    // The video clip under `c` at its start: the highest lower track with a video playing then.
    const Clip *videoBelow(const Clip &c) const;
    // The track nearest `home` with room for [start, start + length), or a new one on top.
    static int freeTrack(Project &, int home, qint64 start, qint64 length);
    QStringList m_recent;
    QVariantMap m_prefs;
    void loadPreferences();
    // At start: removes work folders left by earlier sessions, then the least recently written
    // cache files until the cache fits the limit (all of them after clearCacheAtStart()).
    void trimCache();
    void applyPreferences(Project &) const;
    struct QueuedExport {
        QUrl url;
        QVariantMap settings;
        Project project; // the timeline as it was when queued
        QString status = "waiting";
    };
    QVector<QueuedExport> m_queue;
    bool m_queuePaused = false;
    QTimer m_queueTimer;
    void advanceQueue();
    // `retry` keeps the encoders that failed in this export's earlier attempts left out.
    void exportProject(const Project &, const QUrl &, const QVariantMap &settings,
                       bool retry = false);
    QStringList m_also; // selected besides m_selected
    void remember(const QString &path);
    void saveRecent();
    QString backupFolder(const QString &projectPath) const;
    void backUp(const QString &projectPath);
    QVector<Sound> soundList() const;
    // Places the sound's clip with its start at `frame`; returns false when one is there already.
    bool placeSound(Project &, const Sound &, qint64 frame);
    QMediaCaptureSession *m_voiceSession = nullptr;
    QAudioInput *m_voiceInput = nullptr;
    QMediaRecorder *m_voiceRecorder = nullptr;
    qint64 m_voiceStart = 0;
    QElapsedTimer m_voiceClock;
    std::optional<Asset> m_clipboardAsset;
    // The other clips copied with m_clipboard, and their media.
    QVector<Clip> m_clipboardMore;
    QVector<Asset> m_clipboardMoreAssets;
    QProcess *m_loudnessProcess = nullptr;
    struct Pauses {
        QString clipId, status; // status: idle, finding, ready, failed
        QVector<QPair<qint64, qint64>> ranges; // clip-local frames
        qint64 revision = -1;
    } m_pauses;
    QProcess *m_pauseProcess = nullptr;
    QProcess *m_sceneProcess = nullptr;
    QVariantMap m_scenes; // splitAtScenes(): status finding|done|failed, count
    QThread *m_beatThread = nullptr;
    QVariantMap m_beats;
    QVariantMap pauseState() const;
    struct DropBatch {
        QString trackId;
        qint64 frame = 0;
        QString lastClip;
    };
    struct ImportRequest {
        QUrl url;
        std::shared_ptr<DropBatch> drop;
        QString folder; // library folder for media found in a dropped folder
    };
    // Requests for dropped files and folders (searched for media files, sorted by path).
    QList<ImportRequest> importRequests(const QList<QUrl> &, std::shared_ptr<DropBatch> drop) const;
    bool m_cancelProbe = false;
    QList<ImportRequest> m_importQueue;
    QStringList m_importErrors;
    void fail(const QString &);
    bool mutate(const std::function<void(Project &)> &);
    void edited();
    void requestPreview();
    void probeNext();
    void probeFile(const QUrl &, const QString &replaceId, std::shared_ptr<DropBatch> drop = {},
                   const QString &folder = {});
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
    int m_captionChars = 42, m_captionLines = 1;
    QStringList m_captionAssets; // media of a running caption request
    static int upscaleHeight(const Asset &);
    void addAiMedia(RenderOptions &) const;
    void autosave();
};
} // namespace cutlery
