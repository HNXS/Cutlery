#pragma once
#include "RationalTime.h"
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace cutlery {
QString newId();
struct Track {
    QString name;
    bool locked = false, muted = false, hidden = false, solo = false;
};
struct Asset {
    QString id, path, name, kind; // video, audio, image
    double duration = 0;
    int width = 0, height = 0;
    bool hasAudio = false;
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
};
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
    qint64 snap(qint64 frame, qint64 threshold, const QString &exclude, qint64 playhead,
                qint64 length = 0) const;
};
QString newId();
QString readUtf8File(const QString &path);
void saveProject(const Project &project, const QString &path);
Project loadProject(const QString &path);
} // namespace cutlery
