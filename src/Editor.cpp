#include "Editor.h"
#include "RenderGraph.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>
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
    m_analysis = new MediaAnalysis(m_data + "/cache/waveforms", executable("ffmpeg"), this);
    m_thumbnails = new Thumbnails(m_data + "/cache/thumbnails", executable("ffmpeg"), this);
    m_encoders = new EncoderResolver(executable("ffmpeg"), this);
    QString worker = qEnvironmentVariable("CUTLERY_MATTE_WORKER"),
            model = qEnvironmentVariable("CUTLERY_MATTE_MODEL");
    if (worker.isEmpty()) {
        worker = app + "/cutlery-matte";
#ifdef Q_OS_WIN
        worker += ".exe";
#endif
    }
    if (model.isEmpty())
        model = app + "/models/u2net_human_seg.onnx";
    m_mattes = new Mattes(m_data + "/mattes", executable("ffmpeg"), worker, model, this);
    connect(m_mattes, &Mattes::changed, this, [this] {
        if (!m_mattes->busy())
            m_previewTimer.start(); // a finished matte changes the picture
        emit changed();
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
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(800);
    connect(&m_saveTimer, &QTimer::timeout, this, &Editor::autosave);
    if (executable("ffmpeg").isEmpty() || executable("ffprobe").isEmpty())
        m_error = "FFmpeg/ffprobe not found. Use the portable package, or add both tools to PATH.";
}
Editor::~Editor() {
    if (m_dirty)
        autosave();
    for (auto *p : {m_preview, m_job, m_probe})
        if (p) {
            p->disconnect(this);
            p->kill();
            p->waitForFinished(1500);
        }
    if (!m_jobTemp.isEmpty())
        QFile::remove(m_jobTemp);
}
QVariantList Editor::assets() const {
    QVariantList result;
    for (const auto &a : m_project.assets)
        result << QVariantMap{
            {"id", a.id},     {"name", a.name},        {"path", a.path},
            {"kind", a.kind}, {"seconds", a.duration}, {"missing", !QFileInfo::exists(a.path)}};
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
                              {"name", c.name},
                              {"track", c.track},
                              {"start", c.start},
                              {"duration", c.duration},
                              {"title", c.assetId.isEmpty()},
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
            if (const auto *a = m_project.asset(c.assetId); a && a->kind != "image") {
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
    mutate([&](Project &p) { p.trim(id, start, end); });
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
                        {"audioOnly", c.audioOnly},
                        {"locked", m_project.trackSettings[c.track].locked},
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
                        {"transition", c.transition},
                        {"transitionFrames", c.transitionFrames},
                        {"transitionLength", m_project.transitionLength(c)},
                        {"canTransition", m_project.previousAdjacent(c) != nullptr}};
            // Animated values and keyframe state at the playhead for the inspector.
            const auto local = m_playhead - c.start;
            QVariantMap animated, keyed, counts;
            for (const auto &property : animatableProperties()) {
                const auto list = c.keyframes.value(property);
                animated[property] = c.valueAt(property, local);
                keyed[property] = std::any_of(list.begin(), list.end(),
                                              [&](const Keyframe &k) { return k.frame == local; });
                counts[property] = list.size();
            }
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
            PROP(fadeIn);
            PROP(fadeOut);
            PROP(reverse);
            PROP(flip);
            PROP(muted);
            PROP(hidden);
            PROP(shape);
            PROP(radius);
            PROP(border);
            PROP(borderColor);
            PROP(shadow);
            PROP(chromaKey);
            PROP(keyColor);
            PROP(keySimilarity);
            PROP(keyBlend);
            PROP(aiCutout);
#undef PROP
            if (const auto *a = m_project.asset(c.assetId); a && a->kind == "video") {
                auto cutout = m_mattes->status(*a);
                const auto [from, to] = cutoutSpan(*a, &c);
                const auto m = m_mattes->matte(*a);
                // The last analysed frame is held, so the end may fall short by a frame.
                cutout["covered"] = !m.path.isEmpty() && m.start <= from + 1e-3 &&
                                    m.end + 1 / Mattes::rate >= to;
                selected["cutout"] = cutout;
            }
        }
    return {{"name", m_project.name},
            {"path", m_path},
            {"dirty", m_dirty},
            {"selected", selected},
            {"selectedId", m_selected},
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
            {"importing", m_importing},
            {"analyzing", m_analysis->busy() || m_thumbnails->busy()},
            {"aiAvailable", m_mattes->available()},
            {"aiMissing", m_mattes->missing()},
            {"progress", m_progress},
            {"previewUrl", m_previewUrl},
            {"playing", m_playback->active() || m_resumeTimer.isActive()},
            {"canUndo", !m_undo.empty()},
            {"canRedo", !m_redo.empty()},
            {"hasRecovery", m_hasRecovery},
            {"dataPath", m_data},
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
        if (m_undo.size() > 60)
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
void Editor::newProject() {
    if (m_importing) {
        fail("Wait for media import to finish");
        return;
    }
    cancelJob();
    m_saveTimer.stop();
    m_project = Project{};
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
        m_path = path;
        m_undo.clear();
        m_redo.clear();
        m_selected.clear();
        m_playhead = 0;
        m_dirty = false;
        ++m_revision;
        m_resumeTimer.stop();
        stopPlayback();
        m_previewUrl.clear();
        m_status = "Opened " + QFileInfo(path).fileName();
        m_previewTimer.start();
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
        auto p = m_project;
        p.name = QFileInfo(path).completeBaseName();
        saveProject(p, path);
        m_project = std::move(p);
        m_path = path;
        m_dirty = false;
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
void Editor::autosave() {
    try {
        saveProject(m_project, m_recovery);
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
    emit changed();
}
void Editor::seek(qint64 frame) {
    m_resumeTimer.stop();
    stopPlayback();
    m_playhead = std::clamp(frame, qint64(0), std::max(qint64(0), m_project.duration() - 1));
    m_previewTimer.start();
    emit changed();
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
void Editor::importMedia(const QList<QUrl> &urls) {
    if (!m_importing)
        m_importErrors.clear();
    if (urls.size() + m_importQueue.size() > 500) {
        fail("Drop at most 500 files at a time");
        return;
    }
    for (const auto &url : urls)
        m_importQueue.push_back({url, {}});
    if (!m_probe)
        probeNext();
}
void Editor::dropFiles(const QList<QUrl> &urls, int track, qint64 frame) {
    try {
        m_project.requireEditable(track);
        if (urls.size() + m_importQueue.size() > 500)
            throw std::runtime_error("Drop at most 500 files at a time");
        if (!m_importing)
            m_importErrors.clear();
        auto batch = std::make_shared<DropBatch>();
        batch->trackId = m_project.trackSettings[track].id;
        batch->frame = std::max(qint64(0), frame);
        for (const auto &url : urls)
            m_importQueue.push_back({url, batch});
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
    probeFile(request.url, {}, request.drop);
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
void Editor::probeFile(const QUrl &url, const QString &replaceId, std::shared_ptr<DropBatch> drop) {
    QString path;
    try {
        path = localPath(url);
        if (!QFileInfo(path).isFile())
            throw std::runtime_error("Media file does not exist");
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
    auto complete = [this, process, path, replaceId, drop](bool success) {
        const auto bytes = process->readAllStandardOutput();
        const auto error = QString::fromUtf8(process->readAllStandardError());
        m_probe = nullptr;
        process->deleteLater();
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
                    QStringList{"png", "jpg", "jpeg", "bmp", "webp", "tif", "tiff"}.contains(ext);
                a.kind = still ? "image" : (a.width > 0 ? "video" : "audio");
                if (still)
                    a.duration = 5;
                if ((a.width == 0 && !a.hasAudio) || a.duration <= 0)
                    throw std::runtime_error("No supported finite video/audio stream found");
                QString addedClip, dropWarning;
                const bool imported = mutate([&](Project &p) {
                    for (const auto &clip : p.clips)
                        if (clip.assetId == replaceId)
                            p.requireEditable(clip.track);
                    if (replaceId.isEmpty())
                        p.assets.push_back(a);
                    else
                        for (auto &asset : p.assets)
                            if (asset.id == replaceId) {
                                if (asset.kind != a.kind)
                                    throw std::runtime_error(
                                        "Replacement must have the same media type");
                                asset = a;
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
void Editor::moveClip(const QString &id, qint64 frame, int track) {
    mutate([&](Project &p) { p.move(id, track, frame); });
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
        if (key == "duration")
            c->duration = v.toLongLong();
        else if (key == "sourceIn") {
            const auto t = v.toDouble();
            if (!std::isfinite(t) || t < 0 || t > 86400)
                throw std::runtime_error("Source time must be between 0 and 86400 seconds");
            c->sourceIn = Time(qRound64(t * 1000000), 1000000);
        } else if (key == "speed") {
            const auto speed = v.toDouble();
            if (!std::isfinite(speed) || speed < .25 || speed > 4)
                throw std::runtime_error("Speed must be 0.25–4x");
            auto old = c->speed;
            c->speed = Time(qRound64(speed * 1000), 1000);
            c->scaleKeyframes(old.seconds() / c->speed.seconds());
            c->duration = std::max(
                qint64(1), qint64(std::floor(c->duration * old.seconds() / c->speed.seconds())));
        } else if (key == "text")
            c->text = v.toString();
        else if (key == "fontSize")
            c->fontSize = v.toInt();
        else if (key == "textColor")
            c->textColor = v.toString();
        else if (key == "transition") {
            c->transition = v.toString();
            // New transitions start at half a second, like a typical dissolve.
            if (!c->transition.isEmpty() && c->transitionFrames < 2)
                c->transitionFrames = std::max<qint64>(2, qRound64(0.5 * p.fpsN / p.fpsD));
        } else if (key == "transitionFrames")
            c->transitionFrames = v.toLongLong();
#define FIELD(k, type) else if (key == #k) c->k = v.type()
        FIELD(scale, toDouble);
        FIELD(x, toDouble);
        FIELD(y, toDouble);
        FIELD(rotation, toDouble);
        FIELD(opacity, toDouble);
        FIELD(volume, toDouble);
        FIELD(brightness, toDouble);
        FIELD(contrast, toDouble);
        FIELD(saturation, toDouble);
        FIELD(crop, toDouble);
        FIELD(fadeIn, toDouble);
        FIELD(fadeOut, toDouble);
        FIELD(reverse, toBool);
        FIELD(flip, toBool);
        FIELD(muted, toBool);
        FIELD(hidden, toBool);
        FIELD(radius, toDouble);
        FIELD(border, toDouble);
        FIELD(shadow, toDouble);
        FIELD(chromaKey, toBool);
        FIELD(aiCutout, toBool);
        FIELD(keySimilarity, toDouble);
        FIELD(keyBlend, toDouble);
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
    if (!c || c->audioOnly || (a && a->kind == "audio"))
        return {};
    // The picture's rectangle on the canvas at the playhead, in canvas fractions.
    const double local = m_playhead - c->start;
    const double scale = c->valueAt("scale", local);
    const auto size = m_project.pictureSize(*c, m_project.width * scale,
                                            m_project.height * scale);
    return {{"x", 0.5 + c->valueAt("x", local) - size.width() / m_project.width / 2},
            {"y", 0.5 + c->valueAt("y", local) - size.height() / m_project.height / 2},
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
void Editor::split() {
    mutate([&](Project &p) { p.split(m_selected, m_playhead); });
}
void Editor::remove(bool ripple) {
    mutate([&](Project &p) { p.remove(m_selected, ripple); });
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
        addMattes(options);
        // Only the playhead frame is compiled, so the cost does not grow with its position.
        const auto plan = compileRender(m_project, work->path(), size.width(), size.height(),
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
std::pair<double, double> Editor::cutoutSpan(const Asset &a, const Clip *extra) const {
    double from = 1e300, to = -1e300;
    for (const auto &c : m_project.clips)
        if (c.assetId == a.id && (c.aiCutout || (extra && c.id == extra->id))) {
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
void Editor::addMattes(RenderOptions &options) const {
    for (const auto &c : m_project.clips)
        if (c.aiCutout && !options.mattes.contains(c.assetId))
            if (const auto *a = m_project.asset(c.assetId); a && a->kind == "video")
                if (const auto m = m_mattes->matte(*a); !m.path.isEmpty())
                    options.mattes.insert(a->id, m);
}
void Editor::analyzeCutout() {
    const auto *c = m_project.clip(m_selected);
    const auto *a = c ? m_project.asset(c->assetId) : nullptr;
    if (!a || a->kind != "video")
        return fail("AI background removal works on video clips");
    if (!m_mattes->available())
        return fail(m_mattes->missing() + " Download the AI pack next to Cutlery.exe.");
    auto [from, to] = cutoutSpan(*a, c);
    // Keep what an earlier analysis covered when it overlaps, so other clips stay cut out.
    if (const auto m = m_mattes->matte(*a); !m.path.isEmpty() && m.start <= to && m.end >= from) {
        if (m.start <= from + 1e-3 && m.end + 1 / Mattes::rate >= to)
            return;
        from = std::min(from, m.start);
        to = std::max(to, m.end);
    }
    m_mattes->analyze(*a, from, to);
}
void Editor::cancelCutout() {
    m_mattes->cancel();
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
void Editor::play() {
    m_resumeTimer.stop();
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
        RenderOptions options;
        options.from = m_playhead;
        options.realtime = true;
        options.audio = false;
        addMattes(options);
        request.video =
            compileRender(m_project, work->path(), size.width(), size.height(), options);
        options.audio = true;
        options.video = false;
        request.audio =
            compileRender(m_project, work->path(), size.width(), size.height(), options);
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
    if (m_playback->active() || m_resumeTimer.isActive())
        pause();
    else
        play();
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
static ExportSettings exportSettings(const QVariantMap &m) {
    ExportSettings s;
    s.format = m.value("format", s.format).toString();
    s.quality = m.value("quality", s.quality).toString();
    s.height = m.value("height", 0).toInt();
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
                {"extension", formatExtension(s.format)}};
    } catch (const std::exception &) {
        return {};
    }
}
void Editor::exportWith(const QUrl &url, const QVariantMap &settings) {
    if (m_busy)
        return;
    try {
        const auto output = localPath(url);
        const auto s = exportSettings(settings);
        if (QFileInfo::exists(output))
            throw std::runtime_error("That output file already exists. Choose a new filename.");
        if (QFileInfo(output).suffix().compare(formatExtension(s.format), Qt::CaseInsensitive))
            throw std::runtime_error(
                ("Use a ." + formatExtension(s.format) + " filename for this format").toStdString());
        if (m_project.clips.empty())
            throw std::runtime_error("The timeline is empty");
        const auto size = exportSize(m_project, s.height);
        const double fps = double(m_project.fpsN) / m_project.fpsD;
        m_resumeTimer.stop();
        stopPlayback();
        m_busy = true;
        m_cancelled = false;
        m_progress = 0;
        m_status = "Choosing an encoder…";
        emit changed();
        // Hardware encoders are tried first; each is proven with a short test encode.
        m_encoders->resolve(encoderCandidates(s, size, fps), size, fps,
                            [this, output, size, format = s.format](const Encoder *e) {
                                if (m_cancelled || !m_busy) {
                                    m_busy = false;
                                    emit changed();
                                    return;
                                }
                                if (!e) {
                                    m_busy = false;
                                    m_status = "Export failed";
                                    fail("No " + format.toUpper() +
                                         " encoder works on this computer. Choose AV1, VP9 or "
                                         "ProRes, which always work.");
                                    return;
                                }
                                startRender(output, size, *e);
                            });
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::startRender(const QString &output, QSize size, const Encoder &encoder) {
    try {
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/render-XXXXXX");
        if (!work->isValid())
            throw std::runtime_error("Cannot create render folder");
        RenderOptions options;
        options.highQuality = true;
        options.pixelFormat = encoder.pixelFormat;
        addMattes(options);
        const auto plan =
            compileRender(m_project, work->path(), size.width(), size.height(), options);
        const auto graph = work->filePath("graph.txt");
        writeGraph(graph, plan.graph);
        const QString temp =
            QFileInfo(output).absolutePath() + "/.cutlery-" + newId() + "." + encoder.extension;
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
        auto complete = [this, process, work, output, temp, log,
                         label = encoder.label](bool success) {
            *log += process->readAllStandardError();
            m_job = nullptr;
            m_jobTemp.clear();
            m_busy = false;
            process->deleteLater();
            if (m_cancelled) {
                QFile::remove(temp);
                m_status = "Render cancelled";
            } else if (!success) {
                QFile::remove(temp);
                fail("Render failed. " + QString::fromUtf8(*log).right(4000));
                m_status = "Render failed";
            } else if (!QFile::rename(temp, output)) {
                QFile::remove(temp);
                fail("Cannot publish output. Check the folder permissions and choose a new "
                     "filename.");
            } else {
                m_progress = 1;
                m_status = "Export saved (" + label + "): " + output;
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
        process->start(executable("ffmpeg"), exportArguments(plan, graph, temp, encoder));
        emit changed();
    } catch (const std::exception &e) {
        m_busy = false;
        m_analysis->setPaused(false);
        m_thumbnails->setPaused(false);
        fail(e.what());
    }
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
        QString text = readUtf8File(localPath(url));
        text.replace("\r", "");
        const auto blocks = text.split(QRegularExpression("\\n\\s*\\n"), Qt::SkipEmptyParts);
        const QRegularExpression stamp("(\\d{1,3}):(\\d{2}):(\\d{2})[,.](\\d{3})\\s*-->\\s*(\\d{1,"
                                       "3}):(\\d{2}):(\\d{2})[,.](\\d{3})");
        mutate([&](Project &p) {
            if (p.trackSettings[p.tracks - 1].magnetic)
                throw std::runtime_error("Turn off Magnet on the caption track before importing "
                                         "SRT to preserve caption timing");
            int count = 0;
            for (const auto &block : blocks) {
                auto m = stamp.match(block);
                if (!m.hasMatch())
                    throw std::runtime_error("Invalid SRT timing block");
                auto seconds = [&](int i) {
                    return m.captured(i).toInt() * 3600. + m.captured(i + 1).toInt() * 60. +
                           m.captured(i + 2).toInt() + m.captured(i + 3).toInt() / 1000.;
                };
                Clip c;
                c.id = newId();
                c.name = "Caption";
                c.track = p.tracks - 1;
                p.requireEditable(c.track);
                c.start = qRound64(seconds(1) * p.fpsN / p.fpsD);
                const auto end = qRound64(seconds(5) * p.fpsN / p.fpsD);
                c.duration = end - c.start;
                c.text = block.mid(m.capturedEnd()).trimmed();
                c.fontSize = 48;
                c.y = .32;
                if (c.duration <= 0 || c.text.isEmpty())
                    throw std::runtime_error("Empty or reversed SRT cue");
                p.clips.push_back(c);
                ++count;
            }
            if (!count)
                throw std::runtime_error("No SRT captions found");
        });
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
bool Editor::exportSrt(const QUrl &url) {
    try {
        const auto path = localPath(url);
        QString text;
        int i = 0;
        auto clips = m_project.clips;
        std::stable_sort(clips.begin(), clips.end(),
                         [](const Clip &a, const Clip &b) { return a.start < b.start; });
        auto stamp = [&](qint64 frame) {
            auto ms = qRound64(frameTime(frame, m_project.fpsN, m_project.fpsD).seconds() * 1000);
            return QString("%1:%2:%3,%4")
                .arg(ms / 3600000, 2, 10, QChar('0'))
                .arg(ms / 60000 % 60, 2, 10, QChar('0'))
                .arg(ms / 1000 % 60, 2, 10, QChar('0'))
                .arg(ms % 1000, 3, 10, QChar('0'));
        };
        for (const auto &c : clips)
            if (c.assetId.isEmpty() && !c.hidden && !m_project.trackSettings[c.track].hidden)
                text += QString::number(++i) + "\n" + stamp(c.start) + " --> " +
                        stamp(c.start + c.duration) + "\n" + c.text + "\n\n";
        if (!i)
            throw std::runtime_error("No visible titles/captions to export");
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            throw std::runtime_error("Cannot write SRT");
        const auto data = text.toUtf8();
        if (f.write(data) != data.size() || !f.commit())
            throw std::runtime_error("Cannot save SRT");
        m_status = "SRT saved";
        emit changed();
        return true;
    } catch (const std::exception &e) {
        fail(e.what());
        return false;
    }
}
} // namespace cutlery
