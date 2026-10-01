#pragma once
#include "RationalTime.h"
#include <QJsonObject>
#include <QMap>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace cutlery {
QString newId();
struct Track {
    QString name;
    bool locked = false, muted = false, hidden = false, solo = false;
    bool snapping = true, magnetic = false;
    QString id = newId();
};
struct Asset {
    QString id, path, name, kind; // video, audio, image
    double duration = 0;
    int width = 0, height = 0;
    bool hasAudio = false;
};
// A property value at a clip-local frame. Smooth keyframes ease in and out towards the next one;
// others interpolate linearly.
struct Keyframe {
    qint64 frame = 0;
    double value = 0;
    bool smooth = true;
    bool operator==(const Keyframe &) const = default;
};
struct Clip {
    QString id, assetId, name;
    int track = 0;
    qint64 start = 0, duration = 1;
    Time sourceIn, speed{1};
    bool muted = false, hidden = false, reverse = false, flip = false, audioOnly = false;
    double scale = 1, x = 0, y = 0, rotation = 0, opacity = 1, volume = 1;
    double brightness = 0, contrast = 1, saturation = 1, crop = 0;
    double fadeIn = 0, fadeOut = 0;
    QString text, fontFamily = "Arial", textColor = "#ffffff";
    int fontSize = 72;
    // Transition from the clip that ends exactly where this one starts on the same track. It is
    // centred on the cut; both clips extend into the other's time using source handles or a held
    // frame, so the timeline length does not change.
    QString transition; // an xfade name from transitionTypes(); empty for a straight cut
    qint64 transitionFrames = 0;
    // Animated properties (see animatableProperties()), sorted by frame. Frames are relative to
    // the clip start and stay attached to the picture when the clip is trimmed or split. A
    // property with keyframes ignores its static value.
    QMap<QString, QVector<Keyframe>> keyframes;
    // Overlay styling for picture-in-picture (presenter) layouts.
    QString shape = "rect"; // rect, rounded, circle (centre square)
    double radius = 0.12;   // rounded corners, fraction of the shorter side
    double border = 0;      // border width, fraction of the canvas height at scale 1
    QString borderColor = "#ffffff";
    double shadow = 0; // soft drop shadow strength, 0..1
    // Background removal by colour (green/blue screen).
    bool chromaKey = false;
    QString keyColor = "#00ff00";
    double keySimilarity = 0.25, keyBlend = 0.08;
    // Background removal with the AI person matte of the asset (see AiJobs). Takes precedence
    // over the colour key; without an analysed matte the picture stays as it is.
    bool aiCutout = false;
    // Picture from the AI-upscaled copy of the asset (see AiJobs) when one covers the clip.
    bool aiUpscale = false;
    bool styled() const {
        return shape != "rect" || border > 0 || shadow > 0 || aiCutout;
    }
    double staticValue(const QString &property) const;
    // Property value at a clip-local frame, interpolating keyframes when present.
    double valueAt(const QString &property, double frame) const;
    void shiftKeyframes(qint64 delta);
    void scaleKeyframes(double factor);
};
const QStringList &animatableProperties();
// Supported transitions: FFmpeg xfade names paired with display labels.
const QVector<QPair<QString, QString>> &transitionTypes();
struct Project {
    QString name = "Untitled";
    int width = 1920, height = 1080, fpsN = 30, fpsD = 1, tracks = 3;
    QVector<Track> trackSettings{{"Track 1"}, {"Track 2"}, {"Track 3"}};
    QVector<Asset> assets;
    QVector<Clip> clips;
    qint64 duration() const;
    double seconds() const {
        return frameTime(duration(), fpsN, fpsD).seconds();
    }
    const Asset *asset(const QString &id) const;
    Clip *clip(const QString &id);
    const Clip *clip(const QString &id) const {
        return const_cast<Project *>(this)->clip(id);
    }
    // Size of a clip's picture fitted into a box at scale 1, before styling. Circles use the
    // centre square; equal-edge crop keeps the aspect ratio.
    QSizeF pictureSize(const Clip &c, double boxWidth, double boxHeight) const;
    // The clip a transition into `c` comes from, or nullptr when `c` does not start at a cut.
    const Clip *previousAdjacent(const Clip &c) const;
    // Effective transition length into `c` in frames (0 when inactive), limited by both clips.
    qint64 transitionLength(const Clip &c) const;
    QJsonObject json(const QString &baseDir = {}) const;
    static Project fromJson(const QJsonObject &json, const QString &baseDir);
    void validate() const;
    bool split(const QString &id, qint64 frame);
    void remove(const QString &id, bool ripple);
    void requireEditable(int track) const;
    bool audioEnabled(int track) const;
    void addTrack(const QString &name = {});
    void removeTrack(int track);
    void trim(const QString &id, qint64 start, qint64 end);
    QVector<QString> trackOrder(int track, const QString &exclude = {}) const;
    void packTrack(int track, const QVector<QString> &order);
    qint64 placement(int track, qint64 frame, const QString &exclude = {}) const;
    void move(const QString &id, int track, qint64 frame);
    qint64 snap(qint64 frame, qint64 threshold, const QString &exclude, qint64 playhead,
                qint64 length = 0) const;
};
QString newId();
QString readUtf8File(const QString &path);
void saveProject(const Project &project, const QString &path);
Project loadProject(const QString &path);
} // namespace cutlery
