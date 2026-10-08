#include "Interchange.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace cutlery {
namespace {
QJsonObject rationalTime(double value, double rate) {
    return {{"OTIO_SCHEMA", "RationalTime.1"}, {"rate", rate}, {"value", value}};
}
QJsonObject timeRange(double start, double duration, double rate) {
    return {{"OTIO_SCHEMA", "TimeRange.1"},
            {"start_time", rationalTime(start, rate)},
            {"duration", rationalTime(duration, rate)}};
}
QJsonObject item(const QString &schema, const QString &name) {
    return {{"OTIO_SCHEMA", schema},
            {"name", name},
            {"effects", QJsonArray{}},
            {"markers", QJsonArray{}},
            {"metadata", QJsonObject{}}};
}
// Notes what a clip loses on the way out; `what` collects one line per kind.
void noteLosses(const Clip &c, QMap<QString, int> &what) {
    if (!c.keyframes.isEmpty())
        what["keyframed animation"]++;
    if (c.brightness != 0 || c.contrast != 1 || c.saturation != 1 || c.temperature != 0 ||
        c.tint != 0 || c.vibrance != 0 || c.shadows != 0 || c.highlights != 0 || !c.lut.isEmpty() ||
        !c.curveMaster.isEmpty() || c.hslSaturation != 0 || c.hslHue != 0 || c.exposure != 0)
        what["colour settings"]++;
    if (c.scale != 1 || c.x != 0 || c.y != 0 || c.rotation != 0 || c.crop != 0 || c.flip ||
        c.flipVertical || c.cropLeft != 0 || c.cropRight != 0 || c.cropTop != 0 ||
        c.cropBottom != 0 || c.shape != "rect" || c.feather > 0 || !c.cornerPin.isEmpty())
        what["position, size, crop and shape"]++;
    if (!c.transition.isEmpty())
        what["transitions (as straight cuts)"]++;
    if (!c.fx.isEmpty() || c.blur > 0 || c.sharpen > 0 || c.glow > 0 || c.vignette > 0 ||
        c.grain > 0 || c.stabilize || c.chromaKey || c.aiCutout ||
        !c.lumaKey.isEmpty() || !c.blendMode.isEmpty() || c.pitch != 0 || !c.voice.isEmpty() ||
        !c.canvasFill.isEmpty())
        what["effects"]++;
    if (c.reverse)
        what["reversed clips (exported forwards)"]++;
    if (c.fadeIn > 0 || c.fadeOut > 0 || c.volume != 1)
        what["fades and volume"]++;
}
QStringList lines(const QMap<QString, int> &what) {
    QStringList out;
    for (auto it = what.begin(); it != what.end(); ++it)
        out << QString("%1: %2 clip%3").arg(it.key()).arg(it.value()).arg(it.value() == 1 ? "" : "s");
    return out;
}
bool isMedia(const Project &p, const Clip &c) {
    const auto *a = p.asset(c.assetId);
    return a && !a->isNested();
}
} // namespace

QJsonObject otioTimeline(const Project &p, QStringList *lost) {
    const double rate = double(p.fpsN) / p.fpsD;
    QMap<QString, int> what;
    QJsonArray tracks;
    // OTIO stacks tracks bottom first, like Cutlery's track 0 under track 1.
    for (int t = 0; t < p.tracks; ++t) {
        QVector<const Clip *> clips;
        for (const auto &c : p.clips)
            if (c.track == t)
                clips << &c;
        std::sort(clips.begin(), clips.end(), [](const Clip *a, const Clip *b) { return a->start < b->start; });
        bool pictures = false;
        for (const auto *c : clips)
            if (const auto *a = p.asset(c->assetId); !a || (a->kind != "audio" && !c->audioOnly))
                pictures = true;
        auto track = item("Track.1", p.trackSettings.value(t).name);
        track["kind"] = clips.isEmpty() || pictures ? "Video" : "Audio";
        track["source_range"] = QJsonValue::Null;
        QJsonArray children;
        qint64 at = 0;
        for (const auto *c : clips) {
            if (c->start < at) {
                what["clips overlapping another on their track"]++;
                continue;
            }
            if (c->start > at) {
                auto gap = item("Gap.1", "");
                gap["source_range"] = timeRange(0, double(c->start - at), rate);
                children << gap;
            }
            if (!isMedia(p, *c)) {
                // Titles, graphics, effect areas and nested sequences stay as gaps.
                what[c->effect.isEmpty() && c->assetId.isEmpty() ? "titles and graphics"
                     : c->assetId.isEmpty()                       ? "effect areas and adjustment layers"
                                                                  : "nested sequences"]++;
                auto gap = item("Gap.1", c->name);
                gap["source_range"] = timeRange(0, double(c->duration), rate);
                children << gap;
            } else {
                const auto *a = p.asset(c->assetId);
                const double speed = c->speed.seconds();
                auto clip = item("Clip.2", c->name);
                // The source range in timeline frames; a time warp says how fast it plays.
                clip["source_range"] = timeRange(c->sourceIn.seconds() * rate, double(c->duration), rate);
                QJsonObject reference{{"OTIO_SCHEMA", "ExternalReference.1"},
                                      {"name", a->name},
                                      {"target_url", QUrl::fromLocalFile(a->path).toString()},
                                      {"available_range", a->endless() ? QJsonValue(QJsonValue::Null)
                                                                       : QJsonValue(timeRange(0, a->duration * rate, rate))},
                                      {"metadata", QJsonObject{}}};
                clip["media_references"] = QJsonObject{{"DEFAULT_MEDIA", reference}};
                clip["active_media_reference_key"] = "DEFAULT_MEDIA";
                if (speed != 1)
                    clip["effects"] = QJsonArray{QJsonObject{{"OTIO_SCHEMA", "LinearTimeWarp.1"},
                                                             {"name", "Speed"},
                                                             {"effect_name", "LinearTimeWarp"},
                                                             {"time_scalar", speed},
                                                             {"metadata", QJsonObject{}}}};
                noteLosses(*c, what);
                children << clip;
            }
            at = c->start + c->duration;
        }
        track["children"] = children;
        tracks << track;
    }
    QJsonArray markers;
    for (const auto &m : p.markers) {
        auto marker = item("Marker.2", m.name);
        marker.remove("effects");
        marker.remove("markers");
        marker["marked_range"] = timeRange(double(m.frame), 0, rate);
        marker["color"] = "RED";
        markers << marker;
    }
    auto stack = item("Stack.1", "tracks");
    stack["children"] = tracks;
    stack["markers"] = markers;
    stack["source_range"] = QJsonValue::Null;
    if (lost)
        *lost = lines(what);
    return {{"OTIO_SCHEMA", "Timeline.1"},
            {"name", p.name},
            {"global_start_time", QJsonValue::Null},
            {"tracks", stack},
            {"metadata", QJsonObject{{"cutlery", QJsonObject{{"lost", QJsonArray::fromStringList(lines(what))}}}}}};
}

QString cmxEdl(const Project &p, QStringList *lost) {
    // Whole frames a second for the timecode (29.97 is written non-drop at 30).
    const int fps = std::max(1, int(std::lround(double(p.fpsN) / p.fpsD)));
    const double rate = double(p.fpsN) / p.fpsD;
    auto timecode = [fps](qint64 frames) {
        frames = std::max<qint64>(0, frames);
        return QString("%1:%2:%3:%4")
            .arg(frames / (3600LL * fps), 2, 10, QChar('0'))
            .arg(frames / (60LL * fps) % 60, 2, 10, QChar('0'))
            .arg(frames / fps % 60, 2, 10, QChar('0'))
            .arg(frames % fps, 2, 10, QChar('0'));
    };
    QMap<QString, int> what;
    // The lowest track with media pictures.
    int track = -1;
    for (const auto &c : p.clips)
        if (isMedia(p, c) && p.asset(c.assetId)->kind != "audio" && !c.audioOnly &&
            (track < 0 || c.track < track))
            track = c.track;
    QVector<const Clip *> clips;
    for (const auto &c : p.clips) {
        if (c.track == track && isMedia(p, c))
            clips << &c;
        else
            what[c.track == track ? "titles, graphics and nested sequences" : "clips on other tracks"]++;
    }
    std::sort(clips.begin(), clips.end(), [](const Clip *a, const Clip *b) { return a->start < b->start; });
    const qint64 record = 3600LL * fps; // 01:00:00:00
    QString out = QString("TITLE: %1\nFCM: NON-DROP FRAME\n\n").arg(p.name.left(60));
    int event = 0;
    qint64 at = 0;
    for (const auto *c : clips) {
        if (c->start < at) {
            what["clips overlapping another on their track"]++;
            continue;
        }
        const auto *a = p.asset(c->assetId);
        const qint64 sourceIn = qRound64(c->sourceIn.seconds() * rate);
        const qint64 sourceOut = sourceIn + qRound64(c->duration * c->speed.seconds());
        const QString channels = a->kind == "audio" || c->audioOnly ? "A" : a->hasAudio ? "B" : "V";
        out += QString("%1  AX       %2     C        %3 %4 %5 %6\n")
                   .arg(++event, 3, 10, QChar('0'))
                   .arg(channels, -2)
                   .arg(timecode(sourceIn), timecode(sourceOut), timecode(record + c->start),
                        timecode(record + c->start + c->duration));
        if (c->speed.seconds() != 1)
            out += QString("M2   AX       %1 %2\n")
                       .arg(QString::number(c->speed.seconds() * fps, 'f', 1).rightJustified(5, '0'))
                       .arg(timecode(sourceIn));
        out += "* FROM CLIP NAME: " + QFileInfo(a->path).fileName() + "\n\n";
        noteLosses(*c, what);
        at = c->start + c->duration;
    }
    if (lost)
        *lost = lines(what);
    for (const auto &line : lines(what))
        out += "* NOT CARRIED: " + line + "\n";
    return out;
}
} // namespace cutlery
