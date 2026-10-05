#include "Project.h"
#include <QRegularExpression>
#include <QColor>
#include <algorithm>
#include <cmath>
#include <numbers>
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
const QVector<QPair<QString, QString>> &transitionTypes() {
    static const QVector<QPair<QString, QString>> types{
        {"fade", "Dissolve"},          {"fadeblack", "Dip to black"},
        {"fadewhite", "Dip to white"}, {"wipeleft", "Wipe left"},
        {"wiperight", "Wipe right"},   {"slideleft", "Slide left"},
        {"slideright", "Slide right"}, {"slideup", "Slide up"},
        {"slidedown", "Slide down"},   {"smoothleft", "Smooth left"},
        {"zoomin", "Zoom in"},         {"circleopen", "Circle open"},
        {"radial", "Radial"},          {"pixelize", "Pixelize"},
        {"wipeup", "Wipe up"},         {"wipedown", "Wipe down"},
        {"coverleft", "Push in from right"}, {"coverright", "Push in from left"},
        {"coverup", "Push in from below"},   {"coverdown", "Push in from above"},
        {"revealleft", "Reveal left"}, {"revealright", "Reveal right"},
        {"smoothright", "Smooth right"}, {"circleclose", "Circle close"},
        {"hblur", "Blur"},             {"fadegrays", "Fade through grey"},
        {"squeezeh", "Squeeze"},       {"dissolve", "Pixel dissolve"},
        {"hlwind", "Wind"}};
    return types;
}
const QStringList &animatableProperties() {
    static const QStringList properties{"scale", "x", "y", "rotation", "opacity", "volume", "pan"};
    return properties;
}
// Valid range of each animatable property, shared with static-value validation.
static std::pair<double, double> propertyRange(const QString &p) {
    if (p == "scale")
        return {0.1, 3};
    if (p == "x" || p == "y")
        return {-2, 2};
    if (p == "rotation")
        return {-360, 360};
    if (p == "opacity")
        return {0, 1};
    if (p == "pan")
        return {-1, 1};
    return {0, 4}; // volume
}
namespace {
// Colour, look and sound values stored only when not 0.
const QVector<QPair<QString, double Clip::*>> &lookFields() {
    static const QVector<QPair<QString, double Clip::*>> fields{
        {"temperature", &Clip::temperature}, {"tint", &Clip::tint},
        {"vibrance", &Clip::vibrance},       {"shadows", &Clip::shadows},
        {"highlights", &Clip::highlights},   {"sharpen", &Clip::sharpen},
        {"glow", &Clip::glow},               {"vignette", &Clip::vignette},
        {"grain", &Clip::grain},             {"eqLow", &Clip::eqLow},
        {"eqMid", &Clip::eqMid},             {"eqHigh", &Clip::eqHigh},
        {"lowCut", &Clip::lowCut},           {"compressor", &Clip::compressor},
        {"gate", &Clip::gate},               {"denoise", &Clip::denoise},
        {"deess", &Clip::deess},             {"motionBlur", &Clip::motionBlur},
        {"reverb", &Clip::reverb},           {"echo", &Clip::echo},
        {"pan", &Clip::pan}};
    return fields;
}
} // namespace
const QStringList &Clip::lookProperties() {
    static const QStringList names{"brightness", "contrast",   "saturation", "blur",
                                   "temperature", "tint",      "vibrance",   "shadows",
                                   "highlights", "sharpen",    "glow",       "vignette",
                                   "grain",      "lut",        "lutStrength"};
    return names;
}
double Clip::staticValue(const QString &p) const {
    if (p == "scale")
        return scale;
    if (p == "x")
        return x;
    if (p == "y")
        return y;
    if (p == "rotation")
        return rotation;
    if (p == "opacity")
        return opacity;
    if (p == "pan")
        return pan;
    return volume;
}
double Clip::valueAt(const QString &p, double frame) const {
    const auto k = keyframes.value(p);
    if (k.isEmpty())
        return staticValue(p);
    if (frame <= k.first().frame)
        return k.first().value;
    if (frame >= k.last().frame)
        return k.last().value;
    int i = 0;
    while (frame >= k[i + 1].frame)
        ++i;
    double u = (frame - k[i].frame) / double(k[i + 1].frame - k[i].frame);
    if (k[i].smooth)
        u = u * u * (3 - 2 * u);
    return k[i].value + (k[i + 1].value - k[i].value) * u;
}
void Clip::shiftKeyframes(qint64 delta) {
    for (auto &list : keyframes)
        for (auto &k : list)
            k.frame += delta;
    for (auto &w : wordStarts)
        w += delta;
}
QStringList captionWords(const QString &text) {
    return text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
}
bool Clip::timedWords() const {
    return assetId.isEmpty() && !wordStarts.isEmpty() &&
           wordStarts.size() == captionWords(text).size();
}
void Clip::scaleKeyframes(double factor) {
    for (auto &list : keyframes) {
        QVector<Keyframe> scaled;
        for (auto k : list) {
            k.frame = qRound64(k.frame * factor);
            if (!scaled.isEmpty() && scaled.last().frame == k.frame)
                scaled.last() = k;
            else
                scaled.push_back(k);
        }
        list = scaled;
    }
}
bool isVariableRate(double nominal, double average) {
    return nominal > 0 && average > 0 && std::abs(nominal - average) / nominal > 0.01;
}
double standardRate(double rate) {
    for (double r : {24000. / 1001, 24., 25., 30000. / 1001, 30., 48., 50., 60000. / 1001, 60.})
        if (std::abs(rate - r) / r < 0.04)
            return r;
    return rate;
}
const QStringList &graphicKinds() {
    static const QStringList kinds{"rectangle", "ellipse", "arrow", "line", "bubble"};
    return kinds;
}
QSizeF Project::pictureSize(const Clip &c, double boxWidth, double boxHeight) const {
    if (!c.effect.isEmpty())
        return {boxWidth * c.effectWidth, boxHeight * c.effectHeight};
    if (!c.graphic.isEmpty())
        return {boxWidth * c.graphicWidth, boxHeight * c.graphicHeight};
    const auto *a = asset(c.assetId);
    double aspect = a && a->width > 0 && a->height > 0 ? double(a->width) / a->height
                                                       : double(width) / height;
    if (c.shape == "circle")
        aspect = 1;
    if (boxWidth / boxHeight > aspect)
        return {boxHeight * aspect, boxHeight};
    return {boxWidth, boxWidth / aspect};
}
QPointF Project::anchorShift(const Clip &c, QSizeF base, double scale, double rotation) {
    // The anchor stays where it is at scale 1 without rotation; the centre moves around it.
    const double dx = (c.anchorX - 0.5) * base.width(), dy = (c.anchorY - 0.5) * base.height();
    const double a = rotation * std::numbers::pi / 180, cs = std::cos(a), sn = std::sin(a);
    return {dx - scale * (cs * dx - sn * dy), dy - scale * (sn * dx + cs * dy)};
}
const Clip *Project::previousAdjacent(const Clip &c) const {
    const Clip *found = nullptr;
    for (const auto &x : clips)
        if (x.id != c.id && x.track == c.track && x.start + x.duration == c.start)
            found = &x;
    return found;
}
qint64 Project::transitionLength(const Clip &c) const {
    if (c.transition.isEmpty() || c.transitionFrames < 2 || !c.effect.isEmpty())
        return 0;
    const auto *prev = previousAdjacent(c);
    if (!prev || !prev->effect.isEmpty())
        return 0;
    // Each clip gives at most its whole length, so a transition never spans three clips.
    return std::min({c.transitionFrames, prev->duration, c.duration});
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
    QJsonArray aa, cc, tt;
    for (const auto &t : trackSettings)
        tt.append(QJsonObject{{"name", t.name},
                              {"locked", t.locked},
                              {"muted", t.muted},
                              {"hidden", t.hidden},
                              {"solo", t.solo},
                              {"snapping", t.snapping},
                              {"magnetic", t.magnetic},
                              {"id", t.id}});
    for (const auto &a : assets) {
        auto path = a.path;
        if (!base.isEmpty())
            path = QDir(base).relativeFilePath(path);
        QJsonObject o{{"id", a.id},          {"path", path},           {"name", a.name},
                      {"kind", a.kind},      {"duration", a.duration}, {"width", a.width},
                      {"height", a.height},  {"audio", a.hasAudio}};
        if (a.frameRate > 0)
            o["frameRate"] = a.frameRate;
        if (a.variableRate)
            o["variableRate"] = true;
        if (a.loops)
            o["loops"] = true;
        aa.append(o);
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
                      {"audioOnly", c.audioOnly},
                      {"muted", c.muted},
                      {"hidden", c.hidden},
                      {"reverse", c.reverse},
                      {"flip", c.flip},
                      {"text", c.text},
                      {"fontFamily", c.fontFamily},
                      {"textColor", c.textColor},
                      {"fontSize", c.fontSize}};
        if (!c.link.isEmpty())
            o["link"] = c.link;
        if (!c.group.isEmpty())
            o["group"] = c.group;
        if (c.anchorX != 0.5 || c.anchorY != 0.5)
            o["anchor"] = QJsonArray{c.anchorX, c.anchorY};
        // Text style, stored when it differs from the default.
        if (!c.bold)
            o["bold"] = false;
        if (c.italic)
            o["italic"] = true;
        if (c.align != "center")
            o["align"] = c.align;
        if (c.letterSpacing != 0)
            o["letterSpacing"] = c.letterSpacing;
        if (c.lineSpacing != 1)
            o["lineSpacing"] = c.lineSpacing;
        if (c.outline > 0) {
            o["outline"] = c.outline;
            o["outlineColor"] = c.outlineColor;
        }
        if (c.textShadow != 1)
            o["textShadow"] = c.textShadow;
        if (c.background > 0) {
            o["background"] = c.background;
            o["backgroundColor"] = c.backgroundColor;
        }
        if (!c.textAnimation.isEmpty()) {
            o["textAnimation"] = c.textAnimation;
            o["textAnimationTime"] = c.textAnimationTime;
        }
        if (!c.titleStyle.isEmpty()) {
            o["titleStyle"] = c.titleStyle;
            o["accentColor"] = c.accentColor;
        }
        if (!c.effect.isEmpty()) {
            o["effect"] = c.effect;
            o["effectStrength"] = c.effectStrength;
            o["effectWidth"] = c.effectWidth;
            o["effectHeight"] = c.effectHeight;
        }
        if (c.blur > 0)
            o["blur"] = c.blur;
        for (const auto &[k, field] : lookFields())
            if (c.*field != 0)
                o[k] = c.*field;
        if (!c.slowMotion.isEmpty())
            o["slowMotion"] = c.slowMotion;
        if (!c.fx.isEmpty()) {
            o["fx"] = c.fx;
            o["fxStrength"] = c.fxStrength;
        }
        if (c.stabilize)
            o["stabilize"] = true;
        if (!c.graphic.isEmpty()) {
            o["graphic"] = c.graphic;
            o["fillColor"] = c.fillColor;
            o["strokeColor"] = c.strokeColor;
            o["stroke"] = c.stroke;
            o["graphicWidth"] = c.graphicWidth;
            o["graphicHeight"] = c.graphicHeight;
        }
        if (!c.lut.isEmpty()) {
            o["lut"] = base.isEmpty() ? c.lut : QDir(base).relativeFilePath(c.lut);
            o["lutStrength"] = c.lutStrength;
        }
        if (!c.captionStyle.isEmpty() || !c.wordStarts.isEmpty()) {
            o["captionStyle"] = c.captionStyle;
            o["highlightColor"] = c.highlightColor;
            QJsonArray starts;
            for (auto w : c.wordStarts)
                starts.append(QString::number(w));
            o["wordStarts"] = starts;
        }
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
        if (!c.keyframes.isEmpty()) {
            QJsonObject animated;
            for (auto it = c.keyframes.begin(); it != c.keyframes.end(); ++it) {
                QJsonArray list;
                for (const auto &k : *it)
                    list.append(QJsonArray{QString::number(k.frame), k.value, k.smooth});
                animated[it.key()] = list;
            }
            o["keyframes"] = animated;
        }
        o["shape"] = c.shape;
        o["radius"] = c.radius;
        o["border"] = c.border;
        o["borderColor"] = c.borderColor;
        o["shadow"] = c.shadow;
        o["chromaKey"] = c.chromaKey;
        o["keyColor"] = c.keyColor;
        o["keySimilarity"] = c.keySimilarity;
        o["keyBlend"] = c.keyBlend;
        o["aiCutout"] = c.aiCutout;
        o["aiUpscale"] = c.aiUpscale;
        if (c.eyeContact)
            o["eyeContact"] = true;
        o["transition"] = c.transition;
        o["transitionFrames"] = QString::number(c.transitionFrames);
        cc.append(o);
    }
    QJsonObject o{{"format", "cutlery"}, {"schemaVersion", 11}, {"name", name},
                  {"width", width},      {"height", height},    {"fpsN", fpsN},
                  {"fpsD", fpsD},        {"tracks", tracks},    {"assets", aa},
                  {"clips", cc},         {"trackSettings", tt}};
    if (!markers.isEmpty()) {
        QJsonArray mm;
        for (const auto &m : markers)
            mm.append(QJsonObject{
                {"frame", QString::number(m.frame)}, {"name", m.name}, {"color", m.color}});
        o["markers"] = mm;
    }
    if (inPoint >= 0)
        o["inPoint"] = QString::number(inPoint);
    if (outPoint >= 0)
        o["outPoint"] = QString::number(outPoint);
    return o;
}
Project Project::fromJson(const QJsonObject &o, const QString &base) {
    require(o["format"] == "cutlery" &&
                (o["schemaVersion"].toInt() >= 1 && o["schemaVersion"].toInt() <= 11),
            "Unsupported project format/version. Original left unchanged.");
    require(o["assets"].isArray() && o["clips"].isArray(), "Missing project collections");
    Project p;
    p.name = o["name"].toString();
    p.width = o["width"].toInt();
    p.height = o["height"].toInt();
    p.fpsN = o["fpsN"].toInt();
    p.fpsD = o["fpsD"].toInt();
    p.tracks = o["tracks"].toInt();
    require(p.tracks > 0 && p.tracks <= 64, "Invalid track count");
    const auto markers = o["markers"].toArray();
    require(markers.size() <= 1000, "Too many markers");
    for (const auto &v : markers) {
        const auto m = v.toObject();
        p.markers.push_back({m["frame"].toString().toLongLong(), m["name"].toString(),
                             m["color"].toString("#ffd23f")});
    }
    p.inPoint = o.contains("inPoint") ? o["inPoint"].toString().toLongLong() : -1;
    p.outPoint = o.contains("outPoint") ? o["outPoint"].toString().toLongLong() : -1;
    p.trackSettings.clear();
    if (o["schemaVersion"].toInt() == 1) {
        for (int i = 0; i < p.tracks; ++i)
            p.trackSettings.push_back(Track{QString("Track %1").arg(i + 1)});
    } else {
        require(o["trackSettings"].isArray() && o["trackSettings"].toArray().size() == p.tracks,
                "Invalid track settings");
        for (const auto &v : o["trackSettings"].toArray()) {
            require(v.isObject(), "Invalid track entry");
            const auto t = v.toObject();
            p.trackSettings.push_back(Track{t["name"].toString(), t["locked"].toBool(),
                                            t["muted"].toBool(), t["hidden"].toBool(),
                                            t["solo"].toBool()});
            if (o["schemaVersion"].toInt() >= 3) {
                require(t["snapping"].isBool() && t["magnetic"].isBool(), "Invalid track modes");
                auto &track = p.trackSettings.last();
                track.snapping = t["snapping"].toBool();
                track.magnetic = t["magnetic"].toBool();
                track.id = t["id"].toString();
            }
        }
    }
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
        a.frameRate = j["frameRate"].toDouble(0);
        a.variableRate = j["variableRate"].toBool(false);
        a.loops = j["loops"].toBool(false) && a.kind == "video";
        p.assets.push_back(a);
    }
    for (auto v : o["clips"].toArray()) {
        auto j = v.toObject();
        Clip c;
        c.id = j["id"].toString();
        c.assetId = j["assetId"].toString();
        c.link = j["link"].toString().left(64);
        c.group = j["group"].toString().left(64);
        c.name = j["name"].toString();
        c.track = j["track"].toInt();
        c.start = integer(j["start"]);
        c.duration = integer(j["duration"]);
        c.sourceIn = timeRead(j["sourceIn"]);
        c.speed = timeRead(j["speed"]);
        c.audioOnly = j["audioOnly"].toBool();
        c.muted = j["muted"].toBool();
        c.hidden = j["hidden"].toBool();
        c.reverse = j["reverse"].toBool();
        c.flip = j["flip"].toBool();
        if (j.contains("anchor")) {
            const auto anchor = j["anchor"].toArray();
            require(anchor.size() == 2, "Invalid anchor");
            c.anchorX = anchor[0].toDouble(-1);
            c.anchorY = anchor[1].toDouble(-1);
        }
        c.text = j["text"].toString();
        c.fontFamily = j["fontFamily"].toString("Arial");
        c.textColor = j["textColor"].toString("#ffffff");
        c.fontSize = j["fontSize"].toInt(72);
        c.bold = j["bold"].toBool(true);
        c.italic = j["italic"].toBool(false);
        c.align = j["align"].toString("center");
        c.letterSpacing = j["letterSpacing"].toDouble(0);
        c.lineSpacing = j["lineSpacing"].toDouble(1);
        c.outline = j["outline"].toDouble(0);
        c.outlineColor = j["outlineColor"].toString("#000000");
        c.textShadow = j["textShadow"].toDouble(1);
        c.background = j["background"].toDouble(0);
        c.backgroundColor = j["backgroundColor"].toString("#000000");
        c.titleStyle = j["titleStyle"].toString();
        c.textAnimation = j["textAnimation"].toString();
        c.textAnimationTime = j["textAnimationTime"].toDouble(1.5);
        c.accentColor = j["accentColor"].toString("#64d8bc");
        c.effect = j["effect"].toString();
        c.effectStrength = j["effectStrength"].toDouble(0.6);
        c.effectWidth = j["effectWidth"].toDouble(0.3);
        c.effectHeight = j["effectHeight"].toDouble(0.2);
        c.blur = j["blur"].toDouble(0);
        for (const auto &[k, field] : lookFields())
            c.*field = j[k].toDouble(0);
        c.slowMotion = j["slowMotion"].toString();
        c.fx = j["fx"].toString();
        c.fxStrength = j["fxStrength"].toDouble(0.5);
        c.stabilize = j["stabilize"].toBool(false);
        c.graphic = j["graphic"].toString();
        c.fillColor = j["fillColor"].toString("#ffd23f");
        c.strokeColor = j["strokeColor"].toString("#000000");
        c.stroke = j["stroke"].toDouble(0);
        c.graphicWidth = j["graphicWidth"].toDouble(0.3);
        c.graphicHeight = j["graphicHeight"].toDouble(0.2);
        c.lut = j["lut"].toString();
        if (!c.lut.isEmpty())
            c.lut = QDir::cleanPath(QDir::isRelativePath(c.lut) ? QDir(base).absoluteFilePath(c.lut)
                                                                : c.lut);
        c.lutStrength = j["lutStrength"].toDouble(1);
        c.captionStyle = j["captionStyle"].toString();
        c.highlightColor = j["highlightColor"].toString("#ffd23f");
        for (const auto &w : j["wordStarts"].toArray())
            c.wordStarts.push_back(integer(w));
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
        const auto animated = j["keyframes"].toObject();
        require(animated.size() <= 16, "Invalid keyframes");
        for (auto it = animated.begin(); it != animated.end(); ++it) {
            const auto list = it.value().toArray();
            require(list.size() <= 1000, "Too many keyframes");
            for (const auto &v : list) {
                const auto k = v.toArray();
                require(k.size() == 3 && k[1].isDouble() && k[2].isBool(), "Invalid keyframe");
                c.keyframes[it.key()].push_back({integer(k[0]), k[1].toDouble(), k[2].toBool()});
            }
        }
        c.shape = j["shape"].toString("rect");
        c.radius = j["radius"].toDouble(0.12);
        c.border = j["border"].toDouble(0);
        c.borderColor = j["borderColor"].toString("#ffffff");
        c.shadow = j["shadow"].toDouble(0);
        c.chromaKey = j["chromaKey"].toBool();
        c.keyColor = j["keyColor"].toString("#00ff00");
        c.keySimilarity = j["keySimilarity"].toDouble(0.25);
        c.keyBlend = j["keyBlend"].toDouble(0.08);
        c.aiCutout = j["aiCutout"].toBool();
        c.aiUpscale = j["aiUpscale"].toBool();
        c.eyeContact = j["eyeContact"].toBool(false);
        c.transition = j["transition"].toString();
        if (j.contains("transitionFrames"))
            c.transitionFrames = integer(j["transitionFrames"]);
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
    for (int i = 0; i < markers.size(); ++i)
        require(markers[i].frame >= 0 && markers[i].frame <= 100000000 &&
                    markers[i].name.size() <= 200 && QColor(markers[i].color).isValid() &&
                    (i == 0 || markers[i - 1].frame < markers[i].frame),
                "Invalid marker");
    require(inPoint >= -1 && outPoint >= -1 && inPoint <= 100000000 && outPoint <= 100000000 &&
                (inPoint < 0 || outPoint < 0 || inPoint < outPoint),
            "Invalid in/out range");
    require(trackSettings.size() == tracks, "Track settings do not match track count");
    QSet<QString> trackIds;
    for (const auto &t : trackSettings) {
        require(!t.name.trimmed().isEmpty() && t.name.size() <= 80,
                "Track names must be 1–80 characters");
        require(!t.id.isEmpty() && !trackIds.contains(t.id), "Invalid or duplicate track ID");
        trackIds.insert(t.id);
    }
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
        if (c.audioOnly)
            require(asset(c.assetId) && asset(c.assetId)->hasAudio,
                    "Audio-only clips require source audio");
        require(c.start >= 0 && c.start <= 100000000 && c.duration > 0 &&
                    c.duration <= 100000000 - c.start && c.track >= 0 && c.track < tracks,
                "Invalid clip range/track");
        require(c.sourceIn.n >= 0 && c.speed.seconds() >= 0.1 && c.speed.seconds() <= 10,
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
        for (auto it = c.keyframes.begin(); it != c.keyframes.end(); ++it) {
            require(animatableProperties().contains(it.key()) && !it->isEmpty() &&
                        it->size() <= 1000,
                    "Invalid keyframe property");
            const auto [lo, hi] = propertyRange(it.key());
            for (int i = 0; i < it->size(); ++i) {
                const auto &k = it->at(i);
                require(std::abs(k.frame) <= 100000000 && bounded(k.value, lo, hi) &&
                            (i == 0 || it->at(i - 1).frame < k.frame),
                        "Invalid keyframe");
            }
        }
        require(QStringList{"rect", "rounded", "circle"}.contains(c.shape) &&
                    bounded(c.radius, 0, 0.5) && bounded(c.border, 0, 0.1) &&
                    QColor(c.borderColor).isValid() && bounded(c.shadow, 0, 1) &&
                    QColor(c.keyColor).isValid() && bounded(c.keySimilarity, 0.01, 1) &&
                    bounded(c.keyBlend, 0, 1),
                "Invalid overlay style");
        require(c.transitionFrames >= 0 && c.transitionFrames <= 100000000 &&
                    (c.transition.isEmpty() ||
                     std::any_of(transitionTypes().begin(), transitionTypes().end(),
                                 [&](const auto &t) { return t.first == c.transition; })),
                "Invalid transition");
        require(bounded(c.fadeIn, 0, 3600) && bounded(c.fadeOut, 0, 3600) && c.fontSize >= 8 &&
                    c.fontSize <= 500 && c.text.size() <= 10000 && QColor(c.textColor).isValid(),
                "Invalid text/fade value");
        require(QStringList{"left", "center", "right"}.contains(c.align) &&
                    bounded(c.letterSpacing, -0.1, 0.5) && bounded(c.lineSpacing, 0.7, 3) &&
                    bounded(c.outline, 0, 0.25) && bounded(c.textShadow, 0, 1) &&
                    bounded(c.background, 0, 1) && QColor(c.outlineColor).isValid() &&
                    QColor(c.backgroundColor).isValid() && c.fontFamily.size() <= 200,
                "Invalid text style");
        require((c.graphic.isEmpty() ||
                 (graphicKinds().contains(c.graphic) && c.assetId.isEmpty() && c.effect.isEmpty())) &&
                    QColor(c.fillColor).isValid() && QColor(c.strokeColor).isValid() &&
                    bounded(c.stroke, 0, 0.05) && bounded(c.graphicWidth, 0.01, 1) &&
                    bounded(c.graphicHeight, 0.005, 1),
                "Invalid shape");
        require((c.captionStyle.isEmpty() || c.captionStyle == "karaoke" ||
                 c.captionStyle == "word") &&
                    QColor(c.highlightColor).isValid() && c.wordStarts.size() <= 2000 &&
                    std::is_sorted(c.wordStarts.begin(), c.wordStarts.end()),
                "Invalid caption style");
        require((c.effect.isEmpty() || ((c.effect == "blur" || c.effect == "pixelate") &&
                                        c.assetId.isEmpty())) &&
                    bounded(c.effectStrength, 0, 1) && bounded(c.effectWidth, 0.02, 1) &&
                    bounded(c.effectHeight, 0.02, 1) && bounded(c.blur, 0, 1),
                "Invalid blur or mosaic setting");
        require(bounded(c.temperature, -1, 1) && bounded(c.tint, -1, 1) &&
                    bounded(c.vibrance, -1, 1) && bounded(c.shadows, -1, 1) &&
                    bounded(c.highlights, -1, 1) && bounded(c.sharpen, 0, 1) &&
                    bounded(c.glow, 0, 1) && bounded(c.vignette, 0, 1) && bounded(c.grain, 0, 1) &&
                    bounded(c.lutStrength, 0, 1) && c.lut.size() <= 4096 &&
                    QStringList{"", "blend", "flow"}.contains(c.slowMotion),
                "Invalid colour or look setting");
        require(bounded(c.eqLow, -12, 12) && bounded(c.eqMid, -12, 12) &&
                    bounded(c.eqHigh, -12, 12) && bounded(c.lowCut, 0, 300) &&
                    bounded(c.compressor, 0, 1) && bounded(c.gate, 0, 1) &&
                    bounded(c.denoise, 0, 1) && bounded(c.deess, 0, 1),
                "Invalid sound setting");
        require(QStringList{"", "shake", "glitch", "vhs", "film"}.contains(c.fx) &&
                    bounded(c.fxStrength, 0, 1) && bounded(c.motionBlur, 0, 1) &&
                    bounded(c.reverb, 0, 1) && bounded(c.echo, 0, 1) && bounded(c.pan, -1, 1) &&
                    bounded(c.anchorX, 0, 1) && bounded(c.anchorY, 0, 1),
                "Invalid effect setting");
        require(QStringList{"", "lowerThird", "lowerThirdLine", "titleCard"}.contains(
                    c.titleStyle) &&
                    QColor(c.accentColor).isValid(),
                "Invalid title style");
        require(QStringList{"", "typewriter", "words"}.contains(c.textAnimation) &&
                    bounded(c.textAnimationTime, 0.1, 60),
                "Invalid text animation");
        if (const auto *a = asset(c.assetId); a && !a->endless())
            require(c.sourceIn.seconds() +
                            frameTime(c.duration, fpsN, fpsD).seconds() * c.speed.seconds() <=
                        a->duration + 0.002,
                    "Clip extends past source media");
    }
    require(seconds() <= 24 * 3600, "Alpha projects are limited to 24 hours");
    for (int track = 0; track < tracks; ++track) {
        if (!trackSettings[track].magnetic)
            continue;
        qint64 end = 0;
        for (const auto &id : trackOrder(track)) {
            const auto it =
                std::find_if(clips.begin(), clips.end(), [&](const Clip &c) { return c.id == id; });
            require(it->start == end, "Magnetic tracks must be contiguous");
            end += it->duration;
        }
    }
}
bool Project::split(const QString &id, qint64 frame) {
    auto *c = clip(id);
    if (!c || frame <= c->start || frame >= c->start + c->duration)
        return false;
    requireEditable(c->track);
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
    b.transition.clear();
    b.transitionFrames = 0;
    const bool timed = c->timedWords();
    b.shiftKeyframes(-left);
    if (timed) {
        // A timed caption splits between its words: each half keeps the words it shows.
        const auto words = captionWords(c->text);
        QStringList before, after;
        QVector<qint64> first, second;
        for (int i = 0; i < words.size(); ++i)
            if (c->wordStarts[i] < left) {
                before << words[i];
                first << c->wordStarts[i];
            } else {
                after << words[i];
                second << b.wordStarts[i];
            }
        if (!before.isEmpty() && !after.isEmpty()) {
            c->text = before.join(' ');
            c->wordStarts = first;
            b.text = after.join(' ');
            b.wordStarts = second;
        }
    }
    clips.push_back(b);
    return true;
}
void Project::remove(const QString &id, bool ripple) {
    const auto *c = clip(id);
    if (!c)
        return;
    requireEditable(c->track);
    const auto start = c->start, dur = c->duration;
    const auto track = c->track;
    clips.erase(
        std::remove_if(clips.begin(), clips.end(), [&](const Clip &x) { return x.id == id; }),
        clips.end());
    if (ripple)
        for (auto &x : clips)
            if (x.track == track && x.start >= start + dur)
                x.start -= dur;
    if (trackSettings[track].magnetic)
        packTrack(track, trackOrder(track));
}
qint64 Project::cutRanges(const QString &id, QVector<QPair<qint64, qint64>> ranges) {
    const auto *c = clip(id);
    if (!c)
        return 0;
    requireEditable(c->track);
    const qint64 length = c->duration;
    for (auto &r : ranges)
        r = {std::clamp<qint64>(r.first, 0, length), std::clamp<qint64>(r.second, 0, length)};
    ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
                                [](const auto &r) { return r.second <= r.first; }),
                 ranges.end());
    std::sort(ranges.begin(), ranges.end());
    // Merge overlaps, then cut from the end so earlier positions stay valid.
    QVector<QPair<qint64, qint64>> merged;
    for (const auto &r : ranges)
        if (!merged.isEmpty() && r.first <= merged.last().second)
            merged.last().second = std::max(merged.last().second, r.second);
        else
            merged.push_back(r);
    QString head = id; // the piece that starts where the original clip starts
    const qint64 origin = c->start;
    qint64 removed = 0;
    for (int i = int(merged.size()) - 1; i >= 0; --i) {
        const qint64 from = origin + merged[i].first, to = origin + merged[i].second;
        // Split off what follows the range, then the range itself.
        split(head, to);
        QString victim = head;
        if (split(head, from))
            victim = clips.last().id;
        const auto *v = clip(victim);
        if (!v)
            continue;
        removed += v->duration;
        const bool whole = victim == head;
        remove(victim, true);
        if (whole)
            break; // the cut reached the clip's start: nothing earlier is left
    }
    return removed;
}
QStringList Project::linkedClips(const QString &id) const {
    QStringList ids;
    const auto *c = clip(id);
    if (!c || c->assetId.isEmpty() || c->link == "none")
        return ids;
    if (!c->link.isEmpty()) {
        for (const auto &x : clips)
            if (x.id != id && x.link == c->link && x.track != c->track && x.start == c->start &&
                x.duration == c->duration)
                ids << x.id;
        return ids;
    }
    // Projects from before links: the same source range on another track.
    for (const auto &x : clips)
        if (x.id != id && x.link.isEmpty() && x.track != c->track && x.assetId == c->assetId && x.start == c->start &&
            x.duration == c->duration && x.sourceIn == c->sourceIn && x.speed == c->speed &&
            x.reverse == c->reverse)
            ids << x.id;
    return ids;
}
void Project::requireEditable(int track) const {
    require(track >= 0 && track < tracks && track < trackSettings.size(), "Invalid track");
    require(!trackSettings[track].locked, "This track is locked. Unlock it before editing.");
}
bool Project::audioEnabled(int track) const {
    if (track < 0 || track >= trackSettings.size())
        return false;
    const bool solo = std::any_of(trackSettings.begin(), trackSettings.end(),
                                  [](const Track &t) { return t.solo; });
    return !trackSettings[track].muted && (!solo || trackSettings[track].solo);
}
void Project::addTrack(const QString &name) {
    require(tracks < 64, "A project can have at most 64 tracks");
    trackSettings.push_back(Track{name.isEmpty() ? QString("Track %1").arg(tracks + 1) : name});
    ++tracks;
}
void Project::removeTrack(int track) {
    requireEditable(track);
    require(tracks > 1, "Keep at least one track");
    require(
        std::none_of(clips.begin(), clips.end(), [&](const Clip &c) { return c.track == track; }),
        "Move or delete clips before removing this track");
    trackSettings.removeAt(track);
    --tracks;
    for (auto &c : clips)
        if (c.track > track)
            --c.track;
}
void Project::trim(const QString &id, qint64 start, qint64 end) {
    auto *c = clip(id);
    if (!c)
        return;
    requireEditable(c->track);
    const auto order = trackOrder(c->track);
    require(start >= 0 && start < end && end <= 100000000, "A trim must leave at least one frame");
    const auto oldEnd = c->start + c->duration;
    // The source interval is stored in forward order, even for reverse playback.
    const auto delta = c->reverse ? oldEnd - end : start - c->start;
    const auto *a = asset(c->assetId);
    if (a && a->kind != "image")
        c->sourceIn = c->sourceIn + frameTime(delta, fpsN, fpsD) * c->speed;
    // Keyframes stay on the same picture: they keep their timeline position while the start moves.
    c->shiftKeyframes(c->start - start);
    c->start = start;
    c->duration = end - start;
    if (trackSettings[c->track].magnetic)
        packTrack(c->track, order);
    // Caller validates the complete candidate before committing an undo step.
}
namespace {
// A clip's new timeline range with its source following the picture, as in a trim, but
// without touching other clips.
void setRange(Project &p, Clip &c, qint64 start, qint64 end) {
    require(start >= 0 && end - start >= 1, "Every clip must keep at least one frame");
    const auto delta = c.reverse ? c.start + c.duration - end : start - c.start;
    if (const auto *a = p.asset(c.assetId); a && a->kind != "image")
        c.sourceIn = c.sourceIn + frameTime(delta, p.fpsN, p.fpsD) * c.speed;
    c.shiftKeyframes(c.start - start);
    c.start = start;
    c.duration = end - start;
}
Clip *touching(Project &p, const Clip &c, bool after) {
    for (auto &x : p.clips)
        if (x.id != c.id && x.track == c.track &&
            (after ? x.start == c.start + c.duration : x.start + x.duration == c.start))
            return &x;
    return nullptr;
}
} // namespace
void Project::slip(const QString &id, qint64 frames) {
    auto *c = clip(id);
    require(c != nullptr, "No such clip");
    const auto *a = asset(c->assetId);
    require(a && a->kind != "image", "Only video and audio clips can slip");
    for (const auto &clipId : linkedClips(id) + QStringList{id}) {
        auto *x = clip(clipId);
        requireEditable(x->track);
        x->sourceIn = x->sourceIn + frameTime(frames, fpsN, fpsD) * x->speed;
    }
    require(clip(id)->sourceIn.n >= 0, "The source has no earlier picture to show");
}
void Project::roll(const QString &id, qint64 frames) {
    auto *left = clip(id);
    require(left != nullptr, "No such clip");
    auto *right = touching(*this, *left, true);
    require(right != nullptr, "Roll needs a clip right after this one");
    requireEditable(left->track);
    setRange(*this, *left, left->start, left->start + left->duration + frames);
    setRange(*this, *right, right->start + frames, right->start + right->duration);
}
void Project::slide(const QString &id, qint64 frames) {
    auto *c = clip(id);
    require(c != nullptr, "No such clip");
    requireEditable(c->track);
    auto *before = touching(*this, *c, false);
    auto *after = touching(*this, *c, true);
    require(c->start + frames >= 0, "The clip cannot start before the timeline");
    if (before)
        setRange(*this, *before, before->start, before->start + before->duration + frames);
    if (after)
        setRange(*this, *after, after->start + frames, after->start + after->duration);
    c->start += frames;
    for (const auto &x : clips)
        require(x.id == c->id || x.track != c->track || x.start + x.duration <= c->start ||
                    x.start >= c->start + c->duration,
                "There is no room to slide the clip there");
}
QVector<QString> Project::trackOrder(int track, const QString &exclude) const {
    QVector<const Clip *> ordered;
    for (const auto &c : clips)
        if (c.track == track && c.id != exclude)
            ordered.push_back(&c);
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Clip *a, const Clip *b) { return a->start < b->start; });
    QVector<QString> result;
    for (const auto *c : ordered)
        result.push_back(c->id);
    return result;
}
void Project::packTrack(int track, const QVector<QString> &order) {
    requireEditable(track);
    qint64 end = 0;
    for (const auto &id : order) {
        auto *c = clip(id);
        require(c && c->track == track && c->duration > 0 && c->duration <= 100000000 - end,
                "Invalid magnetic track range");
        c->start = end;
        end += c->duration;
    }
}
qint64 Project::placement(int track, qint64 frame, const QString &exclude) const {
    require(track >= 0 && track < tracks, "Invalid track");
    frame = std::clamp(frame, qint64(0), qint64(100000000));
    if (!trackSettings[track].magnetic)
        return frame;
    qint64 end = 0;
    for (const auto &id : trackOrder(track, exclude)) {
        const auto it =
            std::find_if(clips.begin(), clips.end(), [&](const Clip &c) { return c.id == id; });
        if (frame < it->start + (it->duration + 1) / 2)
            break;
        end += it->duration;
    }
    return end;
}
void Project::move(const QString &id, int track, qint64 frame) {
    auto *c = clip(id);
    if (!c)
        return;
    requireEditable(c->track);
    requireEditable(track);
    const int oldTrack = c->track;
    auto order = trackOrder(track, id);
    int index = 0;
    for (; index < order.size(); ++index) {
        const auto *other = clip(order[index]);
        if (frame < other->start + (other->duration + 1) / 2)
            break;
    }
    c->track = track;
    c->start = std::clamp(frame, qint64(0), qint64(100000000));
    if (trackSettings[track].magnetic) {
        order.insert(index, id);
        packTrack(track, order);
    }
    if (oldTrack != track && trackSettings[oldTrack].magnetic)
        packTrack(oldTrack, trackOrder(oldTrack));
}
qint64 Project::snap(qint64 frame, qint64 threshold, const QString &exclude, qint64 playhead,
                     qint64 length) const {
    frame = std::clamp(frame, qint64(0), qint64(100000000));
    threshold = std::clamp(threshold, qint64(0), qint64(120));
    qint64 best = frame, distance = threshold + 1;
    auto candidate = [&](qint64 edge) {
        for (qint64 offset : {qint64(0), length}) {
            const auto value = edge - offset;
            if (value < 0)
                continue;
            const auto diff = std::abs(value - frame);
            if (diff < distance) {
                distance = diff;
                best = value;
            }
        }
    };
    candidate(0);
    candidate(playhead);
    for (const auto &m : markers)
        candidate(m.frame);
    for (const auto &c : clips)
        if (c.id != exclude) {
            candidate(c.start);
            candidate(c.start + c.duration);
        }
    return best;
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
