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
                              {"locked", m_project.trackSettings[c.track].locked}};
    }
    return result;
}
QVariantList Editor::trackList() const {
    QVariantList result;
    int index = 0;
    for (const auto &t : m_project.trackSettings) {
        result << QVariantMap{{"index", index++}, {"name", t.name},     {"locked", t.locked},
                              {"muted", t.muted}, {"hidden", t.hidden}, {"solo", t.solo}};
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
                        {"textColor", c.textColor}};
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
#undef PROP
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
            {"analyzing", m_analysis->busy()},
            {"progress", m_progress},
            {"previewUrl", m_previewUrl},
            {"playbackUrl", m_playbackUrl},
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
    m_playbackUrl.clear();
    m_playhead = std::clamp(m_playhead, qint64(0), std::max(qint64(0), m_project.duration() - 1));
    if (!m_project.clip(m_selected))
        m_selected.clear();
    m_previewTimer.start();
    m_saveTimer.start();
    m_analysis->setAssets(m_project.assets);
    emit projectChanged();
    emit changed();
}
void Editor::mutate(const std::function<void(Project &)> &fn) {
    try {
        auto next = m_project;
        fn(next);
        next.validate();
        if (next.json() == m_project.json())
            return;
        m_undo.push_back(m_project);
        if (m_undo.size() > 60)
            m_undo.removeFirst();
        m_redo.clear();
        m_project = std::move(next);
        m_error.clear();
        edited();
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
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
    m_playbackUrl.clear();
    ++m_revision;
    m_status = "New project";
    m_analysis->setAssets(m_project.assets);
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
        m_playbackUrl.clear();
        m_previewUrl.clear();
        m_status = "Opened " + QFileInfo(path).fileName();
        m_previewTimer.start();
        m_analysis->setAssets(m_project.assets);
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
    m_importQueue += urls;
    if (!m_probe)
        probeNext();
}
void Editor::probeNext() {
    if (m_importQueue.empty()) {
        m_importing = false;
        emit changed();
        return;
    }
    const auto url = m_importQueue.takeFirst();
    probeFile(url, {});
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
void Editor::probeFile(const QUrl &url, const QString &replaceId) {
    QString path;
    try {
        path = localPath(url);
        if (!QFileInfo(path).isFile())
            throw std::runtime_error("Media file does not exist");
    } catch (const std::exception &e) {
        fail(e.what());
        probeNext();
        return;
    }
    auto *process = new QProcess(this);
    m_probe = process;
    m_importing = true;
    m_status = "Reading " + QFileInfo(path).fileName();
    emit changed();
    auto complete = [this, process, path, replaceId](bool success) {
        const auto bytes = process->readAllStandardOutput();
        const auto error = QString::fromUtf8(process->readAllStandardError());
        m_probe = nullptr;
        process->deleteLater();
        if (!success)
            fail("Cannot read media: " + QFileInfo(path).fileName() + "\n" + error.left(2000));
        else
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
                mutate([&](Project &p) {
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
                });
                m_status = replaceId.isEmpty() ? "Imported " + a.name : "Media relinked";
            } catch (const std::exception &e) {
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
    const auto newClipId = newId();
    mutate([&](Project &p) {
        p.requireEditable(track);
        const auto *a = p.asset(id);
        if (!a)
            throw std::runtime_error("Media no longer exists");
        Clip c;
        c.id = newClipId;
        c.assetId = id;
        c.name = a->name;
        c.track = track;
        for (const auto &clip : p.clips)
            if (clip.track == track)
                c.start = std::max(c.start, clip.start + clip.duration);
        c.duration = std::max(qint64(1), qint64(std::floor(a->duration * p.fpsN / p.fpsD + 1e-6)));
        p.clips.push_back(c);
    });
    select(newClipId);
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
    });
    select(id);
}
void Editor::moveClip(const QString &id, qint64 frame, int track) {
    mutate([&](Project &p) {
        if (auto *c = p.clip(id)) {
            p.requireEditable(c->track);
            p.requireEditable(track);
            c->start = std::max(qint64(0), frame);
            c->track = track;
        }
    });
}
void Editor::setClip(const QString &key, const QVariant &v) {
    mutate([&](Project &p) {
        auto *c = p.clip(m_selected);
        if (!c)
            return;
        p.requireEditable(c->track);
        if (key == "track")
            p.requireEditable(v.toInt());
        if (key == "start")
            c->start = v.toLongLong();
        else if (key == "duration")
            c->duration = v.toLongLong();
        else if (key == "track")
            c->track = v.toInt();
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
            c->duration = std::max(
                qint64(1), qint64(std::floor(c->duration * old.seconds() / c->speed.seconds())));
        } else if (key == "text")
            c->text = v.toString();
        else if (key == "fontSize")
            c->fontSize = v.toInt();
        else if (key == "textColor")
            c->textColor = v.toString();
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
#undef FIELD
    });
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
            copy.start += copy.duration;
            p.clips.push_back(copy);
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
    if (m_project.clips.empty() || m_busy) {
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
        int w = 640, h = qRound(640. * m_project.height / m_project.width / 2) * 2;
        if (h > 640) {
            h = 640;
            w = qRound(640. * m_project.width / m_project.height / 2) * 2;
        }
        w = std::max(64, w);
        h = std::max(64, h);
        const auto plan = compileRender(m_project, work->path(), w, h, false);
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
            renderArguments(plan, graph, {}, "",
                            frameTime(m_playhead, m_project.fpsN, m_project.fpsD).seconds()));
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::renderPlayback() {
    if (!m_busy)
        startRender(m_data + "/cache/playback-" + newId() + ".mp4", "mpeg4", true);
}
void Editor::exportVideo(const QUrl &url, const QString &profile) {
    try {
        if (!m_busy)
            startRender(localPath(url), profile, false);
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::startRender(const QString &output, const QString &profile, bool playback) {
    if (QFileInfo::exists(output)) {
        fail("That output file already exists. Choose a new filename.");
        return;
    }
    if (profile != "mpeg4" && profile != "webm" && profile != "h264") {
        fail("Unknown export profile");
        return;
    }
    try {
        auto work = std::make_shared<QTemporaryDir>(m_data + "/cache/render-XXXXXX");
        if (!work->isValid())
            throw std::runtime_error("Cannot create render folder");
        int w = m_project.width, h = m_project.height;
        if (playback) {
            w = 960;
            h = std::max(64, qRound(960. * m_project.height / m_project.width / 2) * 2);
            if (h > 960) {
                h = 960;
                w = std::max(64, qRound(960. * m_project.width / m_project.height / 2) * 2);
            }
        }
        const auto plan = compileRender(m_project, work->path(), w, h);
        const auto graph = work->filePath("graph.txt");
        writeGraph(graph, plan.graph);
        const QString temp = QFileInfo(output).absolutePath() + "/.cutlery-" + newId() + "." +
                             (profile == "webm" ? "webm" : "mp4");
        auto *process = new QProcess(this);
        m_job = process;
        m_jobTemp = temp;
        m_busy = true;
        m_analysis->setPaused(true);
        m_cancelled = false;
        m_progress = 0;
        m_status = playback ? "Rendering playback cache…" : "Exporting…";
        if (m_preview) {
            m_preview->disconnect(this);
            m_preview->kill();
            m_preview->deleteLater();
            m_preview = nullptr;
        }
        const auto revision = m_revision;
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
        auto complete = [this, process, work, output, temp, playback, revision, log](bool success) {
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
            } else if (playback && revision != m_revision) {
                QFile::remove(output);
                m_status = "Timeline changed. Render playback again.";
            } else {
                m_progress = 1;
                m_status = playback ? "Playback ready — press Play" : "Export saved: " + output;
                if (playback)
                    m_playbackUrl = QUrl::fromLocalFile(output).toString();
            }
            m_analysis->setPaused(false);
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
        process->start(executable("ffmpeg"), renderArguments(plan, graph, temp, profile));
        emit changed();
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
void Editor::cancelJob() {
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
