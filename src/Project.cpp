#include "Project.h"
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <stdexcept>

namespace cutlery {
static void require(bool ok, const QString &message) {
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
QString newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
qint64 Project::duration() const {
    qint64 n = 0;
    for (const auto &c : clips)
        n = std::max(n, c.start + c.duration);
    return n;
}
const Asset *Project::asset(const QString &id) const {
    for (const auto &a : assets)
        if (a.id == id)
            return &a;
    return nullptr;
}
Clip *Project::clip(const QString &id) {
    for (auto &c : clips)
        if (c.id == id)
            return &c;
    return nullptr;
}
static QJsonObject timeJson(Time t) {
    return {{"num", QString::number(t.n)}, {"den", QString::number(t.d)}};
}
static Time timeRead(QJsonValue v) {
    require(v.isObject(), "Missing rational time");
    const auto o = v.toObject();
    bool a = false, b = false;
    auto n = o["num"].toString().toLongLong(&a), d = o["den"].toString().toLongLong(&b);
    require(a && b && d > 0 && d <= 1000000000 && std::abs(double(n)) <= 1e14,
            "Invalid rational time");
    return {n, d};
}
static qint64 integer(QJsonValue v) {
    bool ok = false;
    auto x = v.toString().toLongLong(&ok);
    require(ok, "Invalid integer timestamp");
    return x;
}
QJsonObject Project::json(const QString &base) const {
    QJsonArray aa, cc;
    for (const auto &a : assets) {
        auto path = a.path;
        if (!base.isEmpty())
            path = QDir(base).relativeFilePath(path);
        aa.append(QJsonObject{{"id", a.id},
                              {"path", path},
                              {"name", a.name},
                              {"kind", a.kind},
                              {"duration", a.duration},
                              {"width", a.width},
                              {"height", a.height},
                              {"audio", a.hasAudio}});
    }
    for (const auto &c : clips) {
        QJsonObject o{{"id", c.id},
                      {"assetId", c.assetId},
                      {"name", c.name},
                      {"track", c.track},
                      {"start", QString::number(c.start)},
                      {"duration", QString::number(c.duration)},
                      {"sourceIn", timeJson(c.sourceIn)},
                      {"speed", timeJson(c.speed)},
                      {"muted", c.muted},
                      {"hidden", c.hidden},
                      {"reverse", c.reverse},
                      {"flip", c.flip},
                      {"text", c.text},
                      {"fontFamily", c.fontFamily},
                      {"textColor", c.textColor},
                      {"fontSize", c.fontSize}};
#define PUT(k) o[#k] = c.k
        PUT(scale);
        PUT(x);
        PUT(y);
        PUT(rotation);
        PUT(opacity);
        PUT(volume);
        PUT(brightness);
        PUT(contrast);
        PUT(saturation);
        PUT(crop);
        PUT(fadeIn);
        PUT(fadeOut);
#undef PUT
        cc.append(o);
    }
    return {{"format", "cutlery"}, {"schemaVersion", 1}, {"name", name}, {"width", width},
            {"height", height},    {"fpsN", fpsN},       {"fpsD", fpsD}, {"tracks", tracks},
            {"assets", aa},        {"clips", cc}};
}
Project Project::fromJson(const QJsonObject &o, const QString &base) {
    require(o["format"] == "cutlery" && o["schemaVersion"].toInt() == 1,
            "Unsupported project format/version. Original left unchanged.");
    require(o["assets"].isArray() && o["clips"].isArray(), "Missing project collections");
    Project p;
    p.name = o["name"].toString();
    p.width = o["width"].toInt();
    p.height = o["height"].toInt();
    p.fpsN = o["fpsN"].toInt();
    p.fpsD = o["fpsD"].toInt();
    p.tracks = o["tracks"].toInt();
    require(o["assets"].toArray().size() <= 10000 && o["clips"].toArray().size() <= 10000,
            "Project exceeds alpha resource limits");
    for (auto v : o["assets"].toArray()) {
        auto j = v.toObject();
        Asset a;
        a.id = j["id"].toString();
        a.path = j["path"].toString();
        if (QDir::isRelativePath(a.path))
            a.path = QDir(base).absoluteFilePath(a.path);
        a.path = QDir::cleanPath(a.path);
        a.name = j["name"].toString();
        a.kind = j["kind"].toString();
        a.duration = j["duration"].toDouble();
        a.width = j["width"].toInt();
        a.height = j["height"].toInt();
        a.hasAudio = j["audio"].toBool();
        p.assets.push_back(a);
    }
    for (auto v : o["clips"].toArray()) {
        auto j = v.toObject();
        Clip c;
        c.id = j["id"].toString();
        c.assetId = j["assetId"].toString();
        c.name = j["name"].toString();
        c.track = j["track"].toInt();
        c.start = integer(j["start"]);
        c.duration = integer(j["duration"]);
        c.sourceIn = timeRead(j["sourceIn"]);
        c.speed = timeRead(j["speed"]);
        c.muted = j["muted"].toBool();
        c.hidden = j["hidden"].toBool();
        c.reverse = j["reverse"].toBool();
        c.flip = j["flip"].toBool();
        c.text = j["text"].toString();
        c.fontFamily = j["fontFamily"].toString("Arial");
        c.textColor = j["textColor"].toString("#ffffff");
        c.fontSize = j["fontSize"].toInt(72);
#define GET(k, def) c.k = j[#k].toDouble(def)
        GET(scale, 1);
        GET(x, 0);
        GET(y, 0);
        GET(rotation, 0);
        GET(opacity, 1);
        GET(volume, 1);
        GET(brightness, 0);
        GET(contrast, 1);
        GET(saturation, 1);
        GET(crop, 0);
        GET(fadeIn, 0);
        GET(fadeOut, 0);
#undef GET
        p.clips.push_back(c);
    }
    p.validate();
    return p;
}
void Project::validate() const {
    require(width >= 64 && width <= 7680 && height >= 64 && height <= 7680 && width % 2 == 0 &&
                height % 2 == 0,
            "Canvas must have even dimensions, 64–7680 px");
    require(fpsN > 0 && fpsN <= 120000 && fpsD > 0 && fpsD <= 1001 && double(fpsN) / fpsD >= 1 &&
                double(fpsN) / fpsD <= 120,
            "Frame rate must be 1–120 fps");
    require(tracks > 0 && tracks <= 64, "Alpha supports 1–64 tracks");
    require(assets.size() <= 10000 && clips.size() <= 10000,
            "Project exceeds alpha resource limits");
    QSet<QString> ids;
    for (const auto &a : assets) {
        require(!a.id.isEmpty() && !ids.contains(a.id), "Duplicate asset ID");
        ids.insert(a.id);
        require(a.kind == "video" || a.kind == "audio" || a.kind == "image", "Invalid asset type");
        require(!a.path.isEmpty() && !a.path.contains("://") && std::isfinite(a.duration) &&
                    a.duration >= 0,
                "Invalid asset path/duration");
    }
    ids.clear();
    for (const auto &c : clips) {
        require(!c.id.isEmpty() && !ids.contains(c.id), "Duplicate clip ID");
        ids.insert(c.id);
        require(c.assetId.isEmpty() || asset(c.assetId), "Missing asset reference");
        require(c.start >= 0 && c.start <= 100000000 && c.duration > 0 &&
                    c.duration <= 100000000 - c.start && c.track >= 0 && c.track < tracks,
                "Invalid clip range/track");
        require(c.sourceIn.n >= 0 && c.speed.seconds() >= 0.25 && c.speed.seconds() <= 4,
                "Invalid source time/speed");
        auto bounded = [](double x, double lo, double hi) {
            return std::isfinite(x) && x >= lo && x <= hi;
        };
        require(bounded(c.scale, 0.1, 3) && bounded(c.x, -2, 2) && bounded(c.y, -2, 2) &&
                    bounded(c.rotation, -360, 360),
                "Invalid transform");
        require(bounded(c.opacity, 0, 1) && bounded(c.volume, 0, 4) &&
                    bounded(c.brightness, -0.5, 0.5) && bounded(c.contrast, 0.1, 3) &&
                    bounded(c.saturation, 0, 3) && bounded(c.crop, 0, 0.45),
                "Invalid effect value");
        require(bounded(c.fadeIn, 0, 3600) && bounded(c.fadeOut, 0, 3600) && c.fontSize >= 8 &&
                    c.fontSize <= 500 && c.text.size() <= 10000 && QColor(c.textColor).isValid(),
                "Invalid text/fade value");
        if (const auto *a = asset(c.assetId); a && a->kind != "image")
            require(c.sourceIn.seconds() +
                            frameTime(c.duration, fpsN, fpsD).seconds() * c.speed.seconds() <=
                        a->duration + 0.002,
                    "Clip extends past source media");
    }
    require(seconds() <= 24 * 3600, "Alpha projects are limited to 24 hours");
}
bool Project::split(const QString &id, qint64 frame) {
    auto *c = clip(id);
    if (!c || frame <= c->start || frame >= c->start + c->duration)
        return false;
    Clip b = *c;
    const auto left = frame - c->start;
    b.id = newId();
    b.start = frame;
    b.duration = c->duration - left;
    if (c->reverse) {
        c->sourceIn = c->sourceIn + frameTime(b.duration, fpsN, fpsD) * c->speed;
    } else
        b.sourceIn = c->sourceIn + frameTime(left, fpsN, fpsD) * c->speed;
    c->duration = left;
    c->fadeOut = 0;
    b.fadeIn = 0;
    clips.push_back(b);
    return true;
}
void Project::remove(const QString &id, bool ripple) {
    const auto *c = clip(id);
    if (!c)
        return;
    const auto start = c->start, dur = c->duration;
    const auto track = c->track;
    clips.erase(
        std::remove_if(clips.begin(), clips.end(), [&](const Clip &x) { return x.id == id; }),
        clips.end());
    if (ripple)
        for (auto &x : clips)
            if (x.track == track && x.start >= start + dur)
                x.start -= dur;
}
QString readUtf8File(const QString &path) {
    QFile f(path);
    require(f.open(QIODevice::ReadOnly), "Cannot open " + path);
    require(f.size() <= 32 * 1024 * 1024, "Project/text file too large");
    return QString::fromUtf8(f.readAll());
}
void saveProject(const Project &p, const QString &path) {
    p.validate();
    QSaveFile f(path);
    f.setDirectWriteFallback(false);
    require(f.open(QIODevice::WriteOnly), f.errorString());
    const auto data = QJsonDocument(p.json(QFileInfo(path).absolutePath())).toJson();
    require(f.write(data) == data.size(), f.errorString());
    require(f.commit(), f.errorString());
}
Project loadProject(const QString &path) {
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(readUtf8File(path).toUtf8(), &e);
    require(e.error == QJsonParseError::NoError && doc.isObject(), "Invalid project JSON");
    return Project::fromJson(doc.object(), QFileInfo(path).absolutePath());
}
} // namespace cutlery
