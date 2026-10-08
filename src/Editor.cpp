#include "Editor.h"
#include "Interchange.h"
#include "Captions.h"
#include "RenderGraph.h"
#include <QCoreApplication>
#include <cmath>
#include <QDateTime>
#include <QTimeZone>
#include <QDir>
#include <QDirIterator>
#include <QFontDatabase>
#include <QThread>
#include <QAudioInput>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSet>
#include <QSvgRenderer>
#include <QImageReader>
#include <QColor>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>
#include <cstring>
#include <memory>

namespace cutlery {
static QString localPath(const QUrl &u) {
    if (!u.isLocalFile())
        throw std::runtime_error("Choose a local file");
    return QFileInfo(u.toLocalFile()).absoluteFilePath();
}
static void writeGraph(const QString &path, const QString &graph) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(graph.toUtf8()) < 0)
        throw std::runtime_error("Cannot write render graph");
}
QString Editor::executable(const QString &name) {
    QString suffix;
#ifdef Q_OS_WIN
    suffix = ".exe";
#endif
    const QString custom = qEnvironmentVariable(("CUTLERY_" + name.toUpper()).toUtf8().constData());
    if (!custom.isEmpty() && QFileInfo(custom).isExecutable())
        return custom;
    const auto bundled = QCoreApplication::applicationDirPath() + "/codecs/" + name + suffix;
    if (QFileInfo(bundled).isExecutable())
        return bundled;
    return QStandardPaths::findExecutable(name + suffix);
}
Editor::Editor(FrameProvider *frames, QObject *parent) : QObject(parent), m_frames(frames) {
    const auto app = QCoreApplication::applicationDirPath();
    m_data = QFileInfo::exists(app + "/portable.json")
                 ? app + "/data"
                 : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    m_recovery = m_data + "/recovery.cutlery";
    {
        QFile recent(m_data + "/recent.json");
        if (recent.open(QIODevice::ReadOnly) && recent.size() < 1024 * 1024)
            for (const auto &v : QJsonDocument::fromJson(recent.readAll()).array())
                if (v.isString() && m_recent.size() < 10)
                    m_recent << v.toString();
    }
    loadPreferences();
    trimCache();
    applyPreferences(m_project);
    listTemplates();
    {
        QFile styles(m_data + "/styles.json");
        if (styles.open(QIODevice::ReadOnly) && styles.size() < 1024 * 1024)
            for (const auto &v : QJsonDocument::fromJson(styles.readAll()).array())
                if (v.isObject() && !v.toObject()["name"].toString().isEmpty() && m_textStyles.size() < 200)
                    m_textStyles << v.toObject().toVariantMap();
    }
    {
        QFile brand(m_data + "/brand.json");
        if (brand.open(QIODevice::ReadOnly) && brand.size() < 1024 * 1024) {
            const auto o = QJsonDocument::fromJson(brand.readAll()).object();
            for (const auto &v : o["colors"].toArray())
                if (const QColor c(v.toString()); c.isValid() && m_brandColors.size() < 24 &&
                                                  !m_brandColors.contains(c.name()))
                    m_brandColors << c.name();
            const auto logo = o["logo"].toString();
            if (!logo.isEmpty() && !logo.contains("..") && QFileInfo(m_data + "/" + logo).isFile())
                m_brandLogo = QDir::cleanPath(m_data + "/" + logo);
        }
    }
    listLuts();
    {
        QFile layouts(m_data + "/layouts.json");
        if (layouts.open(QIODevice::ReadOnly) && layouts.size() < 4 * 1024 * 1024)
            for (const auto &v : QJsonDocument::fromJson(layouts.readAll()).array())
                if (v.isObject() && !v.toObject()["name"].toString().isEmpty() &&
                    !v.toObject()["slots"].toArray().isEmpty() && m_layouts.size() < 100)
                    m_layouts.append(v);
    }
    m_analysis = new MediaAnalysis(m_data + "/cache/waveforms", executable("ffmpeg"), this);
    m_thumbnails = new Thumbnails(m_data + "/cache/thumbnails", executable("ffmpeg"), this);
    m_encoders = new EncoderResolver(executable("ffmpeg"), this);
    // Optional AI worker and AI-pack models; overridable for tests and development.
    QString worker = qEnvironmentVariable("CUTLERY_AI_WORKER"),
            models = qEnvironmentVariable("CUTLERY_AI_MODELS");
    if (worker.isEmpty()) {
        worker = app + "/cutlery-ai";
#ifdef Q_OS_WIN
        worker += ".exe";
#endif
    }
    if (models.isEmpty())
        models = app + "/models";
    QString whisper = qEnvironmentVariable("CUTLERY_WHISPER");
    if (whisper.isEmpty()) {
        whisper = app + "/whisper-cli";
#ifdef Q_OS_WIN
        whisper += ".exe";
#endif
    }
    m_ai = new AiJobs(m_data + "/ai", executable("ffmpeg"), executable("ffprobe"), worker,
                      {{"matte", models + "/u2net_human_seg.onnx"},
                       {"upscale", models + "/realesr-general-x4v3.onnx"},
                       {"eyecontact", models + "/face_landmark.onnx"},
                       {"faces", models + "/face_detection_short_range.onnx"},
                       {"transcribe", models + "/ggml-large-v3-turbo-q5_0.bin"},
                       {"vad", models + "/ggml-silero-v6.2.0.bin"},
                       {"whisper", whisper}},
                      this);
    connect(m_ai, &AiJobs::changed, this, &Editor::changed);
    // A finished result changes the picture.
    connect(m_ai, &AiJobs::finished, this, [this] {
        m_previewTimer.start();
        if (m_follow.value("status") == "analysing")
            applyFollowFace();
        if (m_reframe.value("status") == "analysing")
            applyReframe();
        placeCaptions(); // when every transcript of a caption request is ready
    });
    connect(m_thumbnails, &Thumbnails::changed, this, [this] {
        emit thumbnailsChanged();
        emit changed();
    });
    connect(m_analysis, &MediaAnalysis::changed, this, [this] {
        emit analysisChanged();
        emit changed();
    });
    if (!QDir().mkpath(m_data + "/cache"))
        m_error = "Cannot create application data folder: " + m_data;
    m_hasRecovery = QFileInfo::exists(m_recovery);
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(180);
    connect(&m_previewTimer, &QTimer::timeout, this, &Editor::requestPreview);
    m_queueTimer.setInterval(500);
    connect(&m_queueTimer, &QTimer::timeout, this, [this] {
        advanceQueue();
        emit changed();
    });
    m_playback = new Playback(this);
    connect(m_playback, &Playback::frameChanged, this, &Editor::playbackChanged);
    connect(m_playback, &Playback::finished, this, [this] {
        m_playhead = std::max(qint64(0), m_project.duration() - 1);
        m_status = "Ready";
        stopPlayback();
        emit changed();
    });
    connect(m_playback, &Playback::failed, this, [this](const QString &message) {
        m_status = "Ready";
        stopPlayback();
        fail(message);
    });
    // Edits during playback restart it from the current frame once the edit burst settles.
    m_resumeTimer.setSingleShot(true);
    m_resumeTimer.setInterval(150);
    connect(&m_resumeTimer, &QTimer::timeout, this, &Editor::play);
    // Backward shuttle: ten steps a second.
    m_reverseTimer.setInterval(100);
    connect(&m_reverseTimer, &QTimer::timeout, this, [this] {
        const auto step = std::max<qint64>(
            1, qRound64(m_shuttleRate * m_project.fpsN / m_project.fpsD / 10));
        seek(std::max<qint64>(0, m_playhead - step));
        if (m_playhead == 0)
            pause();
        emit playbackChanged();
    });
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(800);
    connect(&m_saveTimer, &QTimer::timeout, this, &Editor::autosave);
    if (executable("ffmpeg").isEmpty() || executable("ffprobe").isEmpty())
        m_error = "FFmpeg/ffprobe not found. Use the portable package, or add both tools to PATH.";
    // Fonts added in Cutlery live in the data folder, so they travel with a portable install.
    loadFonts(m_data + "/fonts");
}
void Editor::loadFonts(const QString &folder) {
    for (const auto &file : QDir(folder).entryInfoList({"*.ttf", "*.otf", "*.ttc"}, QDir::Files)) {
        const auto path = file.absoluteFilePath();
        if (std::any_of(m_fontFiles.begin(), m_fontFiles.end(),
                        [&](const QString &f) { return QFileInfo(f) == file; }))
            continue;
        for (const auto &family :
             QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(path)))
            m_fontFiles.insert(family, path);
    }
}
QStringList Editor::fontFamilies() const {
    return QFontDatabase::families();
}
QString Editor::addFont(const QUrl &url) {
    const auto source = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const QFileInfo info(source);
    try {
        if (!info.isFile() || !QStringList{"ttf", "otf", "ttc"}.contains(info.suffix().toLower()))
            throw std::runtime_error("Choose a .ttf or .otf font file");
        if (info.size() > 64 * 1024 * 1024)
            throw std::runtime_error("The font file is too large");
        QDir().mkpath(m_data + "/fonts");
        const auto target = m_data + "/fonts/" + info.fileName();
        if (!QFileInfo::exists(target) && !QFile::copy(source, target))
            throw std::runtime_error("Cannot copy the font into the Cutlery data folder");
        const int id = QFontDatabase::addApplicationFont(target);
        const auto families = QFontDatabase::applicationFontFamilies(id);
        if (id < 0 || families.isEmpty()) {
            QFile::remove(target);
            throw std::runtime_error("This file is not a usable font");
        }
        for (const auto &family : families)
            m_fontFiles.insert(family, target);
        m_status = "Added font " + families.first();
        emit changed();
        return families.first();
    } catch (const std::exception &e) {
        fail(e.what());
        return {};
    }
}
Editor::~Editor() {
    if (m_dirty)
        autosave();
    for (auto *t : {m_collectThread, m_beatThread})
        if (t) {
            t->disconnect(this);
            t->requestInterruption();
            t->wait();
            delete t;
        }
    for (auto *p : {m_preview, m_job, m_probe, m_pauseProcess, m_loudnessProcess, m_sceneProcess,
                    m_nestedProcess, m_autoColourProcess, m_frameProcess, m_pickProcess})
        if (p) {
            p->disconnect(this);
            p->kill();
            p->waitForFinished(1500);
        }
    if (!m_jobTemp.isEmpty() && !QFile::remove(m_jobTemp))
        QDir(m_jobTemp).removeRecursively(); // a picture sequence's folder
}
QVariantList Editor::assets() const {
    QVariantList result;
    for (const auto &a : m_project.assets)
        result << QVariantMap{
            {"id", a.id},     {"name", a.name},        {"path", a.path},
            {"kind", a.kind}, {"seconds", a.duration},
            {"missing", !a.isNested() && !QFileInfo::exists(a.path)}, {"nested", a.isNested()},
            {"folder", a.folder}, {"rights", a.rights}, {"credit", a.credit}, {"used", std::any_of(m_project.clips.begin(), m_project.clips.end(),
                                                       [&](const Clip &c) { return c.assetId == a.id; })}};
    return result;
}
// Distinct keyframe positions of a clip, for the timeline markers.
static QVariantList keyframeFrames(const Clip &c) {
    QVector<qint64> frames;
    for (const auto &list : c.keyframes)
        for (const auto &k : list)
            if (k.frame >= 0 && k.frame < c.duration && !frames.contains(k.frame))
                frames.push_back(k.frame);
    std::sort(frames.begin(), frames.end());
    QVariantList result;
    for (auto f : frames)
        result << f;
    return result;
}
QVariantList Editor::clips() const {
    QVariantList result;
    for (const auto &c : m_project.clips) {
        const auto *a = m_project.asset(c.assetId);
        result << QVariantMap{{"id", c.id},
                              {"assetId", c.assetId},
                              {"nested", a && a->isNested()},
                              {"name", c.name},
                              {"linked", !m_project.linkedClips(c.id).isEmpty()},
                              {"grouped", !c.group.isEmpty()},
                              {"track", c.track},
                              {"start", c.start},
                              {"duration", c.duration},
                              {"title", c.assetId.isEmpty() && c.effect.isEmpty()},
                              {"effect", c.effect},
                              {"audio", c.audioOnly || (a && a->kind == "audio")},
                              {"hasAudio", a && a->hasAudio},
                              {"sourceIn", c.sourceIn.seconds()},
                              {"speed", c.speed.seconds()},
                              {"reverse", c.reverse},
                              {"muted", c.muted || !m_project.audioEnabled(c.track)},
                              {"hidden", c.hidden || m_project.trackSettings[c.track].hidden},
                              {"locked", m_project.trackSettings[c.track].locked},
                              {"transition", c.transition},
                              {"transitionFrames", c.transitionFrames},
                              {"transitionLength", m_project.transitionLength(c)},
                              {"canTransition", m_project.previousAdjacent(c) != nullptr},
                              {"keyframes", keyframeFrames(c)}};
    }
    return result;
}
QVariantList Editor::trackList() const {
    QVariantList result;
    int index = 0;
    for (const auto &t : m_project.trackSettings) {
        result << QVariantMap{
            {"index", index++},       {"name", t.name},         {"locked", t.locked},
            {"muted", t.muted},       {"hidden", t.hidden},     {"solo", t.solo},
            {"snapping", t.snapping}, {"magnetic", t.magnetic}, {"id", t.id}};
    }
    return result;
}
QVariantMap Editor::trimBounds(const QString &id) const {
    for (const auto &c : m_project.clips)
        if (c.id == id) {
            qint64 first = 0, last = qint64(86400. * m_project.fpsN / m_project.fpsD);
            if (const auto *a = m_project.asset(c.assetId); a && !a->endless()) {
                const double rate = double(m_project.fpsN) / m_project.fpsD / c.speed.seconds();
                const auto head =
                    std::max(qint64(0), qint64(std::floor(c.sourceIn.seconds() * rate + 1e-6)));
                const auto tail = std::max(
                    qint64(0), qint64(std::floor((a->duration - c.sourceIn.seconds()) * rate -
                                                 c.duration + 1e-6)));
                first = std::max(qint64(0), c.start - (c.reverse ? tail : head));
                last = c.start + c.duration + (c.reverse ? head : tail);
            }
            return {{"first", first}, {"last", last}};
        }
    return {};
}
qint64 Editor::snap(qint64 frame, qint64 threshold, const QString &exclude, qint64 length) const {
    return m_project.snap(frame, threshold, exclude, m_playhead, length);
}
void Editor::trimClip(const QString &id, qint64 start, qint64 end) {
    mutate([&](Project &p) {
        const auto linked = p.linkedClips(id);
        p.trim(id, start, end);
        for (const auto &other : linked)
            p.trim(other, start, end);
    });
}
qint64 Editor::placement(int track, qint64 frame, const QString &exclude) const {
    if (track < 0 || track >= m_project.tracks)
        return std::max(qint64(0), frame);
    return m_project.placement(track, frame, exclude);
}
void Editor::addTrack() {
    mutate([](Project &p) { p.addTrack(); });
}
void Editor::removeTrack(int track) {
    mutate([&](Project &p) { p.removeTrack(track); });
}
void Editor::setTrack(int track, const QString &key, const QVariant &value) {
    mutate([&](Project &p) {
        if (track < 0 || track >= p.tracks)
            throw std::runtime_error("Invalid track");
        auto &t = p.trackSettings[track];
        if (key == "name")
            t.name = value.toString().trimmed();
        else if (key == "locked")
            t.locked = value.toBool();
        else if (key == "muted")
            t.muted = value.toBool();
        else if (key == "hidden")
            t.hidden = value.toBool();
        else if (key == "solo")
            t.solo = value.toBool();
        else if (key == "snapping")
            t.snapping = value.toBool();
        else if (key == "magnetic") {
            p.requireEditable(track);
            t.magnetic = value.toBool();
            if (t.magnetic)
                p.packTrack(track, p.trackOrder(track));
        }
    });
}
void Editor::detachAudio() {
    const auto id = newId();
    mutate([&](Project &p) {
        auto *c = p.clip(m_selected);
        if (!c)
            return;
        p.requireEditable(c->track);
        const auto *a = p.asset(c->assetId);
        if (!a || !a->hasAudio || a->kind != "video" || c->audioOnly)
            throw std::runtime_error("Select a video clip with audio");
        // Picture and sound stay linked: they move and trim together until unlinked.
        if (c->link.isEmpty() || c->link == "none")
            c->link = newId();
        auto audio = *c;
        audio.id = id;
        audio.name = c->name + " · audio";
        audio.audioOnly = true;
        audio.hidden = false;
        audio.muted = false;
        p.addTrack("Audio");
        audio.track = p.tracks - 1;
        c->muted = true;
        p.clips.push_back(audio);
    });
    if (m_project.clip(id))
        select(id);
}
qint64 Editor::adjacentCut(bool forward) const {
    qint64 target = forward ? std::max(qint64(0), m_project.duration() - 1) : 0;
    for (const auto &c : m_project.clips)
        for (auto edge : {c.start, c.start + c.duration}) {
            if (forward && edge > m_playhead)
                target = std::min(target, edge);
            if (!forward && edge < m_playhead)
                target = std::max(target, edge);
        }
    return target;
}
QVariantMap Editor::state() const {
    QVariantMap selected;
    for (const auto &c : m_project.clips)
        if (c.id == m_selected) {
            selected = {{"id", c.id},
                        {"assetId", c.assetId},
                        {"nested", m_project.asset(c.assetId) && m_project.asset(c.assetId)->isNested()},
                        {"audioOnly", c.audioOnly},
                        {"picture", c.effect == "adjust" ||
                                        (!c.audioOnly && m_project.asset(c.assetId) &&
                                         m_project.asset(c.assetId)->kind != "audio")},
                        {"video", !c.audioOnly && m_project.asset(c.assetId) &&
                                      m_project.asset(c.assetId)->kind == "video"},
                        {"variableRate", m_project.asset(c.assetId) &&
                                             m_project.asset(c.assetId)->variableRate},
                        {"locked", m_project.trackSettings[c.track].locked},
                        {"linkedCount", int(m_project.linkedClips(c.id).size())},
                        {"canDetach", !c.audioOnly && m_project.asset(c.assetId) &&
                                          m_project.asset(c.assetId)->kind == "video" &&
                                          m_project.asset(c.assetId)->hasAudio},
                        {"name", c.name},
                        {"track", c.track},
                        {"start", c.start},
                        {"duration", c.duration},
                        {"sourceIn", c.sourceIn.seconds()},
                        {"speed", c.speed.seconds()},
                        {"text", c.text},
                        {"fontSize", c.fontSize},
                        {"textColor", c.textColor},
                        {"gradientColor", c.gradientColor},
                        {"transition", c.transition},
                        {"transitionFrames", c.transitionFrames},
                        {"transitionLength", m_project.transitionLength(c)},
                        {"canTransition", m_project.previousAdjacent(c) != nullptr},
                        {"captionStyle", c.captionStyle},
                        {"effect", c.effect},
                        {"titleStyle", c.titleStyle},
                        {"accentColor", c.accentColor},
                        {"effectStrength", c.effectStrength},
                        {"blur", c.blur},
                        {"highlightColor", c.highlightColor},
                        {"timedWords", c.timedWords()},
                        {"hasAudio", m_project.asset(c.assetId) &&
                                         m_project.asset(c.assetId)->hasAudio}};
            // Animated values and keyframe state at the playhead for the inspector.
            const auto local = m_playhead - c.start;
            QVariantMap animated, keyed, counts, easing;
            for (const auto &property : animatableProperties()) {
                const auto list = c.keyframes.value(property);
                animated[property] = c.valueAt(property, local);
                keyed[property] = std::any_of(list.begin(), list.end(),
                                              [&](const Keyframe &k) { return k.frame == local; });
                counts[property] = list.size();
                for (const auto &k : list)
                    if (k.frame == local)
                        easing[property] = k.easing();
            }
            selected["keyEasing"] = easing;
            selected["animated"] = animated;
            selected["keyed"] = keyed;
            selected["keyframeCount"] = counts;
            selected["playheadInside"] = local >= 0 && local < c.duration;
#define PROP(k) selected[#k] = c.k
            PROP(scale);
            PROP(x);
            PROP(y);
            PROP(rotation);
            PROP(opacity);
            PROP(volume);
            PROP(brightness);
            PROP(contrast);
            PROP(saturation);
            PROP(crop);
            PROP(cropLeft);
            PROP(cropRight);
            PROP(cropTop);
            PROP(cropBottom);
            PROP(flipVertical);
            PROP(blendMode);
            PROP(lumaKey);
            PROP(lumaTolerance);
            PROP(lumaSoftness);
            PROP(temperature);
            PROP(tint);
            PROP(vibrance);
            PROP(shadows);
            PROP(highlights);
            PROP(sharpen);
            PROP(glow);
            PROP(vignette);
            PROP(grain);
            PROP(lutStrength);
            PROP(curveMaster);
            PROP(curveRed);
            PROP(curveGreen);
            PROP(curveBlue);
            PROP(hslColors);
            PROP(hslHue);
            PROP(hslSaturation);
            PROP(hslLightness);
            PROP(eqLow);
            PROP(eqMid);
            PROP(eqHigh);
            PROP(lowCut);
            PROP(compressor);
            PROP(gate);
            PROP(denoise);
            PROP(deess);
            PROP(fx);
            PROP(voice);
            PROP(canvasFill);
            PROP(fxStrength);
            PROP(motionBlur);
            PROP(stabilize);
            PROP(reverb);
            PROP(pitch);
            PROP(whites);
            PROP(blacks);
            PROP(liftX);
            PROP(liftY);
            PROP(gammaX);
            PROP(gammaY);
            PROP(gainX);
            PROP(gainY);
            PROP(titleSlide);
            PROP(stabilizeStrength);
            PROP(stabilizeZoom);
            PROP(exposure);
            PROP(echo);
            PROP(pan);
            PROP(textAnimation);
            PROP(textAnimationTime);
            PROP(anchorX);
            PROP(anchorY);
            PROP(slowMotion);
            PROP(fontFamily);
            PROP(graphic);
            PROP(fillColor);
            PROP(strokeColor);
            PROP(stroke);
            PROP(graphicWidth);
            PROP(graphicHeight);
            PROP(bold);
            PROP(italic);
            PROP(align);
            PROP(letterSpacing);
            PROP(lineSpacing);
            PROP(outline);
            PROP(outlineColor);
            PROP(textShadow);
            PROP(textGlow);
            PROP(textGlowColor);
            PROP(background);
            PROP(backgroundColor);
            selected["lut"] = c.lut;
            selected["lutName"] = QFileInfo(c.lut).completeBaseName();
            selected["lutMissing"] = !c.lut.isEmpty() && !QFileInfo(c.lut).isFile();
            PROP(fadeIn);
            PROP(fadeOut);
            PROP(reverse);
            PROP(flip);
            PROP(muted);
            PROP(hidden);
            PROP(shape);
            PROP(radius);
            PROP(feather);
            PROP(effectShape);
            PROP(tiltX);
            PROP(tiltY);
            {
                QVariantList pin;
                for (const auto v : c.cornerPin)
                    pin << v;
                selected["cornerPin"] = pin;
            }
            PROP(border);
            PROP(borderColor);
            PROP(shadow);
            PROP(chromaKey);
            PROP(keyColor);
            PROP(keySimilarity);
            PROP(keyBlend);
            PROP(aiCutout);
            PROP(aiUpscale);
            PROP(eyeContact);
#undef PROP
            if (const auto *a = m_project.asset(c.assetId); a && a->kind == "video") {
                for (const auto &[task, name] : {std::pair{"matte", "cutout"}, {"upscale", "upscale"},
                                                 {"eyecontact", "eyeContactInfo"}}) {
                    auto info = m_ai->status(task, *a, aiVariant(task, *a));
                    info["covered"] = aiCovered(task, *a, &c);
                    selected[name] = info;
                }
                // Already at least 4K: nothing to gain.
                selected["upscaleHeight"] = upscaleHeight(*a);
            }
        }
    return {{"name", m_project.name},
            {"path", m_path},
            {"dirty", m_dirty},
            {"selected", selected},
            {"selectedId", m_selected},
            {"selectedIds", selection()},
            {"exportQueue", [this] {
                 QVariantList list;
                 for (const auto &q : m_queue) {
                     const auto s = q.settings;
                     list << QVariantMap{{"file", QFileInfo(localPath(q.url)).fileName()},
                                         {"label", s.value("format").toString().toUpper() + " · " +
                                                       s.value("quality").toString() +
                                                       (s.value("height").toInt() > 0 ? " · " + QString::number(s.value("height").toInt()) + "p" : QString()) +
                                                       (s.value("range") == "inout" ? " · in/out" : QString())},
                                         {"status", q.status}};
                 }
                 return list;
             }()},
            {"queuePaused", m_queuePaused},
            {"folders", m_project.folders},
            {"nesting", [this] {
                 // The way down from the main timeline: names of the open nested sequences.
                 QStringList names;
                 for (const auto &f : m_nest)
                     if (const auto *a = f.parent.asset(f.assetId))
                         names << a->name;
                 return names;
             }()},
            {"nestedRendering", m_nestedProcess != nullptr},
            {"importFolder", m_project.folders.contains(m_importFolder) ? m_importFolder : QString()},
            {"playhead", m_playhead},
            {"duration", m_project.duration()},
            {"fps", double(m_project.fpsN) / m_project.fpsD},
            {"fpsN", m_project.fpsN},
            {"fpsD", m_project.fpsD},
            {"width", m_project.width},
            {"height", m_project.height},
            {"tracks", m_project.tracks},
            {"status", m_status},
            {"error", m_error},
            {"busy", m_busy},
            {"canRetryExport", m_exportFailed && !m_busy && !m_lastExportUrl.isEmpty()},
            {"importing", m_importing},
            {"importRemaining", m_importQueue.size() + (m_probe ? 1 : 0)},
            {"analyzing", m_analysis->busy() || m_thumbnails->busy()},
            {"aiMissing", QVariantMap{{"matte", m_ai->missing("matte")},
                                      {"upscale", m_ai->missing("upscale")},
                                      {"eyecontact", m_ai->missing("eyecontact")},
                                      {"faces", m_ai->missing("faces")},
                                      {"transcribe", m_ai->missing("transcribe")}}},
            {"captions", captionState()},
            {"pauses", pauseState()},
            {"scenes", m_scenes},
            {"beats", m_beats},
            {"collect", m_collect},
            {"follow", m_follow},
            {"transcript", transcriptState()},
            {"autoColour", m_autoColour},
            {"textStyles", m_textStyles},
            {"brandColors", m_brandColors},
            {"brandLogo", m_brandLogo},
            {"lutLibrary", m_lutLibrary},
            {"layouts", [this] {
                 QVariantList list;
                 for (const auto &v : m_layouts)
                     list << QVariantMap{{"name", v.toObject()["name"].toString()},
                                         {"count", int(v.toObject()["slots"].toArray().size())}};
                 return list;
             }()},
            {"templates", m_templates},
            {"reframe", m_reframe},
            {"conform", m_conform},
            {"markers", [this] {
                 QVariantList list;
                 for (const auto &m : m_project.markers)
                     list << QVariantMap{{"frame", m.frame}, {"name", m.name}, {"color", m.color}};
                 return list;
             }()},
            {"inPoint", m_project.inPoint},
            {"outPoint", m_project.outPoint},
            {"voiceOver", QVariantMap{{"available", !QMediaDevices::audioInputs().isEmpty()},
                                      {"recording", m_voiceRecorder != nullptr},
                                      {"seconds", m_voiceRecorder && m_voiceClock.isValid()
                                                      ? m_voiceClock.elapsed() / 1000.
                                                      : 0.}}},
            {"loudness", [this] {
                 auto l = m_mixLoudness;
                 // A measurement describes the mix it was made on.
                 if (l.value("status") == "ready" && l.value("revision").toLongLong() != m_revision)
                     l["status"] = "stale";
                 return l;
             }()},
            {"progress", m_progress},
            {"clipboard", m_clipboard ? m_clipboard->name : QString()},
            {"previewUrl", m_previewUrl},
            {"playing", m_playback->active() || m_resumeTimer.isActive()},
            {"canUndo", !m_undo.empty()},
            {"canRedo", !m_redo.empty()},
            {"hasRecovery", m_hasRecovery},
            {"recent", [this] {
                 QVariantList list;
                 for (const auto &path : m_recent)
                     list << QVariantMap{{"path", path},
                                         {"name", QFileInfo(path).completeBaseName()},
                                         {"exists", QFileInfo::exists(path)}};
                 return list;
             }()},
            {"dataPath", m_data},
            {"preferences", m_prefs},
            {"revision", m_revision}};
}
void Editor::fail(const QString &error) {
    m_error = error;
    emit changed();
}
void Editor::clearError() {
    m_error.clear();
    emit changed();
}
void Editor::edited() {
    ++m_revision;
    m_dirty = true;
    if (m_playback->active()) {
        m_playhead = m_playback->frame();
        stopPlayback();
        m_resumeTimer.start();
    }
    m_playhead = std::clamp(m_playhead, qint64(0), std::max(qint64(0), m_project.duration() - 1));
    if (!m_project.clip(m_selected))
        m_selected.clear();
    m_previewTimer.start();
    m_saveTimer.start();
    renderNested();
    m_analysis->setAssets(m_project.assets);
    m_thumbnails->setAssets(m_project.assets);
    emit projectChanged();
    emit changed();
}
bool Editor::mutate(const std::function<void(Project &)> &fn) {
    try {
        auto next = m_project;
        fn(next);
        next.validate();
        if (next.json() == m_project.json())
            return true;
        m_undo.push_back(m_project);
        while (m_undo.size() > m_prefs.value("undoSteps", 60).toInt())
            m_undo.removeFirst();
        m_redo.clear();
        m_project = std::move(next);
        m_error.clear();
        edited();
        return true;
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
        return false;
    }
}
// Templates ---------------------------------------------------------------------------------
static QString templateFileName(const QString &name) {
    // A file name from the template's name: letters, digits, spaces, dashes and underscores.
    QString safe;
    for (const auto ch : name.trimmed())
        safe += ch.isLetterOrNumber() || ch == ' ' || ch == '-' || ch == '_' ? ch : QChar('_');
    return safe.left(60) + ".cutlery";
}
QVariantList Editor::templates() const {
    return m_templates;
}
void Editor::listTemplates() {
    QVariantList list;
    const QDir dir(m_data + "/templates");
    for (const auto &info : dir.entryInfoList({"*.cutlery"}, QDir::Files, QDir::Name))
        list << QVariantMap{{"name", info.completeBaseName()}, {"path", info.absoluteFilePath()}};
    m_templates = list;
}
void Editor::saveTemplate(const QString &name) {
    const auto label = name.trimmed();
    if (label.isEmpty())
        return fail("Name the template");
    if (m_project.clips.empty() && m_nest.isEmpty())
        return fail("The timeline is empty");
    try {
        QDir().mkpath(m_data + "/templates");
        const auto path = m_data + "/templates/" + templateFileName(label);
        auto p = wholeProject();
        p.name = QFileInfo(path).completeBaseName();
        saveProject(p, path);
        listTemplates();
        m_status = "Template saved: " + p.name;
        emit changed();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
bool Editor::newFromTemplate(const QString &name) {
    const auto path = m_data + "/templates/" + templateFileName(name);
    if (!QFileInfo::exists(path)) {
        fail("No such template");
        return false;
    }
    if (!openProject(QUrl::fromLocalFile(path)))
        return false;
    // A new, unsaved project; the template stays as it is and out of the recent list.
    m_recent.removeAll(QFileInfo(path).absoluteFilePath());
    saveRecent();
    m_path.clear();
    m_project.name = "Untitled";
    m_dirty = true;
    m_status = "New project from the template " + name;
    emit changed();
    return true;
}
void Editor::removeTemplate(const QString &name) {
    QFile::remove(m_data + "/templates/" + templateFileName(name));
    listTemplates();
    emit changed();
}
void Editor::newProject() {
    if (m_importing) {
        fail("Wait for media import to finish");
        return;
    }
    cancelJob();
    m_saveTimer.stop();
    m_project = Project{};
    m_nest.clear();
    applyPreferences(m_project);
    m_path.clear();
    m_selected.clear();
    m_undo.clear();
    m_redo.clear();
    m_playhead = 0;
    m_dirty = false;
    m_previewUrl.clear();
    m_resumeTimer.stop();
    stopPlayback();
    ++m_revision;
    m_status = "New project";
    m_analysis->setAssets(m_project.assets);
    m_thumbnails->setAssets(m_project.assets);
    emit projectChanged();
    emit changed();
}
bool Editor::openProject(const QUrl &url) {
    if (m_importing) {
        fail("Wait for media import to finish");
        return false;
    }
    try {
        const auto path = localPath(url);
        auto p = loadProject(path);
        cancelJob();
        m_saveTimer.stop();
        m_project = std::move(p);
        m_nest.clear();
        m_path = path;
        m_undo.clear();
        m_redo.clear();
        loadHistory(path);
        m_selected.clear();
        m_playhead = 0;
        m_dirty = false;
        ++m_revision;
        m_resumeTimer.stop();
        stopPlayback();
        m_previewUrl.clear();
        m_status = "Opened " + QFileInfo(path).fileName();
        if (QFileInfo(path).absoluteFilePath() != QFileInfo(m_recovery).absoluteFilePath())
            remember(path);
        // A collected project carries the fonts it uses.
        loadFonts(QFileInfo(path).dir().filePath("fonts"));
        m_previewTimer.start();
        renderNested();
        m_analysis->setAssets(m_project.assets);
        m_thumbnails->setAssets(m_project.assets);
        emit projectChanged();
        emit changed();
        return true;
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
        return false;
    }
}
bool Editor::save(const QUrl &url) {
    try {
        const auto path = url.isEmpty() ? m_path : localPath(url);
        if (path.isEmpty())
            throw std::runtime_error("Choose a project filename");
        // Inside a nested sequence the whole project is saved, with the sequence as it is now.
        auto p = wholeProject();
        p.name = QFileInfo(path).completeBaseName();
        if (QFileInfo::exists(path))
            backUp(path);
        saveProject(p, path);
        remember(path);
        if (m_nest.isEmpty())
            m_project = std::move(p);
        else
            m_nest.first().parent.name = p.name;
        m_path = path;
        m_dirty = false;
        if (m_nest.isEmpty())
            saveHistory(path);
        m_status = "Project saved";
        m_saveTimer.stop();
        QFile::remove(m_recovery);
        m_hasRecovery = false;
        emit changed();
        return true;
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
        return false;
    }
}
// App-wide settings, kept in the data folder: the format of new projects, how long imported
// pictures last, how many earlier versions are kept and whether the start screen shows.
static QVariantMap defaultPreferences() {
    return {{"width", 1920},     {"height", 1080},  {"fpsN", 30},
            {"fpsD", 1},         {"stillSeconds", 5.}, {"backups", 20},
            {"startScreen", true}, {"cacheGB", 20}, {"undoSteps", 60}};
}
// The valid preferences in `values`, over `base`; throws on an invalid value.
static QVariantMap checkedPreferences(const QVariantMap &base, const QVariantMap &values) {
    auto p = base;
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (!p.contains(it.key()))
            throw std::runtime_error(("Unknown setting: " + it.key()).toStdString());
        p[it.key()] = it.value();
    }
    const int w = p["width"].toInt(), h = p["height"].toInt(), n = p["fpsN"].toInt(),
              d = p["fpsD"].toInt();
    if (w < 64 || h < 64 || w > 7680 || h > 7680 || w % 2 || h % 2)
        throw std::runtime_error("The size of new projects must be even, 64–7680 px");
    if (n <= 0 || d <= 0 || d > 1001 || double(n) / d < 1 || double(n) / d > 120)
        throw std::runtime_error("The frame rate must be 1–120 fps");
    const double still = p["stillSeconds"].toDouble();
    if (!(still >= 0.5 && still <= 60))
        throw std::runtime_error("Pictures last 0.5–60 seconds");
    const int backups = p["backups"].toInt();
    if (backups < 0 || backups > 100)
        throw std::runtime_error("Keep 0–100 earlier versions");
    const int undo = p["undoSteps"].toInt();
    if (undo < 10 || undo > 500)
        throw std::runtime_error("Keep 10–500 undo steps");
    const int cache = p["cacheGB"].toInt();
    if (cache < 1 || cache > 2000)
        throw std::runtime_error("The cache limit must be 1–2000 GB");
    return {{"width", w},
            {"height", h},
            {"fpsN", n},
            {"fpsD", d},
            {"stillSeconds", still},
            {"backups", backups},
            {"startScreen", p["startScreen"].toBool()},
            {"cacheGB", cache},
            {"undoSteps", undo}};
}
void Editor::loadPreferences() {
    m_prefs = defaultPreferences();
    QFile file(m_data + "/settings.json");
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024)
        return;
    // A broken or outdated file falls back to the defaults, setting by setting.
    const auto stored = QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
    for (auto it = stored.begin(); it != stored.end(); ++it)
        try {
            m_prefs = checkedPreferences(m_prefs, {{it.key(), it.value()}});
        } catch (const std::exception &) {
        }
}
namespace {
struct CacheFile {
    QString path;
    qint64 bytes;
    QDateTime written;
};
// The files of the caches that can be made again: waveforms, thumbnails, nested renders.
QVector<CacheFile> cacheFiles(const QString &data) {
    QVector<CacheFile> files;
    for (const auto *folder : {"waveforms", "thumbnails", "nested"}) {
        QDirIterator it(data + "/cache/" + QLatin1String(folder), QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const auto info = it.fileInfo();
            files.push_back({info.absoluteFilePath(), info.size(), info.lastModified()});
        }
    }
    return files;
}
} // namespace
void Editor::trimCache() {
    const QDir cache(m_data + "/cache");
    if (!cache.exists())
        return;
    // Work folders of renders, stills and measurements are removed when they finish; those
    // older than an hour were left by a session that ended abruptly.
    const auto hourAgo = QDateTime::currentDateTime().addSecs(-3600);
    for (const auto &info : cache.entryInfoList(
             {"play-*", "still-*", "frame-*", "render-*", "loudness-*", "nested-*"},
             QDir::Dirs | QDir::NoDotAndDotDot))
        if (info.lastModified() < hourAgo)
            QDir(info.absoluteFilePath()).removeRecursively();
    const bool clearAll = QFile::exists(cache.filePath("clear-at-start"));
    QFile::remove(cache.filePath("clear-at-start"));
    const qint64 limit = clearAll ? 0 : qint64(m_prefs.value("cacheGB").toInt()) << 30;
    auto files = cacheFiles(m_data);
    qint64 total = 0;
    for (const auto &f : files)
        total += f.bytes;
    if (total <= limit)
        return;
    std::sort(files.begin(), files.end(),
              [](const CacheFile &a, const CacheFile &b) { return a.written < b.written; });
    for (const auto &f : files) {
        if (total <= limit)
            break;
        if (QFile::remove(f.path))
            total -= f.bytes;
    }
}
QVariantMap Editor::cacheUsage() const {
    qint64 bytes = 0;
    const auto files = cacheFiles(m_data);
    for (const auto &f : files)
        bytes += f.bytes;
    return {{"bytes", bytes},
            {"files", files.size()},
            {"clearAtStart", QFile::exists(m_data + "/cache/clear-at-start")}};
}
void Editor::clearCacheAtStart() {
    QDir().mkpath(m_data + "/cache");
    QFile marker(m_data + "/cache/clear-at-start");
    if (!marker.open(QIODevice::WriteOnly))
        return fail("Cannot write to " + m_data + "/cache");
    m_status = "The cache is emptied when Cutlery starts next";
    emit changed();
}
void Editor::applyPreferences(Project &p) const {
    p.width = m_prefs.value("width").toInt();
    p.height = m_prefs.value("height").toInt();
    p.fpsN = m_prefs.value("fpsN").toInt();
    p.fpsD = m_prefs.value("fpsD").toInt();
}
void Editor::setPreferences(const QVariantMap &values) {
    try {
        const auto next = checkedPreferences(m_prefs, values);
        QDir().mkpath(m_data);
        QSaveFile f(m_data + "/settings.json");
        const auto data = QJsonDocument(QJsonObject::fromVariantMap(next)).toJson();
        if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
            throw std::runtime_error("Cannot save the settings in " + m_data.toStdString());
        m_prefs = next;
        // An untouched new project takes the new format at once.
        if (m_path.isEmpty() && m_project.clips.empty() && m_project.assets.empty() && !m_dirty)
            applyPreferences(m_project);
        m_status = "Settings saved";
        emit projectChanged();
        emit changed();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::remember(const QString &path) {
    const auto file = QFileInfo(path).absoluteFilePath();
    m_recent.removeAll(file);
    m_recent.prepend(file);
    while (m_recent.size() > 10)
        m_recent.removeLast();
    saveRecent();
}
void Editor::saveRecent() {
    QSaveFile f(m_data + "/recent.json");
    const auto data = QJsonDocument(QJsonArray::fromStringList(m_recent)).toJson();
    if (f.open(QIODevice::WriteOnly) && f.write(data) == data.size())
        f.commit();
}
bool Editor::openRecent(const QString &path) {
    if (!QFileInfo::exists(path)) {
        m_recent.removeAll(path);
        saveRecent();
        fail("The project is no longer there: " + path);
        return false;
    }
    return openProject(QUrl::fromLocalFile(path));
}
QString Editor::backupFolder(const QString &projectPath) const {
    const auto key = QCryptographicHash::hash(QFileInfo(projectPath).absoluteFilePath().toUtf8(),
                                              QCryptographicHash::Sha1)
                         .toHex()
                         .left(16);
    return m_data + "/backups/" + QString::fromLatin1(key);
}
void Editor::backUp(const QString &projectPath) {
    const int keep = m_prefs.value("backups").toInt();
    if (keep <= 0)
        return;
    // A failed backup never stops the save itself.
    const auto folder = backupFolder(projectPath);
    if (!QDir().mkpath(folder))
        return;
    QFile name(folder + "/project.txt");
    if (name.open(QIODevice::WriteOnly | QIODevice::Truncate))
        name.write(QFileInfo(projectPath).absoluteFilePath().toUtf8());
    // Versions are named by time and listed in name order, so a new one must sort after every
    // existing one: a save in the same millisecond as the last (the Windows clock is coarse)
    // takes the next millisecond.
    auto files = QDir(folder).entryList({"*.cutlery"}, QDir::Files, QDir::Name);
    auto stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
    if (!files.isEmpty() && stamp <= files.last().left(19)) {
        auto last = QDateTime::fromString(files.last().left(19), "yyyyMMdd-HHmmss-zzz");
        last.setTimeZone(QTimeZone::utc());
        if (last.isValid())
            stamp = last.addMSecs(1).toString("yyyyMMdd-HHmmss-zzz");
    }
    QFile::copy(projectPath, folder + "/" + stamp + ".cutlery");
    files = QDir(folder).entryList({"*.cutlery"}, QDir::Files, QDir::Name);
    while (files.size() > keep)
        QFile::remove(folder + "/" + files.takeFirst());
}
QVariantList Editor::backups() const {
    QVariantList list;
    if (m_path.isEmpty())
        return list;
    const auto folder = backupFolder(m_path);
    const auto files = QDir(folder).entryList({"*.cutlery"}, QDir::Files, QDir::Name | QDir::Reversed);
    for (const auto &f : files) {
        const auto time = QDateTime::fromString(f.left(19), "yyyyMMdd-HHmmss-zzz");
        auto utc = time;
        utc.setTimeZone(QTimeZone::utc());
        list << QVariantMap{{"file", folder + "/" + f},
                            {"time", utc.toLocalTime().toString("yyyy-MM-dd HH:mm:ss")},
                            {"bytes", QFileInfo(folder + "/" + f).size()}};
    }
    return list;
}
bool Editor::restoreBackup(const QString &file) {
    try {
        if (m_path.isEmpty())
            throw std::runtime_error("Open the project first");
        const auto folder = QFileInfo(backupFolder(m_path)).absoluteFilePath();
        if (QFileInfo(file).absolutePath() != folder || !QFileInfo::exists(file))
            throw std::runtime_error("That is not a backup of this project");
        // The version is checked before anything is replaced.
        loadProject(file);
        const auto path = m_path;
        if (QFileInfo::exists(path))
            backUp(path);
        QFile::remove(path + ".restoring");
        if (!QFile::copy(file, path + ".restoring"))
            throw std::runtime_error("Cannot write next to the project");
        QFile::remove(path);
        if (!QFile::rename(path + ".restoring", path))
            throw std::runtime_error("Cannot replace the project file");
        if (!openProject(QUrl::fromLocalFile(path)))
            return false;
        m_status = "Restored the version from " + QFileInfo(file).completeBaseName().left(15);
        emit changed();
        return true;
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
        return false;
    }
}
void Editor::autosave() {
    try {
        saveProject(wholeProject(), m_recovery);
        m_hasRecovery = true;
    } catch (const std::exception &e) {
        fail("Recovery save failed: " + QString::fromUtf8(e.what()));
    }
}
void Editor::recover() {
    if (openProject(QUrl::fromLocalFile(m_recovery))) {
        m_path.clear();
        m_dirty = true;
        m_status = "Recovery opened. Save As to keep it.";
        emit changed();
    }
}
void Editor::select(const QString &id) {
    m_selected = id;
    m_also.clear();
    if (const auto *c = m_project.clip(id); c && !c->group.isEmpty())
        for (const auto &x : m_project.clips)
            if (x.group == c->group && x.id != id)
                m_also << x.id;
    emit changed();
}
QStringList Editor::selection() const {
    QStringList ids;
    for (const auto &id : QStringList{m_selected} + m_also)
        if (!id.isEmpty() && m_project.clip(id) && !ids.contains(id))
            ids << id;
    return ids;
}
void Editor::toggleSelect(const QString &id) {
    const auto *c = m_project.clip(id);
    if (!c)
        return;
    // The whole group goes in or out together.
    QStringList members{id};
    if (!c->group.isEmpty())
        for (const auto &x : m_project.clips)
            if (x.group == c->group && x.id != id)
                members << x.id;
    auto current = selection();
    if (current.contains(id)) {
        for (const auto &m : members)
            current.removeAll(m);
    } else
        current = members + current;
    m_selected = current.isEmpty() ? QString() : current.takeFirst();
    m_also = current;
    emit changed();
}
void Editor::selectAll() {
    m_also.clear();
    for (const auto &c : m_project.clips)
        if (c.id != m_selected)
            m_also << c.id;
    if (m_selected.isEmpty() && !m_also.isEmpty())
        m_selected = m_also.takeFirst();
    emit changed();
}
void Editor::groupSelection() {
    const auto ids = selection();
    if (ids.size() < 2)
        return fail("Ctrl+click to select at least two clips to group");
    const auto group = newId();
    mutate([&](Project &p) {
        for (const auto &id : ids)
            p.clip(id)->group = group;
    });
}
void Editor::ungroupSelection() {
    const auto ids = selection();
    mutate([&](Project &p) {
        bool any = false;
        for (const auto &id : ids)
            if (auto *c = p.clip(id); !c->group.isEmpty()) {
                c->group.clear();
                any = true;
            }
        if (!any)
            throw std::runtime_error("The selection has no group");
    });
}
void Editor::seek(qint64 frame) {
    m_resumeTimer.stop();
    stopPlayback();
    m_playhead = std::clamp(frame, qint64(0), std::max(qint64(0), m_project.duration() - 1));
    m_previewTimer.start();
    emit changed();
}
QString Editor::historyFile(const QString &projectPath) const {
    const auto key = QCryptographicHash::hash(
        QDir::cleanPath(QFileInfo(projectPath).absoluteFilePath()).toUtf8(), QCryptographicHash::Sha1);
    return m_data + "/history/" + QString::fromLatin1(key.toHex()) + ".json";
}
static QByteArray fileHash(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result().toHex();
}
void Editor::saveHistory(const QString &projectPath) {
    // Best effort: a project saves fine without its history.
    const int keep = std::min(m_prefs.value("undoSteps", 60).toInt(), 100);
    QJsonArray undo, redo;
    for (qsizetype i = std::max<qsizetype>(0, m_undo.size() - keep); i < m_undo.size(); ++i)
        undo.append(m_undo[i].json());
    for (qsizetype i = std::max<qsizetype>(0, m_redo.size() - keep); i < m_redo.size(); ++i)
        redo.append(m_redo[i].json());
    const auto file = historyFile(projectPath);
    QDir().mkpath(QFileInfo(file).absolutePath());
    const auto data = QJsonDocument(QJsonObject{{"file", QFileInfo(projectPath).absoluteFilePath()},
                                                {"saved", QString::fromLatin1(fileHash(projectPath))},
                                                {"undo", undo},
                                                {"redo", redo}})
                          .toJson(QJsonDocument::Compact);
    QSaveFile f(file);
    if (data.size() > 64 * 1024 * 1024 || !f.open(QIODevice::WriteOnly) || f.write(data) != data.size() ||
        !f.commit())
        return;
    // The 50 most recently saved projects keep their history.
    auto old = QDir(QFileInfo(file).absolutePath()).entryInfoList({"*.json"}, QDir::Files, QDir::Time);
    for (qsizetype i = 50; i < old.size(); ++i)
        QFile::remove(old[i].absoluteFilePath());
}
void Editor::loadHistory(const QString &projectPath) {
    QFile f(historyFile(projectPath));
    if (!f.open(QIODevice::ReadOnly) || f.size() > 64 * 1024 * 1024)
        return;
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
    // Only for the file as it was saved: an edit elsewhere makes the history meaningless.
    if (o["saved"].toString().toLatin1() != fileHash(projectPath) || o["saved"].toString().isEmpty())
        return;
    try {
        QVector<Project> undo, redo;
        for (const auto &v : o["undo"].toArray())
            undo.push_back(Project::fromJson(v.toObject(), {}));
        for (const auto &v : o["redo"].toArray())
            redo.push_back(Project::fromJson(v.toObject(), {}));
        m_undo = std::move(undo);
        m_redo = std::move(redo);
    } catch (const std::exception &) {
        m_undo.clear();
        m_redo.clear();
    }
}
void Editor::undo() {
    if (m_undo.empty())
        return;
    m_redo.push_back(m_project);
    m_project = m_undo.takeLast();
    edited();
}
void Editor::redo() {
    if (m_redo.empty())
        return;
    m_undo.push_back(m_project);
    m_project = m_redo.takeLast();
    edited();
}
QList<Editor::ImportRequest> Editor::importRequests(const QList<QUrl> &urls,
                                                   std::shared_ptr<DropBatch> drop) const {
    static const QStringList media{
        "mp4", "mov", "mkv", "webm", "avi", "m4v", "mts", "m2ts", "mpg", "mpeg", "wmv", "flv",
        "3gp", "mp3", "wav", "m4a", "aac", "flac", "ogg", "opus", "wma", "aif", "aiff", "png",
        "jpg", "jpeg", "webp", "bmp", "tif", "tiff", "gif", "svg", "mxf", "vob", "ts", "m2t",
        "dv", "avif", "heic", "heif"};
    QList<ImportRequest> requests;
    for (const auto &url : urls) {
        const QFileInfo info(url.toLocalFile());
        if (!url.isLocalFile() || !info.isDir()) {
            requests.push_back({url, drop, {}});
            continue;
        }
        auto name = info.fileName().trimmed().left(100);
        if (name.isEmpty())
            name = "Imported";
        QStringList files;
        QDirIterator it(info.absoluteFilePath(), QDir::Files | QDir::Readable,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto file = it.next();
            if (media.contains(QFileInfo(file).suffix().toLower()))
                files << file;
            if (files.size() > 500)
                throw std::runtime_error("Import at most 500 files at a time");
        }
        std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
            return QString::localeAwareCompare(a, b) < 0;
        });
        for (const auto &file : files)
            requests.push_back({QUrl::fromLocalFile(file), drop, name});
    }
    if (requests.size() + m_importQueue.size() > 500)
        throw std::runtime_error("Import at most 500 files at a time");
    return requests;
}
void Editor::importMedia(const QList<QUrl> &urls) {
    try {
        const auto requests = importRequests(urls, {});
        if (!m_importing)
            m_importErrors.clear();
        if (requests.isEmpty())
            throw std::runtime_error("No media files found");
        m_importQueue += requests;
        if (!m_probe)
            probeNext();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::cancelImport() {
    const auto left = m_importQueue.size() + (m_probe ? 1 : 0);
    if (left == 0)
        return;
    m_importQueue.clear();
    if (m_probe) {
        m_cancelProbe = true;
        m_probe->kill();
    }
    m_status = QString("Import stopped; %1 file%2 not added").arg(left).arg(left == 1 ? "" : "s");
    emit changed();
}
void Editor::dropFiles(const QList<QUrl> &urls, int track, qint64 frame) {
    try {
        m_project.requireEditable(track);
        auto batch = std::make_shared<DropBatch>();
        batch->trackId = m_project.trackSettings[track].id;
        batch->frame = std::max(qint64(0), frame);
        const auto requests = importRequests(urls, batch);
        if (!m_importing)
            m_importErrors.clear();
        if (requests.isEmpty())
            throw std::runtime_error("No media files found");
        m_importQueue += requests;
        if (!m_probe)
            probeNext();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::probeNext() {
    if (m_importQueue.empty()) {
        m_importing = false;
        if (!m_importErrors.empty())
            m_error = "Some files could not be added:\n" + m_importErrors.join('\n');
        emit changed();
        return;
    }
    const auto request = m_importQueue.takeFirst();
    probeFile(request.url, {}, request.drop, request.folder);
}
void Editor::relink(const QString &id, const QUrl &url) {
    if (m_probe) {
        fail("Wait for import to finish");
        return;
    }
    if (!m_project.asset(id)) {
        fail("Select a media clip to relink");
        return;
    }
    probeFile(url, id);
}
void Editor::relinkFolder(const QUrl &url) {
    const auto root = localPath(url);
    if (!QFileInfo(root).isDir())
        return fail("Choose a folder");
    QVector<int> missing;
    for (int i = 0; i < m_project.assets.size(); ++i)
        if (!QFileInfo::exists(m_project.assets[i].path))
            missing << i;
    if (missing.isEmpty()) {
        m_status = "No media is missing";
        emit changed();
        return;
    }
    // Every file in the folder and below it by name (at most 50000 files).
    QMultiHash<QString, QString> files;
    QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    for (int count = 0; it.hasNext() && count < 50000; ++count) {
        const auto path = it.next();
        files.insert(QFileInfo(path).fileName().toLower(), path);
    }
    // A file of the same name; among several, the one whose folders match the old path's best.
    auto parts = [](const QString &path) {
        return QDir::fromNativeSeparators(path).toLower().split('/', Qt::SkipEmptyParts);
    };
    QHash<QString, QString> found; // asset id → new path
    for (int i : missing) {
        const auto &a = m_project.assets[i];
        const auto old = parts(a.path);
        QString best;
        int bestScore = -1;
        for (const auto &candidate : files.values(QFileInfo(a.path).fileName().toLower())) {
            const auto now = parts(candidate);
            int score = 0;
            while (score < old.size() && score < now.size() &&
                   old[old.size() - 1 - score] == now[now.size() - 1 - score])
                ++score;
            if (score > bestScore || (score == bestScore && candidate < best)) {
                bestScore = score;
                best = candidate;
            }
        }
        if (!best.isEmpty())
            found[a.id] = QDir::cleanPath(best);
    }
    if (!found.isEmpty())
        mutate([&](Project &p) {
            for (auto &a : p.assets)
                if (found.contains(a.id))
                    a.path = found[a.id];
        });
    const auto left = missing.size() - found.size();
    m_status = left == 0 ? QString("Relinked %1 media files").arg(found.size())
                         : QString("Relinked %1 of %2 media files; %3 still missing")
                               .arg(found.size())
                               .arg(missing.size())
                               .arg(left);
    emit projectChanged();
    emit changed();
}
QString rasterizeSvg(const QString &svg, const QString &folder, int longest) {
    QSvgRenderer renderer(svg);
    if (!renderer.isValid())
        throw std::runtime_error("Cannot read this SVG file");
    QSizeF size = renderer.viewBoxF().size();
    if (size.isEmpty())
        size = renderer.defaultSize();
    if (size.isEmpty())
        throw std::runtime_error("The SVG file has no size");
    size.scale(longest, longest, Qt::KeepAspectRatio);
    QImage image(std::max(1, qRound(size.width())), std::max(1, qRound(size.height())),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    painter.end();
    // One folder per source file and version, so the picture keeps the file's name.
    QFile source(svg);
    if (!source.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot read this SVG file");
    const auto key = QCryptographicHash::hash(source.readAll(), QCryptographicHash::Sha1).toHex().left(12);
    const auto dir = folder + "/" + QString::fromLatin1(key);
    QDir().mkpath(dir);
    const auto png = dir + "/" + QFileInfo(svg).completeBaseName() + ".png";
    if (!image.save(png, "PNG"))
        throw std::runtime_error("Cannot write the picture of the SVG file");
    return png;
}
// AVIF and HEIC/HEIF pictures as a PNG in `folder` (FFmpeg decodes them, but only image files
// can be looped as stills), one folder per file content so the picture keeps the file's name.
static QString convertPicture(const QString &picture, const QString &ffmpeg, const QString &folder) {
    QFile source(picture);
    if (!source.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot read this picture");
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&source))
        throw std::runtime_error("Cannot read this picture");
    const auto dir = folder + "/" + QString::fromLatin1(hash.result().toHex().left(12));
    const auto png = dir + "/" + QFileInfo(picture).completeBaseName() + ".png";
    if (QFileInfo::exists(png))
        return png;
    QDir().mkpath(dir);
    QProcess p;
    p.start(ffmpeg, {"-hide_banner", "-nostdin", "-v", "error", "-y", "-i", picture, "-frames:v",
                     "1", "-update", "1", png});
    if (!p.waitForFinished(60000) || p.exitCode() != 0 || !QFileInfo::exists(png)) {
        p.kill();
        QFile::remove(png);
        throw std::runtime_error("Cannot decode this picture: " +
                                 QString::fromUtf8(p.readAllStandardError()).left(300).toStdString());
    }
    return png;
}
void Editor::probeFile(const QUrl &url, const QString &replaceId, std::shared_ptr<DropBatch> drop,
                       const QString &folder) {
    QString path;
    try {
        path = localPath(url);
        if (!QFileInfo(path).isFile())
            throw std::runtime_error("Media file does not exist");
        // Vector graphics become a sharp, transparent picture that is imported instead.
        if (QFileInfo(path).suffix().compare("svg", Qt::CaseInsensitive) == 0)
            path = rasterizeSvg(path, m_data + "/svg");
        else if (QStringList{"avif", "heic", "heif"}.contains(QFileInfo(path).suffix().toLower()))
            path = convertPicture(path, executable("ffmpeg"), m_data + "/pictures");
    } catch (const std::exception &e) {
        m_importErrors << QString::fromUtf8(e.what()) + ": " + url.fileName();
        fail(e.what());
        probeNext();
        return;
    }
    auto *process = new QProcess(this);
    m_probe = process;
    m_importing = true;
    m_status = "Reading " + QFileInfo(path).fileName();
    emit changed();
    auto complete = [this, process, path, replaceId, drop, folder](bool success) {
        const auto bytes = process->readAllStandardOutput();
        const auto error = QString::fromUtf8(process->readAllStandardError());
        m_probe = nullptr;
        process->deleteLater();
        if (m_cancelProbe) {
            m_cancelProbe = false;
            probeNext();
            return;
        }
        if (!success) {
            const auto message =
                "Cannot read media: " + QFileInfo(path).fileName() + "\n" + error.left(1000);
            m_importErrors << message;
            fail(message);
        } else
            try {
                auto root = QJsonDocument::fromJson(bytes).object();
                Asset a;
                a.id = replaceId.isEmpty() ? newId() : replaceId;
                a.path = path;
                a.name = QFileInfo(path).fileName();
                a.duration = root["format"].toObject()["duration"].toString().toDouble();
                for (const auto &v : root["streams"].toArray()) {
                    const auto s = v.toObject();
                    const auto type = s["codec_type"].toString();
                    if (type == "video" && a.width == 0) {
                        a.width = s["width"].toInt();
                        a.height = s["height"].toInt();
                        auto rate = [&](const char *key) {
                            const auto parts = s[key].toString().split('/');
                            return parts.size() == 2 && parts[1].toDouble() > 0
                                       ? parts[0].toDouble() / parts[1].toDouble()
                                       : 0.;
                        };
                        const double nominal = rate("r_frame_rate"), average = rate("avg_frame_rate");
                        a.frameRate = average > 0 ? average : nominal;
                        a.variableRate = isVariableRate(nominal, average);
                        // Phones store portrait video as rotated landscape; FFmpeg decodes
                        // it upright, so report the upright size.
                        int rotation = s["tags"].toObject()["rotate"].toString().toInt();
                        for (const auto &d : s["side_data_list"].toArray())
                            if (d.toObject().contains("rotation"))
                                rotation = d.toObject()["rotation"].toInt();
                        if (std::abs(rotation) % 180 == 90)
                            std::swap(a.width, a.height);
                    }
                    if (type == "audio")
                        a.hasAudio = true;
                    const auto duration = s["duration"].toString().toDouble();
                    if (duration > 0 && (a.duration == 0 || duration < a.duration))
                        a.duration = duration;
                }
                const auto ext = QFileInfo(path).suffix().toLower();
                const bool still =
                    QStringList{"png", "jpg", "jpeg", "bmp", "webp", "tif", "tiff", "avif", "heic",
                                "heif"}
                        .contains(ext);
                a.kind = still ? "image" : (a.width > 0 ? "video" : "audio");
                // An animated GIF plays in a loop, like a sticker.
                a.loops = ext == "gif" && a.kind == "video";
                if (still) {
                    a.duration = m_prefs.value("stillSeconds").toDouble();
                    a.frameRate = 0;
                    a.variableRate = false;
                }
                if ((a.width == 0 && !a.hasAudio) || a.duration <= 0)
                    throw std::runtime_error("No supported finite video/audio stream found");
                QString addedClip, dropWarning;
                const bool imported = mutate([&](Project &p) {
                    for (const auto &clip : p.clips)
                        if (clip.assetId == replaceId)
                            p.requireEditable(clip.track);
                    if (replaceId.isEmpty()) {
                        // Into the dropped folder's own library folder, or else the one on
                        // show if it still exists.
                        if (!folder.isEmpty()) {
                            if (!p.folders.contains(folder))
                                p.folders << folder;
                            a.folder = folder;
                        } else if (p.folders.contains(m_importFolder))
                            a.folder = m_importFolder;
                        p.assets.push_back(a);
                    } else
                        for (auto &asset : p.assets)
                            if (asset.id == replaceId) {
                                if (asset.kind != a.kind)
                                    throw std::runtime_error(
                                        "Replacement must have the same media type");
                                const auto folder = asset.folder;
                                asset = a;
                                asset.folder = folder;
                            }
                    if (drop) {
                        int track = -1;
                        for (int i = 0; i < p.tracks; ++i)
                            if (p.trackSettings[i].id == drop->trackId)
                                track = i;
                        if (track < 0 || p.trackSettings[track].locked)
                            dropWarning = a.name + ": imported into the library; destination track "
                                                   "was removed or locked.";
                        else {
                            auto frame = drop->frame;
                            if (auto *last = p.clip(drop->lastClip); last && last->track == track)
                                frame = last->start + last->duration;
                            addedClip = insert(p, a.id, track, frame);
                        }
                    }
                });
                if (!imported)
                    m_importErrors << a.name + ": " + m_error;
                else {
                    m_status = replaceId.isEmpty() ? "Imported " + a.name : "Media relinked";
                    if (!dropWarning.isEmpty())
                        m_importErrors << dropWarning;
                    if (!addedClip.isEmpty()) {
                        drop->lastClip = addedClip;
                        const auto *c = m_project.clip(addedClip);
                        drop->frame = c->start + c->duration;
                        select(addedClip);
                    }
                }
            } catch (const std::exception &e) {
                m_importErrors << QFileInfo(path).fileName() + ": " + QString::fromUtf8(e.what());
                fail(e.what());
            }
        probeNext();
        emit changed();
    };
    connect(process, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            complete(false);
    });
    QTimer::singleShot(30000, process, [process] {
        if (process->state() != QProcess::NotRunning)
            process->kill();
    });
    process->start(executable("ffprobe"), {"-v", "error", "-protocol_whitelist", "file,pipe",
                                           "-show_format", "-show_streams", "-of", "json", path});
}
void Editor::addAsset(const QString &id, int track) {
    qint64 end = 0;
    for (const auto &c : m_project.clips)
        if (c.track == track)
            end = std::max(end, c.start + c.duration);
    insertAsset(id, track, end);
}
QString Editor::insert(Project &p, const QString &id, int track, qint64 frame) {
    p.requireEditable(track);
    const auto *a = p.asset(id);
    if (!a)
        throw std::runtime_error("Media no longer exists");
    Clip c;
    c.id = newId();
    c.assetId = id;
    c.name = a->name;
    c.track = track;
    c.start = frame;
    c.duration = std::max(qint64(1), qint64(std::floor(a->duration * p.fpsN / p.fpsD + 1e-6)));
    p.clips.push_back(c);
    p.move(c.id, track, frame);
    return c.id;
}
bool Editor::insertAsset(const QString &id, int track, qint64 frame) {
    QString added;
    if (!mutate([&](Project &p) { added = insert(p, id, track, frame); }))
        return false;
    select(added);
    return true;
}
void Editor::addTitle() {
    const auto id = newId();
    mutate([&](Project &p) {
        Clip c;
        c.id = id;
        c.name = "Title";
        c.text = "Your story starts here";
        c.track = p.tracks - 1;
        p.requireEditable(c.track);
        c.start = m_playhead;
        c.duration = qRound64(3. * p.fpsN / p.fpsD);
        p.clips.push_back(c);
        p.move(c.id, c.track, c.start);
    });
    select(id);
}
void Editor::addCaption() {
    const auto id = newId();
    mutate([&](Project &p) {
        // The caption track: the automatic one if there is one, else the top track.
        int track = p.tracks - 1;
        for (int t = 0; t < p.tracks; ++t)
            if (p.trackSettings[t].name == captionTrackName)
                track = t;
        p.requireEditable(track);
        Clip c;
        c.id = id;
        c.name = "Caption";
        c.text = "Caption";
        c.track = track;
        c.start = m_playhead;
        c.fontSize = 48;
        c.y = .32;
        c.duration = qRound64(2. * p.fpsN / p.fpsD);
        // Up to the next clip on the track, so captions follow each other without overlapping.
        for (const auto &x : p.clips)
            if (x.track == track && x.start + x.duration > c.start) {
                if (x.start <= c.start)
                    throw std::runtime_error("There is a caption at the playhead already");
                c.duration = std::min(c.duration, x.start - c.start);
            }
        p.clips.push_back(c);
    });
    select(id);
}
bool Editor::overwriteAsset(const QString &assetId, int track, qint64 frame) {
    QString added;
    if (!mutate([&](Project &p) {
            p.requireEditable(track);
            const auto *a = p.asset(assetId);
            if (!a)
                throw std::runtime_error("Media no longer exists");
            frame = std::max<qint64>(0, frame);
            const qint64 end =
                frame + std::max(qint64(1), qint64(std::floor(a->duration * p.fpsN / p.fpsD + 1e-6)));
            // Edit without magnet so nothing slides; tracks keep their setting afterwards.
            QVector<bool> magnetic;
            for (auto &t : p.trackSettings) {
                magnetic << t.magnetic;
                t.magnetic = false;
            }
            // Cut whatever runs across the start and the end of the range, with its sound.
            for (const qint64 cut : {frame, end}) {
                QStringList across;
                for (const auto &x : p.clips)
                    if (x.track == track && x.start < cut && x.start + x.duration > cut)
                        across << x.id;
                for (const auto &id : across)
                    for (const auto &g : QStringList{id} + p.linkedClips(id))
                        p.split(g, cut);
            }
            // After splitting, remove every piece (with its sound) inside the range.
            QStringList inside;
            for (const auto &x : p.clips)
                if (x.track == track && x.start >= frame && x.start + x.duration <= end)
                    inside << x.id;
            for (const auto &id : inside) {
                for (const auto &partner : p.linkedClips(id))
                    if (const auto *x = p.clip(partner); x && x->start >= frame &&
                                                         x->start + x->duration <= end)
                        p.remove(partner, false);
                p.remove(id, false);
            }
            Clip c;
            c.id = newId();
            c.assetId = assetId;
            c.name = a->name;
            c.track = track;
            c.start = frame;
            c.duration = end - frame;
            p.clips.push_back(c);
            added = c.id;
            for (int t = 0; t < p.trackSettings.size(); ++t)
                p.trackSettings[t].magnetic = magnetic[t];
        }))
        return false;
    select(added);
    return true;
}
void Editor::addTitleTemplate(const QString &style) {
    if (!QStringList{"lowerThird", "lowerThirdLine", "titleCard"}.contains(style))
        return fail("Unknown title template");
    const auto id = newId();
    mutate([&](Project &p) {
        Clip c;
        c.id = id;
        c.name = style == "titleCard" ? "Title card" : "Lower third";
        c.titleStyle = style;
        c.text = style == "titleCard" ? "Chapter title\nWhat this part is about"
                                      : "Your Name\nYour role or topic";
        c.fadeIn = 0.3;
        c.fadeOut = 0.3;
        c.track = p.tracks - 1;
        p.requireEditable(c.track);
        c.start = m_playhead;
        c.duration = qRound64((style == "titleCard" ? 3. : 5.) * p.fpsN / p.fpsD);
        p.clips.push_back(c);
        p.move(c.id, c.track, c.start);
    });
    select(id);
}
void Editor::addEffect(const QString &effect) {
    if (effect != "blur" && effect != "pixelate" && effect != "adjust")
        return fail("Unknown effect");
    const auto id = newId();
    mutate([&](Project &p) {
        Clip c;
        c.id = id;
        c.name = effect == "blur" ? "Blur area" : effect == "adjust" ? "Adjustment layer" : "Mosaic area";
        c.effect = effect;
        c.track = p.tracks - 1;
        p.requireEditable(c.track);
        c.start = m_playhead;
        c.duration = qRound64(5. * p.fpsN / p.fpsD);
        p.clips.push_back(c);
        p.move(c.id, c.track, c.start);
    });
    select(id);
}
void Editor::arrange(const QString &layout, bool fill) {
    static const QStringList layouts{"side",   "stack",  "grid",   "pip-tl",    "pip-tr",
                                     "pip-bl", "pip-br", "presenter", "full"};
    if (!layouts.contains(layout))
        return fail("Unknown layout");
    // Pictures in the selection, lowest track first, then by start.
    QVector<const Clip *> clips;
    for (const auto &id : selection()) {
        const auto *c = m_project.clip(id);
        const auto *a = c ? m_project.asset(c->assetId) : nullptr;
        if (c && a && a->kind != "audio" && !c->audioOnly)
            clips << c;
    }
    std::sort(clips.begin(), clips.end(), [](const Clip *a, const Clip *b) {
        return a->track != b->track ? a->track < b->track : a->start < b->start;
    });
    const int need = layout == "full" ? 1 : 2, most = layout == "grid" ? 4 : layout == "full" ? 64 : 2;
    if (clips.size() < need)
        return fail(layout == "full" ? "Select the pictures to show full size"
                                     : "Select two pictures (Ctrl+click) to arrange");
    if (clips.size() > most)
        return fail(QString("This layout takes at most %1 pictures").arg(most));
    const double W = m_project.width, H = m_project.height, aspect = W / H;
    // Slots as centre and size in canvas fractions; each picture fits inside its slot.
    struct Slot {
        double cx, cy, w, h;
        bool round = false;
    };
    QVector<Slot> areas;
    const double small = 0.3, margin = 0.03;
    if (layout == "side")
        areas = {{0.25, 0.5, 0.5, 1}, {0.75, 0.5, 0.5, 1}};
    else if (layout == "stack")
        areas = {{0.5, 0.25, 1, 0.5}, {0.5, 0.75, 1, 0.5}};
    else if (layout == "grid")
        areas = {{0.25, 0.25, 0.5, 0.5}, {0.75, 0.25, 0.5, 0.5}, {0.25, 0.75, 0.5, 0.5}, {0.75, 0.75, 0.5, 0.5}};
    else if (layout.startsWith("pip")) {
        const bool right = layout.endsWith('r'), bottom = layout[4] == 'b';
        const double w = small, h = small; // fits inside; the picture keeps its shape
        areas = {{0.5, 0.5, 1, 1},
                 {right ? 1 - margin - w / 2 : margin + w / 2,
                  bottom ? 1 - margin * aspect - h / 2 : margin * aspect + h / 2, w, h}};
    } else if (layout == "presenter") {
        const double d = 0.24; // the round presenter's diameter, as a fraction of the width
        areas = {{0.02 + 0.37, 0.5, 0.74, 0.9},
                 {1 - 0.02 - d / 2, 1 - 0.05 - d * aspect / 2, d, d * aspect, true}};
    }
    mutate([&](Project &p) {
        for (int i = 0; i < clips.size(); ++i) {
            auto *c = p.clip(clips[i]->id);
            p.requireEditable(c->track);
            for (const auto &k : {"scale", "x", "y"})
                c->keyframes.remove(k);
            c->cropLeft = c->cropRight = c->cropTop = c->cropBottom = 0;
            // Crops the picture to the shape of a w × h area (canvas pixels).
            auto cropTo = [&](double w, double h) {
                const auto size = p.pictureSize(*c, W, H);
                const double picture = size.width() / size.height(), area = w / h;
                if (picture > area)
                    c->cropLeft = c->cropRight = (1 - area / picture) / 2;
                else
                    c->cropTop = c->cropBottom = (1 - picture / area) / 2;
            };
            if (layout == "full") {
                if (c->shape == "circle")
                    c->shape = "rect";
                if (fill)
                    cropTo(W, H);
                c->scale = 1;
                c->x = c->y = 0;
                continue;
            }
            const auto &slot = areas[i];
            if (fill && !slot.round)
                cropTo(slot.w * W, slot.h * H);
            if (slot.round)
                c->shape = "circle";
            else if (c->shape == "circle")
                c->shape = "rect";
            // The picture's size at scale 1, then the scale that fits it into the slot.
            const auto fit = p.pictureSize(*c, W, H);
            c->scale = std::clamp(std::min(slot.w * W / fit.width(), slot.h * H / fit.height()), 0.1, 5.);
            c->x = slot.cx - 0.5;
            c->y = slot.cy - 0.5;
        }
    });
    m_status = "Arranged " + QString::number(clips.size()) + " pictures";
    emit changed();
}
QVector<const Clip *> Editor::selectedPictures() const {
    QVector<const Clip *> clips;
    for (const auto &id : selection()) {
        const auto *c = m_project.clip(id);
        const auto *a = c ? m_project.asset(c->assetId) : nullptr;
        if (c && !c->audioOnly && (!a || a->kind != "audio"))
            clips << c;
    }
    std::sort(clips.begin(), clips.end(), [](const Clip *a, const Clip *b) {
        return a->track != b->track ? a->track < b->track : a->start < b->start;
    });
    return clips;
}
void Editor::saveLayouts() {
    QDir().mkpath(m_data);
    QSaveFile f(m_data + "/layouts.json");
    const auto data = QJsonDocument(m_layouts).toJson();
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
        fail("Cannot save the layouts in " + m_data);
}
// The numbers of a picture's placement a saved layout keeps (besides shape and border colour).
static const QVector<QPair<QString, double Clip::*>> &layoutFields() {
    static const QVector<QPair<QString, double Clip::*>> fields{
        {"crop", &Clip::crop},         {"cropLeft", &Clip::cropLeft},
        {"cropRight", &Clip::cropRight}, {"cropTop", &Clip::cropTop},
        {"cropBottom", &Clip::cropBottom}, {"radius", &Clip::radius},
        {"feather", &Clip::feather},   {"border", &Clip::border},
        {"shadow", &Clip::shadow}};
    return fields;
}
void Editor::saveLayout(const QString &name) {
    const auto label = name.trimmed().left(60);
    if (label.isEmpty())
        return fail("Name the layout");
    const auto clips = selectedPictures();
    if (clips.isEmpty())
        return fail("Select the clips whose places to keep (Ctrl+click several)");
    QJsonArray places;
    for (const auto *c : clips) {
        // Position, size and rotation as shown at the playhead, also when animated.
        const double local = std::clamp<qint64>(m_playhead - c->start, 0, c->duration - 1);
        QJsonObject slot{{"scale", c->valueAt("scale", local)},
                         {"x", c->valueAt("x", local)},
                         {"y", c->valueAt("y", local)},
                         {"rotation", c->valueAt("rotation", local)},
                         {"shape", c->shape},
                         {"borderColor", c->borderColor}};
        for (const auto &[k, field] : layoutFields())
            slot[k] = c->*field;
        places.append(slot);
    }
    const QJsonObject layout{{"name", label}, {"slots", places}};
    for (qsizetype i = 0; i < m_layouts.size(); ++i)
        if (m_layouts[i].toObject()["name"].toString().compare(label, Qt::CaseInsensitive) == 0) {
            m_layouts[i] = layout;
            saveLayouts();
            m_status = "Layout updated: " + label;
            emit changed();
            return;
        }
    if (m_layouts.size() >= 100)
        return fail("Remove a layout first (100 at most)");
    m_layouts.append(layout);
    saveLayouts();
    m_status = QString("Layout saved: %1 (%2 picture%3)")
                   .arg(label)
                   .arg(places.size())
                   .arg(places.size() == 1 ? "" : "s");
    emit changed();
}
void Editor::applyLayout(const QString &name) {
    QJsonArray places;
    for (const auto &v : m_layouts)
        if (v.toObject()["name"].toString() == name)
            places = v.toObject()["slots"].toArray();
    if (places.isEmpty())
        return fail("No such layout");
    QStringList ids;
    for (const auto *c : selectedPictures())
        ids << c->id;
    if (ids.isEmpty())
        return fail("Select the clips to place (Ctrl+click several)");
    if (ids.size() > places.size())
        return fail(QString("This layout has places for %1 picture%2")
                        .arg(places.size())
                        .arg(places.size() == 1 ? "" : "s"));
    // The project's checks reject values a hand-edited file might hold.
    mutate([&](Project &p) {
        for (int i = 0; i < ids.size(); ++i) {
            auto *c = p.clip(ids[i]);
            p.requireEditable(c->track);
            const auto slot = places[i].toObject();
            c->scale = slot["scale"].toDouble(1);
            c->x = slot["x"].toDouble(0);
            c->y = slot["y"].toDouble(0);
            c->rotation = slot["rotation"].toDouble(0);
            c->shape = slot["shape"].toString("rect");
            c->borderColor = slot["borderColor"].toString("#ffffff");
            for (const auto &[k, field] : layoutFields())
                c->*field = slot[k].toDouble(0);
            for (const auto &k : {"scale", "x", "y", "rotation"})
                c->keyframes.remove(k);
        }
    });
}
void Editor::removeLayout(const QString &name) {
    for (qsizetype i = 0; i < m_layouts.size(); ++i)
        if (m_layouts[i].toObject()["name"].toString() == name) {
            m_layouts.removeAt(i);
            saveLayouts();
            emit changed();
            return;
        }
}
void Editor::speedRamp(const QString &preset) {
    // Speed factors along the clip (0..1), joined by straight lines.
    static const QHash<QString, QVector<QPointF>> curves{
        {"montage", {{0, 2.5}, {0.25, 0.6}, {0.5, 2.5}, {0.75, 0.6}, {1, 2.5}}},
        {"hero", {{0, 1}, {0.35, 1}, {0.5, 0.25}, {0.65, 1}, {1, 1}}},
        {"bullet", {{0, 2.5}, {0.4, 0.2}, {0.6, 0.2}, {1, 2.5}}},
        {"jumpCut", {{0, 0.6}, {0.45, 0.6}, {0.55, 4}, {1, 4}}},
        {"flashIn", {{0, 4}, {1, 1}}},
        {"flashOut", {{0, 1}, {1, 4}}}};
    if (!curves.contains(preset))
        return fail("Unknown speed ramp");
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || a->kind == "image")
        return fail("Select a video or sound clip for a speed ramp");
    const int parts = int(std::min<qint64>(8, c->duration / 3));
    if (parts < 2)
        return fail("The clip is too short for a speed ramp");
    const auto &curve = curves[preset];
    auto factor = [&](double t) {
        for (int i = 1; i < curve.size(); ++i)
            if (t <= curve[i].x())
                return curve[i - 1].y() + (curve[i].y() - curve[i - 1].y()) *
                                              (t - curve[i - 1].x()) / (curve[i].x() - curve[i - 1].x());
        return curve.last().y();
    };
    const auto id = c->id;
    const bool done = mutate([&](Project &p) {
        const auto *first = p.clip(id);
        const qint64 start = first->start, end = first->start + first->duration;
        const auto group = QStringList{id} + p.linkedClips(id);
        QSet<int> tracks;
        for (const auto &clipId : group) {
            p.requireEditable(p.clip(clipId)->track);
            tracks.insert(p.clip(clipId)->track);
        }
        // Equal parts of the source, cut from the end so the clip keeps its id as the first.
        for (int k = parts - 1; k >= 1; --k)
            for (const auto &clipId : group)
                p.split(clipId, start + qRound64(double(k) * (end - start) / parts));
        qint64 newEnd = end;
        for (const auto track : tracks) {
            QVector<Clip *> pieces;
            for (auto &x : p.clips)
                if (x.track == track && x.start >= start && x.start < end)
                    pieces << &x;
            std::sort(pieces.begin(), pieces.end(),
                      [](const Clip *l, const Clip *r) { return l->start < r->start; });
            qint64 cursor = start;
            for (int k = 0; k < pieces.size(); ++k) {
                auto *piece = pieces[k];
                const auto old = piece->speed;
                const double speed =
                    std::clamp(old.seconds() * factor((k + 0.5) / pieces.size()), 0.1, 100.);
                piece->speed = Time(qRound64(speed * 1000), 1000);
                piece->scaleKeyframes(old.seconds() / piece->speed.seconds());
                piece->duration = std::max<qint64>(
                    1, qint64(std::floor(piece->duration * old.seconds() / piece->speed.seconds())));
                piece->start = cursor;
                cursor += piece->duration;
            }
            newEnd = cursor;
        }
        // Later clips on those tracks follow the new length.
        for (auto &x : p.clips)
            if (tracks.contains(x.track) && x.start >= end)
                x.start += newEnd - end;
    });
    if (!done)
        return;
    m_status = "Speed ramp applied";
    emit changed();
}
void Editor::applyMotion(const QString &preset) {
    static const QStringList presets{"popIn", "popOut", "slideLeft", "slideUp", "pulse", "wiggle", "none"};
    if (!presets.contains(preset))
        return fail("Unknown motion");
    const auto ids = selection();
    if (ids.isEmpty())
        return fail("Select a clip to animate");
    mutate([&](Project &p) {
        const double fps = double(p.fpsN) / p.fpsD;
        for (const auto &id : ids) {
            auto *c = p.clip(id);
            if (!c || c->audioOnly)
                continue;
            if (const auto *a = p.asset(c->assetId); a && a->kind == "audio")
                continue;
            p.requireEditable(c->track);
            const auto at = [&](double seconds) {
                return std::clamp<qint64>(qRound64(seconds * fps), 0, c->duration - 1);
            };
            const auto end = c->duration - 1;
            // Motions move around the clip's own (static) value, which they leave as it is, so
            // removing them returns the clip to it.
            auto rest = [&](const QString &property) { return c->staticValue(property); };
            auto set = [&](const QString &property, QVector<Keyframe> keys) {
                QVector<Keyframe> sorted;
                for (const auto &k : keys)
                    if (sorted.isEmpty() || k.frame > sorted.last().frame)
                        sorted << k;
                if (sorted.size() < 2)
                    c->keyframes.remove(property);
                else
                    c->keyframes[property] = sorted;
            };
            const double scale = rest("scale"), x = rest("x"), y = rest("y");
            auto key = [](qint64 frame, double value, const QString &ease) {
                Keyframe k{frame, value, ease != "linear"};
                k.ease = ease;
                return k;
            };
            if (preset == "none") {
                for (const auto *property : {"scale", "x", "y", "rotation"})
                    set(property, {});
            } else if (preset == "popIn") {
                set("scale", {key(0, 0.1, "out"), key(at(0.25), std::min(5., scale * 1.15), "smooth"),
                              key(at(0.4), scale, "smooth")});
            } else if (preset == "popOut") {
                set("scale", {key(std::max<qint64>(0, end - at(0.3)), scale, "in"), key(end, 0.1, "linear")});
            } else if (preset == "slideLeft") {
                set("x", {key(0, std::max(-2., x - 1), "out"), key(at(0.5), x, "smooth")});
            } else if (preset == "slideUp") {
                set("y", {key(0, std::min(2., y + 1), "out"), key(at(0.5), y, "smooth")});
            } else if (preset == "pulse") {
                // Beats of 0.6 s over the whole clip (at most 400 keyframes).
                QVector<Keyframe> keys;
                for (int i = 0; i < 400; ++i) {
                    const auto frame = at(0.3 * i);
                    keys << key(frame, i % 2 ? std::min(5., scale * 1.12) : scale, "smooth");
                    if (frame >= end)
                        break;
                }
                set("scale", keys);
            } else if (preset == "wiggle") {
                // A short shake of ±8° that settles within a second.
                const double angle = rest("rotation");
                QVector<Keyframe> keys;
                for (int i = 0; i <= 8; ++i)
                    keys << key(at(0.12 * i),
                                angle + (i == 8 ? 0 : (i % 2 ? 8. : -8.) * (1 - i / 8.)), "smooth");
                set("rotation", keys);
            }
        }
    });
    m_status = preset == "none" ? QString("Motion removed") : "Motion applied: " + preset;
    emit changed();
}
void Editor::addGraphic(const QString &kind) {
    if (!graphicKinds().contains(kind))
        return fail("Unknown shape");
    const auto id = newId();
    mutate([&](Project &p) {
        Clip c;
        c.id = id;
        c.graphic = kind;
        static const QHash<QString, QString> names{
            {"bubble", "Speech bubble"}, {"arrow", "Arrow"},   {"line", "Line"},
            {"ellipse", "Circle"},       {"rectangle", "Box"}, {"check", "Check mark"},
            {"cross", "Cross"},          {"star", "Star"},     {"heart", "Heart"},
            {"warning", "Warning"},      {"info", "Info"},     {"cursor", "Mouse pointer"},
            {"click", "Mouse click"},    {"lightbulb", "Light bulb"}};
        c.name = names.value(kind, "Shape");
        if (kind == "bubble") {
            c.text = "Hello!";
            c.fillColor = "#ffffff";
            c.textColor = "#14181d";
            c.textShadow = 0;
            c.fontSize = 48;
            c.stroke = 0.004;
        } else if (kind == "arrow" || kind == "line") {
            c.graphicHeight = kind == "arrow" ? 0.12 : 0.012;
            c.fillColor = kind == "arrow" ? "#ff5a5f" : "#ffffff";
        } else if (QStringList{"check", "cross", "star", "heart", "warning", "info", "cursor",
                               "click", "lightbulb"}
                       .contains(kind)) {
            // Icons: square, at about a ninth of the picture's height.
            static const QHash<QString, QString> colours{
                {"check", "#3ecf6e"}, {"cross", "#ff5a5f"},   {"star", "#ffd23f"},
                {"heart", "#ff5a7a"}, {"warning", "#ffb020"}, {"info", "#4aa3ff"},
                {"cursor", "#ffffff"}, {"click", "#ffffff"},  {"lightbulb", "#ffd23f"}};
            c.graphicHeight = 0.16;
            c.graphicWidth = 0.16 * p.height / p.width;
            c.fillColor = colours.value(kind);
            if (kind == "cursor" || kind == "click") {
                c.strokeColor = "#000000";
                c.stroke = 0.004;
            }
        } else if (kind == "ellipse") {
            // An outline circle, like a highlight around something on screen.
            c.graphicWidth = 0.2;
            c.graphicHeight = 0.2 * p.width / p.height;
            c.fillColor = "#00000000";
            c.strokeColor = "#ff5a5f";
            c.stroke = 0.008;
        }
        c.track = p.tracks - 1;
        p.requireEditable(c.track);
        c.start = m_playhead;
        c.duration = qRound64(4. * p.fpsN / p.fpsD);
        p.clips.push_back(c);
        p.move(c.id, c.track, c.start);
    });
    select(id);
}
void Editor::moveClip(const QString &id, qint64 frame, int track) {
    mutate([&](Project &p) {
        const auto linked = p.linkedClips(id);
        const auto *c = p.clip(id);
        if (!c)
            return;
        const auto old = c->start;
        const int oldTrack = c->track;
        // Other selected clips (and everyone's linked sound or picture) come along.
        QStringList others;
        const auto selected = selection();
        if (selected.contains(id))
            for (const auto &x : selected)
                if (x != id)
                    others << x;
        p.move(id, track, frame);
        const auto delta = p.clip(id)->start - old;
        const int shift = p.clip(id)->track - oldTrack;
        QStringList followers = linked;
        for (const auto &x : others)
            for (const auto &f : QStringList{x} + p.linkedClips(x))
                if (f != id && !followers.contains(f))
                    followers << f;
        for (const auto &other : followers) {
            const auto *x = p.clip(other);
            // Selected clips change track by the same amount where that track exists.
            const int target = others.contains(other) && x->track + shift >= 0 &&
                                       x->track + shift < p.tracks
                                   ? x->track + shift
                                   : x->track;
            if (delta || target != x->track)
                p.move(other, target, std::max<qint64>(0, x->start + delta));
        }
    });
}
void Editor::unlinkClip() {
    mutate([&](Project &p) {
        const auto linked = p.linkedClips(m_selected);
        if (linked.isEmpty())
            throw std::runtime_error("This clip is not linked");
        for (const auto &id : linked + QStringList{m_selected})
            p.clip(id)->link = "none";
    });
}
void Editor::setClip(const QString &key, const QVariant &v) {
    setClipValues({{key, v}});
}
void Editor::setClipValues(const QVariantMap &values) {
    // One undo step for several values, e.g. position and size from a preview drag.
    mutate([&](Project &p) {
        for (auto it = values.begin(); it != values.end(); ++it)
            applyClipValue(p, it.key(), it.value());
    });
}
void Editor::applyClipValue(Project &p, const QString &key, const QVariant &v) {
    {
        auto *c = p.clip(m_selected);
        if (!c)
            return;
        p.requireEditable(c->track);
        const auto order = p.trackOrder(c->track);
        if (key == "track" || key == "start") {
            p.move(c->id, key == "track" ? v.toInt() : c->track,
                   key == "start" ? v.toLongLong() : c->start);
            return;
        }
        if (c->keyframes.contains(key)) {
            // Animated property: edits set the keyframe at the playhead.
            const auto frame = m_playhead - c->start;
            if (frame < 0 || frame >= c->duration)
                throw std::runtime_error("Move the playhead into the clip to change an animated "
                                         "value");
            auto &list = c->keyframes[key];
            auto it = std::find_if(list.begin(), list.end(),
                                   [&](const Keyframe &k) { return k.frame >= frame; });
            if (it != list.end() && it->frame == frame)
                it->value = v.toDouble();
            else
                list.insert(it, {frame, v.toDouble(), true});
            return;
        }
        // Plain numbers, looked up rather than chained (compilers limit else-if depth).
        static const QHash<QString, double Clip::*> numbers{
            {"scale", &Clip::scale},
            {"x", &Clip::x},
            {"y", &Clip::y},
            {"rotation", &Clip::rotation},
            {"opacity", &Clip::opacity},
            {"volume", &Clip::volume},
            {"brightness", &Clip::brightness},
            {"contrast", &Clip::contrast},
            {"saturation", &Clip::saturation},
            {"crop", &Clip::crop},
            {"temperature", &Clip::temperature},
            {"tint", &Clip::tint},
            {"vibrance", &Clip::vibrance},
            {"shadows", &Clip::shadows},
            {"highlights", &Clip::highlights},
            {"sharpen", &Clip::sharpen},
            {"glow", &Clip::glow},
            {"vignette", &Clip::vignette},
            {"grain", &Clip::grain},
            {"lutStrength", &Clip::lutStrength},
            {"hslHue", &Clip::hslHue},
            {"hslSaturation", &Clip::hslSaturation},
            {"hslLightness", &Clip::hslLightness},
            {"eqLow", &Clip::eqLow},
            {"eqMid", &Clip::eqMid},
            {"eqHigh", &Clip::eqHigh},
            {"lowCut", &Clip::lowCut},
            {"compressor", &Clip::compressor},
            {"gate", &Clip::gate},
            {"denoise", &Clip::denoise},
            {"deess", &Clip::deess},
            {"fxStrength", &Clip::fxStrength},
            {"motionBlur", &Clip::motionBlur},
            {"reverb", &Clip::reverb},
            {"pitch", &Clip::pitch},
            {"whites", &Clip::whites},
            {"blacks", &Clip::blacks},
            {"liftX", &Clip::liftX},
            {"liftY", &Clip::liftY},
            {"gammaX", &Clip::gammaX},
            {"gammaY", &Clip::gammaY},
            {"gainX", &Clip::gainX},
            {"gainY", &Clip::gainY},
            {"stabilizeStrength", &Clip::stabilizeStrength},
            {"exposure", &Clip::exposure},
            {"echo", &Clip::echo},
            {"pan", &Clip::pan},
            {"textAnimationTime", &Clip::textAnimationTime},
            {"anchorX", &Clip::anchorX},
            {"anchorY", &Clip::anchorY},
            {"letterSpacing", &Clip::letterSpacing},
            {"lineSpacing", &Clip::lineSpacing},
            {"outline", &Clip::outline},
            {"textShadow", &Clip::textShadow},
            {"textGlow", &Clip::textGlow},
            {"background", &Clip::background},
            {"stroke", &Clip::stroke},
            {"graphicWidth", &Clip::graphicWidth},
            {"graphicHeight", &Clip::graphicHeight},
            {"fadeIn", &Clip::fadeIn},
            {"fadeOut", &Clip::fadeOut},
            {"lumaTolerance", &Clip::lumaTolerance},
            {"lumaSoftness", &Clip::lumaSoftness},
            {"radius", &Clip::radius},
            {"feather", &Clip::feather},
            {"tiltX", &Clip::tiltX},
            {"tiltY", &Clip::tiltY},
            {"border", &Clip::border},
            {"shadow", &Clip::shadow},
            {"keySimilarity", &Clip::keySimilarity},
            {"keyBlend", &Clip::keyBlend},
        };
        if (const auto number = numbers.constFind(key); number != numbers.constEnd())
            c->*number.value() = v.toDouble();
        else if (key == "duration")
            c->duration = v.toLongLong();
        else if (key == "sourceIn") {
            const auto t = v.toDouble();
            if (!std::isfinite(t) || t < 0 || t > 86400)
                throw std::runtime_error("Source time must be between 0 and 86400 seconds");
            c->sourceIn = Time(qRound64(t * 1000000), 1000000);
        } else if (key == "speed") {
            const auto speed = v.toDouble();
            if (!std::isfinite(speed) || speed < .1 || speed > 100)
                throw std::runtime_error("Speed must be 0.1–100x");
            auto old = c->speed;
            c->speed = Time(qRound64(speed * 1000), 1000);
            c->scaleKeyframes(old.seconds() / c->speed.seconds());
            c->duration = std::max(
                qint64(1), qint64(std::floor(c->duration * old.seconds() / c->speed.seconds())));
        } else if (key == "text")
            c->text = v.toString();
        else if (key == "fontSize")
            c->fontSize = v.toInt();
        else if (key == "titleStyle")
            c->titleStyle = v.toString();
        else if (key == "accentColor")
            c->accentColor = v.toString();
        else if (key == "effect")
            c->effect = v.toString();
        else if (key == "effectStrength")
            c->effectStrength = v.toDouble();
        else if (key == "cornerPin") {
            // 8 corner coordinates, or an empty list for none.
            QVector<double> pin;
            for (const auto &x : v.toList())
                pin << x.toDouble();
            if (!pin.isEmpty() && pin.size() != 8)
                throw std::runtime_error("A corner pin has four corners");
            for (auto &x : pin)
                x = std::clamp(x, 0., 1.);
            c->cornerPin = pin;
        }
        else if (key == "blur")
            c->blur = v.toDouble();
        else if (key == "lut") {
            // A file URL or path; empty removes the LUT.
            const auto path = v.typeId() == QMetaType::QUrl
                                  ? v.toUrl().toLocalFile()
                                  : v.toString();
            const QFileInfo info(path);
            if (!path.isEmpty() &&
                (!info.isFile() || !QStringList{"cube", "3dl"}.contains(info.suffix().toLower())))
                throw std::runtime_error("Choose a .cube or .3dl LUT file");
            if (info.size() > 64 * 1024 * 1024)
                throw std::runtime_error("The LUT file is too large");
            c->lut = path.isEmpty() ? QString() : QDir::cleanPath(info.absoluteFilePath());
        }
        else if (key == "captionStyle")
            c->captionStyle = v.toString();
        else if (key == "highlightColor")
            c->highlightColor = v.toString();
        else if (key == "textColor")
            c->textColor = v.toString();
        else if (key == "gradientColor")
            c->gradientColor = v.toString();
        else if (key == "cropLeft" || key == "cropRight" || key == "cropTop" ||
                 key == "cropBottom") {
            // The opposite edge gives way so that a tenth of the picture stays.
            double &edge = key == "cropLeft"    ? c->cropLeft
                           : key == "cropRight" ? c->cropRight
                           : key == "cropTop"   ? c->cropTop
                                                : c->cropBottom;
            double &other = key == "cropLeft"    ? c->cropRight
                            : key == "cropRight" ? c->cropLeft
                            : key == "cropTop"   ? c->cropBottom
                                                 : c->cropTop;
            edge = std::clamp(v.toDouble(), 0., 0.9);
            other = std::min(other, 0.9 - edge);
        } else if (key == "transition") {
            c->transition = v.toString();
            // New transitions start at half a second, like a typical dissolve.
            if (!c->transition.isEmpty() && c->transitionFrames < 2)
                c->transitionFrames = std::max<qint64>(2, qRound64(0.5 * p.fpsN / p.fpsD));
        } else if (key == "transitionFrames")
            c->transitionFrames = v.toLongLong();
#define FIELD(k, type) else if (key == #k) c->k = v.type()
        FIELD(curveMaster, toString);
        FIELD(curveRed, toString);
        FIELD(curveGreen, toString);
        FIELD(curveBlue, toString);
        FIELD(hslColors, toString);
        FIELD(fx, toString);
        FIELD(voice, toString);
        FIELD(canvasFill, toString);
        FIELD(stabilize, toBool);
        FIELD(titleSlide, toBool);
        FIELD(stabilizeZoom, toBool);
        FIELD(textAnimation, toString);
        FIELD(slowMotion, toString);
        FIELD(bold, toBool);
        FIELD(italic, toBool);
        FIELD(align, toString);
        FIELD(outlineColor, toString);
        FIELD(textGlowColor, toString);
        FIELD(backgroundColor, toString);
        FIELD(fontFamily, toString);
        FIELD(graphic, toString);
        FIELD(fillColor, toString);
        FIELD(strokeColor, toString);
        FIELD(reverse, toBool);
        FIELD(flip, toBool);
        FIELD(flipVertical, toBool);
        FIELD(blendMode, toString);
        FIELD(lumaKey, toString);
        FIELD(muted, toBool);
        FIELD(hidden, toBool);
        FIELD(effectShape, toString);
        FIELD(chromaKey, toBool);
        FIELD(aiCutout, toBool);
        FIELD(aiUpscale, toBool);
        FIELD(eyeContact, toBool);
        FIELD(shape, toString);
        FIELD(borderColor, toString);
        FIELD(keyColor, toString);
#undef FIELD
        if (p.trackSettings[c->track].magnetic)
            p.packTrack(c->track, order);
    }
}
QVariantMap Editor::clipBounds(const QString &id) const {
    const auto *c = m_project.clip(id);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!c || c->audioOnly || (a && a->kind == "audio") || c->effect == "adjust")
        return {};
    // The picture's rectangle on the canvas at the playhead, in canvas fractions.
    const double local = m_playhead - c->start;
    if (!c->titleStyle.isEmpty() && c->keyframes.isEmpty()) {
        // Title templates are drawn tightly; the frame surrounds the plate.
        const auto t = titlePlate(*c, m_project.width, m_project.height, m_project.height);
        return {{"x", (t.position.x() + c->x * m_project.width) / m_project.width},
                {"y", (t.position.y() + c->y * m_project.height) / m_project.height},
                {"width", double(t.image.width()) / m_project.width},
                {"height", double(t.image.height()) / m_project.height},
                {"rotation", 0.},
                {"inside", local >= 0 && local < c->duration}};
    }
    const double scale = c->valueAt("scale", local);
    const auto size = m_project.pictureSize(*c, m_project.width * scale,
                                            m_project.height * scale);
    const auto shift = Project::anchorShift(
        *c, m_project.pictureSize(*c, m_project.width, m_project.height), scale,
        c->valueAt("rotation", local));
    return {{"x", 0.5 + c->valueAt("x", local) + shift.x() / m_project.width -
                      size.width() / m_project.width / 2},
            {"y", 0.5 + c->valueAt("y", local) + shift.y() / m_project.height -
                      size.height() / m_project.height / 2},
            {"anchorX", c->anchorX},
            {"anchorY", c->anchorY},
            {"width", size.width() / m_project.width},
            {"height", size.height() / m_project.height},
            {"rotation", c->valueAt("rotation", local)},
            {"inside", local >= 0 && local < c->duration}};
}
void Editor::placeClip(const QString &corner) {
    const auto *c = m_project.clip(m_selected);
    if (!c)
        return;
    if (corner == "full") {
        setClipValues({{"scale", 1.}, {"x", 0.}, {"y", 0.}});
        return;
    }
    const double local = m_playhead - c->start;
    double scale = c->valueAt("scale", local);
    if (scale > 0.6)
        scale = 0.3;
    // Keep the whole element, border included, inside a margin of 3% of the height.
    const auto size = m_project.pictureSize(*c, m_project.width * scale, m_project.height * scale);
    const double border = c->border * m_project.height * scale;
    const double margin = 0.03 * m_project.height;
    const double dx = 0.5 - (size.width() / 2 + border + margin) / m_project.width,
                 dy = 0.5 - (size.height() / 2 + border + margin) / m_project.height;
    const double x = corner.endsWith("Left") ? -dx : dx, y = corner.startsWith("top") ? -dy : dy;
    setClipValues({{"scale", scale}, {"x", x}, {"y", y}});
}
void Editor::setKeyframeEasing(const QString &property, const QString &easing) {
    if (!keyframeEasings().contains(easing))
        return fail("Unknown easing");
    mutate([&](Project &p) {
        auto *c = p.clip(m_selected);
        if (!c)
            return;
        p.requireEditable(c->track);
        const auto frame = m_playhead - c->start;
        for (auto &k : c->keyframes[property])
            if (k.frame == frame) {
                k.ease = easing;
                k.smooth = easing != "linear";
                return;
            }
        throw std::runtime_error("No keyframe at the playhead");
    });
}
void Editor::toggleKeyframe(const QString &property) {
    mutate([&](Project &p) {
        auto *c = p.clip(m_selected);
        if (!c)
            return;
        p.requireEditable(c->track);
        if (!animatableProperties().contains(property))
            throw std::runtime_error("This property cannot be animated");
        const auto frame = m_playhead - c->start;
        if (frame < 0 || frame >= c->duration)
            throw std::runtime_error("Move the playhead into the clip to add a keyframe");
        auto &list = c->keyframes[property];
        auto it = std::find_if(list.begin(), list.end(),
                               [&](const Keyframe &k) { return k.frame >= frame; });
        if (it != list.end() && it->frame == frame) {
            // Removing the last keyframe keeps the value it had as the static value.
            const auto value = it->value;
            list.erase(it);
            if (list.isEmpty()) {
                c->keyframes.remove(property);
                if (property == "scale")
                    c->scale = value;
                else if (property == "x")
                    c->x = value;
                else if (property == "y")
                    c->y = value;
                else if (property == "rotation")
                    c->rotation = value;
                else if (property == "opacity")
                    c->opacity = value;
                else if (property == "pan")
                    c->pan = value;
                else
                    c->volume = value;
            }
        } else
            list.insert(it, {frame, c->valueAt(property, frame), true});
    });
}
qint64 Editor::adjacentKeyframe(bool forward) const {
    const auto *c = m_project.clip(m_selected);
    if (!c)
        return m_playhead;
    qint64 target = m_playhead;
    for (const auto &list : c->keyframes)
        for (const auto &k : list) {
            const auto frame = c->start + k.frame;
            if (frame < c->start || frame >= c->start + c->duration)
                continue;
            if (forward && frame > m_playhead && (target == m_playhead || frame < target))
                target = frame;
            if (!forward && frame < m_playhead && (target == m_playhead || frame > target))
                target = frame;
        }
    return target;
}
void Editor::slipClip(const QString &id, qint64 frames) {
    if (frames != 0)
        mutate([&](Project &p) { p.slip(id, frames); });
}
void Editor::rollCut(const QString &id, qint64 frames) {
    if (frames != 0)
        mutate([&](Project &p) { p.roll(id, frames); });
}
void Editor::slideClip(const QString &id, qint64 frames) {
    if (frames != 0)
        mutate([&](Project &p) { p.slide(id, frames); });
}
void Editor::split() {
    mutate([&](Project &p) { p.split(m_selected, m_playhead); });
}
void Editor::remove(bool ripple) {
    auto ids = selection();
    if (ids.size() <= 1) {
        mutate([&](Project &p) { p.remove(m_selected, ripple); });
        return;
    }
    // Several clips: the latest first, so ripple closes each gap at the right place.
    if (mutate([&](Project &p) {
            std::sort(ids.begin(), ids.end(), [&](const QString &a, const QString &b) {
                return p.clip(a)->start > p.clip(b)->start;
            });
            for (const auto &id : ids)
                p.remove(id, ripple);
        }))
        m_also.clear();
}
void Editor::duplicate() {
    const auto id = newId();
    mutate([&](Project &p) {
        if (auto *c = p.clip(m_selected)) {
            p.requireEditable(c->track);
            auto copy = *c;
            copy.id = id;
            copy.transition.clear();
            copy.transitionFrames = 0;
            copy.start += copy.duration;
            p.clips.push_back(copy);
            p.move(copy.id, copy.track, copy.start);
        }
    });
    select(id);
}
void Editor::startVoiceOver() {
    if (m_voiceRecorder || m_busy)
        return;
    const auto device = QMediaDevices::defaultAudioInput();
    if (device.isNull())
        return fail("No microphone found. Connect one and allow Cutlery to use it in Windows' "
                    "privacy settings.");
    QDir().mkpath(m_data + "/recordings");
    const auto file =
        m_data + "/recordings/voice-" +
        QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".wav";
    m_voiceSession = new QMediaCaptureSession(this);
    m_voiceInput = new QAudioInput(device, this);
    m_voiceRecorder = new QMediaRecorder(this);
    m_voiceSession->setAudioInput(m_voiceInput);
    m_voiceSession->setRecorder(m_voiceRecorder);
    QMediaFormat format(QMediaFormat::Wave);
    format.setAudioCodec(QMediaFormat::AudioCodec::Wave);
    m_voiceRecorder->setMediaFormat(format);
    m_voiceRecorder->setQuality(QMediaRecorder::HighQuality);
    m_voiceRecorder->setOutputLocation(QUrl::fromLocalFile(file));
    m_voiceStart = m_playhead;
    auto cleanup = [this] {
        for (QObject *o : std::initializer_list<QObject *>{m_voiceRecorder, m_voiceInput,
                                                           m_voiceSession})
            if (o)
                o->deleteLater();
        m_voiceRecorder = nullptr;
        m_voiceInput = nullptr;
        m_voiceSession = nullptr;
        m_voiceClock.invalidate();
    };
    connect(m_voiceRecorder, &QMediaRecorder::errorOccurred, this,
            [this, cleanup](QMediaRecorder::Error, const QString &message) {
                pause();
                cleanup();
                fail("Recording failed: " + message);
            });
    connect(m_voiceRecorder, &QMediaRecorder::durationChanged, this, &Editor::changed);
    connect(m_voiceRecorder, &QMediaRecorder::recorderStateChanged, this,
            [this, cleanup](QMediaRecorder::RecorderState state) {
                if (state != QMediaRecorder::StoppedState || !m_voiceRecorder)
                    return;
                const auto location = m_voiceRecorder->actualLocation();
                const auto length = qRound64(m_voiceClock.elapsed() / 1000. * m_project.fpsN /
                                             m_project.fpsD);
                const auto frame = m_voiceStart;
                cleanup();
                if (!location.isLocalFile() || !QFileInfo(location.toLocalFile()).isFile())
                    return fail("The recording was not saved");
                // On the lowest free track that is not magnetic (a magnetic main track would
                // push its clips aside), or on a new track.
                int track = -1;
                for (int t = 0; t < m_project.tracks && track < 0; ++t) {
                    const auto &settings = m_project.trackSettings[t];
                    const bool free = std::none_of(
                        m_project.clips.begin(), m_project.clips.end(), [&](const Clip &o) {
                            return o.track == t && o.start < frame + std::max<qint64>(1, length) &&
                                   frame < o.start + o.duration;
                        });
                    if (!settings.magnetic && !settings.locked && free)
                        track = t;
                }
                if (track < 0) {
                    mutate([](Project &p) { p.addTrack(); });
                    track = m_project.tracks - 1;
                }
                dropFiles({location}, track, frame);
                m_status = "Voice-over added";
                emit changed();
            });
    m_voiceRecorder->record();
    m_voiceClock.start();
    // The timeline plays along, so the narration matches the picture (use headphones).
    if (!m_project.clips.empty())
        play();
    m_status = "Recording voice-over…";
    emit changed();
}
void Editor::stopVoiceOver() {
    if (!m_voiceRecorder)
        return;
    pause();
    m_voiceRecorder->stop();
}
QVector<Sound> Editor::soundList() const {
    const auto pack = qEnvironmentVariableIsSet("CUTLERY_SOUNDS_DIR")
                          ? qEnvironmentVariable("CUTLERY_SOUNDS_DIR")
                          : QCoreApplication::applicationDirPath() + "/sounds";
    return soundLibrary(m_data + "/sounds", pack);
}
QVariantList Editor::sounds() const {
    QVariantList list;
    for (const auto &s : soundList())
        list << QVariantMap{{"id", s.id},           {"name", s.name},       {"category", s.category},
                            {"seconds", s.seconds}, {"licence", s.licence}, {"source", s.source},
                            {"builtIn", s.builtIn}, {"peak", s.peak}};
    return list;
}
QUrl Editor::soundFile(const QString &id) {
    try {
        for (const auto &s : soundList())
            if (s.id == id) {
                ensureSoundFile(s);
                return QUrl::fromLocalFile(s.path);
            }
        throw std::runtime_error("No such sound");
    } catch (const std::exception &e) {
        fail(e.what());
        return {};
    }
}
bool Editor::placeSound(Project &p, const Sound &s, qint64 frame) {
    const auto path = QDir::cleanPath(s.path);
    const Asset *asset = nullptr;
    for (const auto &a : p.assets)
        if (QDir::cleanPath(a.path) == path)
            asset = &a;
    if (!asset) {
        Asset a;
        a.id = newId();
        a.path = path;
        a.name = s.name;
        a.kind = "audio";
        a.duration = s.seconds;
        a.hasAudio = true;
        a.rights = "free"; // Cutlery's own sounds
        p.assets.push_back(a);
        asset = &p.assets.back();
    }
    frame = std::max<qint64>(0, frame);
    for (const auto &c : p.clips)
        if (c.assetId == asset->id && c.start == frame)
            return false;
    Clip c;
    c.id = newId();
    c.assetId = asset->id;
    c.name = s.name;
    c.start = frame;
    c.duration = std::max<qint64>(1, qint64(std::floor(s.seconds * p.fpsN / p.fpsD + 1e-6)));
    c.track = freeTrack(p, 0, c.start, c.duration);
    p.requireEditable(c.track);
    p.clips.push_back(c);
    return true;
}
void Editor::addSound(const QString &id) {
    try {
        const auto list = soundList();
        const auto it = std::find_if(list.begin(), list.end(), [&](const Sound &s) { return s.id == id; });
        if (it == list.end())
            throw std::runtime_error("No such sound");
        ensureSoundFile(*it);
        QString added;
        if (mutate([&](Project &p) {
                if (!placeSound(p, *it, m_playhead))
                    throw std::runtime_error("This sound starts at the playhead already");
                added = p.clips.back().id;
            })) {
            select(added);
            m_status = "Added " + it->name;
            emit changed();
        }
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::addSoundAtTransitions(const QString &id) {
    try {
        const auto list = soundList();
        const auto it = std::find_if(list.begin(), list.end(), [&](const Sound &s) { return s.id == id; });
        if (it == list.end())
            throw std::runtime_error("No such sound");
        ensureSoundFile(*it);
        const double fps = double(m_project.fpsN) / m_project.fpsD;
        // Loudest at the cut.
        const auto lead = qRound64(it->peak * fps);
        int added = 0;
        mutate([&](Project &p) {
            QVector<qint64> cuts;
            for (const auto &c : p.clips)
                if (p.transitionLength(c) > 0 && !cuts.contains(c.start))
                    cuts << c.start;
            if (cuts.isEmpty())
                throw std::runtime_error("No transitions: add a transition between two clips first");
            std::sort(cuts.begin(), cuts.end());
            for (const auto cut : cuts)
                added += placeSound(p, *it, cut - lead);
            if (!added)
                throw std::runtime_error("Every transition has this sound already");
        });
        if (!added)
            return;
        m_status = QString("Added %1 at %2 transition%3").arg(it->name).arg(added).arg(added == 1 ? "" : "s");
        emit changed();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
int Editor::freeTrack(Project &p, int home, qint64 start, qint64 length) {
    auto fits = [&](int track) {
        if (track < 0 || track >= p.tracks || p.trackSettings[track].locked)
            return false;
        if (p.trackSettings[track].magnetic)
            return true;
        return std::none_of(p.clips.begin(), p.clips.end(), [&](const Clip &o) {
            return o.track == track && o.start < start + length && start < o.start + o.duration;
        });
    };
    for (int d = 0; d < p.tracks; ++d)
        for (int t : {home + d, home - d})
            if (fits(t))
                return t;
    p.addTrack();
    return p.tracks - 1;
}
void Editor::toggleMarker() {
    mutate([&](Project &p) {
        auto it = std::find_if(p.markers.begin(), p.markers.end(),
                               [&](const Marker &m) { return m.frame == m_playhead; });
        if (it != p.markers.end()) {
            p.markers.erase(it);
            return;
        }
        if (p.markers.size() >= 1000)
            throw std::runtime_error("At most 1000 markers");
        const auto at = std::lower_bound(
            p.markers.begin(), p.markers.end(), m_playhead,
            [](const Marker &m, qint64 frame) { return m.frame < frame; });
        p.markers.insert(at, Marker{m_playhead, QString("Marker %1").arg(p.markers.size() + 1)});
    });
}
void Editor::setMarker(int index, const QString &key, const QVariant &value) {
    mutate([&](Project &p) {
        if (index < 0 || index >= p.markers.size())
            throw std::runtime_error("No such marker");
        if (key == "name")
            p.markers[index].name = value.toString().left(200);
        else if (key == "color")
            p.markers[index].color = value.toString();
        else
            throw std::runtime_error("Unknown marker setting");
    });
}
void Editor::removeMarker(int index) {
    mutate([&](Project &p) {
        if (index >= 0 && index < p.markers.size())
            p.markers.remove(index);
    });
}
qint64 Editor::adjacentMarker(bool forward) const {
    qint64 best = -1;
    for (const auto &m : m_project.markers)
        if (forward ? m.frame > m_playhead && (best < 0 || m.frame < best)
                    : m.frame < m_playhead && m.frame > best)
            best = m.frame;
    return best;
}
void Editor::setInPoint() {
    mutate([&](Project &p) {
        p.inPoint = m_playhead;
        if (p.outPoint >= 0 && p.outPoint <= p.inPoint)
            p.outPoint = -1;
    });
}
void Editor::setOutPoint() {
    mutate([&](Project &p) {
        p.outPoint = m_playhead + 1;
        if (p.inPoint >= p.outPoint)
            p.inPoint = -1;
    });
}
void Editor::clearInOut() {
    mutate([](Project &p) { p.inPoint = p.outPoint = -1; });
}
void Editor::collectProject(const QUrl &folderUrl) {
    if (m_collectThread)
        return;
    try {
        const auto folder = QDir::cleanPath(localPath(folderUrl));
        const QDir dir(folder);
        if (dir.exists() && !dir.isEmpty())
            throw std::runtime_error("Choose an empty or new folder");
        struct Copy {
            QString from, to;
        };
        auto copies = std::make_shared<QVector<Copy>>();
        QSet<QString> taken;
        QHash<QString, QString> mapped; // source → target, so shared files are copied once
        auto target = [&](const QString &from, const QString &sub) {
            const auto key = QFileInfo(from).absoluteFilePath();
            if (mapped.contains(key))
                return mapped[key];
            const QFileInfo info(from);
            QString name = info.fileName();
            for (int i = 2; taken.contains(sub + "/" + name.toLower()); ++i)
                name = info.completeBaseName() + QString("-%1.").arg(i) + info.suffix();
            taken.insert(sub + "/" + name.toLower());
            const auto to = folder + "/" + sub + "/" + name;
            copies->push_back({key, to});
            mapped[key] = to;
            return to;
        };
        auto project = wholeProject();
        QSet<QString> families;
        // Media, LUTs and fonts of the project and of the nested sequences inside it.
        std::function<void(Project &)> collect = [&](Project &p) {
            for (auto &a : p.assets) {
                if (a.isNested()) {
                    auto child = Project::fromJson(a.nested, {});
                    collect(child);
                    a.nested = child.json();
                    continue; // its picture is rendered again from the collected media
                }
                if (!QFileInfo(a.path).isFile())
                    throw std::runtime_error(("Missing media: " + a.name +
                                              ". Relink it before collecting the project.")
                                                 .toStdString());
                a.path = target(a.path, "media");
            }
            for (auto &c : p.clips) {
                if (!c.lut.isEmpty() && QFileInfo(c.lut).isFile())
                    c.lut = target(c.lut, "luts");
                if (c.assetId.isEmpty())
                    families.insert(c.fontFamily);
            }
        };
        collect(project);
        for (const auto &family : families)
            if (m_fontFiles.contains(family))
                target(m_fontFiles[family], "fonts");
        project.name = dir.dirName();
        const auto file = folder + "/" + project.name + ".cutlery";
        qint64 total = 0;
        for (const auto &c : *copies)
            total += QFileInfo(c.from).size();
        m_collect = {{"status", "copying"}, {"progress", 0.}, {"path", file}};
        m_collectThread = QThread::create([this, copies, project, file, total] {
            qint64 done = 0;
            QString error;
            try {
                for (const auto &c : *copies) {
                    if (QThread::currentThread()->isInterruptionRequested())
                        throw std::runtime_error("Cancelled");
                    QDir().mkpath(QFileInfo(c.to).absolutePath());
                    if (!QFile::copy(c.from, c.to))
                        throw std::runtime_error(
                            ("Cannot copy " + QFileInfo(c.from).fileName()).toStdString());
                    done += QFileInfo(c.to).size();
                    const double progress = total > 0 ? double(done) / total : 1.;
                    QMetaObject::invokeMethod(this, [this, progress] {
                        m_collect["progress"] = progress;
                        emit changed();
                    });
                }
                QDir().mkpath(QFileInfo(file).absolutePath());
                saveProject(project, file);
            } catch (const std::exception &e) {
                error = QString::fromUtf8(e.what());
            }
            QMetaObject::invokeMethod(this, [this, error, count = copies->size()] {
                if (error.isEmpty()) {
                    m_collect["status"] = "done";
                    m_collect["progress"] = 1.;
                    m_status = QString("Project collected with %1 file%2")
                                   .arg(count)
                                   .arg(count == 1 ? "" : "s");
                } else {
                    m_collect["status"] = "failed";
                    m_collect["error"] = error;
                    fail("Collecting failed: " + error);
                }
                emit changed();
            });
        });
        connect(m_collectThread, &QThread::finished, this, [this] {
            m_collectThread->deleteLater();
            m_collectThread = nullptr;
        });
        m_collectThread->start();
        emit changed();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::conformFrameRate() {
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || a->kind != "video")
        return fail("Select a video clip to convert");
    if (m_job || m_probe || m_busy)
        return fail("Wait for the current job to finish");
    if (!QFileInfo(a->path).isFile())
        return fail("The media file is missing");
    const double rate = standardRate(a->frameRate > 0 ? a->frameRate
                                                      : double(m_project.fpsN) / m_project.fpsD);
    QDir().mkpath(m_data + "/conformed");
    const auto output = m_data + "/conformed/" + QFileInfo(a->path).completeBaseName() + "-" +
                        MediaAnalysis::fingerprint(*a).left(8) + "-cfr.mov";
    const auto temp = output + ".part.mov";
    const auto assetId = a->id;
    auto *process = new QProcess(this);
    m_job = process;
    m_busy = true;
    m_cancelled = false;
    m_progress = 0;
    m_conform = {{"status", "converting"}, {"progress", 0.}, {"assetId", assetId}};
    m_status = "Converting to a constant frame rate…";
    auto pending = std::make_shared<QByteArray>();
    auto log = std::make_shared<QByteArray>();
    connect(process, &QProcess::readyReadStandardError, this, [process, log] {
        *log += process->readAllStandardError();
        if (log->size() > 16000)
            *log = log->right(8000);
    });
    connect(process, &QProcess::readyReadStandardOutput, this,
            [this, process, pending, duration = a->duration] {
                *pending += process->readAllStandardOutput();
                int i;
                while ((i = pending->indexOf('\n')) >= 0) {
                    const auto line = pending->left(i);
                    pending->remove(0, i + 1);
                    if (line.startsWith("out_time_us=") && duration > 0) {
                        m_progress = std::clamp(line.mid(12).toDouble() / 1e6 / duration, 0., 1.);
                        m_conform["progress"] = m_progress;
                    }
                }
                emit changed();
            });
    auto complete = [this, process, temp, output, assetId, log](bool success) {
        m_job = nullptr;
        m_busy = false;
        process->deleteLater();
        if (m_cancelled || !success || !QFile::rename(temp, output)) {
            QFile::remove(temp);
            m_conform = {{"status", m_cancelled ? "cancelled" : "failed"}, {"assetId", assetId}};
            if (!m_cancelled)
                fail("Conversion failed. " + QString::fromUtf8(*log).right(1000));
            emit changed();
            return;
        }
        m_conform = {{"status", "done"}, {"progress", 1.}, {"assetId", assetId}};
        // Relinking probes the new file; the clips keep their trims.
        probeFile(QUrl::fromLocalFile(output), assetId);
        emit changed();
    };
    connect(process, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            complete(false);
    });
    QFile::remove(output);
    QStringList args{"-hide_banner", "-nostdin", "-y",      "-loglevel", "error",
                     "-i",           a->path,    "-map",    "0:v:0",     "-map",
                     "0:a:0?",       "-fps_mode", "cfr",    "-r",        QString::number(rate, 'f', 6),
                     "-c:v",         "prores_ks", "-profile:v", "2",     "-pix_fmt",
                     "yuv422p10le",  "-c:a",     "pcm_s16le", "-ar",     "48000",
                     "-progress",    "pipe:1",   temp};
    process->start(executable("ffmpeg"), args);
    emit changed();
}
QVariantMap Editor::imageSequence(const QString &file) {
    const QFileInfo info(file);
    static const QRegularExpression numbered("^(.*?)(\\d+)\\.(png|jpe?g|tiff?|bmp|webp|exr|dpx)$",
                                             QRegularExpression::CaseInsensitiveOption);
    const auto m = numbered.match(info.fileName());
    if (!m.hasMatch())
        return {};
    const auto prefix = m.captured(1), digits = m.captured(2), ext = m.captured(3);
    // Siblings with the same prefix, digit count and extension, numbered without gaps from the
    // chosen file's number down and up.
    QSet<qint64> numbers;
    const QRegularExpression sibling("^" + QRegularExpression::escape(prefix) + "(\\d{" +
                                         QString::number(digits.size()) + "})\\." +
                                         QRegularExpression::escape(ext) + "$",
                                     QRegularExpression::CaseInsensitiveOption);
    for (const auto &name : info.dir().entryList(QDir::Files)) {
        const auto s = sibling.match(name);
        if (s.hasMatch())
            numbers.insert(s.captured(1).toLongLong());
    }
    qint64 start = digits.toLongLong(), end = start;
    while (numbers.contains(start - 1))
        --start;
    while (numbers.contains(end + 1))
        ++end;
    if (end - start + 1 < 2)
        return {};
    // FFmpeg's image2 pattern: a literal % in the folder or name is written %%.
    auto literal = info.dir().filePath(prefix);
    literal.replace("%", "%%");
    return {{"pattern", literal + "%0" + QString::number(digits.size()) + "d." + ext},
            {"start", start},
            {"count", end - start + 1},
            {"name", prefix.isEmpty() ? info.dir().dirName() : prefix}};
}
void Editor::importImageSequence(const QUrl &firstImage, double fps) {
    if (m_job || m_busy)
        return fail("Wait for the current job to finish");
    QString path;
    try {
        path = localPath(firstImage);
    } catch (const std::exception &e) {
        return fail(e.what());
    }
    const auto sequence = imageSequence(path);
    if (sequence.isEmpty())
        return fail("Choose an image whose name ends in a frame number, e.g. shot_0001.png, with "
                    "the following frames beside it");
    if (!std::isfinite(fps) || fps < 1 || fps > 120)
        return fail("Choose a frame rate of 1–120 fps");
    QDir().mkpath(m_data + "/sequences");
    auto name = sequence["name"].toString();
    name.remove(QRegularExpression("[^A-Za-z0-9_-]+$"));
    const auto output = m_data + "/sequences/" + (name.isEmpty() ? "sequence" : name) + "-" +
                        newId().left(8) + ".mov";
    const auto temp = output + ".part.mov";
    const auto count = sequence["count"].toLongLong();
    auto *process = new QProcess(this);
    m_job = process;
    m_busy = true;
    m_cancelled = false;
    m_progress = 0;
    m_status = QString("Importing %1 images…").arg(count);
    auto pending = std::make_shared<QByteArray>();
    auto log = std::make_shared<QByteArray>();
    connect(process, &QProcess::readyReadStandardError, this, [process, log] {
        *log += process->readAllStandardError();
        if (log->size() > 16000)
            *log = log->right(8000);
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, pending, count] {
        *pending += process->readAllStandardOutput();
        int i;
        while ((i = pending->indexOf('\n')) >= 0) {
            const auto line = pending->left(i);
            pending->remove(0, i + 1);
            if (line.startsWith("frame="))
                m_progress = std::clamp(line.mid(6).toDouble() / count, 0., 1.);
        }
        emit changed();
    });
    auto complete = [this, process, temp, output, log](bool success) {
        m_job = nullptr;
        m_busy = false;
        process->deleteLater();
        if (m_cancelled || !success || !QFile::rename(temp, output)) {
            QFile::remove(temp);
            if (!m_cancelled)
                fail("Image sequence import failed. " + QString::fromUtf8(*log).right(1000));
            emit changed();
            return;
        }
        importMedia({QUrl::fromLocalFile(output)});
    };
    connect(process, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            complete(false);
    });
    // ProRes 4444 keeps transparency (e.g. rendered animations) and edits smoothly; even sizes
    // are required by the codec's chroma layout.
    process->start(executable("ffmpeg"),
                   {"-hide_banner", "-nostdin", "-y", "-loglevel", "error", "-framerate",
                    QString::number(fps, 'f', 6), "-start_number",
                    QString::number(sequence["start"].toLongLong()), "-f", "image2", "-i",
                    sequence["pattern"].toString(), "-frames:v", QString::number(count), "-vf",
                    "scale=trunc(iw/2)*2:trunc(ih/2)*2", "-c:v", "prores_ks", "-profile:v", "4",
                    "-pix_fmt", "yuva444p10le", "-progress", "pipe:1", temp});
    emit changed();
}
const Clip *Editor::videoBelow(const Clip &c) const {
    const Clip *best = nullptr;
    for (const auto &v : m_project.clips) {
        const auto *a = m_project.asset(v.assetId);
        if (!a || a->kind != "video" || v.audioOnly || v.track >= c.track || v.reverse ||
            v.start + v.duration <= c.start || v.start >= c.start + c.duration)
            continue;
        if (!best || v.track > best->track)
            best = &v;
    }
    return best;
}
void Editor::followFace() {
    const auto *c = m_project.clip(m_selected);
    if (!c)
        return fail("Select a blur or mosaic area to follow a face");
    const auto *v = videoBelow(*c);
    if (!v)
        return fail("Place the area over a video clip on a lower track");
    const auto *a = m_project.asset(v->assetId);
    if (!m_ai->available("faces"))
        return fail(m_ai->missing("faces") + " Download the AI pack next to Cutlery.exe.");
    const double fps = double(m_project.fpsN) / m_project.fpsD, s = v->speed.seconds();
    // The source range under the clip, a second either side.
    const qint64 f0 = std::max(c->start, v->start),
                 f1 = std::min(c->start + c->duration, v->start + v->duration);
    const double from = v->sourceIn.seconds() + (f0 - v->start) / fps * s - 1,
                 to = v->sourceIn.seconds() + (f1 - v->start) / fps * s + 1;
    m_follow = {{"status", "analysing"}, {"clipId", c->id}, {"videoId", v->id}};
    const auto r = m_ai->result("faces", *a);
    if (!r.path.isEmpty() && r.start <= std::max(0., from) + 0.01 &&
        r.end >= std::min(a->duration, to) - 0.01) {
        applyFollowFace();
        return;
    }
    m_ai->start("faces", *a, std::min(from, r.path.isEmpty() ? from : r.start),
                std::max(to, r.path.isEmpty() ? to : r.end));
    m_status = "Finding faces…";
    emit changed();
}
void Editor::applyFollowFace() {
    const auto areaId = m_follow.value("clipId").toString(),
               videoId = m_follow.value("videoId").toString();
    const auto *c = m_project.clip(areaId);
    const auto *v = m_project.clip(videoId);
    const auto *a = v ? m_project.asset(v->assetId) : nullptr;
    const auto r = a ? m_ai->result("faces", *a) : MatteSource{};
    QFile file(r.path);
    if (!c || !v || r.path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_follow = {{"status", "failed"}, {"clipId", areaId}};
        if (a && m_ai->status("faces", *a).value("status") == "failed")
            fail("Finding faces failed: " + m_ai->status("faces", *a).value("error").toString());
        emit changed();
        return;
    }
    // Detections per analysed frame (8 per second from the result's start).
    struct Box {
        double x, y, w, h;
    };
    QHash<qint64, QVector<Box>> frames;
    for (const auto &line : QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts)) {
        const auto f = line.split(' ');
        const auto index = qRound64(f[0].toDouble() * r.rate);
        if (f.size() == 6)
            frames[index] << Box{f[1].toDouble(), f[2].toDouble(), f[3].toDouble(), f[4].toDouble()};
    }
    const double fps = double(m_project.fpsN) / m_project.fpsD, s = v->speed.seconds();
    const qint64 f0 = std::max(c->start, v->start),
                 f1 = std::min(c->start + c->duration, v->start + v->duration);
    // Canvas position (fractions) of a point of the video picture at a timeline frame.
    auto toCanvas = [&](double u, double w, qint64 frame, bool vertical) {
        const double local = frame - v->start, scale = v->valueAt("scale", local);
        const auto size = m_project.pictureSize(*v, m_project.width * scale,
                                                m_project.height * scale);
        if (!vertical && v->flip)
            u = 1 - u;
        const double extent = vertical ? size.height() / m_project.height
                                       : size.width() / m_project.width;
        const double centre = 0.5 + v->valueAt(vertical ? "y" : "x", local);
        return std::pair{centre + (u - 0.5) * extent, w * extent};
    };
    // Follow the face nearest the area's position at the start, then the one nearest the last
    // position; a face more than a quarter of the picture away is a different one.
    const qint64 step = std::max<qint64>(1, qRound64(fps / r.rate));
    double px = 0.5 + c->valueAt("x", f0 - c->start), py = 0.5 + c->valueAt("y", f0 - c->start);
    double maxW = 0, maxH = 0;
    QVector<Keyframe> xs, ys;
    bool found = false;
    for (qint64 frame = f0; frame < f1; frame += step) {
        const double source = v->sourceIn.seconds() + (frame - v->start) / fps * s;
        const auto list = frames.value(qRound64((source - r.start) * r.rate));
        double bestDistance = found ? 0.25 : 2;
        std::optional<std::array<double, 4>> best;
        for (const auto &b : list) {
            const auto [cx, cw] = toCanvas(b.x, b.w, frame, false);
            const auto [cy, ch] = toCanvas(b.y, b.h, frame, true);
            const double d = std::hypot(cx - px, cy - py);
            if (d < bestDistance) {
                bestDistance = d;
                best = std::array<double, 4>{cx, cy, cw, ch};
            }
        }
        if (!best)
            continue;
        found = true;
        px = (*best)[0];
        py = (*best)[1];
        maxW = std::max(maxW, (*best)[2]);
        maxH = std::max(maxH, (*best)[3]);
        xs << Keyframe{frame - c->start, std::clamp(px - 0.5, -2., 2.), false};
        ys << Keyframe{frame - c->start, std::clamp(py - 0.5, -2., 2.), false};
    }
    if (xs.isEmpty()) {
        m_follow = {{"status", "failed"}, {"clipId", areaId}};
        fail("No face found under this clip");
        return;
    }
    // A short moving average steadies the detector's jitter.
    auto steady = [](QVector<Keyframe> k) {
        auto out = k;
        for (int i = 1; i + 1 < k.size(); ++i)
            out[i].value = (k[i - 1].value + k[i].value + k[i + 1].value) / 3;
        return out;
    };
    const auto count = xs.size();
    mutate([&](Project &p) {
        auto *area = p.clip(areaId);
        if (!area)
            return;
        p.requireEditable(area->track);
        area->keyframes["x"] = steady(xs);
        area->keyframes["y"] = steady(ys);
        if (!area->effect.isEmpty()) {
            // Cover the face with some room; the area's size is fixed for the clip.
            area->keyframes.remove("scale");
            area->scale = 1;
            area->effectWidth = std::clamp(maxW * 1.5, 0.02, 1.);
            area->effectHeight = std::clamp(maxH * 1.4, 0.02, 1.);
        }
    });
    m_follow = {{"status", "done"}, {"clipId", areaId}, {"keyframes", count}};
    m_status = QString("Following a face with %1 keyframes").arg(count);
    emit changed();
}
// The source seconds a clip shows, for face analysis.
static std::pair<double, double> sourceRange(const Clip &c, double fps) {
    const double s = c.speed.seconds();
    return {c.sourceIn.seconds(), c.sourceIn.seconds() + c.duration / fps * s};
}
void Editor::reframe(int width, int height) {
    if (width < 16 || height < 16 || width % 2 || height % 2)
        return fail("Choose an even width and height");
    if (m_project.clips.empty())
        return fail("The timeline is empty");
    QStringList reframed, faces;
    if (!mutate([&](Project &p) {
        const int oldWidth = p.width, oldHeight = p.height;
        p.width = width;
        p.height = height;
        for (auto &c : p.clips) {
            const auto *a = p.asset(c.assetId);
            // Only pictures that filled the old canvas; titles, graphics and pictures in
            // a corner keep their place.
            const bool full = a && (a->kind == "video" || a->kind == "image") &&
                              !c.audioOnly && c.effect.isEmpty() && c.graphic.isEmpty() &&
                              c.shape != "circle" && std::abs(c.scale - 1) < 1e-6 &&
                              std::abs(c.x) < 1e-6 && std::abs(c.y) < 1e-6 &&
                              std::abs(c.rotation) < 1e-6 && !c.keyframes.contains("x") &&
                              !c.keyframes.contains("y") && !c.keyframes.contains("scale");
            if (!full || p.trackSettings.value(c.track).locked)
                continue;
            // Zoom so the picture covers the canvas, centred.
            const auto fit = p.pictureSize(c, width, height);
            c.scale = std::min(5., std::max(width / fit.width(), height / fit.height()));
            reframed << c.id;
            if (a->kind == "video" && !c.reverse)
                faces << c.id;
        }
        if (reframed.isEmpty() && oldWidth == width && oldHeight == height)
            throw std::runtime_error("Nothing to reframe");
    }))
        return;
    m_reframe = {{"status", "done"}, {"clips", reframed}, {"faces", 0}};
    m_status = QString("Reframed to %1 × %2: %3 clips centred").arg(width).arg(height).arg(
        reframed.size());
    if (!faces.isEmpty() && !m_ai->available("faces")) {
        m_status += ". The AI pack keeps faces in the picture.";
    } else if (!faces.isEmpty()) {
        // Analyse the source range each clip shows; clips of one file share the result.
        const double fps = double(m_project.fpsN) / m_project.fpsD;
        QHash<QString, std::pair<double, double>> ranges;
        for (const auto &id : faces) {
            const auto *c = m_project.clip(id);
            const auto [from, to] = sourceRange(*c, fps);
            auto &r = ranges[c->assetId];
            r = ranges.contains(c->assetId) && r.second > r.first
                    ? std::pair{std::min(r.first, from), std::max(r.second, to)}
                    : std::pair{from, to};
        }
        for (auto it = ranges.begin(); it != ranges.end(); ++it) {
            const auto *a = m_project.asset(it.key());
            const auto r = m_ai->result("faces", *a);
            const double from = std::max(0., it->first - 0.5),
                         to = std::min(a->duration, it->second + 0.5);
            if (!r.path.isEmpty() && r.start <= from + 0.01 && r.end >= to - 0.01)
                continue;
            m_ai->start("faces", *a, r.path.isEmpty() ? from : std::min(from, r.start),
                        r.path.isEmpty() ? to : std::max(to, r.end));
        }
        m_reframe = {{"status", "analysing"}, {"clips", faces}, {"faces", 0}};
        m_status = "Finding faces to reframe…";
        if (!m_ai->busy())
            applyReframe();
    }
    emit changed();
}
void Editor::applyReframe() {
    // Wait until every file's analysis has ended.
    const auto ids = m_reframe.value("clips").toStringList();
    for (const auto &id : ids)
        if (const auto *c = m_project.clip(id))
            if (const auto *a = m_project.asset(c->assetId)) {
                const auto st = m_ai->status("faces", *a).value("status").toString();
                if (st == "queued" || st == "running")
                    return;
            }
    const double fps = double(m_project.fpsN) / m_project.fpsD;
    QHash<QString, QVector<Keyframe>> xs, ys;
    for (const auto &id : ids) {
        const auto *c = m_project.clip(id);
        const auto *a = c ? m_project.asset(c->assetId) : nullptr;
        if (!a)
            continue;
        const auto r = m_ai->result("faces", *a);
        QFile file(r.path);
        if (r.path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        struct Box {
            double x, y, w, h;
        };
        QHash<qint64, QVector<Box>> frames;
        for (const auto &line :
             QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts)) {
            const auto f = line.split(' ');
            if (f.size() == 6)
                frames[qRound64(f[0].toDouble() * r.rate)]
                    << Box{f[1].toDouble(), f[2].toDouble(), f[3].toDouble(), f[4].toDouble()};
        }
        // The main face's centre (source fractions) at each analysed moment: the largest face,
        // or the one closest to the last when it is still there, so the frame does not jump
        // between people.
        const double s = c->speed.seconds();
        const qint64 step = std::max<qint64>(1, qRound64(fps / r.rate));
        QVector<std::pair<qint64, QPointF>> path;
        std::optional<QPointF> last;
        for (qint64 local = 0; local < c->duration; local += step) {
            const double source = c->sourceIn.seconds() + local / fps * s;
            const auto list = frames.value(qRound64((source - r.start) * r.rate));
            const Box *best = nullptr;
            for (const auto &b : list) {
                const QPointF centre(b.x + b.w / 2, b.y + b.h / 2);
                if (last && QLineF(centre, *last).length() < 0.15) {
                    best = &b;
                    break;
                }
                if (!best || b.w * b.h > best->w * best->h)
                    best = &b;
            }
            if (best) {
                last = QPointF(best->x + best->w / 2, best->y + best->h / 2);
                path << std::pair{local, *last};
            }
        }
        if (path.isEmpty())
            continue;
        // A calm camera: a moving average over up to a second either side (shrunk near the
        // ends so it stays centred), sampled every half second.
        const int reach = std::max(1, int(std::round(r.rate)));
        const auto size = m_project.pictureSize(*c, m_project.width * c->scale,
                                                m_project.height * c->scale);
        const double extentX = size.width() / m_project.width,
                     extentY = size.height() / m_project.height;
        const int every = std::max(1, int(std::round(r.rate / 2)));
        auto &kx = xs[id];
        auto &ky = ys[id];
        for (int i = 0; i < path.size(); i += every) {
            const int k = std::min({reach, i, int(path.size()) - 1 - i});
            QPointF sum;
            for (int j = i - k; j <= i + k; ++j)
                sum += path[j].second;
            auto centre = sum / (2 * k + 1);
            if (c->flip)
                centre.setX(1 - centre.x());
            // Put the face in the middle, as far as the picture still covers the canvas.
            const double limitX = std::max(0., (extentX - 1) / 2),
                         limitY = std::max(0., (extentY - 1) / 2);
            kx << Keyframe{path[i].first, std::clamp(-(centre.x() - 0.5) * extentX, -limitX, limitX),
                           false};
            // Faces sit in the upper part of a frame; vertical moves only when there is room.
            ky << Keyframe{path[i].first,
                           std::clamp(-(centre.y() - 0.4) * extentY, -limitY, limitY), false};
        }
    }
    int count = 0;
    if (!xs.isEmpty())
        mutate([&](Project &p) {
            for (auto it = xs.begin(); it != xs.end(); ++it) {
                auto *c = p.clip(it.key());
                if (!c || p.trackSettings.value(c->track).locked)
                    continue;
                // A single keyframe is just a position.
                if (it->size() == 1) {
                    c->x = it->first().value;
                    c->y = ys[it.key()].first().value;
                } else {
                    c->keyframes["x"] = *it;
                    if (std::any_of(ys[it.key()].begin(), ys[it.key()].end(),
                                    [](const Keyframe &k) { return std::abs(k.value) > 1e-6; }))
                        c->keyframes["y"] = ys[it.key()];
                }
                ++count;
            }
        });
    m_reframe = {{"status", "done"}, {"clips", ids}, {"faces", count}};
    m_status = count ? QString("Reframed: %1 of %2 clips follow a face").arg(count).arg(ids.size())
                     : QString("Reframed; no faces found, the pictures stay centred");
    emit changed();
}
void Editor::addFolder(const QString &name) {
    const auto folder = name.trimmed();
    if (folder.isEmpty())
        return fail("Name the folder");
    if (m_project.folders.contains(folder))
        return fail("There is a folder of that name already");
    if (mutate([&](Project &p) { p.folders << folder; }))
        m_importFolder = folder;
    emit changed();
}
void Editor::renameFolder(const QString &from, const QString &to) {
    const auto name = to.trimmed();
    if (name == from)
        return;
    if (name.isEmpty())
        return fail("Name the folder");
    if (m_project.folders.contains(name))
        return fail("There is a folder of that name already");
    if (mutate([&](Project &p) {
            const auto i = p.folders.indexOf(from);
            if (i < 0)
                throw std::runtime_error("No such folder");
            p.folders[i] = name;
            for (auto &a : p.assets)
                if (a.folder == from)
                    a.folder = name;
        }) &&
        m_importFolder == from)
        m_importFolder = name;
    emit changed();
}
void Editor::removeFolder(const QString &name) {
    // The media stays in the library, at the top level.
    mutate([&](Project &p) {
        p.folders.removeAll(name);
        for (auto &a : p.assets)
            if (a.folder == name)
                a.folder.clear();
    });
    if (m_importFolder == name)
        m_importFolder.clear();
    emit changed();
}
void Editor::moveToFolder(const QStringList &assetIds, const QString &folder) {
    mutate([&](Project &p) {
        if (!folder.isEmpty() && !p.folders.contains(folder))
            throw std::runtime_error("No such folder");
        for (auto &a : p.assets)
            if (assetIds.contains(a.id))
                a.folder = folder;
    });
}
void Editor::setAssetRights(const QStringList &assetIds, const QString &rights,
                            const QString &credit) {
    if (!Project::rightsKinds().contains(rights))
        return fail("Unknown usage rights");
    mutate([&](Project &p) {
        for (auto &a : p.assets)
            if (assetIds.contains(a.id)) {
                a.rights = rights;
                a.credit = credit.trimmed().left(500);
            }
    });
}
QVariantMap Editor::rightsCheck() const {
    QStringList personal, unknown, unrecorded;
    QVariantList credits;
    for (const auto &a : m_project.assets) {
        if (std::none_of(m_project.clips.begin(), m_project.clips.end(),
                         [&](const Clip &c) { return c.assetId == a.id; }))
            continue;
        if (a.rights == "personal")
            personal << a.name;
        else if (a.rights == "unknown")
            unknown << a.name;
        else if (a.rights.isEmpty())
            unrecorded << a.name;
        if (!a.credit.isEmpty() || a.rights == "attribution")
            credits << QVariantMap{{"name", a.name}, {"credit", a.credit}};
    }
    return {{"personal", personal}, {"unknown", unknown}, {"unrecorded", unrecorded},
            {"credits", credits}};
}
bool Editor::exportCredits(const QUrl &file) {
    const auto credits = rightsCheck()["credits"].toList();
    if (credits.isEmpty()) {
        fail("No media on the timeline has a credit line");
        return false;
    }
    QString text;
    for (const auto &v : credits) {
        const auto m = v.toMap();
        const auto credit = m["credit"].toString();
        text += credit.isEmpty() ? m["name"].toString() + " (credit missing)\n"
                                 : m["name"].toString() + ": " + credit + "\n";
    }
    QSaveFile f(file.isLocalFile() ? file.toLocalFile() : file.toString());
    const auto data = text.toUtf8();
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) {
        fail("Cannot write the credits file");
        return false;
    }
    m_status = QString("Saved %1 credit line%2").arg(credits.size()).arg(credits.size() == 1 ? "" : "s");
    emit changed();
    return true;
}
QVariantMap Editor::searchMedia(const QString &query) const {
    const auto words = query.toLower().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QVariantMap result;
    if (words.isEmpty())
        return result;
    static const QStringList languages{"auto", "de", "en", "fr", "es", "it", "nl", "pl", "pt", "tr"};
    for (const auto &a : m_project.assets) {
        const auto about = QStringList{a.name, QFileInfo(a.path).fileName(), a.folder, a.kind,
                                       a.width > 0 ? QString("%1x%2").arg(a.width).arg(a.height)
                                                   : QString(),
                                       a.rights, a.credit}
                               .join('\n')
                               .toLower();
        // What is said in it, from any cached transcript.
        QVector<Cue> said;
        if ((a.kind == "video" || a.kind == "audio") && !a.isNested() && QFileInfo::exists(a.path))
            for (const auto &language : languages) {
                const auto r = m_ai->result("transcribe", a, language);
                if (r.path.isEmpty())
                    continue;
                const auto modified = QFileInfo(r.path).lastModified().toMSecsSinceEpoch();
                auto &entry = m_spoken[r.path];
                if (entry.first != modified) {
                    try {
                        entry.second = parseSrt(readUtf8File(r.path));
                        for (auto &cue : entry.second)
                            cue.start += r.start;
                    } catch (const std::exception &) {
                        entry.second.clear();
                    }
                    entry.first = modified;
                }
                said += entry.second;
            }
        QString spoken;
        for (const auto &cue : said)
            spoken += cue.text.toLower() + ' ';
        QString snippet;
        bool all = true;
        for (const auto &w : words) {
            if (about.contains(w))
                continue;
            const auto at = spoken.indexOf(w);
            if (at < 0) {
                all = false;
                break;
            }
            if (snippet.isEmpty()) {
                // The cue the match starts in gives the time; a few words around it the text.
                int offset = 0, index = 0;
                for (; index < said.size(); ++index) {
                    offset += int(said[index].text.size()) + 1;
                    if (offset > at)
                        break;
                }
                index = std::min(index, int(said.size()) - 1);
                QStringList around;
                for (int i = std::max(0, index - 3); i < std::min(int(said.size()), index + 5); ++i)
                    around << said[i].text;
                const int s = int(said[index].start);
                snippet = QString("%1:%2 … %3 …")
                              .arg(s / 60)
                              .arg(s % 60, 2, 10, QChar('0'))
                              .arg(around.join(' '));
            }
        }
        if (all)
            result[a.id] = snippet;
    }
    return result;
}
void Editor::removeAssets(const QStringList &assetIds) {
    // Only media no clip uses; the files on disk stay.
    mutate([&](Project &p) {
        for (const auto &c : p.clips)
            if (assetIds.contains(c.assetId))
                throw std::runtime_error("Media used on the timeline cannot be removed");
        p.assets.erase(std::remove_if(p.assets.begin(), p.assets.end(),
                                      [&](const Asset &a) { return assetIds.contains(a.id); }),
                       p.assets.end());
    });
}
void Editor::setImportFolder(const QString &folder) {
    const auto next = m_project.folders.contains(folder) ? folder : QString();
    if (next == m_importFolder)
        return;
    m_importFolder = next;
    emit changed();
}
// Nested sequences ---------------------------------------------------------------------------
QString Editor::nestedPath(const QJsonObject &content) const {
    const auto key = QCryptographicHash::hash(QJsonDocument(content).toJson(QJsonDocument::Compact),
                                              QCryptographicHash::Sha1)
                         .toHex()
                         .left(20);
    return m_data + "/cache/nested/" + QString::fromLatin1(key) + ".mov";
}
// Puts `child` into the nested asset `assetId` of `parent`: its content, length and cache file.
// Clips of it that now run past its end are shortened.
void Editor::storeNested(Project &parent, const QString &assetId, const Project &child) const {
    for (auto &a : parent.assets)
        if (a.id == assetId) {
            a.nested = child.json();
            a.path = nestedPath(a.nested);
            a.duration = std::max(child.seconds(), double(child.fpsD) / child.fpsN);
            a.width = child.width;
            a.height = child.height;
            a.frameRate = double(child.fpsN) / child.fpsD;
            const double fps = double(parent.fpsN) / parent.fpsD;
            for (auto &c : parent.clips)
                if (c.assetId == assetId) {
                    const double room = (a.duration - c.sourceIn.seconds()) / c.speed.seconds();
                    c.duration = std::max<qint64>(1, std::min<qint64>(c.duration, qint64(room * fps)));
                    if (c.sourceIn.seconds() >= a.duration)
                        c.sourceIn = Time(0, 1);
                }
        }
}
// The project for previews and playback: a nested sequence still being rendered shows as a
// note instead of its picture and sound.
Project Editor::viewable() const {
    auto p = m_project;
    for (auto &c : p.clips)
        if (const auto *a = p.asset(c.assetId); a && a->isNested() && !QFileInfo::exists(a->path)) {
            c.assetId.clear();
            c.text = "Rendering the nested sequence…";
            c.fontSize = std::max(24, p.height / 20);
            c.keyframes.clear();
        }
    return p;
}
Project Editor::wholeProject() const {
    auto project = m_project;
    for (int i = int(m_nest.size()) - 1; i >= 0; --i) {
        auto parent = m_nest[i].parent;
        storeNested(parent, m_nest[i].assetId, project);
        project = std::move(parent);
    }
    return project;
}
void Editor::nestSelection() {
    const auto ids = selection();
    if (ids.isEmpty())
        return fail("Select the clips to nest");
    QString nestedClip;
    mutate([&](Project &p) {
        QVector<Clip> inside;
        qint64 from = std::numeric_limits<qint64>::max(), to = 0;
        int low = p.tracks, high = 0;
        for (const auto &id : ids) {
            const auto *c = p.clip(id);
            p.requireEditable(c->track);
            inside << *c;
            from = std::min(from, c->start);
            to = std::max(to, c->start + c->duration);
            low = std::min(low, c->track);
            high = std::max(high, c->track);
        }
        // The nested sequence: same canvas and rate, its own tracks from the lowest one used.
        Project child;
        child.name = QString("Nested %1").arg(
            std::count_if(p.assets.begin(), p.assets.end(), [](const Asset &a) { return a.isNested(); }) + 1);
        child.width = p.width;
        child.height = p.height;
        child.fpsN = p.fpsN;
        child.fpsD = p.fpsD;
        child.tracks = high - low + 1;
        child.trackSettings = p.trackSettings.mid(low, child.tracks);
        for (auto &t : child.trackSettings)
            t.locked = t.solo = false;
        for (auto c : inside) {
            c.start -= from;
            c.track -= low;
            child.clips << c;
            if (const auto *a = p.asset(c.assetId); a && !child.asset(a->id)) {
                auto copy = *a;
                copy.folder.clear();
                child.assets << copy;
            }
        }
        child.validate();
        p.clips.erase(std::remove_if(p.clips.begin(), p.clips.end(),
                                     [&](const Clip &c) { return ids.contains(c.id); }),
                      p.clips.end());
        Asset a;
        a.id = newId();
        a.name = child.name;
        a.kind = "video";
        a.hasAudio = true;
        p.assets << a;
        storeNested(p, a.id, child);
        Clip c;
        c.id = newId();
        c.assetId = a.id;
        c.name = a.name;
        c.track = low;
        c.start = from;
        c.duration = to - from;
        nestedClip = c.id;
        p.clips << c;
    });
    if (!nestedClip.isEmpty()) {
        select(nestedClip);
        m_status = QString("Nested %1 clips; double-click to open the sequence").arg(ids.size());
        emit changed();
    }
}
void Editor::openNested(const QString &clipId) {
    const auto *c = m_project.clip(clipId.isEmpty() ? m_selected : clipId);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || !a->isNested())
        return fail("Select a nested sequence");
    try {
        auto child = Project::fromJson(a->nested, {});
        m_nest << NestFrame{m_project, a->id, m_undo, m_redo, c->id, m_playhead};
        stopPlayback();
        m_project = std::move(child);
        m_undo.clear();
        m_redo.clear();
        m_selected.clear();
        m_also.clear();
        // Where the playhead was inside the clip.
        const double fps = double(m_project.fpsN) / m_project.fpsD,
                     parentFps = double(m_nest.last().parent.fpsN) / m_nest.last().parent.fpsD;
        m_playhead = std::clamp<qint64>(
            qint64((c->sourceIn.seconds() + (m_playhead - c->start) / parentFps * c->speed.seconds()) * fps),
            0, std::max<qint64>(0, m_project.duration() - 1));
        ++m_revision;
        m_status = "Editing the nested sequence " + a->name;
        m_previewTimer.start();
        renderNested();
        m_analysis->setAssets(m_project.assets);
        m_thumbnails->setAssets(m_project.assets);
        emit projectChanged();
        emit changed();
    } catch (const std::exception &e) {
        m_nest.clear();
        fail(e.what());
    }
}
void Editor::closeNested() {
    if (m_nest.isEmpty())
        return;
    if (m_project.clips.empty())
        return fail("A nested sequence needs at least one clip; delete its clip on the timeline above instead");
    stopPlayback();
    const auto frame = m_nest.takeLast();
    const auto child = m_project;
    m_project = frame.parent;
    m_undo = frame.undo;
    m_redo = frame.redo;
    m_playhead = frame.playhead;
    m_selected = frame.clipId;
    m_also.clear();
    ++m_revision;
    const auto *a = m_project.asset(frame.assetId);
    const bool changedInside = a && a->nested != child.json();
    // Any change inside is one undo step out here.
    if (changedInside)
        mutate([&](Project &p) { storeNested(p, frame.assetId, child); });
    m_status = changedInside ? "Back on the timeline; rendering the changed sequence…"
                             : "Back on the timeline";
    m_previewTimer.start();
    renderNested();
    m_analysis->setAssets(m_project.assets);
    m_thumbnails->setAssets(m_project.assets);
    emit projectChanged();
    emit changed();
}
void Editor::unnest() {
    const auto *selected = m_project.clip(m_selected);
    const auto *a = selected ? m_project.asset(selected->assetId) : nullptr;
    if (!a || !a->isNested())
        return fail("Select a nested sequence");
    const auto id = selected->id;
    mutate([&](Project &p) {
        const auto c = *p.clip(id);
        const auto *asset = p.asset(c.assetId);
        const auto child = Project::fromJson(asset->nested, {});
        if (c.sourceIn.seconds() != 0 || c.speed.seconds() != 1 || !c.keyframes.isEmpty() ||
            child.fpsN * p.fpsD != p.fpsN * child.fpsD)
            throw std::runtime_error("Only a nested sequence at normal speed, from its start, "
                                     "without keyframes and at the timeline's frame rate can be "
                                     "taken apart");
        if (c.track + child.tracks > p.tracks)
            throw std::runtime_error(QString("Taking it apart needs %1 tracks from this one; add "
                                             "tracks first")
                                         .arg(child.tracks)
                                         .toStdString());
        for (int t = 0; t < child.tracks; ++t)
            p.requireEditable(c.track + t);
        p.clips.erase(std::remove_if(p.clips.begin(), p.clips.end(),
                                     [&](const Clip &x) { return x.id == id; }),
                      p.clips.end());
        for (const auto &x : child.assets)
            if (!p.asset(x.id))
                p.assets << x;
        // Its clips, within the part of the sequence the clip showed.
        for (auto x : child.clips) {
            if (x.start >= c.duration)
                continue;
            x.start += c.start;
            x.track += c.track;
            x.duration = std::min(x.duration, c.start + c.duration - x.start);
            p.clips << x;
        }
        // The sequence leaves the library when nothing else uses it.
        if (std::none_of(p.clips.begin(), p.clips.end(),
                         [&](const Clip &x) { return x.assetId == c.assetId; }))
            p.assets.erase(std::remove_if(p.assets.begin(), p.assets.end(),
                                          [&](const Asset &x) { return x.id == c.assetId; }),
                           p.assets.end());
    });
}
// Renders nested sequences whose cache file is missing, one at a time, innermost first.
void Editor::renderNested() {
    if (m_nestedProcess)
        return;
    // The cache files belong to this computer: point nested media at them.
    std::function<void(Project &)> repoint = [&](Project &p) {
        for (auto &a : p.assets)
            if (a.isNested())
                a.path = nestedPath(a.nested);
    };
    repoint(m_project);
    std::function<std::optional<QJsonObject>(const Project &)> pending =
        [&](const Project &p) -> std::optional<QJsonObject> {
        for (const auto &a : p.assets)
            if (a.isNested()) {
                try {
                    if (auto inner = pending(Project::fromJson(a.nested, {})))
                        return inner;
                } catch (const std::exception &) {
                    continue;
                }
                if (!QFileInfo::exists(nestedPath(a.nested)) && !m_nestedFailed.contains(nestedPath(a.nested)))
                    return a.nested;
            }
        return std::nullopt;
    };
    const auto next = pending(m_project);
    if (!next)
        return;
    const auto output = nestedPath(*next);
    try {
        auto child = Project::fromJson(*next, {});
        repoint(child);
        if (child.clips.empty())
            throw std::runtime_error("empty");
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/nested-XXXXXX");
        if (!work->isValid() || !QDir().mkpath(QFileInfo(output).absolutePath()))
            throw std::runtime_error("Cannot create a work folder");
        RenderOptions options;
        options.highQuality = true;
        options.pixelFormat = "yuv422p10le";
        const auto plan = compileRender(child, work->path(), child.width, child.height, options);
        const auto graph = work->filePath("graph.txt");
        writeGraph(graph, plan.graph);
        Encoder prores;
        prores.name = "prores_ks";
        prores.videoArguments = {"-c:v", "prores_ks", "-profile:v", "2"};
        prores.audioArguments = {"-c:a", "pcm_s16le", "-ar", "48000"};
        prores.pixelFormat = "yuv422p10le";
        prores.extension = "mov";
        const auto temp = work->filePath("nested.mov");
        auto *process = new QProcess(this);
        m_nestedProcess = process;
        auto log = std::make_shared<QByteArray>();
        connect(process, &QProcess::readyReadStandardError, this, [process, log] {
            *log += process->readAllStandardError();
            if (log->size() > 64000)
                *log = log->right(32000);
        });
        auto complete = [this, process, work, temp, output, log](bool success) {
            process->deleteLater();
            m_nestedProcess = nullptr;
            if (success && QFile::rename(temp, output)) {
                // The picture of every clip using it changes.
                m_analysis->setAssets(m_project.assets);
                m_thumbnails->setAssets(m_project.assets);
                ++m_revision;
                m_previewTimer.start();
                emit projectChanged();
            } else {
                m_nestedFailed << output;
                fail("Rendering a nested sequence failed: " +
                     QString::fromUtf8(*log).trimmed().right(300));
            }
            renderNested();
            emit changed();
        };
        connect(process, &QProcess::finished, this,
                [complete](int code, QProcess::ExitStatus status) {
                    complete(code == 0 && status == QProcess::NormalExit);
                });
        connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                complete(false);
        });
        process->start(executable("ffmpeg"), exportArguments(plan, graph, temp, prores));
        emit changed();
    } catch (const std::exception &e) {
        m_nestedFailed << output;
        renderNested();
    }
}
void Editor::copy() {
    const auto *c = m_project.clip(m_selected);
    if (!c)
        return;
    m_clipboard = *c;
    const auto *a = m_project.asset(c->assetId);
    m_clipboardAsset = a ? std::optional<Asset>(*a) : std::nullopt;
    m_clipboardMore.clear();
    m_clipboardMoreAssets.clear();
    for (const auto &id : selection())
        if (id != m_selected) {
            const auto *x = m_project.clip(id);
            m_clipboardMore << *x;
            if (const auto *xa = m_project.asset(x->assetId))
                m_clipboardMoreAssets << *xa;
        }
    m_status = m_clipboardMore.isEmpty()
                   ? "Copied " + c->name
                   : QString("Copied %1 clips").arg(m_clipboardMore.size() + 1);
    emit changed();
}
void Editor::paste() {
    if (!m_clipboard)
        return;
    QStringList added;
    mutate([&](Project &p) {
        auto clips = QVector<Clip>{*m_clipboard} + m_clipboardMore;
        auto assets = m_clipboardMoreAssets;
        if (m_clipboardAsset)
            assets.prepend(*m_clipboardAsset);
        for (auto a : assets)
            if (!p.asset(a.id)) {
                // Another project's folder may not exist here.
                if (!p.folders.contains(a.folder))
                    a.folder.clear();
                p.assets.push_back(a);
            }
        // The earliest copied clip lands at the playhead; the others keep their distance to it.
        qint64 earliest = clips.first().start;
        for (const auto &c : clips)
            earliest = std::min(earliest, c.start);
        // New links and groups, so the copies stay together but apart from the originals.
        QHash<QString, QString> renamed;
        auto rename = [&](const QString &key) {
            if (key.isEmpty() || key == "none")
                return key;
            if (!renamed.contains(key))
                renamed[key] = newId();
            return renamed[key];
        };
        for (auto copy : clips) {
            copy.id = newId();
            copy.transition.clear();
            copy.transitionFrames = 0;
            copy.link = rename(copy.link);
            copy.group = rename(copy.group);
            // Timing is in frames: a clip from a project with another frame rate keeps its
            // length. A clip goes to its own track when that is free there (magnetic tracks make
            // room), otherwise to the nearest free track above or below, or to a new track.
            copy.start = m_playhead + (copy.start - earliest);
            copy.track = freeTrack(p, std::min(copy.track, p.tracks - 1), copy.start, copy.duration);
            p.clips.push_back(copy);
            p.move(copy.id, copy.track, copy.start);
            added << copy.id;
        }
    });
    if (added.isEmpty())
        return;
    m_selected = added.takeFirst();
    m_also = added;
    emit changed();
}
void Editor::selectArea(qint64 from, qint64 to, int low, int high, bool add) {
    if (!add) {
        m_selected.clear();
        m_also.clear();
    }
    auto current = selection();
    for (const auto &c : m_project.clips)
        if (c.track >= low && c.track <= high && c.start < to && c.start + c.duration > from &&
            !current.contains(c.id)) {
            current << c.id;
            if (!c.group.isEmpty())
                for (const auto &x : m_project.clips)
                    if (x.group == c.group && !current.contains(x.id))
                        current << x.id;
        }
    m_selected = current.isEmpty() ? QString() : current.takeFirst();
    m_also = current;
    emit changed();
}
void Editor::pasteAttributes(const QString &group) {
    if (!m_clipboard || (group != "look" && group != "all"))
        return;
    mutate([&](Project &p) {
        auto *c = p.clip(m_selected);
        if (!c)
            return;
        p.requireEditable(c->track);
        const auto &from = *m_clipboard;
        c->brightness = from.brightness;
        c->contrast = from.contrast;
        c->saturation = from.saturation;
        c->blur = from.blur;
        c->temperature = from.temperature;
        c->tint = from.tint;
        c->vibrance = from.vibrance;
        c->shadows = from.shadows;
        c->whites = from.whites;
        c->blacks = from.blacks;
        c->liftX = from.liftX;
        c->liftY = from.liftY;
        c->gammaX = from.gammaX;
        c->gammaY = from.gammaY;
        c->gainX = from.gainX;
        c->gainY = from.gainY;
        c->highlights = from.highlights;
        c->sharpen = from.sharpen;
        c->glow = from.glow;
        c->vignette = from.vignette;
        c->grain = from.grain;
        c->lut = from.lut;
        c->lutStrength = from.lutStrength;
        c->curveMaster = from.curveMaster;
        c->curveRed = from.curveRed;
        c->curveGreen = from.curveGreen;
        c->curveBlue = from.curveBlue;
        c->hslColors = from.hslColors;
        c->hslHue = from.hslHue;
        c->hslSaturation = from.hslSaturation;
        c->hslLightness = from.hslLightness;
        c->fx = from.fx;
        c->fxStrength = from.fxStrength;
        c->canvasFill = from.canvasFill;
        c->motionBlur = from.motionBlur;
        if (group == "look")
            return;
        c->scale = from.scale;
        c->anchorX = from.anchorX;
        c->anchorY = from.anchorY;
        c->x = from.x;
        c->y = from.y;
        c->rotation = from.rotation;
        c->opacity = from.opacity;
        c->crop = from.crop;
        c->cropLeft = from.cropLeft;
        c->cropRight = from.cropRight;
        c->cropTop = from.cropTop;
        c->cropBottom = from.cropBottom;
        c->flip = from.flip;
        c->flipVertical = from.flipVertical;
        c->blendMode = from.blendMode;
        c->volume = from.volume;
        c->eqLow = from.eqLow;
        c->eqMid = from.eqMid;
        c->eqHigh = from.eqHigh;
        c->lowCut = from.lowCut;
        c->compressor = from.compressor;
        c->gate = from.gate;
        c->denoise = from.denoise;
        c->deess = from.deess;
        c->reverb = from.reverb;
        c->pitch = from.pitch;
        c->voice = from.voice;
        c->echo = from.echo;
        c->pan = from.pan;
        c->fadeIn = from.fadeIn;
        c->fadeOut = from.fadeOut;
        // Keyframes keep their clip-relative frames; those past the end of a shorter clip stay
        // and hold the value from the last one inside.
        c->keyframes = from.keyframes;
        c->shape = from.shape;
        c->radius = from.radius;
        c->feather = from.feather;
        c->cornerPin = from.cornerPin;
        c->tiltX = from.tiltX;
        c->tiltY = from.tiltY;
        c->border = from.border;
        c->borderColor = from.borderColor;
        c->shadow = from.shadow;
        c->chromaKey = from.chromaKey;
        c->keyColor = from.keyColor;
        c->keySimilarity = from.keySimilarity;
        c->keyBlend = from.keyBlend;
        c->lumaKey = from.lumaKey;
        c->lumaTolerance = from.lumaTolerance;
        c->lumaSoftness = from.lumaSoftness;
        c->aiCutout = from.aiCutout;
        c->eyeContact = from.eyeContact;
    });
}
void Editor::configure(int w, int h, int n, int d) {
    mutate([&](Project &p) {
        if (!p.clips.empty() && (p.fpsN != n || p.fpsD != d))
            throw std::runtime_error("Set the frame rate before adding clips");
        p.width = w;
        p.height = h;
        p.fpsN = n;
        p.fpsD = d;
    });
}
void Editor::requestPreview() {
    if (m_preview) {
        m_preview->disconnect(this);
        m_preview->kill();
        m_preview->deleteLater();
        m_preview = nullptr;
    }
    if (m_project.clips.empty() || m_busy || m_playback->active() || m_resumeTimer.isActive()) {
        if (m_project.clips.empty()) {
            m_previewUrl.clear();
            emit changed();
        }
        return;
    }
    try {
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/still-XXXXXX");
        if (!work->isValid())
            throw std::runtime_error("Cannot create preview folder");
        const auto size = previewSize(640);
        RenderOptions options;
        options.audio = false;
        options.from = m_playhead;
        options.to = m_playhead + 1;
        addAiMedia(options);
        // Only the playhead frame is compiled, so the cost does not grow with its position.
        const auto plan = compileRender(viewable(), work->path(), size.width(), size.height(),
                                        options);
        const auto graph = work->filePath("graph.txt");
        writeGraph(graph, plan.graph);
        auto *process = new QProcess(this);
        m_preview = process;
        const auto revision = m_revision, frame = m_playhead;
        auto complete = [this, process, work, revision, frame](bool success) {
            const auto data = process->readAllStandardOutput();
            const auto error = process->readAllStandardError();
            m_preview = nullptr;
            process->deleteLater();
            if (revision != m_revision || frame != m_playhead)
                return;
            QImage img;
            if (success && img.loadFromData(data, "PNG")) {
                m_frames->frame = img;
                m_playback->showImage(img);
                m_previewUrl = "image://frames/" + QString::number(++m_previewSerial);
                emit changed();
            } else
                fail("Preview failed. " + QString::fromUtf8(error).right(2500));
        };
        connect(process, &QProcess::finished, this,
                [complete](int code, QProcess::ExitStatus status) {
                    complete(code == 0 && status == QProcess::NormalExit);
                });
        connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                complete(false);
        });
        process->start(
            executable("ffmpeg"),
            renderArguments(plan, graph, {}, "", 0));
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
bool Editor::usesAi(const Clip &c, const QString &task) const {
    if (task == "transcribe")
        return speaks(m_project, c) && !c.reverse;
    if (task == "eyecontact")
        return c.eyeContact;
    return task == "upscale" ? c.aiUpscale : c.aiCutout;
}
QString Editor::aiVariant(const QString &task, const Asset &a) const {
    if (task == "upscale")
        return QString::number(upscaleHeight(a));
    return task == "transcribe" ? m_captionLanguage : QString();
}
std::pair<double, double> Editor::aiSpan(const QString &task, const Asset &a,
                                         const Clip *extra) const {
    double from = 1e300, to = -1e300;
    for (const auto &c : m_project.clips)
        if (c.assetId == a.id && (usesAi(c, task) || (extra && c.id == extra->id))) {
            // Transition handles and held frames reach up to a second beyond the trim.
            const double in = c.sourceIn.seconds(),
                         length = frameTime(c.duration, m_project.fpsN, m_project.fpsD).seconds() *
                                  c.speed.seconds();
            from = std::min(from, in - 1);
            to = std::max(to, in + length + 1);
        }
    if (from > to)
        return {0, 0};
    return {std::clamp(from, 0., a.duration), std::clamp(to, 0., a.duration)};
}
int Editor::upscaleHeight(const Asset &a) {
    // Four times the source (the model's factor), at most 4K; 0 when the source is 4K already.
    return a.height > 0 && a.height < 2160 ? std::min(4 * a.height, 2160) / 2 * 2 : 0;
}
bool Editor::aiCovered(const QString &task, const Asset &a, const Clip *c) const {
    const auto r = m_ai->result(task, a, aiVariant(task, a));
    const auto [from, to] = aiSpan(task, a, c);
    // A matte holds its last analysed frame, so its end may fall short by one.
    const double slack = task == "matte" ? 1 / AiJobs::matteRate : 0.05;
    return !r.path.isEmpty() && r.start <= from + 1e-3 && r.end + slack >= to;
}
void Editor::addAiMedia(RenderOptions &options) const {
    for (const auto &c : m_project.clips)
        if (const auto *a = m_project.asset(c.assetId); a && a->kind == "video") {
            if (c.aiCutout && !options.mattes.contains(a->id))
                if (const auto m = m_ai->result("matte", *a); !m.path.isEmpty())
                    options.mattes.insert(a->id, m);
            if (c.eyeContact && !options.eyeContact.contains(a->id))
                if (const auto e = m_ai->result("eyecontact", *a); !e.path.isEmpty())
                    options.eyeContact.insert(a->id, e);
            if (c.aiUpscale && upscaleHeight(*a) > 0 && !options.upscaled.contains(a->id))
                if (const auto u = m_ai->result("upscale", *a, aiVariant("upscale", *a));
                    !u.path.isEmpty())
                    options.upscaled.insert(a->id, u);
        }
}
bool Editor::startAi(const QString &task, const Asset &a, const Clip *extra) {
    if (aiCovered(task, a, extra))
        return false;
    auto [from, to] = aiSpan(task, a, extra);
    // Keep what an earlier result covered when it overlaps, so other clips keep it.
    if (const auto r = m_ai->result(task, a, aiVariant(task, a));
        !r.path.isEmpty() && r.start <= to && r.end >= from) {
        from = std::min(from, r.start);
        to = std::max(to, r.end);
    }
    m_ai->start(task, a, from, to, aiVariant(task, a));
    return true;
}
void Editor::runAi(const QString &task) {
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || a->kind != "video")
        return fail("AI processing works on video clips");
    if (!m_ai->available(task))
        return fail(m_ai->missing(task) + " Download the AI pack next to Cutlery.exe.");
    if (task == "upscale" && upscaleHeight(*a) == 0)
        return fail("This video is already 4K or larger");
    startAi(task, *a, c);
}
void Editor::cancelAi() {
    m_captionAssets.clear();
    m_ai->cancel();
}
QStringList Editor::speakingAssets() const {
    QStringList ids;
    for (const auto &c : m_project.clips)
        if (usesAi(c, "transcribe") && !ids.contains(c.assetId))
            ids << c.assetId;
    return ids;
}
void Editor::generateCaptions(const QString &language, const QString &style, int maxChars,
                              int lines) {
    static const QRegularExpression code("^(auto|[a-z]{2,3})$");
    if (!code.match(language).hasMatch())
        return fail("Unknown caption language");
    if (!QStringList{"", "karaoke", "word"}.contains(style))
        return fail("Unknown caption style");
    if (maxChars < 20 || maxChars > 80 || lines < 1 || lines > 2)
        return fail("Captions take 20–80 characters on one or two lines");
    m_captionStyle = style;
    m_captionChars = maxChars;
    m_captionLines = style.isEmpty() ? lines : 1;
    if (!m_ai->available("transcribe"))
        return fail(m_ai->missing("transcribe") + " Download the AI pack next to Cutlery.exe.");
    m_captionLanguage = language;
    m_captionAssets = speakingAssets();
    if (m_captionAssets.isEmpty())
        return fail("No audible clips to caption");
    for (const auto &id : m_captionAssets)
        startAi("transcribe", *m_project.asset(id));
    m_status = "Recognising speech…";
    emit changed();
    placeCaptions();
}
void Editor::findPauses(double thresholdDb, double minPause) {
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || !a->hasAudio)
        return fail("Select a clip with sound to find pauses");
    if (c->reverse)
        return fail("Pauses cannot be removed from reversed clips");
    thresholdDb = std::clamp(thresholdDb, -70., -10.);
    minPause = std::clamp(minPause, 0.2, 10.);
    if (m_pauseProcess) {
        m_pauseProcess->disconnect(this);
        m_pauseProcess->kill();
        m_pauseProcess->deleteLater();
    }
    const double speed = c->speed.seconds(), in = c->sourceIn.seconds(),
                 length = frameTime(c->duration, m_project.fpsN, m_project.fpsD).seconds() *
                          speed;
    m_pauses = {c->id, "finding", {}, m_revision};
    auto *p = new QProcess(this);
    m_pauseProcess = p;
    auto log = std::make_shared<QByteArray>();
    connect(p, &QProcess::readyReadStandardError, this,
            [p, log] { *log += p->readAllStandardError(); });
    const qint64 frames = c->duration;
    const double fps = double(m_project.fpsN) / m_project.fpsD;
    auto complete = [this, p, log, in, speed, fps, frames, length](bool success) {
        *log += p->readAllStandardError();
        p->deleteLater();
        m_pauseProcess = nullptr;
        if (!success) {
            m_pauses.status = "failed";
            emit changed();
            return;
        }
        // silencedetect logs "silence_start: S" and "silence_end: E" in seconds from the seek.
        static const QRegularExpression mark("silence_(start|end): (-?[0-9.]+)");
        double open = -1;
        // Keep a little sound around speech so words are not clipped.
        const double padding = 0.12;
        auto add = [&](double a, double b) {
            a += padding;
            b -= padding;
            if (b - a < 0.1)
                return;
            m_pauses.ranges.push_back({qint64(std::ceil(a / speed * fps)),
                                       qint64(std::floor(b / speed * fps))});
        };
        for (auto it = mark.globalMatch(QString::fromUtf8(*log)); it.hasNext();) {
            const auto m = it.next();
            const double t = std::max(0., m.captured(2).toDouble());
            if (m.captured(1) == "start")
                open = t;
            else if (open >= 0) {
                add(open == 0 ? -padding : open, t);
                open = -1;
            }
        }
        if (open >= 0) // silence until the end of the clip
            add(open, length + padding);
        m_pauses.ranges.erase(std::remove_if(m_pauses.ranges.begin(), m_pauses.ranges.end(),
                                             [&](const auto &r) {
                                                 return r.second <= r.first || r.first >= frames;
                                             }),
                              m_pauses.ranges.end());
        m_pauses.status = "ready";
        emit changed();
    };
    connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            complete(false);
    });
    p->start(executable("ffmpeg"),
             {"-hide_banner", "-nostdin", "-nostats", "-ss", QString::number(in, 'f', 6), "-t",
              QString::number(length, 'f', 6), "-i", a->path, "-map", "0:a:0", "-vn", "-af",
              QString("silencedetect=noise=%1dB:d=%2")
                  .arg(thresholdDb, 0, 'f', 1)
                  .arg(minPause, 0, 'f', 3),
              "-f", "null", "-"});
    emit changed();
}
void Editor::splitAtMarkers() {
    const auto *c = m_project.clip(m_selected);
    if (!c)
        return fail("Select a clip to split");
    QVector<qint64> cuts;
    for (const auto &m : m_project.markers)
        if (m.frame > c->start && m.frame < c->start + c->duration)
            cuts << m.frame;
    if (cuts.isEmpty())
        return fail("No markers inside the clip. Add markers (M) or let Cutlery mark the beats.");
    const auto id = c->id;
    if (mutate([&](Project &p) {
            const auto linked = p.linkedClips(id);
            // From the end, so the original keeps its id as the first piece.
            for (auto it = cuts.rbegin(); it != cuts.rend(); ++it)
                for (const auto &clipId : QStringList{id} + linked)
                    p.split(clipId, *it);
        })) {
        m_status = QString("Split into %1 pieces at the markers").arg(cuts.size() + 1);
        emit changed();
    }
}
void Editor::fitToMarkers() {
    auto ids = selection();
    if (ids.isEmpty())
        return fail("Select the clips to put on the beat");
    if (m_project.markers.isEmpty())
        return fail("Add markers first (M), or let Cutlery mark the beats of a music clip");
    std::sort(ids.begin(), ids.end(), [&](const QString &a, const QString &b) {
        return m_project.clip(a)->start < m_project.clip(b)->start;
    });
    const int track = m_project.clip(ids.first())->track;
    for (const auto &id : ids)
        if (m_project.clip(id)->track != track)
            return fail("Select clips on one track");
    int fitted = 0;
    if (mutate([&](Project &p) {
            qint64 t = p.clip(ids.first())->start;
            const double fps = double(p.fpsN) / p.fpsD;
            for (const auto &id : ids) {
                auto *c = p.clip(id);
                // The longest the clip can run with its media (pictures and titles: any).
                qint64 longest = std::numeric_limits<qint64>::max();
                if (const auto *a = p.asset(c->assetId); a && a->kind != "image" && !a->loops)
                    longest = std::max<qint64>(
                        1, qint64(std::floor((a->duration - c->sourceIn.seconds()) /
                                             c->speed.seconds() * fps + 1e-6)));
                // The marker nearest to where the clip would end anyway, among those it can
                // reach; without one it keeps its length.
                qint64 best = -1;
                for (const auto &m : p.markers)
                    if (m.frame > t && m.frame - t <= longest &&
                        (best < 0 || std::abs(m.frame - t - c->duration) <
                                         std::abs(best - t - c->duration)))
                        best = m.frame;
                const qint64 duration = best > 0 ? best - t : c->duration;
                const auto linked = p.linkedClips(id);
                for (const auto &clipId : QStringList{id} + linked)
                    if (auto *x = p.clip(clipId)) {
                        x->start = t;
                        x->duration = duration;
                    }
                fitted += best > 0;
                t += duration;
            }
            if (p.trackSettings[track].magnetic)
                p.packTrack(track, p.trackOrder(track));
        })) {
        m_status = QString("%1 of %2 clips end on a marker").arg(fitted).arg(ids.size());
        emit changed();
    }
}
void Editor::pickKeyColor(double x, double y) {
    try {
        const auto *c = m_project.clip(m_selected);
        if (!c || c->assetId.isEmpty() || c->audioOnly)
            throw std::runtime_error("Select the clip with the green or blue screen");
        if (m_pickProcess)
            return;
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/still-XXXXXX");
        if (!work->isValid())
            throw std::runtime_error("Cannot create a work folder");
        // The clip alone, without its keys, where it sits on the canvas.
        Project alone = m_project;
        Clip clip = *c;
        clip.chromaKey = false;
        clip.lumaKey.clear();
        clip.aiCutout = false;
        clip.blendMode.clear();
        alone.clips = {clip};
        const int width = std::min(640, m_project.width) / 2 * 2;
        const int height = std::max(2, int(std::lround(double(width) * m_project.height /
                                                       m_project.width / 2)) * 2);
        RenderOptions options;
        options.audio = false;
        options.transparent = true;
        options.pixelFormat = "rgba";
        options.from = std::clamp<qint64>(m_playhead, c->start, c->start + c->duration - 1);
        options.to = options.from + 1;
        const auto plan = compileRender(alone, work->path(), width, height, options);
        const auto graph = work->filePath("graph.txt");
        writeGraph(graph, plan.graph);
        auto *p = new QProcess(this);
        m_pickProcess = p;
        auto png = std::make_shared<QByteArray>();
        connect(p, &QProcess::readyReadStandardOutput, this, [p, png] { *png += p->readAllStandardOutput(); });
        const auto id = c->id;
        auto complete = [this, p, png, work, id, x, y](bool success) {
            *png += p->readAllStandardOutput();
            p->deleteLater();
            m_pickProcess = nullptr;
            QImage image;
            if (!success || !image.loadFromData(*png, "PNG"))
                return fail("Cannot read the picture to pick a colour from");
            image = image.convertToFormat(QImage::Format_ARGB32);
            const int px = std::clamp(int(x * image.width()), 0, image.width() - 1),
                      py = std::clamp(int(y * image.height()), 0, image.height() - 1);
            // The average of a small square around the point, to step over noise.
            double r = 0, g = 0, b = 0;
            int n = 0;
            for (int j = std::max(0, py - 2); j <= std::min(image.height() - 1, py + 2); ++j)
                for (int i = std::max(0, px - 2); i <= std::min(image.width() - 1, px + 2); ++i) {
                    const auto colour = image.pixelColor(i, j);
                    if (colour.alpha() < 250)
                        continue;
                    r += colour.red();
                    g += colour.green();
                    b += colour.blue();
                    ++n;
                }
            if (n == 0)
                return fail("Click on the clip's screen colour in the viewer");
            const QColor picked(qRound(r / n), qRound(g / n), qRound(b / n));
            const auto selected = m_selected;
            m_selected = id;
            mutate([&](Project &project) {
                applyClipValue(project, "keyColor", picked.name());
                applyClipValue(project, "chromaKey", true);
            });
            m_selected = selected;
            m_status = "Key colour " + picked.name();
            emit changed();
        };
        connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
            complete(code == 0 && status == QProcess::NormalExit);
        });
        connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                complete(false);
        });
        p->start(executable("ffmpeg"), renderArguments(plan, graph, {}, "", 0));
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::splitAtScenes(double sensitivity) {
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || a->kind != "video" || c->audioOnly)
        return fail("Select a video clip to split at scene changes");
    if (c->reverse)
        return fail("Reversed clips cannot be split at scene changes");
    if (m_sceneProcess)
        return;
    const double speed = c->speed.seconds(), in = c->sourceIn.seconds(),
                 fps = double(m_project.fpsN) / m_project.fpsD,
                 length = c->duration / fps * speed;
    // FFmpeg's scene score (0–100) above which a frame starts a new shot.
    const double threshold = 25 - 20 * std::clamp(sensitivity, 0., 1.);
    m_scenes = {{"status", "finding"}};
    auto *p = new QProcess(this);
    m_sceneProcess = p;
    auto log = std::make_shared<QByteArray>();
    connect(p, &QProcess::readyReadStandardError, this,
            [p, log] { *log += p->readAllStandardError(); });
    auto complete = [this, p, log, id = c->id, revision = m_revision, start = c->start,
                     frames = c->duration, speed, fps](bool success) {
        *log += p->readAllStandardError();
        p->deleteLater();
        m_sceneProcess = nullptr;
        if (!success) {
            m_scenes = {{"status", "failed"}};
            emit changed();
            return;
        }
        if (revision != m_revision) {
            m_scenes = {};
            return fail("The clip changed while scenes were found; try again");
        }
        // "lavfi.scd.time: T" in seconds from the start of the analysed range. Shots shorter
        // than half a second are not split off (flashes, fast pans).
        static const QRegularExpression mark("lavfi\\.scd\\.time: (-?[0-9.]+)");
        const qint64 shortest = std::max<qint64>(1, qRound64(0.5 * fps));
        QVector<qint64> cuts;
        for (auto it = mark.globalMatch(QString::fromUtf8(*log)); it.hasNext();) {
            const auto local = qRound64(it.next().captured(1).toDouble() / speed * fps);
            if (local >= shortest && local <= frames - shortest &&
                (cuts.isEmpty() || local - cuts.last() >= shortest))
                cuts << local;
        }
        if (!cuts.isEmpty())
            mutate([&](Project &project) {
                const auto linked = project.linkedClips(id);
                // From the end, so the original keeps its id as the first shot.
                for (auto it = cuts.rbegin(); it != cuts.rend(); ++it)
                    for (const auto &clipId : QStringList{id} + linked)
                        project.split(clipId, start + *it);
            });
        m_scenes = {{"status", "done"}, {"count", cuts.size()}};
        m_status = cuts.isEmpty() ? QString("No scene changes found")
                                  : QString("Split into %1 shots").arg(cuts.size() + 1);
        emit changed();
    };
    connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            complete(false);
    });
    p->start(executable("ffmpeg"),
             {"-hide_banner", "-nostdin", "-nostats", "-ss", QString::number(in, 'f', 6), "-t",
              QString::number(length, 'f', 6), "-i", a->path, "-map", "0:v:0", "-an", "-sn",
              "-vf", QString("scale=320:-2,scdet=threshold=%1").arg(threshold, 0, 'f', 1), "-f",
              "null", "-"});
    emit changed();
}
void Editor::markBeats(int every) {
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || !a->hasAudio)
        return fail("Select a clip with sound to find its beats");
    if (c->reverse)
        return fail("Reversed clips cannot be searched for beats");
    if (m_beatThread)
        return;
    every = std::clamp(every, 1, 4);
    const double speed = c->speed.seconds(), in = c->sourceIn.seconds(),
                 fps = double(m_project.fpsN) / m_project.fpsD,
                 length = std::min(c->duration / fps * speed, 3600.);
    m_beats = {{"status", "finding"}};
    const auto path = a->path, ffmpeg = executable("ffmpeg");
    // Decoded and analysed in the background: mono at 11025 Hz, as 32-bit floats.
    m_beatThread = QThread::create([this, path, ffmpeg, in, length, speed, fps, every,
                                    id = c->id, revision = m_revision, start = c->start,
                                    frames = c->duration] {
        constexpr int rate = 11025;
        QProcess decoder;
        decoder.start(ffmpeg, {"-hide_banner", "-nostdin", "-v", "error", "-ss",
                               QString::number(in, 'f', 6), "-t", QString::number(length, 'f', 6),
                               "-i", path, "-map", "0:a:0", "-vn", "-ac", "1", "-ar",
                               QString::number(rate), "-f", "f32le", "pipe:1"});
        QByteArray pcm;
        bool ok = decoder.waitForStarted();
        while (ok && decoder.state() != QProcess::NotRunning) {
            if (QThread::currentThread()->isInterruptionRequested()) {
                decoder.kill();
                decoder.waitForFinished(1500);
                return;
            }
            decoder.waitForReadyRead(100);
            pcm += decoder.readAllStandardOutput();
        }
        pcm += decoder.readAllStandardOutput();
        ok = ok && decoder.exitStatus() == QProcess::NormalExit && decoder.exitCode() == 0;
        QVector<float> samples(pcm.size() / 4);
        std::memcpy(samples.data(), pcm.constData(), size_t(samples.size()) * 4);
        double bpm = 0;
        const auto beats = ok ? detectBeats(samples, rate, &bpm) : QVector<double>{};
        QMetaObject::invokeMethod(this, [=, this] {
            if (!ok) {
                m_beats = {{"status", "failed"}};
                return fail("Cannot read the clip's sound");
            }
            if (revision != m_revision) {
                m_beats = {};
                return fail("The clip changed while beats were found; try again");
            }
            // Source seconds to timeline frames inside the clip.
            QVector<qint64> marks;
            for (qsizetype i = 0; i < beats.size(); i += every) {
                const auto local = qRound64(beats[i] / speed * fps);
                if (local >= 0 && local < frames && (marks.isEmpty() || marks.last() < start + local))
                    marks << start + local;
            }
            int added = 0;
            const bool marked = mutate([&](Project &p) {
                for (const auto frame : marks) {
                    const auto at = std::lower_bound(
                        p.markers.begin(), p.markers.end(), frame,
                        [](const Marker &m, qint64 f) { return m.frame < f; });
                    if (at != p.markers.end() && at->frame == frame)
                        continue;
                    if (p.markers.size() >= 1000)
                        throw std::runtime_error("At most 1000 markers: mark every 2nd or 4th beat");
                    p.markers.insert(at, Marker{frame, QString("Beat %1").arg(++added), "#4fc3f7"});
                }
            });
            if (!marked) {
                m_beats = {{"status", "failed"}};
                emit changed();
                return;
            }
            m_beats = {{"status", "done"}, {"count", added}, {"bpm", qRound(bpm / speed)}, {"clipId", id}};
            m_status = beats.isEmpty() ? QString("No beats found")
                                       : QString("%1 beat markers (about %2 BPM)")
                                             .arg(added)
                                             .arg(qRound(bpm / speed));
            emit changed();
        });
    });
    connect(m_beatThread, &QThread::finished, this, [this] {
        m_beatThread->deleteLater();
        m_beatThread = nullptr;
        emit changed();
    });
    m_beatThread->start();
    emit changed();
}
void Editor::freezeFrame(double seconds) {
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || a->kind != "video" || c->audioOnly)
        return fail("Select a video clip to freeze a frame of it");
    if (m_playhead < c->start || m_playhead >= c->start + c->duration)
        return fail("Move the playhead into the clip to freeze that frame");
    seconds = std::clamp(seconds, 0.1, 60.);
    const double fps = double(m_project.fpsN) / m_project.fpsD;
    // A reversed clip shows the source from its end backwards.
    const double local = (m_playhead - c->start) / fps, length = c->duration / fps;
    const double source =
        c->sourceIn.seconds() +
        (c->reverse ? std::max(0., length - local - 1 / fps) : local) * c->speed.seconds();
    QDir().mkpath(m_data + "/freeze");
    const auto still = m_data + "/freeze/" + newId() + ".png";
    auto *p = new QProcess(this);
    auto complete = [this, p, still, id = c->id, revision = m_revision, frame = m_playhead,
                     seconds, fps](bool success) {
        p->deleteLater();
        QImage image;
        if (!success || !image.load(still))
            return fail("Could not read that frame of the clip");
        if (revision != m_revision)
            return fail("The clip changed while the frame was read; try again");
        const auto length = std::max<qint64>(1, qRound64(seconds * fps));
        QString added;
        mutate([&](Project &project) {
            const auto *original = project.clip(id);
            if (!original)
                throw std::runtime_error("The clip no longer exists");
            const auto source = *original;
            auto linked = project.linkedClips(id);
            QSet<int> tracks{source.track};
            for (const auto &other : linked)
                tracks.insert(project.clip(other)->track);
            for (const auto track : tracks)
                project.requireEditable(track);
            if (frame > source.start)
                for (const auto &clipId : QStringList{id} + linked)
                    project.split(clipId, frame);
            // Everything from the playhead on these tracks moves later by the length of the still.
            for (auto &x : project.clips)
                if (tracks.contains(x.track) && x.start >= frame)
                    x.start += length;
            Asset a;
            a.id = newId();
            a.path = still;
            a.name = "Freeze frame";
            a.kind = "image";
            a.width = image.width();
            a.height = image.height();
            a.duration = 5;
            project.assets.push_back(a);
            // The still keeps the clip's framing and look, but not its motion or sound.
            Clip f = source;
            f.id = newId();
            f.assetId = a.id;
            f.name = "Freeze frame";
            f.start = frame;
            f.duration = length;
            f.sourceIn = Time(0, 1);
            f.speed = Time(1, 1);
            f.keyframes.clear();
            f.transition.clear();
            f.transitionFrames = 0;
            f.fadeIn = f.fadeOut = 0;
            f.reverse = false;
            f.aiCutout = f.aiUpscale = f.eyeContact = false;
            f.slowMotion.clear();
            project.clips.push_back(f);
            added = f.id;
        });
        if (!added.isEmpty()) {
            select(added);
            m_status = QString("Froze the frame for %1 s").arg(seconds, 0, 'f', 1);
            emit changed();
        }
    };
    connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            complete(false);
    });
    p->start(executable("ffmpeg"),
             {"-hide_banner", "-nostdin", "-v", "error", "-y", "-ss", QString::number(source, 'f', 6),
              "-i", a->path, "-map", "0:v:0", "-frames:v", "1", still});
}
// Text-based editing ----------------------------------------------------------------------
QStringList Editor::wordRun(const Clip &c) const {
    auto adjacent = [&](const Clip &x, bool before) -> const Clip * {
        for (const auto &y : m_project.clips)
            if (y.id != x.id && y.track == x.track && y.assetId == x.assetId && !y.reverse &&
                (before ? y.start + y.duration == x.start : y.start == x.start + x.duration))
                return &y;
        return nullptr;
    };
    const Clip *first = &c;
    QStringList ids{c.id};
    while (const auto *p = adjacent(*first, true)) {
        if (ids.contains(p->id))
            break;
        ids.prepend(p->id);
        first = p;
    }
    const Clip *last = &c;
    while (const auto *n = adjacent(*last, false)) {
        if (ids.contains(n->id))
            break;
        ids << n->id;
        last = n;
    }
    return ids;
}
QVector<Editor::ClipWord> Editor::clipWords(const Clip &selected) const {
    const auto *a = m_project.asset(selected.assetId);
    if (!a || selected.reverse)
        return {};
    const auto r = m_ai->result("transcribe", *a, m_captionLanguage);
    if (r.path.isEmpty())
        return {};
    if (m_words.clipId == selected.id && m_words.path == r.path && m_words.revision == m_revision)
        return m_words.words;
    QVector<ClipWord> words;
    try {
        const auto cues = parseSrt(readUtf8File(r.path));
        const double fps = double(m_project.fpsN) / m_project.fpsD;
        for (const auto &id : wordRun(selected)) {
            const auto &c = *m_project.clip(id);
            const double speed = c.speed.seconds(), in = c.sourceIn.seconds();
            for (const auto &cue : cues) {
                const auto text = cue.text.trimmed();
                const qint64 from = qRound64((r.start + cue.start - in) / speed * fps),
                             to = qRound64((r.start + cue.end - in) / speed * fps);
                if (text.isEmpty() || from < 0 || from >= c.duration)
                    continue;
                words.push_back({c.id, text, from, std::clamp(to, from + 1, c.duration),
                                 isFillerWord(text)});
            }
        }
    } catch (const std::exception &) {
        words.clear();
    }
    m_words = {selected.id, r.path, m_revision, words};
    return words;
}
QVariantMap Editor::transcriptState() const {
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || !a->hasAudio)
        return {{"status", "none"}};
    QVariantMap result{{"clipId", c->id}, {"language", m_captionLanguage}};
    const auto st = m_ai->status("transcribe", *a, m_captionLanguage);
    const auto status = st["status"].toString();
    if (status == "queued" || status == "running") {
        result["status"] = "running";
        result["progress"] = st["progress"].toDouble();
        return result;
    }
    if (status == "failed") {
        result["status"] = "failed";
        result["error"] = st["error"];
        return result;
    }
    if (status != "ready") {
        result["status"] = m_ai->available("transcribe") ? "none" : "unavailable";
        result["missing"] = m_ai->missing("transcribe");
        return result;
    }
    QVariantList list;
    int fillers = 0;
    for (const auto &w : clipWords(*c)) {
        const qint64 origin = m_project.clip(w.clipId)->start;
        list << QVariantMap{{"text", w.text},
                            {"start", origin + w.start},
                            {"end", origin + w.end},
                            {"filler", w.filler}};
        fillers += w.filler;
    }
    result["status"] = "ready";
    result["words"] = list;
    result["fillers"] = fillers;
    return result;
}
void Editor::transcribeClip(const QString &language) {
    static const QRegularExpression code("^(auto|[a-z]{2,3})$");
    if (!code.match(language).hasMatch())
        return fail("Unknown language");
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || !a->hasAudio)
        return fail("Select a clip with speech");
    if (c->reverse)
        return fail("Reversed clips cannot be edited by their words");
    m_captionLanguage = language;
    if (!aiCovered("transcribe", *a, c)) {
        if (!m_ai->available("transcribe"))
            return fail(m_ai->missing("transcribe") + " Download the AI pack next to Cutlery.exe.");
        startAi("transcribe", *a, c);
        m_status = "Recognising speech…";
    }
    emit changed();
}
void Editor::cutClipWords(const QList<int> &indices, const QString &what) {
    const auto *c = m_project.clip(m_selected);
    if (!c)
        return fail("Select the clip first");
    const auto words = clipWords(*c);
    if (words.isEmpty())
        return fail("Transcribe the clip first");
    // The ranges of each piece, from that piece's own words.
    const double fps = double(m_project.fpsN) / m_project.fpsD;
    QVector<QPair<QString, QVector<QPair<qint64, qint64>>>> cuts;
    const auto run = wordRun(*c);
    for (const auto &id : run) {
        QVector<qint64> starts, ends;
        QList<int> chosen;
        for (int i = 0; i < words.size(); ++i)
            if (words[i].clipId == id) {
                if (indices.contains(i))
                    chosen << int(starts.size());
                starts << words[i].start;
                ends << words[i].end;
            }
        // A piece's last word is cut to the piece's end when the speech goes on in the next.
        const qint64 length = m_project.clip(id)->duration;
        const auto ranges = wordCutRanges(starts, ends, chosen, length,
                                          id == run.last() ? qRound64(0.15 * fps) : length);
        if (!ranges.isEmpty())
            cuts.push_back({id, ranges});
    }
    if (cuts.isEmpty())
        return fail("Choose the words to cut");
    const auto selected = c->id;
    qint64 removed = 0;
    if (!mutate([&](Project &p) {
            // Later pieces first: cutting one moves those after it.
            for (int i = int(cuts.size()) - 1; i >= 0; --i) {
                const auto linked = p.linkedClips(cuts[i].first);
                removed += p.cutRanges(cuts[i].first, cuts[i].second);
                for (const auto &other : linked)
                    p.cutRanges(other, cuts[i].second);
            }
        }))
        return;
    // Keep a piece of the recording selected so the transcript stays on show.
    if (!m_project.clip(selected))
        for (const auto &x : m_project.clips)
            if (x.assetId == c->assetId)
                m_selected = x.id;
    m_status = QString("Cut %1 (%2 s)")
                   .arg(what)
                   .arg(frameTime(removed, m_project.fpsN, m_project.fpsD).seconds(), 0, 'f', 1);
    emit changed();
}
void Editor::cutWords(const QVariantList &indices) {
    QList<int> chosen;
    for (const auto &v : indices)
        chosen << v.toInt();
    cutClipWords(chosen, chosen.size() == 1 ? QString("1 word") : QString("%1 words").arg(chosen.size()));
}
void Editor::removeFillers() {
    const auto *c = m_project.clip(m_selected);
    if (!c)
        return fail("Select the clip first");
    QList<int> chosen;
    const auto words = clipWords(*c);
    for (int i = 0; i < words.size(); ++i)
        if (words[i].filler)
            chosen << i;
    if (chosen.isEmpty())
        return fail(words.isEmpty() ? "Transcribe the clip first" : "No filler words found");
    cutClipWords(chosen, chosen.size() == 1 ? QString("1 filler word")
                                            : QString("%1 filler words").arg(chosen.size()));
}
// The correction for measured levels: luma at the 10th and 90th percentile and the average
// chroma (0..255, as FFmpeg's signalstats reports them).
QVariantMap autoColourCorrection(double low, double high, double u, double v) {
    // Stretch the middle 80 % of the levels to 25..230 around mid-grey.
    const double contrast = std::clamp(205. / std::max(1., high - low), 0.7, 1.8);
    const double mid = (low + high) / 2;
    const double brightness = std::clamp(-(mid - 128) * contrast / 255, -0.35, 0.35);
    // Blue (u above 128) is warmed, red–yellow cooled; green (both low) gets magenta.
    const double temperature = std::clamp(((u - 128) - (v - 128)) / 30, -0.6, 0.6);
    const double tint = std::clamp((128 - (u + v) / 2) / 20, -0.5, 0.5);
    auto round = [](double x) { return std::round(x * 100) / 100; };
    return {{"brightness", round(brightness)},
            {"contrast", round(contrast)},
            {"temperature", round(temperature)},
            {"tint", round(tint)}};
}
void Editor::autoColour() {
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || (a->kind != "video" && a->kind != "image") || c->audioOnly)
        return fail("Select a video or picture clip");
    if (m_autoColourProcess)
        return;
    const double length = a->kind == "image"
                              ? 0.
                              : std::min(20., frameTime(c->duration, m_project.fpsN, m_project.fpsD).seconds() *
                                                  c->speed.seconds());
    QStringList args{"-hide_banner", "-nostdin", "-v", "error"};
    if (a->kind == "video")
        args << "-ss" << QString::number(c->sourceIn.seconds(), 'f', 3) << "-t"
             << QString::number(std::max(0.1, length), 'f', 3);
    args << "-i" << a->path << "-vf"
         << "fps=2,scale=320:-2,signalstats,metadata=print:file=-" << "-f" << "null" << "-";
    auto *p = new QProcess(this);
    m_autoColourProcess = p;
    m_autoColour = {{"status", "measuring"}, {"clipId", c->id}};
    const auto id = c->id;
    const auto revision = m_revision;
    auto out = std::make_shared<QByteArray>();
    connect(p, &QProcess::readyReadStandardOutput, this, [p, out] {
        *out += p->readAllStandardOutput();
        if (out->size() > 8 * 1024 * 1024)
            *out = out->right(4 * 1024 * 1024);
    });
    auto complete = [this, p, out, id, revision](bool success) {
        *out += p->readAllStandardOutput();
        p->deleteLater();
        m_autoColourProcess = nullptr;
        QHash<QString, double> sums;
        QHash<QString, int> counts;
        static const QRegularExpression line("lavfi\\.signalstats\\.(YLOW|YHIGH|UAVG|VAVG)=([0-9.]+)");
        for (auto it = line.globalMatch(QString::fromUtf8(*out)); it.hasNext();) {
            const auto m = it.next();
            sums[m.captured(1)] += m.captured(2).toDouble();
            counts[m.captured(1)] += 1;
        }
        if (!success || counts.value("YLOW") == 0 || counts.value("UAVG") == 0) {
            m_autoColour = {{"status", "failed"}, {"clipId", id}};
            fail("Could not measure the picture");
            return;
        }
        auto avg = [&](const QString &k) { return sums[k] / std::max(1, counts[k]); };
        const auto fix = autoColourCorrection(avg("YLOW"), avg("YHIGH"), avg("UAVG"), avg("VAVG"));
        if (revision != m_revision && !m_project.clip(id)) {
            m_autoColour = {{"status", "failed"}, {"clipId", id}};
            return;
        }
        mutate([&](Project &pr) {
            auto *c = pr.clip(id);
            if (!c)
                return;
            pr.requireEditable(c->track);
            c->brightness = fix["brightness"].toDouble();
            c->contrast = fix["contrast"].toDouble();
            c->temperature = fix["temperature"].toDouble();
            c->tint = fix["tint"].toDouble();
        });
        m_autoColour = {{"status", "done"}, {"clipId", id}};
        m_status = "Colour corrected automatically";
        emit changed();
    };
    connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            complete(false);
    });
    p->start(executable("ffmpeg"), args);
    emit changed();
}
// Text styles -------------------------------------------------------------------------------
static const QStringList &textStyleKeys() {
    static const QStringList keys{"fontFamily",  "fontSize",       "textColor",     "gradientColor",
                                  "bold",        "italic",         "align",         "letterSpacing",
                                  "lineSpacing", "outline",        "outlineColor",  "textShadow",
                                  "textGlow",    "textGlowColor",
                                  "background",  "backgroundColor", "textAnimation", "textAnimationTime",
                                  "highlightColor"};
    return keys;
}
QVariantList Editor::textStyles() const {
    return m_textStyles;
}
void Editor::saveTextStyles() {
    QDir().mkpath(m_data);
    QSaveFile f(m_data + "/styles.json");
    const auto data = QJsonDocument(QJsonArray::fromVariantList(m_textStyles)).toJson();
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
        fail("Cannot save the text styles in " + m_data);
}
void Editor::saveTextStyle(const QString &name) {
    const auto label = name.trimmed().left(60);
    if (label.isEmpty())
        return fail("Name the style");
    const auto *c = m_project.clip(m_selected);
    if (!c || !c->assetId.isEmpty() || !c->effect.isEmpty() || c->text.isEmpty())
        return fail("Select a title to take the style from");
    const auto selected = state()["selected"].toMap();
    QVariantMap values;
    for (const auto &k : textStyleKeys())
        values[k] = selected.value(k);
    QVariantMap style{{"name", label}, {"values", values}};
    for (auto &s : m_textStyles)
        if (s.toMap()["name"].toString().compare(label, Qt::CaseInsensitive) == 0) {
            s = style;
            saveTextStyles();
            m_status = "Style updated: " + label;
            emit changed();
            return;
        }
    if (m_textStyles.size() >= 200)
        return fail("Remove a style first (200 at most)");
    m_textStyles << style;
    saveTextStyles();
    m_status = "Style saved: " + label;
    emit changed();
}
void Editor::applyTextStyle(const QString &name) {
    QVariantMap values;
    for (const auto &s : m_textStyles)
        if (s.toMap()["name"].toString() == name)
            values = s.toMap()["values"].toMap();
    if (values.isEmpty())
        return fail("No such style");
    QStringList targets;
    for (const auto &id : selection())
        if (const auto *c = m_project.clip(id); c && c->assetId.isEmpty() && c->effect.isEmpty())
            targets << id;
    if (targets.isEmpty())
        return fail("Select the titles to style");
    const auto selected = m_selected;
    mutate([&](Project &p) {
        for (const auto &id : targets) {
            m_selected = id; // applyClipValue works on the selected clip
            for (const auto &k : textStyleKeys())
                if (values.contains(k) && values[k].isValid())
                    applyClipValue(p, k, values[k]);
        }
    });
    m_selected = selected;
    m_status = QString("Applied the style %1 to %2 clip%3")
                   .arg(name)
                   .arg(targets.size())
                   .arg(targets.size() == 1 ? "" : "s");
    emit changed();
}
void Editor::removeTextStyle(const QString &name) {
    for (int i = 0; i < m_textStyles.size(); ++i)
        if (m_textStyles[i].toMap()["name"].toString() == name) {
            m_textStyles.removeAt(i);
            saveTextStyles();
            emit changed();
            return;
        }
}
void Editor::saveBrand() {
    QDir().mkpath(m_data);
    QSaveFile f(m_data + "/brand.json");
    QJsonObject o{{"colors", QJsonArray::fromStringList(m_brandColors)}};
    if (!m_brandLogo.isEmpty())
        o["logo"] = QDir(m_data).relativeFilePath(m_brandLogo);
    const auto data = QJsonDocument(o).toJson();
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
        fail("Cannot save the brand kit in " + m_data);
}
void Editor::addBrandColor(const QString &color) {
    const QColor c(color.trimmed());
    if (!c.isValid())
        return fail("Enter a colour as #rrggbb");
    if (m_brandColors.contains(c.name()))
        return;
    if (m_brandColors.size() >= 24)
        return fail("Remove a brand colour first (24 at most)");
    m_brandColors << c.name();
    saveBrand();
    emit changed();
}
void Editor::removeBrandColor(const QString &color) {
    if (m_brandColors.removeAll(QColor(color).name()) == 0)
        return;
    saveBrand();
    emit changed();
}
void Editor::setBrandLogo(const QUrl &file) {
    // Earlier logo files stay in the data folder: saved projects may still show them.
    if (file.isEmpty()) {
        m_brandLogo.clear();
    } else {
        const auto path = file.isLocalFile() ? file.toLocalFile() : file.toString();
        const QFileInfo info(path);
        QImageReader reader(path);
        if (!QStringList{"png", "jpg", "jpeg", "webp", "bmp"}.contains(info.suffix().toLower()) ||
            !reader.canRead())
            return fail("Choose a picture for the logo (PNG with transparency works best)");
        // A new name each time, so projects that use an earlier logo keep it.
        QDir().mkpath(m_data + "/brand");
        const auto target = QString("%1/brand/logo-%2.%3")
                                .arg(m_data)
                                .arg(QDateTime::currentMSecsSinceEpoch())
                                .arg(info.suffix().toLower());
        if (!QFile::copy(path, target))
            return fail("Cannot copy the logo into " + m_data);
        m_brandLogo = QDir::cleanPath(target);
    }
    saveBrand();
    m_status = m_brandLogo.isEmpty() ? "Logo removed from the brand kit" : "Logo saved in the brand kit";
    emit changed();
}
void Editor::addBrandLogo(const QString &corner) {
    if (!QStringList{"topLeft", "topRight", "bottomLeft", "bottomRight"}.contains(corner))
        return fail("Unknown corner");
    if (m_brandLogo.isEmpty() || !QFileInfo(m_brandLogo).isFile())
        return fail("Choose a logo for the brand kit first");
    QImageReader reader(m_brandLogo);
    const auto size = reader.size();
    if (!size.isValid())
        return fail("The logo cannot be read");
    const auto id = newId();
    mutate([&](Project &p) {
        QString assetId;
        for (const auto &a : p.assets)
            if (QDir::cleanPath(a.path) == m_brandLogo)
                assetId = a.id;
        if (assetId.isEmpty()) {
            Asset a;
            a.id = newId();
            a.path = m_brandLogo;
            a.name = "Logo";
            a.kind = "image";
            a.width = size.width();
            a.height = size.height();
            a.duration = 5;
            p.assets.push_back(a);
            assetId = a.id;
        }
        const qint64 length = std::max(p.duration(), qRound64(5. * p.fpsN / p.fpsD));
        // The top track when nothing is on it, otherwise a new one above.
        int track = p.tracks - 1;
        const bool used = std::any_of(p.clips.begin(), p.clips.end(),
                                      [&](const Clip &c) { return c.track == track; });
        if (used || p.trackSettings[track].locked || p.trackSettings[track].name == captionTrackName) {
            p.addTrack();
            track = p.tracks - 1;
        }
        Clip c;
        c.id = id;
        c.assetId = assetId;
        c.name = "Logo";
        c.track = track;
        c.start = 0;
        c.duration = length;
        // About an eighth of the picture's height (or a fifth of its width for wide logos),
        // 3 % of the height from the edges.
        const double fit = std::min(0.125 * p.height / size.height(), 0.2 * p.width / size.width());
        const double contain = std::min(double(p.width) / size.width(), double(p.height) / size.height());
        c.scale = std::clamp(fit / contain, 0.02, 1.);
        const double w = size.width() * contain * c.scale, h = size.height() * contain * c.scale;
        const double margin = 0.03 * p.height;
        const double dx = 0.5 - (w / 2 + margin) / p.width, dy = 0.5 - (h / 2 + margin) / p.height;
        c.x = corner.endsWith("Left") ? -dx : dx;
        c.y = corner.startsWith("top") ? -dy : dy;
        p.clips.push_back(c);
    });
    if (m_project.clip(id)) {
        select(id);
        m_status = "Logo added for the whole video";
        emit changed();
    }
}
void Editor::listLuts() {
    m_lutLibrary.clear();
    QList<QFileInfo> files = QDir(m_data + "/luts")
                                 .entryInfoList({"*.cube", "*.3dl", "*.CUBE", "*.3DL"},
                                                QDir::Files);
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.completeBaseName().compare(b.completeBaseName(), Qt::CaseInsensitive) < 0;
    });
    for (const auto &f : files)
        m_lutLibrary << QVariantMap{{"name", f.completeBaseName()},
                                    {"path", QDir::cleanPath(f.absoluteFilePath())}};
}
QString Editor::addLutToLibrary(const QUrl &file) {
    const auto path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const QFileInfo info(path);
    if (!info.isFile() || !QStringList{"cube", "3dl"}.contains(info.suffix().toLower())) {
        fail("Choose a .cube or .3dl LUT file");
        return {};
    }
    if (info.size() > 64 * 1024 * 1024) {
        fail("The LUT file is too large");
        return {};
    }
    QDir().mkpath(m_data + "/luts");
    auto target = QDir::cleanPath(m_data + "/luts/" + info.fileName());
    if (QDir::cleanPath(info.absoluteFilePath()) != target) {
        // Another LUT of the same name gets a number.
        for (int n = 2; QFileInfo::exists(target); ++n)
            target = QString("%1/luts/%2 (%3).%4")
                         .arg(m_data, info.completeBaseName())
                         .arg(n)
                         .arg(info.suffix());
        if (!QFile::copy(path, target)) {
            fail("Cannot copy the LUT into " + m_data);
            return {};
        }
    }
    listLuts();
    m_status = "LUT added to the library: " + QFileInfo(target).completeBaseName();
    emit changed();
    return target;
}
void Editor::removePauses() {
    if (m_pauses.status != "ready" || m_pauses.revision != m_revision || m_pauses.ranges.isEmpty())
        return fail("Find pauses again: the clip has changed");
    const auto id = m_pauses.clipId;
    const auto ranges = m_pauses.ranges;
    qint64 removed = 0;
    const bool ok = mutate([&](Project &p) {
        const auto linked = p.linkedClips(id);
        removed = p.cutRanges(id, ranges);
        for (const auto &other : linked)
            p.cutRanges(other, ranges);
    });
    if (!ok)
        return;
    m_pauses = {};
    m_status = QString("Removed %1 pause%2 (%3 s)")
                   .arg(ranges.size())
                   .arg(ranges.size() == 1 ? "" : "s")
                   .arg(frameTime(removed, m_project.fpsN, m_project.fpsD).seconds(), 0, 'f', 1);
    emit changed();
}
QVariantMap Editor::pauseState() const {
    if (m_pauses.clipId.isEmpty())
        return {{"status", "idle"}};
    qint64 frames = 0;
    for (const auto &r : m_pauses.ranges)
        frames += r.second - r.first;
    return {{"status", m_pauses.revision == m_revision ? m_pauses.status : QString("stale")},
            {"clipId", m_pauses.clipId},
            {"count", int(m_pauses.ranges.size())},
            {"seconds", frameTime(frames, m_project.fpsN, m_project.fpsD).seconds()}};
}
QVariantMap Editor::captionState() const {
    if (m_captionAssets.isEmpty())
        return {{"running", false}, {"language", m_captionLanguage}};
    double done = 0;
    for (const auto &id : m_captionAssets)
        if (const auto *a = m_project.asset(id)) {
            const auto st = m_ai->status("transcribe", *a, m_captionLanguage);
            done += st["status"] == "ready" ? 1. : st["progress"].toDouble();
        }
    return {{"running", true},
            {"language", m_captionLanguage},
            {"progress", done / m_captionAssets.size()}};
}
void Editor::placeCaptions() {
    if (m_captionAssets.isEmpty())
        return;
    QHash<QString, QVector<Cue>> transcripts;
    for (const auto &id : m_captionAssets) {
        const auto *a = m_project.asset(id);
        if (!a)
            continue;
        const auto status = m_ai->status("transcribe", *a, m_captionLanguage)["status"].toString();
        if (status == "queued" || status == "running")
            return; // wait for the remaining media
        if (status == "failed") {
            m_captionAssets.clear();
            return fail("Speech recognition failed: " +
                        m_ai->status("transcribe", *a, m_captionLanguage)["error"].toString());
        }
        const auto r = m_ai->result("transcribe", *a, m_captionLanguage);
        if (r.path.isEmpty())
            continue;
        try {
            // One cue per word, grouped into caption lines with each word's start.
            auto words = parseSrt(readUtf8File(r.path));
            for (auto &w : words) {
                w.start += r.start;
                w.end += r.start;
            }
            auto cues = groupWords(words, m_captionChars * m_captionLines);
            for (auto &cue : cues)
                cue.text = layoutCaption(cue.text, m_captionLines, m_captionChars);
            transcripts.insert(id, cues);
        } catch (const std::exception &e) {
            m_captionAssets.clear();
            return fail(QString("Cannot read the transcript: ") + e.what());
        }
    }
    m_captionAssets.clear();
    int count = 0;
    mutate([&](Project &p) {
        // Captions go to their own track, replaced on every run.
        int track = -1;
        for (int t = 0; t < p.tracks; ++t)
            if (p.trackSettings[t].name == captionTrackName)
                track = t;
        if (track < 0) {
            p.addTrack(captionTrackName);
            track = p.tracks - 1;
        }
        p.requireEditable(track);
        p.clips.erase(std::remove_if(p.clips.begin(), p.clips.end(),
                                     [&](const Clip &c) { return c.track == track; }),
                      p.clips.end());
        const auto captions = captionClips(p, transcripts, track, m_captionStyle);
        count = int(captions.size());
        p.clips += captions;
    });
    m_status = count ? QString("%1 captions added").arg(count) : QString("No speech found");
    emit changed();
}
QSize Editor::previewSize(int longSide) const {
    int w = longSide, h = qRound(double(longSide) * m_project.height / m_project.width / 2) * 2;
    if (h > longSide) {
        h = longSide;
        w = qRound(double(longSide) * m_project.width / m_project.height / 2) * 2;
    }
    return {std::max(64, w), std::max(64, h)};
}
void Editor::setVideoSink(QObject *sink) {
    m_playback->setVideoSink(qobject_cast<QVideoSink *>(sink));
    m_playback->showImage(m_frames->frame);
}
bool Editor::captureScopeFrame() {
    const auto image = m_playback->currentImage();
    if (image.isNull())
        return false;
    m_frames->live = image;
    return true;
}
void Editor::play() {
    m_resumeTimer.stop();
    m_reverseTimer.stop();
    if (m_project.clips.empty() || m_busy || m_playback->active())
        return;
    if (m_preview) {
        m_preview->disconnect(this);
        m_preview->kill();
        m_preview->deleteLater();
        m_preview = nullptr;
    }
    try {
        if (m_playhead >= m_project.duration() - 1)
            m_playhead = 0;
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/play-XXXXXX");
        if (!work->isValid())
            throw std::runtime_error("Cannot create playback folder");
        const auto size = previewSize(960);
        Playback::Request request;
        request.ffmpeg = executable("ffmpeg");
        request.fpsN = m_project.fpsN;
        request.fpsD = m_project.fpsD;
        request.from = m_playhead;
        request.work = work;
        request.rate = m_playRate;
        RenderOptions options;
        options.from = m_playhead;
        options.realtime = true;
        options.rate = m_playRate;
        options.audio = false;
        addAiMedia(options);
        request.video =
            compileRender(viewable(), work->path(), size.width(), size.height(), options);
        options.audio = true;
        options.video = false;
        request.audio =
            compileRender(viewable(), work->path(), size.width(), size.height(), options);
        request.videoGraph = work->filePath("video.txt");
        request.audioGraph = work->filePath("audio.txt");
        writeGraph(request.videoGraph, request.video.graph);
        writeGraph(request.audioGraph, request.audio.graph);
        m_analysis->setPaused(true);
        m_thumbnails->setPaused(true);
        m_playback->start(request);
        m_status = "Playing";
    } catch (const std::exception &e) {
        fail(e.what());
    }
    emit playbackChanged();
    emit changed();
}
void Editor::pause() {
    const bool resuming = m_resumeTimer.isActive();
    m_resumeTimer.stop();
    if (m_reverseTimer.isActive()) {
        m_reverseTimer.stop();
        m_status = "Ready";
        emit playbackChanged();
        emit changed();
    }
    if (!m_playback->active() && !resuming)
        return;
    if (m_playback->active())
        m_playhead = m_playback->frame();
    stopPlayback();
    m_status = "Ready";
    requestPreview();
    emit changed();
}
void Editor::togglePlayback() {
    if (m_playback->active() || m_resumeTimer.isActive() || m_reverseTimer.isActive())
        pause();
    else {
        m_playRate = 1;
        play();
    }
}
void Editor::shuttle(bool forward) {
    if (forward) {
        const bool playing = m_playback->active();
        if (m_reverseTimer.isActive())
            pause();
        const double next = playing ? std::min(4., m_playRate * 2) : 1;
        if (playing && next == m_playRate)
            return;
        if (playing) {
            m_playhead = m_playback->frame();
            stopPlayback();
        }
        m_playRate = next;
        play();
        return;
    }
    // Backward: step the playhead back, faster on each press; previews follow.
    if (m_playback->active())
        pause();
    m_shuttleRate = m_reverseTimer.isActive() ? std::min(4., m_shuttleRate * 2) : 1;
    m_reverseTimer.start();
    m_status = QString("Scrubbing back %1×").arg(m_shuttleRate);
    emit playbackChanged();
    emit changed();
}
void Editor::stopPlayback() {
    if (!m_playback->active())
        return;
    m_playback->stop();
    if (!m_busy) {
        m_analysis->setPaused(false);
        m_thumbnails->setPaused(false);
    }
    emit playbackChanged();
}
void Editor::exportVideo(const QUrl &url, const QString &profile) {
    if (profile != "mpeg4" && profile != "webm" && profile != "h264") {
        fail("Unknown export profile");
        return;
    }
    exportWith(url, {{"format", profile == "webm" ? "vp9" : profile}, {"quality", "balanced"}});
}
// The folder a picture sequence chosen as "name.png" is written to: "name" beside it.
static QString sequenceFolder(const QString &output) {
    const QFileInfo file(output);
    return file.absolutePath() + "/" + file.completeBaseName();
}
static ExportSettings exportSettings(const QVariantMap &m) {
    ExportSettings s;
    s.format = m.value("format", s.format).toString();
    s.quality = m.value("quality", s.quality).toString();
    s.height = m.value("height", 0).toInt();
    s.loudness = m.value("loudness", 0).toDouble();
    s.fps = m.value("fps", 0).toDouble();
    s.bitrate = m.value("bitrate", 0).toInt();
    const auto captions = m.value("captions").toString();
    if (!QStringList{"", "srt", "vtt"}.contains(captions))
        throw std::runtime_error("Captions beside the video are SRT or VTT");
    s.channels = m.value("channels", 2).toInt();
    s.sampleRate = m.value("sampleRate", 48000).toInt();
    if (s.channels != 1 && s.channels != 2)
        throw std::runtime_error("Export sound in mono (1) or stereo (2)");
    if (s.sampleRate != 48000 && s.sampleRate != 44100)
        throw std::runtime_error("Export sound at 48000 or 44100 Hz");
    if (s.fps != 0 && std::none_of(exportFrameRates().begin(), exportFrameRates().end(),
                                   [&](const auto &r) { return std::abs(r.second - s.fps) < 0.01; }))
        throw std::runtime_error("Unknown export frame rate");
    if (s.bitrate != 0 && (s.bitrate < 200 || s.bitrate > 400000))
        throw std::runtime_error("Bitrate must be between 200 and 400000 kbit/s");
    if (s.format == "gif")
        s.fps = 0, s.bitrate = 0; // GIF has its own frame rate and no bitrate
    if (s.loudness != 0 && (s.loudness < -36 || s.loudness > -6))
        throw std::runtime_error("Loudness target must be between -36 and -6 LUFS");
    if (!exportFormats().contains(s.format))
        throw std::runtime_error("Unknown export format");
    if (!QStringList{"max", "high", "balanced", "small"}.contains(s.quality))
        throw std::runtime_error("Unknown export quality");
    if (s.height != 0 && (s.height < 144 || s.height > 4320))
        throw std::runtime_error("Export height must be between 144 and 4320 pixels");
    return s;
}
QVariantMap Editor::exportPreview(const QVariantMap &settings) const {
    try {
        const auto s = exportSettings(settings);
        const auto size = exportSize(m_project, s.height);
        return {{"width", size.width()},
                {"height", size.height()},
                {"extension", formatExtension(s.format)},
                {"audio", audioFormat(s.format)}};
    } catch (const std::exception &) {
        return {};
    }
}
void Editor::queueExport(const QUrl &url, const QVariantMap &settings) {
    try {
        if (!m_nest.isEmpty())
            throw std::runtime_error("Go back to the main timeline to export");
        const auto output = localPath(url);
        const auto s = exportSettings(settings);
        if (QFileInfo::exists(output))
            throw std::runtime_error("That output file already exists. Choose a new filename.");
        // A picture sequence goes into a new folder named like the file.
        if (s.format == "png" && QFileInfo::exists(sequenceFolder(output)))
            throw std::runtime_error("A folder of that name already exists. Choose a new filename.");
        if (QFileInfo(output).suffix().compare(formatExtension(s.format), Qt::CaseInsensitive))
            throw std::runtime_error(
                ("Use a ." + formatExtension(s.format) + " filename for this format").toStdString());
        for (const auto &q : m_queue)
            if (q.status == "waiting" && localPath(q.url) == output)
                throw std::runtime_error("That file is in the queue already");
        m_queue.push_back({url, settings, m_project});
        m_queuePaused = false;
        m_status = "Added to the export queue: " + QFileInfo(output).fileName();
        advanceQueue();
        emit changed();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::removeQueued(int index) {
    if (index < 0 || index >= m_queue.size() || m_queue[index].status == "exporting")
        return;
    m_queue.remove(index);
    emit changed();
}
void Editor::startQueue() {
    m_queuePaused = false;
    advanceQueue();
    emit changed();
}
void Editor::advanceQueue() {
    // The running export finished: its file tells whether it worked.
    for (auto &q : m_queue)
        if (q.status == "exporting" && !m_busy) {
            const auto path = localPath(q.url);
            if (QFileInfo::exists(q.settings.value("format") == "png" ? sequenceFolder(path) : path))
                q.status = "done";
            else if (m_cancelled) {
                q.status = "cancelled";
                m_queuePaused = true;
            } else
                q.status = "failed";
        }
    if (m_busy || m_queuePaused) {
        if (m_busy && !m_queueTimer.isActive())
            m_queueTimer.start();
        return;
    }
    for (auto &q : m_queue)
        if (q.status == "waiting") {
            q.status = "exporting";
            exportProject(q.project, q.url, q.settings);
            if (!m_busy)
                q.status = "failed";
            else
                m_queueTimer.start();
            if (q.status == "failed")
                continue; // try the next one
            return;
        }
    m_queueTimer.stop();
}
void Editor::exportFrame(const QUrl &url) {
    try {
        const auto output = localPath(url);
        const auto suffix = QFileInfo(output).suffix().toLower();
        if (!QStringList{"png", "jpg", "jpeg"}.contains(suffix))
            throw std::runtime_error("Use a .png or .jpg filename");
        if (m_project.clips.empty())
            throw std::runtime_error("The timeline is empty");
        if (m_frameProcess)
            return;
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/frame-XXXXXX");
        if (!work->isValid())
            throw std::runtime_error("Cannot create a work folder");
        RenderOptions options;
        options.audio = false;
        options.highQuality = true;
        options.pixelFormat = "rgb24";
        options.from = std::clamp<qint64>(m_playhead, 0, std::max<qint64>(0, m_project.duration() - 1));
        options.to = options.from + 1;
        addAiMedia(options);
        const auto plan = compileRender(viewable(), work->path(), m_project.width, m_project.height, options);
        const auto graph = work->filePath("graph.txt");
        writeGraph(graph, plan.graph);
        auto *p = new QProcess(this);
        m_frameProcess = p;
        m_status = "Saving the picture…";
        auto png = std::make_shared<QByteArray>();
        connect(p, &QProcess::readyReadStandardOutput, this, [p, png] { *png += p->readAllStandardOutput(); });
        auto complete = [this, p, png, work, output, suffix](bool success) {
            *png += p->readAllStandardOutput();
            p->deleteLater();
            m_frameProcess = nullptr;
            QImage image;
            QSaveFile file(output);
            if (!success || !image.loadFromData(*png, "PNG") || !file.open(QIODevice::WriteOnly) ||
                !image.save(&file, suffix == "png" ? "PNG" : "JPG", suffix == "png" ? -1 : 95) ||
                !file.commit())
                return fail("Cannot save the picture to " + output);
            m_status = "Picture saved: " + output;
            emit changed();
        };
        connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
            complete(code == 0 && status == QProcess::NormalExit);
        });
        connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                complete(false);
        });
        p->start(executable("ffmpeg"), renderArguments(plan, graph, {}, "", 0));
        emit changed();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::extractAudio(const QUrl &url) {
    try {
        const auto *c = m_project.clip(m_selected);
        const auto *a = c ? m_project.asset(c->assetId) : nullptr;
        if (!c || !a || !a->hasAudio || c->muted || c->volume <= 0)
            throw std::runtime_error("Select a clip that plays sound");
        const auto suffix = QFileInfo(localPath(url)).suffix().toLower();
        if (!audioFormat(suffix))
            throw std::runtime_error("Use a .wav, .mp3 or .m4a filename");
        // The clip alone, at the start of a one-track timeline.
        auto p = m_project;
        auto one = *c;
        one.start = 0;
        one.track = 0;
        one.link.clear();
        one.group.clear();
        one.transition.clear();
        one.transitionFrames = 0;
        p.clips = {one};
        p.tracks = 1;
        p.trackSettings = {Track{"Track 1"}};
        p.markers.clear();
        p.inPoint = p.outPoint = -1;
        exportProject(p, url, {{"format", suffix}, {"quality", "max"}});
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::convertAsset(const QString &assetId, const QUrl &url, const QVariantMap &settings) {
    try {
        const auto *a = m_project.asset(assetId);
        if (!a || a->kind == "image" || a->isNested())
            throw std::runtime_error("Choose a video or sound file to convert");
        Project p;
        p.name = QFileInfo(a->path).completeBaseName();
        p.assets = {*a};
        if (a->width > 0 && a->height > 0) {
            p.width = std::max(64, a->width / 2 * 2);
            p.height = std::max(64, a->height / 2 * 2);
        } else {
            p.width = m_project.width;
            p.height = m_project.height;
        }
        // The file's own frame rate, with the usual NTSC rates kept exact.
        p.fpsN = m_project.fpsN;
        p.fpsD = m_project.fpsD;
        if (a->frameRate > 0) {
            for (const auto &[n, d] : {std::pair{24000, 1001}, {30000, 1001}, {60000, 1001}})
                if (std::abs(a->frameRate - double(n) / d) < 0.01) {
                    p.fpsN = n;
                    p.fpsD = d;
                }
            if (p.fpsD == m_project.fpsD && p.fpsN == m_project.fpsN &&
                std::abs(a->frameRate - double(p.fpsN) / p.fpsD) >= 0.01) {
                p.fpsN = std::clamp(int(std::lround(a->frameRate)), 1, 120);
                p.fpsD = 1;
            }
        }
        Clip c;
        c.id = newId();
        c.assetId = a->id;
        c.name = a->name;
        c.duration = std::max(qint64(1), qint64(std::floor(a->duration * p.fpsN / p.fpsD + 1e-6)));
        p.clips = {c};
        auto all = settings;
        all["range"] = "all";
        exportProject(p, url, all);
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::exportTimeline(const QUrl &url) {
    try {
        const auto output = localPath(url);
        const auto suffix = QFileInfo(output).suffix().toLower();
        if (suffix != "otio" && suffix != "edl")
            throw std::runtime_error("Use a .otio or .edl filename");
        const auto project = wholeProject();
        if (project.clips.empty())
            throw std::runtime_error("The timeline is empty");
        QStringList lost;
        const QByteArray data = suffix == "otio"
                                    ? QJsonDocument(otioTimeline(project, &lost)).toJson()
                                    : cmxEdl(project, &lost).toUtf8();
        QSaveFile file(output);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
            throw std::runtime_error("Cannot write " + output.toStdString());
        m_status = "Timeline exported to " + QFileInfo(output).fileName() +
                   (lost.isEmpty() ? QString() : ". Not carried: " + lost.join("; "));
        emit changed();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::exportWith(const QUrl &url, const QVariantMap &settings) {
    exportProject(m_project, url, settings);
}
void Editor::exportProject(const Project &project, const QUrl &url, const QVariantMap &settings,
                           bool retry) {
    if (m_busy)
        return;
    if (!retry)
        m_failedEncoders.clear();
    m_exportFailed = false;
    try {
        if (!m_nest.isEmpty())
            throw std::runtime_error("Go back to the main timeline to export");
        for (const auto &a : project.assets)
            if (a.isNested() && !QFileInfo::exists(a.path))
                throw std::runtime_error("Wait until the nested sequences are rendered");
        const auto output = localPath(url);
        const auto s = exportSettings(settings);
        if (QFileInfo::exists(output))
            throw std::runtime_error("That output file already exists. Choose a new filename.");
        if (s.format == "png" && QFileInfo::exists(sequenceFolder(output)))
            throw std::runtime_error("A folder of that name already exists. Choose a new filename.");
        if (QFileInfo(output).suffix().compare(formatExtension(s.format), Qt::CaseInsensitive))
            throw std::runtime_error(
                ("Use a ." + formatExtension(s.format) + " filename for this format").toStdString());
        if (project.clips.empty())
            throw std::runtime_error("The timeline is empty");
        // The in/out range, or the whole timeline.
        const auto range = settings.value("range", "all").toString();
        if (range != "all" && range != "inout")
            throw std::runtime_error("Unknown export range");
        m_exportFrom = 0;
        m_exportTo = -1;
        if (range == "inout") {
            if (project.inPoint < 0 && project.outPoint < 0)
                throw std::runtime_error("Set an in or out point first (I / O)");
            m_exportFrom = std::max<qint64>(0, project.inPoint);
            m_exportTo = project.outPoint >= 0 ? std::min(project.outPoint, project.duration())
                                                 : -1;
            if (m_exportFrom >= (m_exportTo >= 0 ? m_exportTo : project.duration()))
                throw std::runtime_error("The in/out range is outside the timeline");
        }
        const auto size = exportSize(project, s.height);
        const double fps = s.fps > 0 ? s.fps : double(project.fpsN) / project.fpsD;
        m_exportProject = project;
        m_lastExportUrl = url;
        m_lastExportSettings = settings;
        auto candidates = encoderCandidates(s, size, fps);
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                        [&](const Encoder &e) {
                                            return m_failedEncoders.contains(e.name);
                                        }),
                         candidates.end());
        m_resumeTimer.stop();
        stopPlayback();
        m_busy = true;
        m_cancelled = false;
        m_progress = 0;
        m_loudness.clear();
        m_status = "Choosing an encoder…";
        emit changed();
        // Hardware encoders are tried first; each is proven with a short test encode.
        m_encoders->resolve(candidates, size, fps,
                            [this, output, size, format = s.format,
                             loudness = s.loudness](const Encoder *e) {
                                if (m_cancelled || !m_busy) {
                                    m_busy = false;
                                    emit changed();
                                    return;
                                }
                                if (!e) {
                                    m_busy = false;
                                    m_status = "Export failed";
                                    m_exportFailed = true;
                                    fail(QString(m_failedEncoders.isEmpty()
                                                     ? "No "
                                                     : "The export failed, and no other ") +
                                         format.toUpper() +
                                         " encoder works on this computer. Choose AV1, VP9 or "
                                         "ProRes, which always work.");
                                    return;
                                }
                                if (loudness != 0 && !e->noAudio)
                                    measureLoudness(output, size, *e, loudness);
                                else
                                    startRender(output, size, *e);
                            });
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::analyzeLoudness() {
    if (m_loudnessProcess || m_project.clips.empty())
        return;
    try {
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/loudness-XXXXXX");
        if (!work->isValid())
            throw std::runtime_error("Cannot create a work folder");
        RenderOptions options;
        options.video = false;
        options.measureLoudness = true;
        const auto plan = compileRender(viewable(), work->path(), 320, 180, options);
        const auto graph = work->filePath("measure.txt");
        writeGraph(graph, plan.graph);
        auto *p = new QProcess(this);
        m_loudnessProcess = p;
        const auto revision = m_revision;
        m_mixLoudness = {{"status", "measuring"}};
        auto log = std::make_shared<QByteArray>();
        connect(p, &QProcess::readyReadStandardError, this, [p, log] {
            *log += p->readAllStandardError();
            if (log->size() > 64000)
                *log = log->right(32000);
        });
        auto complete = [this, p, log, work, revision](bool success) {
            *log += p->readAllStandardError();
            p->deleteLater();
            m_loudnessProcess = nullptr;
            const auto text = QString::fromUtf8(*log);
            const double integrated = parseIntegratedLoudness(text), peak = parseTruePeak(text);
            if (!success || std::isnan(integrated))
                m_mixLoudness = {{"status", "failed"}};
            else
                m_mixLoudness = {{"status", "ready"},
                                 {"integrated", std::isinf(integrated) ? -70. : integrated},
                                 {"peak", std::isnan(peak) || std::isinf(peak) ? -90. : peak},
                                 {"revision", revision}};
            emit changed();
        };
        connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
            complete(code == 0 && status == QProcess::NormalExit);
        });
        connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                complete(false);
        });
        p->start(executable("ffmpeg"), measureArguments(plan, graph));
        emit changed();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::measureLoudness(const QString &output, QSize size, const Encoder &encoder,
                             double target) {
    try {
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/render-XXXXXX");
        if (!work->isValid())
            throw std::runtime_error("Cannot create render folder");
        RenderOptions options;
        options.video = false;
        options.measureLoudness = true;
        options.from = m_exportFrom;
        options.to = m_exportTo;
        const auto plan = compileRender(m_exportProject, work->path(), size.width(), size.height(),
                                        options);
        const auto graph = work->filePath("measure.txt");
        writeGraph(graph, plan.graph);
        auto *process = new QProcess(this);
        m_job = process;
        m_busy = true;
        m_cancelled = false;
        m_progress = 0;
        m_status = "Measuring loudness…";
        auto log = std::make_shared<QByteArray>();
        connect(process, &QProcess::readyReadStandardError, this, [process, log] {
            *log += process->readAllStandardError();
            if (log->size() > 64000)
                *log = log->right(32000);
        });
        connect(process, &QProcess::readyReadStandardOutput, this,
                [this, process, duration = plan.duration] {
                    for (const auto &line : process->readAllStandardOutput().split('\n'))
                        if (line.startsWith("out_time_us="))
                            m_progress =
                                std::clamp(line.mid(12).toDouble() / 1e6 / duration, 0., 1.);
                    emit changed();
                });
        auto complete = [this, process, work, output, size, encoder, target, log](bool success) {
            *log += process->readAllStandardError();
            m_job = nullptr;
            m_busy = false;
            process->deleteLater();
            if (m_cancelled) {
                m_status = "Export cancelled";
                emit changed();
                return;
            }
            const double measured = parseIntegratedLoudness(QString::fromUtf8(*log));
            if (!success || std::isnan(measured)) {
                m_status = "Export failed";
                return fail("Loudness measurement failed. " + QString::fromUtf8(*log).right(2000));
            }
            // Silence (or near silence) stays as it is; otherwise one gain for the whole mix,
            // within reason, with the limiter catching the peaks it creates.
            const double gain = measured < -60 ? 0 : std::clamp(target - measured, -30., 30.);
            m_loudness = {{"measured", measured}, {"target", target}, {"gain", gain}};
            startRender(output, size, encoder, gain);
        };
        connect(process, &QProcess::finished, this,
                [complete](int code, QProcess::ExitStatus status) {
                    complete(code == 0 && status == QProcess::NormalExit);
                });
        connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                complete(false);
        });
        process->start(executable("ffmpeg"), measureArguments(plan, graph));
        emit changed();
    } catch (const std::exception &e) {
        m_busy = false;
        fail(e.what());
    }
}
// The visible titles and captions of `p` between frames `from` and `to` (-1: the end) as a
// subtitle file ("srt", "vtt" or "ass"), timed from `from`. Throws when there are none.
static QString subtitleText(const Project &p, const QString &format, qint64 from, qint64 to) {
    auto clips = p.clips;
    std::stable_sort(clips.begin(), clips.end(),
                     [](const Clip &a, const Clip &b) { return a.start < b.start; });
    const qint64 end = to < 0 ? p.duration() : to;
    auto seconds = [&](qint64 frame) { return frameTime(frame - from, p.fpsN, p.fpsD).seconds(); };
    QVector<Cue> cues;
    const Clip *first = nullptr;
    for (const auto &c : clips)
        if (c.assetId.isEmpty() && c.effect.isEmpty() && c.graphic.isEmpty() && !c.hidden &&
            !p.trackSettings[c.track].hidden && c.start < end && c.start + c.duration > from) {
            cues.push_back({seconds(std::max(c.start, from)),
                            seconds(std::min(c.start + c.duration, end)), c.text});
            if (!first)
                first = &c;
        }
    if (cues.isEmpty())
        throw std::runtime_error("No visible titles/captions to export");
    // The ASS default style follows the first caption's font and size.
    return writeSubtitles(cues, format, p.width, p.height,
                          first->fontFamily.isEmpty() ? QString("Arial") : first->fontFamily,
                          first->fontSize);
}
static void writeText(const QString &path, const QString &text) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot write the captions");
    const auto data = text.toUtf8();
    if (f.write(data) != data.size() || !f.commit())
        throw std::runtime_error("Cannot save the captions");
}
void Editor::startRender(const QString &output, QSize size, const Encoder &encoder,
                         double gainDb) {
    try {
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/render-XXXXXX");
        if (!work->isValid())
            throw std::runtime_error("Cannot create render folder");
        RenderOptions options;
        options.highQuality = true;
        options.pixelFormat = encoder.pixelFormat;
        options.videoTail = encoder.videoTail;
        options.transparent = encoder.alpha;
        options.video = !encoder.audioOnly;
        options.audio = !encoder.noAudio;
        options.from = m_exportFrom;
        options.to = m_exportTo;
        if (gainDb != 0 || m_loudness.contains("target")) {
            // Normalised exports keep peaks about 1 dB below full scale, as streaming services
            // expect; the limiter works on samples, so leave some room for true peaks.
            options.gainDb = gainDb;
            options.limit = 0.84;
        }
        addAiMedia(options);
        const auto plan =
            compileRender(m_exportProject, work->path(), size.width(), size.height(), options);
        const auto graph = work->filePath("graph.txt");
        writeGraph(graph, plan.graph);
        const QString temp = QFileInfo(output).absolutePath() + "/.cutlery-" + newId() +
                             (encoder.sequence ? QString() : "." + encoder.extension);
        // A sequence is written into a work folder that becomes the output folder at the end.
        const QString target = encoder.sequence ? sequenceFolder(output) : output;
        if (encoder.sequence && !QDir().mkpath(temp))
            throw std::runtime_error("Cannot write to the output folder");
        const QString written =
            encoder.sequence ? temp + "/" + QFileInfo(target).fileName() + "_%05d.png" : temp;
        m_resumeTimer.stop();
        stopPlayback();
        auto *process = new QProcess(this);
        m_job = process;
        m_jobTemp = temp;
        m_busy = true;
        m_analysis->setPaused(true);
        m_thumbnails->setPaused(true);
        m_cancelled = false;
        m_progress = 0;
        m_status = "Exporting with " + encoder.label + "…";
        if (m_preview) {
            m_preview->disconnect(this);
            m_preview->kill();
            m_preview->deleteLater();
            m_preview = nullptr;
        }
        auto log = std::make_shared<QByteArray>();
        auto pending = std::make_shared<QByteArray>();
        connect(process, &QProcess::readyReadStandardError, this, [process, log] {
            *log += process->readAllStandardError();
            if (log->size() > 16000)
                *log = log->right(16000);
        });
        connect(process, &QProcess::readyReadStandardOutput, this,
                [this, process, pending, duration = plan.duration] {
                    *pending += process->readAllStandardOutput();
                    int i;
                    while ((i = pending->indexOf('\n')) >= 0) {
                        const auto line = pending->left(i);
                        pending->remove(0, i + 1);
                        if (line.startsWith("out_time_us="))
                            m_progress =
                                std::clamp(line.mid(12).toDouble() / 1000000 / duration, 0., 1.);
                    }
                    emit changed();
                });
        auto complete = [this, process, work, output = target, temp, log, label = encoder.label,
                         name = encoder.name, probed = encoder.probe](bool success) {
            const auto discard = [temp] {
                if (!QFile::remove(temp))
                    QDir(temp).removeRecursively();
            };
            *log += process->readAllStandardError();
            m_job = nullptr;
            m_jobTemp.clear();
            m_busy = false;
            process->deleteLater();
            if (m_cancelled) {
                discard();
                m_status = "Render cancelled";
            } else if (!success) {
                discard();
                m_exportFailed = true;
                if (probed && m_failedEncoders.isEmpty()) {
                    // A hardware encoder that passed its test can still fail on the real video
                    // (drivers, memory): try once more with the next encoder.
                    m_failedEncoders << name;
                    m_analysis->setPaused(false);
                    m_thumbnails->setPaused(false);
                    exportProject(m_exportProject, m_lastExportUrl, m_lastExportSettings, true);
                    if (m_busy) {
                        m_status = label + " failed; trying the next encoder…";
                        emit changed();
                        return;
                    }
                }
                fail("Render failed. " + QString::fromUtf8(*log).right(4000));
                m_status = "Render failed";
            } else if (!QDir().rename(temp, output)) {
                discard();
                fail("Cannot publish output. Check the folder permissions and choose a new "
                     "filename.");
            } else {
                m_progress = 1;
                m_status = "Export saved (" + label + "): " + output;
                // Captions beside the video, named like it, timed like the exported range.
                const auto captions = m_lastExportSettings.value("captions").toString();
                if (!captions.isEmpty()) {
                    const QFileInfo video(output);
                    const auto path = video.absolutePath() + "/" + video.completeBaseName() + "." + captions;
                    try {
                        if (QFileInfo::exists(path))
                            throw std::runtime_error("a file of that name exists");
                        writeText(path, subtitleText(m_exportProject, captions, m_exportFrom, m_exportTo));
                        m_status += " · captions " + QFileInfo(path).fileName();
                    } catch (const std::exception &e) {
                        m_status += QString(" · no caption file: ") + e.what();
                    }
                }
                if (m_loudness.contains("measured"))
                    m_status += QString(" · loudness %1 → %2 LUFS")
                                    .arg(m_loudness["measured"].toDouble(), 0, 'f', 1)
                                    .arg(m_loudness["target"].toDouble(), 0, 'f', 0);
            }
            m_analysis->setPaused(false);
            m_thumbnails->setPaused(false);
            m_previewTimer.start();
            emit changed();
        };
        connect(process, &QProcess::finished, this,
                [complete](int code, QProcess::ExitStatus status) {
                    complete(code == 0 && status == QProcess::NormalExit);
                });
        connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                complete(false);
        });
        process->start(executable("ffmpeg"), exportArguments(plan, graph, written, encoder));
        emit changed();
    } catch (const std::exception &e) {
        m_busy = false;
        m_analysis->setPaused(false);
        m_thumbnails->setPaused(false);
        fail(e.what());
    }
}
void Editor::retryExport() {
    if (!m_exportFailed || m_busy || m_lastExportUrl.isEmpty())
        return;
    exportProject(m_exportProject, m_lastExportUrl, m_lastExportSettings);
}
void Editor::cancelJob() {
    if (m_encoders->busy()) {
        m_encoders->cancel();
        m_cancelled = true;
        m_busy = false;
        m_status = "Export cancelled";
        emit changed();
    }
    if (m_job) {
        m_cancelled = true;
        m_job->kill();
    }
}
void Editor::importSrt(const QUrl &url) {
    try {
        const auto path = localPath(url);
        const auto suffix = QFileInfo(path).suffix().toLower();
        const auto cues = parseSubtitles(readUtf8File(path), suffix);
        // Plain text has no timing; its captions start at the playhead.
        const qint64 offset = suffix == "txt" ? m_playhead : 0;
        mutate([&](Project &p) {
            if (p.trackSettings[p.tracks - 1].magnetic)
                throw std::runtime_error("Turn off Magnet on the caption track before importing "
                                         "captions to preserve their timing");
            for (const auto &cue : cues) {
                Clip c;
                c.id = newId();
                c.name = "Caption";
                c.track = p.tracks - 1;
                p.requireEditable(c.track);
                c.start = offset + qRound64(cue.start * p.fpsN / p.fpsD);
                const auto end = offset + qRound64(cue.end * p.fpsN / p.fpsD);
                c.duration = end - c.start;
                c.text = cue.text;
                c.fontSize = 48;
                c.y = .32;
                if (c.duration <= 0 || c.text.isEmpty())
                    throw std::runtime_error("Empty or reversed caption cue");
                p.clips.push_back(c);
            }
            if (cues.isEmpty())
                throw std::runtime_error("No captions found");
        });
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
bool Editor::exportSrt(const QUrl &url) {
    try {
        const auto path = localPath(url);
        const auto suffix = QFileInfo(path).suffix().toLower();
        const QString format = suffix == "vtt" ? "vtt" : suffix == "ass" ? "ass" : "srt";
        writeText(path, subtitleText(m_project, format, 0, -1));
        m_status = format.toUpper() + " saved";
        emit changed();
        return true;
    } catch (const std::exception &e) {
        fail(e.what());
        return false;
    }
}
} // namespace cutlery
