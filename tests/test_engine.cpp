#include "Editor.h"
#include "AiJobs.h"
#include "KeyboardShortcuts.h"
#include "MediaAnalysis.h"
#include "Project.h"
#include "RenderGraph.h"
#include "Thumbnails.h"
#include <QJsonArray>
#include <QPainter>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>
#include <tuple>
using namespace cutlery;
class EngineTest : public QObject {
    Q_OBJECT
    static QByteArray run(const QString &exe, const QStringList &args) {
        QProcess p;
        p.start(exe, args);
        if (!p.waitForStarted(5000))
            throw std::runtime_error(p.errorString().toStdString());
        if (!p.waitForFinished(60000)) {
            p.kill();
            p.waitForFinished();
            throw std::runtime_error("Process timed out");
        }
        if (p.exitCode() != 0 || p.exitStatus() != QProcess::NormalExit)
            throw std::runtime_error(p.readAllStandardError().toStdString());
        return p.readAllStandardOutput();
    }
    static Project sample() {
        Project p;
        Asset a;
        a.id = "source";
        a.name = "Video";
        a.path = QDir::tempPath() + "/source.mp4";
        a.kind = "video";
        a.duration = 10;
        p.assets.push_back(a);
        Clip c;
        c.id = "clip";
        c.assetId = a.id;
        c.duration = 120;
        p.clips.push_back(c);
        return p;
    }
  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
    }
    void rational() {
        QCOMPARE(frameTime(30000, 30000, 1001), Time(1001));
        QCOMPARE(Time(2, 3) * Time(3, 4), Time(1, 2));
        QCOMPARE(Time(1, 3) + Time(1, 6), Time(1, 2));
        QVERIFY_EXCEPTION_THROWN(Time(1, 0), std::invalid_argument);
        const auto max = std::numeric_limits<qint64>::max();
        QVERIFY_EXCEPTION_THROWN(Time::mul(max, 2), std::overflow_error);
        QVERIFY_EXCEPTION_THROWN(Time(max) + Time(1), std::overflow_error);
        QCOMPARE(Time::mul(-max, 1), -max);
    }
    void roundtrip() {
        QTemporaryDir dir;
        auto p = sample();
        p.assets[0].path = dir.filePath("media/café's video.mp4");
        p.clips[0].sourceIn = Time(1, 3);
        p.clips[0].speed = Time(3, 2);
        p.clips[0].text = "Hello: [cut], 100%";
        auto path = dir.filePath("story.cutlery");
        saveProject(p, path);
        auto q = loadProject(path);
        QCOMPARE(q.json(), p.json());
        QVERIFY(readUtf8File(path).contains("media/café"));
        auto bad = p.json();
        bad["schemaVersion"] = 99;
        QVERIFY_EXCEPTION_THROWN(Project::fromJson(bad, dir.path()), std::runtime_error);
        const auto original = readUtf8File(path);
        p.clips[0].duration = -1;
        QVERIFY_EXCEPTION_THROWN(saveProject(p, path), std::runtime_error);
        QCOMPARE(readUtf8File(path), original);
    }
    void splitAndRipple() {
        auto p = sample();
        p.clips[0].sourceIn = Time(1);
        p.clips[0].speed = Time(3, 2);
        QVERIFY(p.split("clip", 45));
        QCOMPARE(p.clips[0].duration, qint64(45));
        QCOMPARE(p.clips[1].sourceIn, Time(13, 4));
        p.validate();
        p = sample();
        p.clips[0].reverse = true;
        QVERIFY(p.split("clip", 45));
        QCOMPARE(p.clips[0].sourceIn, Time(5, 2));
        QCOMPARE(p.clips[1].sourceIn, Time(0));
        p.remove("clip", true);
        QCOMPARE(p.clips[0].start, qint64(0));
        p.validate();
    }
    void rejectedRanges() {
        auto p = sample();
        p.clips[0].start = std::numeric_limits<qint64>::max();
        QVERIFY_EXCEPTION_THROWN(p.validate(), std::runtime_error);
        p = sample();
        p.clips[0].duration = 301;
        QVERIFY_EXCEPTION_THROWN(p.validate(), std::runtime_error);
        p = sample();
        p.clips[0].scale = std::numeric_limits<double>::quiet_NaN();
        QVERIFY_EXCEPTION_THROWN(p.validate(), std::runtime_error);
    }
    void editHistory() {
        FrameProvider frames;
        Editor e(&frames);
        e.addTitle();
        QCOMPARE(e.project().clips.size(), 1);
        e.seek(30);
        e.split();
        QCOMPARE(e.project().clips.size(), 2);
        e.undo();
        QCOMPARE(e.project().clips.size(), 1);
        e.redo();
        QCOMPARE(e.project().clips.size(), 2);
        QTemporaryDir dir;
        QVERIFY(e.save(QUrl::fromLocalFile(dir.filePath("test.cutlery"))));
        QVERIFY(!e.state()["dirty"].toBool());
        e.setClip("duration", -4);
        QVERIFY(!e.state()["error"].toString().isEmpty());
        QCOMPARE(e.project().clips[0].duration, qint64(30));
    }
    void srtRoundtrip() {
        FrameProvider frames;
        Editor e(&frames);
        QTemporaryDir dir;
        const auto input = dir.filePath("captions.srt");
        QFile f(input);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("1\n00:00:00,500 --> 00:00:01,500\nHello, world!\n\n");
        f.close();
        e.importSrt(QUrl::fromLocalFile(input));
        QCOMPARE(e.project().clips.size(), 1);
        QCOMPARE(e.project().clips[0].start, qint64(15));
        QCOMPARE(e.project().clips[0].duration, qint64(30));
        QVERIFY(e.exportSrt(QUrl::fromLocalFile(dir.filePath("out.srt"))));
        QCOMPARE(readUtf8File(dir.filePath("out.srt")), readUtf8File(input));
        e.save(QUrl::fromLocalFile(dir.filePath("captions.cutlery")));
    }
    void remoteReferencesRejected() {
        QTemporaryDir dir;
        const auto path = dir.filePath("remote.m3u8");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("#EXTM3U\n#EXT-X-TARGETDURATION:1\n#EXTINF:1,\nhttps://example.invalid/"
                "segment.ts\n#EXT-X-ENDLIST\n");
        f.close();
        FrameProvider frames;
        Editor editor(&frames);
        editor.importMedia({QUrl::fromLocalFile(path)});
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["importing"].toBool(), 10000);
        QVERIFY(editor.project().assets.isEmpty());
        QVERIFY2(editor.state()["error"].toString().contains("whitelist"),
                 qPrintable(editor.state()["error"].toString()));
    }
    void schemaMigrationAndTracks() {
        auto p = sample();
        auto legacy = p.json();
        legacy["schemaVersion"] = 1;
        legacy.remove("trackSettings");
        auto q = Project::fromJson(legacy, QDir::tempPath());
        QCOMPARE(q.tracks, 3);
        QCOMPARE(q.trackSettings.size(), 3);
        QVERIFY(!q.trackSettings[0].locked);
        q.addTrack("Music");
        q.trackSettings[3].solo = true;
        q.trackSettings[1].hidden = true;
        const auto restored = Project::fromJson(q.json(), QDir::tempPath());
        QCOMPARE(restored.json(), q.json());
        QVERIFY(!restored.audioEnabled(0));
        QVERIFY(restored.audioEnabled(3));
        QVERIFY_EXCEPTION_THROWN(q.removeTrack(0), std::runtime_error);
        q.removeTrack(2);
        QCOMPARE(q.tracks, 3);
        QCOMPARE(q.trackSettings[2].name, QString("Music"));
        q.trackSettings[0].locked = true;
        QVERIFY_EXCEPTION_THROWN(q.split("clip", 30), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(q.remove("clip", false), std::runtime_error);
    }
    void exactTrimmingAndSnapping() {
        auto p = sample();
        p.clips[0].start = 60;
        p.clips[0].sourceIn = Time(1);
        p.clips[0].speed = Time(3, 2);
        p.trim("clip", 90, 150);
        p.validate();
        QCOMPARE(p.clips[0].sourceIn, Time(5, 2));
        QCOMPARE(p.clips[0].duration, qint64(60));
        p = sample();
        p.clips[0].reverse = true;
        p.clips[0].start = 60;
        p.clips[0].sourceIn = Time(1);
        p.clips[0].speed = Time(3, 2);
        p.trim("clip", 90, 150);
        p.validate();
        QCOMPARE(p.clips[0].sourceIn, Time(5, 2));
        QCOMPARE(p.snap(88, 4, {}, 12), qint64(90));
        QCOMPARE(p.snap(118, 4, "other", 0, 30), qint64(120));
        QCOMPARE(p.snap(88, 4, "clip", 12), qint64(88));
        QTemporaryDir dir;
        FrameProvider frames;
        Editor e(&frames);
        e.addTitle();
        const auto id = e.project().clips[0].id;
        e.trimClip(id, 15, 75);
        QCOMPARE(e.project().clips[0].start, qint64(15));
        e.undo();
        QCOMPARE(e.project().clips[0].start, qint64(0));
        e.redo();
        e.setTrack(2, "locked", true);
        const auto original = e.project().json();
        e.trimClip(id, 30, 60);
        QCOMPARE(e.project().json(), original);
        e.moveClip(id, 30, 0);
        QCOMPARE(e.project().json(), original);
        e.remove();
        QCOMPARE(e.project().json(), original);
        e.setTrack(2, "locked", false);
        e.trimClip(id, 15, 15);
        QCOMPARE(e.project().clips[0].duration, qint64(60));
        e.save(QUrl::fromLocalFile(dir.filePath("trim.cutlery")));
    }
    void magneticTracks() {
        QTemporaryDir dir;
        Project p;
        for (int i = 0; i < 3; ++i) {
            Clip c;
            c.id = QString(QChar('a' + i));
            c.text = c.id;
            c.duration = (i + 1) * 30;
            c.start = (i + 1) * (i + 1) * 30;
            p.clips.push_back(c);
        }
        Clip other;
        other.id = "other";
        other.text = "Unchanged";
        other.track = 1;
        other.start = 450;
        other.duration = 30;
        p.clips.push_back(other);
        const auto path = dir.filePath("magnetic.cutlery");
        saveProject(p, path);
        FrameProvider frames;
        Editor e(&frames);
        QVERIFY(e.openProject(QUrl::fromLocalFile(path)));
        auto clip = [&](const QString &id) {
            for (const auto &c : e.project().clips)
                if (c.id == id)
                    return c;
            return Clip{};
        };
        e.setTrack(0, "magnetic", true);
        QCOMPARE(clip("a").start, qint64(0));
        QCOMPARE(clip("b").start, qint64(30));
        QCOMPARE(clip("c").start, qint64(90));
        QCOMPARE(clip("other").start, qint64(450));
        e.moveClip("c", 0, 0);
        QCOMPARE(clip("c").start, qint64(0));
        QCOMPARE(clip("a").start, qint64(90));
        QCOMPARE(clip("b").start, qint64(120));
        e.moveClip("a", 500, 1);
        QCOMPARE(clip("a").start, qint64(500));
        QCOMPARE(clip("b").start, qint64(90));
        e.undo();
        QCOMPARE(clip("a").track, 0);
        QCOMPARE(clip("b").start, qint64(120));
        e.trimClip("c", 15, 75);
        QCOMPARE(clip("c").start, qint64(0));
        QCOMPARE(clip("c").duration, qint64(60));
        QCOMPARE(clip("a").start, qint64(60));
        e.select("a");
        e.remove();
        QCOMPARE(clip("b").start, qint64(60));
        e.select("c");
        e.duplicate();
        QCOMPARE(clip("b").start, qint64(120));
        e.setTrack(0, "snapping", false);
        QVERIFY(e.save());
        QCOMPARE(loadProject(path).json(), e.project().json());
        e.setTrack(0, "locked", true);
        const auto locked = e.project().json();
        e.moveClip("c", 400, 1);
        QCOMPARE(e.project().json(), locked);
        e.setTrack(0, "magnetic", false);
        QCOMPARE(e.project().json(), locked);
        auto legacy = p.json();
        legacy["schemaVersion"] = 2;
        const auto restored = Project::fromJson(legacy, dir.path());
        QVERIFY(restored.trackSettings[0].snapping);
        QVERIFY(!restored.trackSettings[0].magnetic);
        QCOMPARE(restored.clips[0].start, p.clips[0].start);
        e.save();
    }
    void fileDropPlacement() {
        QTemporaryDir dir;
        const auto path = dir.filePath("Drop image.png");
        QImage image(160, 90, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QVERIFY(image.save(path));
        FrameProvider frames;
        Editor e(&frames);
        // The target survives removal of a lower empty track while ffprobe is running.
        e.dropFiles({QUrl::fromLocalFile(path), QUrl::fromLocalFile(dir.filePath("missing.mp4")),
                     QUrl::fromLocalFile(path)},
                    1, 60);
        e.removeTrack(0);
        QTRY_VERIFY_WITH_TIMEOUT(!e.state()["importing"].toBool(), 15000);
        QCOMPARE(e.project().assets.size(), 2);
        QCOMPARE(e.project().clips.size(), 2);
        QCOMPARE(e.project().clips[0].track, 0);
        QCOMPARE(e.project().clips[0].start, qint64(60));
        QCOMPARE(e.project().clips[1].start, qint64(210));
        QVERIFY(e.state()["error"].toString().contains("missing.mp4"));
        e.undo();
        QCOMPARE(e.project().clips.size(), 1);
        e.redo();
        QCOMPARE(e.project().clips.size(), 2);
        e.setTrack(0, "locked", true);
        const auto before = e.project().json();
        QVERIFY(!e.insertAsset(e.project().assets.first().id, 0, 0));
        QCOMPARE(e.project().json(), before);
        // A removed destination imports safely into the library, never an unrelated track.
        e.dropFiles({QUrl::fromLocalFile(path)}, 1, 0);
        e.removeTrack(1);
        QTRY_VERIFY_WITH_TIMEOUT(!e.state()["importing"].toBool(), 15000);
        QCOMPARE(e.project().assets.size(), 3);
        QCOMPARE(e.project().clips.size(), 2);
        QVERIFY(e.state()["error"].toString().contains("destination track"));
        QVERIFY(e.save(QUrl::fromLocalFile(dir.filePath("dropped.cutlery"))));
    }
    void shortcutPreferences() {
        QTemporaryDir dir;
        const auto path = dir.filePath("shortcuts.json");
        KeyboardShortcuts keys(path);
        QCOMPARE(keys.bindings().size(), 30);
        QVERIFY(!keys.assign("play", "Ctrl+B"));
        QVERIFY(keys.error().contains("Already assigned"));
        QVERIFY(!keys.assign("play", "Ctrl+NotARealKey"));
        QVERIFY(keys.assign("split", ""));
        QVERIFY(keys.assign("play", "Ctrl+B"));
        KeyboardShortcuts loaded(path);
        QCOMPARE(loaded.bindings(), keys.bindings());
        QVERIFY(loaded.reset());
        QCOMPARE(loaded.bindings()[16].toMap()["sequence"].toString(), QString("Space"));
        KeyboardShortcuts failed(dir.path());
        QVERIFY(!failed.assign("play", "Ctrl+J"));
        QCOMPARE(failed.bindings()[16].toMap()["sequence"].toString(), QString("Space"));
    }
    void thumbnailStrips() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        const auto video = dir.filePath("colours.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i",
                     "color=red:s=160x90:r=30:d=1[a];color=lime:s=160x90:r=30:d=1[b];"
                     "color=blue:s=160x90:r=30:d=1[c];color=white:s=160x90:r=30:d=1[d];"
                     "[a][b][c][d]concat=n=4:v=1:a=0",
                     "-c:v", "mpeg4", "-q:v", "2", "-g", "30", "-bf", "0", "-threads", "1", video});
        const auto still = dir.filePath("still.png");
        QImage image(320, 240, QImage::Format_RGB32);
        image.fill(Qt::yellow);
        QVERIFY(image.save(still));
        Asset v;
        v.id = "video";
        v.path = video;
        v.kind = "video";
        v.duration = 4;
        v.width = 160;
        v.height = 90;
        Asset i;
        i.id = "image";
        i.path = still;
        i.kind = "image";
        i.duration = 5;
        i.width = 320;
        i.height = 240;
        Asset sound = v;
        sound.id = "sound";
        sound.kind = "audio";
        auto layout = Thumbnails::layout(v);
        QCOMPARE(layout.count, 4);
        QCOMPARE(layout.interval, 1.);
        QCOMPARE(layout.tile, QSize(96, 54));
        Asset longVideo = v;
        longVideo.duration = 3600;
        QCOMPARE(Thumbnails::layout(longVideo).count, 200);
        QCOMPARE(Thumbnails::layout(longVideo).interval, 18.);
        QCOMPARE(Thumbnails::layout(i).count, 1);
        QCOMPARE(Thumbnails::layout(i).tile, QSize(72, 54));
        const auto cache = dir.filePath("cache");
        {
            Thumbnails thumbnails(cache, ffmpeg);
            thumbnails.setAssets({v, i, sound});
            QCOMPARE(thumbnails.strip("sound")["status"].toString(), QString("none"));
            QTRY_VERIFY_WITH_TIMEOUT(!thumbnails.busy(), 30000);
            const auto strip = thumbnails.strip("video");
            QCOMPARE(strip["status"].toString(), QString("ready"));
            QImage film(QUrl(strip["url"].toString()).toLocalFile());
            QCOMPARE(film.size(), QSize(4 * 96, 54));
            const QColor expected[] = {Qt::red, Qt::green, Qt::blue, Qt::white};
            for (int tile = 0; tile < 4; ++tile) {
                const auto c = film.pixelColor(tile * 96 + 48, 27);
                const auto e = expected[tile];
                QVERIFY2(std::abs(c.red() - e.red()) < 40 && std::abs(c.green() - e.green()) < 40 &&
                             std::abs(c.blue() - e.blue()) < 40,
                         qPrintable(QString("tile %1 is %2").arg(tile).arg(c.name())));
            }
            QImage poster(QUrl(thumbnails.strip("image")["url"].toString()).toLocalFile());
            QCOMPARE(poster.size(), QSize(72, 54));
            QVERIFY(poster.pixelColor(36, 27).red() > 200 && poster.pixelColor(36, 27).blue() < 60);
        }
        // A second session reuses the cached strip without running FFmpeg.
        Thumbnails reused(cache, ffmpeg);
        reused.setAssets({v});
        QVERIFY(!reused.busy());
        QCOMPARE(reused.strip("video")["status"].toString(), QString("ready"));
        // Changed media gets a new fingerprint and is extracted again.
        QTest::qWait(20);
        QVERIFY(image.save(still));
        {
            QFile touched(still);
            QVERIFY(touched.open(QIODevice::ReadWrite));
            QVERIFY(touched.setFileTime(QDateTime::currentDateTime().addSecs(5),
                                        QFileDevice::FileModificationTime));
        }
        reused.setAssets({v, i});
        QVERIFY(reused.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!reused.busy(), 30000);
        QCOMPARE(reused.strip("image")["status"].toString(), QString("ready"));
        // Missing media is reported instead of blocking the queue.
        Asset missing = v;
        missing.id = "missing";
        missing.path = dir.filePath("gone.mp4");
        reused.setAssets({missing});
        QTRY_VERIFY_WITH_TIMEOUT(!reused.busy(), 5000);
        QCOMPARE(reused.strip("missing")["status"].toString(), QString("unavailable"));
    }
    void transitions() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        auto source = [&](const QString &name, const QString &colour, int hz) {
            const auto path = dir.filePath(name);
            run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i",
                         QString("color=%1:s=160x90:r=30:d=2").arg(colour), "-f", "lavfi", "-i",
                         QString("sine=frequency=%1:sample_rate=48000:duration=2").arg(hz), "-c:v",
                         "ffv1", "-threads", "1", "-c:a", "pcm_s16le", "-shortest", path});
            return path;
        };
        Project p;
        p.width = 160;
        p.height = 90;
        for (auto [id, colour, hz] : {std::tuple{"red", "red", 440}, {"blue", "blue", 660}}) {
            Asset a;
            a.id = id;
            a.path = source(QString(id) + ".mkv", colour, hz);
            a.kind = "video";
            a.duration = 2;
            a.hasAudio = true;
            p.assets.push_back(a);
        }
        Clip a;
        a.id = "a";
        a.assetId = "red";
        a.duration = 30;
        Clip b = a;
        b.id = "b";
        b.assetId = "blue";
        b.start = 30;
        p.clips = {a, b};
        // Model rules: only a clip starting at a cut can transition; length is clamped.
        QCOMPARE(p.transitionLength(p.clips[1]), qint64(0));
        p.clips[1].transition = "fade";
        p.clips[1].transitionFrames = 10;
        QCOMPARE(p.previousAdjacent(p.clips[1])->id, QString("a"));
        QCOMPARE(p.transitionLength(p.clips[1]), qint64(10));
        p.clips[1].transitionFrames = 500;
        QCOMPARE(p.transitionLength(p.clips[1]), qint64(30));
        p.clips[1].transitionFrames = 10;
        QVERIFY(p.previousAdjacent(p.clips[0]) == nullptr);
        auto broken = p;
        broken.clips[1].start = 31;
        QCOMPARE(broken.transitionLength(broken.clips[1]), qint64(0));
        auto invalid = p;
        invalid.clips[1].transition = "not-a-transition";
        QVERIFY_EXCEPTION_THROWN(invalid.validate(), std::runtime_error);
        const auto roundtrip = Project::fromJson(p.json(dir.path()), dir.path());
        QCOMPARE(roundtrip.clips[1].transition, QString("fade"));
        QCOMPARE(roundtrip.clips[1].transitionFrames, qint64(10));
        QVERIFY(p.json()["schemaVersion"].toInt() >= 4);
        auto split = p;
        QVERIFY(split.split("b", 45));
        QCOMPARE(split.clips[1].transition, QString("fade"));
        QVERIFY(split.clips.last().transition.isEmpty());

        const auto graph = dir.filePath("graph.txt");
        auto writeGraph = [&](const RenderPlan &plan) {
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
        };
        auto still = [&](const Project &project, qint64 frame, bool window) {
            RenderOptions options;
            options.audio = false;
            if (window) {
                options.from = frame;
                options.to = frame + 1;
            }
            const auto plan = compileRender(project, dir.filePath("work"), 160, 90, options);
            writeGraph(plan);
            QImage image;
            image.loadFromData(
                run(ffmpeg, renderArguments(plan, graph, {}, "",
                                            window ? 0 : frameTime(frame, 30, 1).seconds())),
                "PNG");
            return image.convertToFormat(QImage::Format_RGB32);
        };
        // Dissolve over frames 25..34, centred on the cut at 30; the length is unchanged.
        QCOMPARE(p.duration(), qint64(60));
        auto colour = [&](qint64 frame, bool window, int x = 80) {
            return still(p, frame, window).pixelColor(x, 45);
        };
        QVERIFY(colour(20, false).red() > 200 && colour(20, false).blue() < 40);
        const auto middle = colour(30, false);
        QVERIFY2(middle.red() > 80 && middle.red() < 180 && middle.blue() > 80 &&
                     middle.blue() < 180,
                 qPrintable(middle.name()));
        QVERIFY(colour(26, false).red() > colour(33, false).red());
        QVERIFY(colour(40, false).blue() > 200 && colour(40, false).red() < 40);
        // Windows starting inside the transition show the same blend as the full timeline.
        for (qint64 frame : {25, 27, 30, 34, 35}) {
            const auto full = colour(frame, false), window = colour(frame, true);
            QVERIFY2(std::abs(full.red() - window.red()) < 6 &&
                         std::abs(full.blue() - window.blue()) < 6,
                     qPrintable(QString("frame %1: %2 vs %3")
                                    .arg(frame)
                                    .arg(full.name(), window.name())));
        }
        // A wipe reveals the incoming clip from one side.
        auto wipe = p;
        wipe.clips[1].transition = "wipeleft";
        const auto half = still(wipe, 30, false);
        const auto left = half.pixelColor(10, 45), right = half.pixelColor(150, 45);
        QVERIFY2((left.red() > 200) != (right.red() > 200),
                 qPrintable(left.name() + " " + right.name()));
        // Audio: equal-power crossfade keeps level steady, and the mix length is exact.
        RenderOptions sound;
        sound.video = false;
        const auto plan = compileRender(p, dir.filePath("work"), 160, 90, sound);
        writeGraph(plan);
        const auto pcm = run(ffmpeg, streamArguments(plan, graph, false));
        QCOMPARE(pcm.size(), qsizetype(2 * 48000) * 4);
        auto rms = [&](double from, double to) {
            double sum = 0;
            qint64 count = 0;
            for (qint64 i = qint64(from * 48000); i < qint64(to * 48000); ++i) {
                const auto *sample = reinterpret_cast<const qint16 *>(pcm.constData()) + i * 2;
                sum += double(sample[0]) * sample[0];
                ++count;
            }
            return std::sqrt(sum / count);
        };
        const double steady = rms(0.2, 0.7);
        for (double t : {0.84, 0.92, 1.0, 1.08}) {
            const double level = rms(t, t + 0.03);
            QVERIFY2(level > steady * 0.8 && level < steady * 1.25,
                     qPrintable(QString("%1 s: %2 vs %3").arg(t).arg(level).arg(steady)));
        }
    }
    void keyframes() {
        // Model: interpolation, editing semantics and persistence.
        Clip c;
        c.duration = 60;
        c.keyframes["scale"] = {{0, 1, false}, {50, .5, false}};
        QCOMPARE(c.valueAt("scale", -5), 1.);
        QCOMPARE(c.valueAt("scale", 25), .75);
        QCOMPARE(c.valueAt("scale", 80), .5);
        QCOMPARE(c.valueAt("opacity", 10), 1.);
        c.keyframes["scale"][0].smooth = true;
        QVERIFY(c.valueAt("scale", 10) > .9 && c.valueAt("scale", 40) < .6);
        QCOMPARE(c.valueAt("scale", 25), .75);
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        const auto source = dir.filePath("red.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=red:s=160x90:r=30:d=3", "-f",
                     "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=3", "-c:v",
                     "ffv1", "-threads", "1", "-c:a", "pcm_s16le", "-shortest", source});
        Project p;
        p.width = 160;
        p.height = 90;
        Asset a;
        a.id = "red";
        a.path = source;
        a.kind = "video";
        a.duration = 3;
        a.hasAudio = true;
        p.assets.push_back(a);
        Clip clip;
        clip.id = "clip";
        clip.assetId = "red";
        clip.duration = 60;
        p.clips.push_back(clip);
        auto edited = p;
        edited.clips[0].keyframes["x"] = {{10, 0, true}, {40, .25, true}};
        edited.trim("clip", 5, 60);
        QCOMPARE(edited.clips[0].keyframes["x"][0].frame, qint64(5));
        QVERIFY(edited.split("clip", 30));
        QCOMPARE(edited.clips.last().keyframes["x"][1].frame, qint64(10));
        QCOMPARE(edited.clips.last().valueAt("x", 10), .25);
        const auto saved = Project::fromJson(edited.json(dir.path()), dir.path());
        QCOMPARE(saved.clips.last().keyframes["x"], edited.clips.last().keyframes["x"]);
        QVERIFY(edited.json()["schemaVersion"].toInt() >= 5);
        for (auto bad : {QVector<Keyframe>{{10, 0, true}, {5, 0, true}},
                         QVector<Keyframe>{{0, 9, true}}}) {
            auto invalid = p;
            invalid.clips[0].keyframes["scale"] = bad;
            QVERIFY_EXCEPTION_THROWN(invalid.validate(), std::runtime_error);
        }
        auto unknown = p;
        unknown.clips[0].keyframes["brightness"] = {{0, 0, true}};
        QVERIFY_EXCEPTION_THROWN(unknown.validate(), std::runtime_error);

        // Rendering: measure the red picture on the black canvas.
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Project &project, qint64 frame, bool window) {
            RenderOptions options;
            options.audio = false;
            if (window) {
                options.from = frame;
                options.to = frame + 1;
            }
            const auto plan = compileRender(project, dir.filePath("work"), 160, 90, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage image;
            image.loadFromData(
                run(ffmpeg, renderArguments(plan, graph, {}, "",
                                            window ? 0 : frameTime(frame, 30, 1).seconds())),
                "PNG");
            return image.convertToFormat(QImage::Format_RGB32);
        };
        auto redWidth = [](const QImage &image, int y) {
            int n = 0;
            for (int x = 0; x < image.width(); ++x)
                n += image.pixelColor(x, y).red() > 128;
            return n;
        };
        auto shrinking = p;
        shrinking.clips[0].keyframes["scale"] = {{0, 1, false}, {50, .5, false}};
        QVERIFY(std::abs(redWidth(still(shrinking, 0, false), 45) - 160) <= 2);
        QVERIFY(std::abs(redWidth(still(shrinking, 25, false), 45) - 120) <= 3);
        const auto small = still(shrinking, 55, false);
        QVERIFY(std::abs(redWidth(small, 45) - 80) <= 3);
        QVERIFY(small.pixelColor(5, 5).red() < 40);
        // A window inside the animation sees the same frame.
        for (qint64 frame : {10, 25, 49}) {
            const auto full = redWidth(still(shrinking, frame, false), 45),
                       window = redWidth(still(shrinking, frame, true), 45);
            QVERIFY2(std::abs(full - window) <= 1,
                     qPrintable(QString("frame %1: %2 vs %3").arg(frame).arg(full).arg(window)));
        }
        auto moving = p;
        moving.clips[0].scale = .5;
        moving.clips[0].keyframes["x"] = {{0, 0, false}, {30, .25, false}};
        moving.clips[0].keyframes["rotation"] = {{0, 0, false}, {30, 90, false}};
        const auto turned = still(moving, 30, false);
        // 80x45 picture rotated a quarter turn and moved right by 40 px: 45 wide, 80 tall.
        QVERIFY(turned.pixelColor(120, 10).red() > 128 && turned.pixelColor(120, 80).red() > 128);
        QVERIFY(turned.pixelColor(90, 45).red() < 128 && turned.pixelColor(150, 45).red() < 128);
        QVERIFY(std::abs(redWidth(turned, 45) - 45) <= 3);
        auto fading = p;
        fading.clips[0].keyframes["opacity"] = {{0, 1, false}, {60, 0, false}};
        QVERIFY(still(fading, 0, false).pixelColor(80, 45).red() > 240);
        const auto halfway = still(fading, 30, false).pixelColor(80, 45).red();
        QVERIFY2(std::abs(halfway - 128) < 12, qPrintable(QString::number(halfway)));
        QVERIFY(std::abs(still(fading, 30, true).pixelColor(80, 45).red() - halfway) <= 2);

        // Volume keyframes: full level, then silence by frame 45.
        auto quieter = p;
        quieter.clips[0].keyframes["volume"] = {{15, 1, false}, {45, 0, false}};
        RenderOptions sound;
        sound.video = false;
        const auto plan = compileRender(quieter, dir.filePath("work"), 160, 90, sound);
        QFile g(graph);
        QVERIFY(g.open(QIODevice::WriteOnly | QIODevice::Truncate));
        g.write(plan.graph.toUtf8());
        g.close();
        const auto pcm = run(ffmpeg, streamArguments(plan, graph, false));
        auto rms = [&](double from, double to) {
            double sum = 0;
            qint64 count = 0;
            for (qint64 i = qint64(from * 48000); i < qint64(to * 48000); ++i) {
                const auto *sample = reinterpret_cast<const qint16 *>(pcm.constData()) + i * 2;
                sum += double(sample[0]) * sample[0];
                ++count;
            }
            return std::sqrt(sum / count);
        };
        const double loud = rms(.1, .4), middle = rms(.97, 1.03), end = rms(1.6, 1.9);
        QVERIFY2(middle > loud * .4 && middle < loud * .6,
                 qPrintable(QString("%1 %2").arg(loud).arg(middle)));
        QVERIFY(end < loud * .02);
    }
    void exportProfiles() {
        const auto ffmpeg = Editor::executable("ffmpeg"), probe = Editor::executable("ffprobe");
        QVERIFY2(!ffmpeg.isEmpty() && !probe.isEmpty(), "FFmpeg is required for integration tests");
        Project shape;
        shape.width = 1920;
        shape.height = 1080;
        QCOMPARE(exportSize(shape, 0), QSize(1920, 1080));
        QCOMPARE(exportSize(shape, 2160), QSize(3840, 2160));
        shape.width = 1080;
        shape.height = 1920;
        QCOMPARE(exportSize(shape, 1080), QSize(608, 1080));
        ExportSettings h264;
        const auto hardware = encoderCandidates(h264, {1920, 1080}, 30);
        QCOMPARE(hardware.size(), 4);
        QCOMPARE(hardware.first().name, QString("h264_nvenc"));
        QCOMPARE(hardware.last().name, QString("h264_mf"));
        QVERIFY(std::all_of(hardware.begin(), hardware.end(), [](const Encoder &e) { return e.probe; }));
        ExportSettings av1{"av1", "max", 0};
        QCOMPARE(encoderCandidates(av1, {1920, 1080}, 30).last().name, QString("libsvtav1"));
        QVERIFY(!encoderCandidates(av1, {1920, 1080}, 30).last().probe);
        ExportSettings prores{"prores", "high", 0};
        QCOMPARE(encoderCandidates(prores, {1920, 1080}, 30).first().pixelFormat,
                 QString("yuv422p10le"));
        QCOMPARE(formatExtension("vp9"), QString("webm"));

        // A missing hardware encoder is skipped; with no working candidate the result is null.
        EncoderResolver resolver(ffmpeg);
        Encoder fake;
        fake.name = "not_a_real_encoder";
        fake.videoArguments = {"-c:v", "not_a_real_encoder"};
        fake.probe = true;
        Encoder fallback = encoderCandidates({"mpeg4", "high", 0}, {320, 180}, 30).first();
        QString chosen = "pending";
        resolver.resolve({fake, fallback}, {320, 180}, 30,
                         [&](const Encoder *e) { chosen = e ? e->name : QString(); });
        QTRY_COMPARE_WITH_TIMEOUT(chosen, QString("mpeg4"), 20000);
        chosen = "pending";
        resolver.resolve({fake}, {320, 180}, 30,
                         [&](const Encoder *e) { chosen = e ? e->name : QString(); });
        QCOMPARE(chosen, QString()); // Cached as unavailable: answered without a new probe.

        QTemporaryDir dir;
        const auto source = dir.filePath("bars.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "testsrc2=s=320x180:r=30:d=1", "-f",
                     "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=1", "-c:v",
                     "ffv1", "-c:a", "pcm_s16le", "-shortest", source});
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["importing"].toBool(), 15000);
        editor.addAsset(editor.project().assets.first().id, 0);
        auto streams = [&](const QString &file) {
            return QJsonDocument::fromJson(run(probe, {"-v", "error", "-show_streams", "-of", "json",
                                                       file}))
                .object()["streams"]
                .toArray();
        };
        struct Case {
            QString format, quality;
            int height;
            QString video, audio, file;
            int width;
        };
        const QVector<Case> cases{{"av1", "small", 180, "av1", "aac", "a.mp4", 320},
                                  {"vp9", "balanced", 0, "vp9", "opus", "v.webm", 160},
                                  {"prores", "high", 0, "prores", "pcm_s16le", "p.mov", 160},
                                  {"mpeg4", "max", 0, "mpeg4", "aac", "m.mp4", 160}};
        for (const auto &c : cases) {
            const auto out = dir.filePath(c.file);
            editor.exportWith(QUrl::fromLocalFile(out),
                              {{"format", c.format}, {"quality", c.quality}, {"height", c.height}});
            QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 60000);
            QVERIFY2(QFileInfo::exists(out), qPrintable(c.format + ": " +
                                                        editor.state()["error"].toString()));
            const auto list = streams(out);
            QCOMPARE(list.size(), 2);
            QCOMPARE(list[0].toObject()["codec_name"].toString(), c.video);
            QCOMPARE(list[0].toObject()["width"].toInt(), c.width);
            QCOMPARE(list[1].toObject()["codec_name"].toString(), c.audio);
        }
        QVERIFY(editor.state()["status"].toString().contains("MPEG-4"));
        // A filename must match the format.
        editor.exportWith(QUrl::fromLocalFile(dir.filePath("wrong.mp4")), {{"format", "vp9"}});
        QVERIFY(editor.state()["error"].toString().contains(".webm"));
        editor.clearError();
        // H.264 depends on the machine: it either exports or explains what to choose instead.
        const auto h264File = dir.filePath("h.mp4");
        editor.exportWith(QUrl::fromLocalFile(h264File), {{"format", "h264"}});
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 90000);
        if (QFileInfo::exists(h264File))
            QCOMPARE(streams(h264File)[0].toObject()["codec_name"].toString(), QString("h264"));
        else
            QVERIFY2(editor.state()["error"].toString().contains("No H264 encoder"),
                     qPrintable(editor.state()["error"].toString()));
    }
    void overlayStyles() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        const auto background = dir.filePath("blue.png");
        QImage blue(160, 90, QImage::Format_RGB32);
        blue.fill(Qt::blue);
        QVERIFY(blue.save(background));
        // Speaker stand-in: red square on a green screen, 160x90.
        const auto speaker = dir.filePath("speaker.png");
        QImage screen(160, 90, QImage::Format_RGB32);
        screen.fill(QColor(0, 255, 0));
        QPainter(&screen).fillRect(60, 25, 40, 40, Qt::red);
        QVERIFY(screen.save(speaker));
        Project p;
        p.width = 160;
        p.height = 90;
        for (auto [id, path] : {std::pair{"bg", background}, {"speaker", speaker}}) {
            Asset a;
            a.id = id;
            a.path = path;
            a.kind = "image";
            a.duration = 5;
            a.width = 160;
            a.height = 90;
            p.assets.push_back(a);
        }
        Clip bg;
        bg.id = "bg";
        bg.assetId = "bg";
        bg.duration = 30;
        Clip pip = bg;
        pip.id = "pip";
        pip.assetId = "speaker";
        pip.track = 1;
        p.clips = {bg, pip};
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Project &project) {
            RenderOptions options;
            options.audio = false;
            options.to = 1;
            const auto plan = compileRender(project, dir.filePath("work"), 160, 90, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage image;
            image.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return image.convertToFormat(QImage::Format_RGB32);
        };
        auto isBlue = [](QColor c) { return c.blue() > 200 && c.red() < 60 && c.green() < 60; };
        auto isRed = [](QColor c) { return c.red() > 200 && c.green() < 60 && c.blue() < 60; };
        auto isWhite = [](QColor c) { return c.red() > 200 && c.green() > 200 && c.blue() > 200; };
        // Circle: a 90x90 centre square scaled to 45x45, white ring, soft shadow.
        auto circle = p;
        auto &c = circle.clips[1];
        c.scale = .5;
        c.shape = "circle";
        c.border = 0.1; // 5 px at this size: picture radius 23, ring to 28
        c.shadow = 1;
        auto image = still(circle);
        QVERIFY(isRed(image.pixelColor(80, 45)));
        QVERIFY2(isBlue(image.pixelColor(52, 17)), qPrintable(image.pixelColor(52, 17).name()));
        QVERIFY2(isWhite(image.pixelColor(80, 45 - 26)), qPrintable(image.pixelColor(80, 19).name()));
        int darkest = 255;
        for (int y = 45 + 29; y < 45 + 34; ++y)
            darkest = std::min(darkest, image.pixelColor(80, y).blue());
        QVERIFY2(darkest < 240, qPrintable(QString::number(darkest))); // shadow below the ring
        QVERIFY(isBlue(image.pixelColor(5, 5)));
        // Rounded rectangle: corners show the background, the edge midpoint shows the picture.
        auto rounded = p;
        rounded.clips[1].scale = .5;
        rounded.clips[1].shape = "rounded";
        rounded.clips[1].radius = .4;
        image = still(rounded);
        QVERIFY2(isBlue(image.pixelColor(41, 23)), qPrintable(image.pixelColor(41, 23).name()));
        QVERIFY(image.pixelColor(80, 24).green() > 200);
        // Green screen: the key colour disappears, the subject stays.
        auto keyed = p;
        keyed.clips[1].chromaKey = true;
        image = still(keyed);
        QVERIFY2(isBlue(image.pixelColor(20, 20)), qPrintable(image.pixelColor(20, 20).name()));
        QVERIFY(isRed(image.pixelColor(80, 45)));
        // Keying, a circle and animated scale together.
        keyed.clips[1].shape = "circle";
        keyed.clips[1].keyframes["scale"] = {{0, .5, false}, {29, 1, false}};
        image = still(keyed);
        QVERIFY(isRed(image.pixelColor(80, 45)));
        QVERIFY(isBlue(image.pixelColor(60, 45)));
        // Style values persist, and invalid ones are rejected.
        const auto saved = Project::fromJson(circle.json(dir.path()), dir.path());
        QCOMPARE(saved.clips[1].shape, QString("circle"));
        QCOMPARE(saved.clips[1].border, .1);
        QVERIFY(circle.json()["schemaVersion"].toInt() >= 6);
        auto invalid = p;
        invalid.clips[1].shape = "star";
        QVERIFY_EXCEPTION_THROWN(invalid.validate(), std::runtime_error);

        // Editor: bounds for the preview handles, one-step position edits, portrait probing.
        FrameProvider frames;
        Editor editor(&frames);
        const auto file = dir.filePath("p.cutlery");
        saveProject(rounded, file);
        QVERIFY(editor.openProject(QUrl::fromLocalFile(file)));
        auto bounds = editor.clipBounds("pip");
        QCOMPARE(bounds["width"].toDouble(), .5);
        QCOMPARE(bounds["x"].toDouble(), .25);
        editor.select("pip");
        editor.setClipValues({{"x", .2}, {"y", -.2}, {"scale", .3}});
        QCOMPARE(editor.project().clips[1].x, .2);
        QCOMPARE(editor.project().clips[1].scale, .3);
        editor.undo();
        QCOMPARE(editor.project().clips[1].x, 0.);
        QCOMPARE(editor.project().clips[1].scale, .5);
        const auto landscape = dir.filePath("landscape.mp4"), portrait = dir.filePath("portrait.mp4");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "testsrc2=s=160x90:r=30:d=1", "-c:v",
                     "mpeg4", landscape});
        run(ffmpeg, {"-v", "error", "-display_rotation", "90", "-i", landscape, "-c", "copy",
                     portrait});
        editor.importMedia({QUrl::fromLocalFile(portrait)});
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["importing"].toBool(), 15000);
        QCOMPARE(editor.project().assets.last().width, 90);
        QCOMPARE(editor.project().assets.last().height, 160);
    }
    void aiCutout() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        const auto background = dir.filePath("blue.png");
        QImage blue(160, 90, QImage::Format_RGB32);
        blue.fill(Qt::blue);
        QVERIFY(blue.save(background));
        // Speaker: a red video. Matte: left half opaque for the first second, then the right.
        const auto speaker = dir.filePath("speaker.mkv"), matteFile = dir.filePath("matte.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=red:s=160x90:r=30:d=3", "-c:v",
                     "ffv1", speaker});
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=black:s=80x45:r=8:d=3", "-vf",
                     "format=gray,geq=lum='if(lt(T,1),if(lt(X,W/2),255,0),if(lt(X,W/2),0,255))'",
                     "-c:v", "ffv1", "-g", "1", matteFile});
        Project p;
        p.width = 160;
        p.height = 90;
        Asset bgAsset;
        bgAsset.id = "bg";
        bgAsset.path = background;
        bgAsset.kind = "image";
        bgAsset.duration = 5;
        bgAsset.width = 160;
        bgAsset.height = 90;
        Asset video = bgAsset;
        video.id = "speaker";
        video.path = speaker;
        video.kind = "video";
        video.duration = 3;
        p.assets = {bgAsset, video};
        Clip bg;
        bg.id = "bg";
        bg.assetId = "bg";
        bg.duration = 60;
        Clip pip = bg;
        pip.id = "pip";
        pip.assetId = "speaker";
        pip.track = 1;
        pip.aiCutout = true;
        p.clips = {bg, pip};
        QVERIFY(pip.styled());
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Project &project, qint64 frame, const MatteSource &m) {
            RenderOptions options;
            options.audio = false;
            options.from = frame;
            options.to = frame + 1;
            if (!m.path.isEmpty())
                options.mattes.insert("speaker", m);
            const auto plan = compileRender(project, dir.filePath("work"), 160, 90, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage image;
            image.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return image.convertToFormat(QImage::Format_RGB32);
        };
        auto isBlue = [](QColor c) { return c.blue() > 200 && c.red() < 60 && c.green() < 60; };
        auto isRed = [](QColor c) { return c.red() > 200 && c.green() < 60 && c.blue() < 60; };
        const MatteSource matte{matteFile, 0, 3, 8};
        // Without an analysed matte the picture stays whole.
        auto image = still(p, 15, {});
        QVERIFY(isRed(image.pixelColor(20, 45)) && isRed(image.pixelColor(140, 45)));
        image = still(p, 15, matte); // source 0.5 s
        QVERIFY2(isRed(image.pixelColor(20, 45)), qPrintable(image.pixelColor(20, 45).name()));
        QVERIFY2(isBlue(image.pixelColor(140, 45)), qPrintable(image.pixelColor(140, 45).name()));
        image = still(p, 45, matte); // source 1.5 s
        QVERIFY(isBlue(image.pixelColor(20, 45)) && isRed(image.pixelColor(140, 45)));
        // Seeking between analysed frames: source 0.8 + 0.5 s.
        auto trimmed = p;
        trimmed.clips[1].sourceIn = Time(4, 5);
        image = still(trimmed, 15, matte);
        QVERIFY(isBlue(image.pixelColor(20, 45)) && isRed(image.pixelColor(140, 45)));
        // A matte starting later in the source: source 1.3 s is matte time 0.8 s.
        image = still(trimmed, 15, MatteSource{matteFile, .5, 3.5, 8});
        QVERIFY(isRed(image.pixelColor(20, 45)) && isBlue(image.pixelColor(140, 45)));
        // Mirrored picture, mirrored matte; with a circle on top.
        auto flipped = p;
        flipped.clips[1].flip = true;
        flipped.clips[1].shape = "circle";
        image = still(flipped, 15, matte);
        QVERIFY(isBlue(image.pixelColor(70, 45)) && isRed(image.pixelColor(90, 45)));
        QVERIFY(isBlue(image.pixelColor(5, 45)));
        // The cutout persists in the project file.
        QVERIFY(Project::fromJson(p.json(dir.path()), dir.path()).clips[1].aiCutout);
        QVERIFY(p.json()["schemaVersion"].toInt() >= 7);

        // AI upscale: the clip's picture comes from the upscaled copy, timed from its start.
        // Stand-in copy: green for its first second, then yellow, at twice the size.
        const auto upscaledFile = dir.filePath("upscaled.mov");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i",
                     "color=c=green:s=320x180:r=30:d=1[a];color=c=yellow:s=320x180:r=30:d=2[b];"
                     "[a][b]concat=n=2:v=1:a=0",
                     "-c:v", "prores_ks", upscaledFile});
        auto upscale = trimmed; // source 1.3 s at frame 15
        upscale.clips[1].aiCutout = false;
        upscale.clips[1].aiUpscale = true;
        auto isGreen = [](QColor c) { return c.green() > 100 && c.red() < 60 && c.blue() < 60; };
        auto isYellow = [](QColor c) { return c.green() > 200 && c.red() > 200 && c.blue() < 60; };
        auto upscaled = [&](double start) {
            RenderOptions options;
            options.audio = false;
            options.from = 15;
            options.to = 16;
            options.upscaled.insert("speaker", MatteSource{upscaledFile, start, start + 3, 30});
            const auto plan = compileRender(upscale, dir.filePath("work"), 160, 90, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage image;
            image.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return image.convertToFormat(QImage::Format_RGB32).pixelColor(80, 45);
        };
        QVERIFY2(isYellow(upscaled(0)), qPrintable(upscaled(0).name()));
        QVERIFY2(isGreen(upscaled(.5)), qPrintable(upscaled(.5).name()));
        // Without an upscaled copy the source is used.
        QVERIFY(isRed(still(upscale, 15, {}).pixelColor(80, 45)));
        QVERIFY(Project::fromJson(upscale.json(dir.path()), dir.path()).clips[1].aiUpscale);

#ifdef CUTLERY_AI_WORKER
        // The worker with stand-in models on a red left half and a blue right half: the matte
        // is the red channel; the upscale doubles the size by nearest neighbour.
        const auto halves = dir.filePath("halves.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=blue:s=160x90:r=30:d=2", "-vf",
                     "drawbox=x=0:y=0:w=80:h=90:c=red:t=fill", "-c:v", "ffv1", halves});
        video.path = halves;
        video.duration = 2;
        AiJobs jobs(dir.filePath("ai"), ffmpeg, Editor::executable("ffprobe"), CUTLERY_AI_WORKER,
                    {{"matte", CUTLERY_SOURCE_DIR "/tests/fixtures/red-matte.onnx"},
                     {"upscale", CUTLERY_SOURCE_DIR "/tests/fixtures/nearest-x2.onnx"}});
        QVERIFY2(jobs.available("matte"), qPrintable(jobs.missing("matte")));
        QCOMPARE(jobs.status("matte", video)["status"].toString(), QString("none"));
        jobs.start("matte", video, .5, 1.5);
        jobs.start("upscale", video, .5, 1.5, 180);
        QVERIFY(jobs.busy());
        QCOMPARE(jobs.status("upscale", video, 180)["status"].toString(), QString("queued"));
        QTRY_VERIFY_WITH_TIMEOUT(!jobs.busy(), 60000);
        QCOMPARE(jobs.status("matte", video)["status"].toString(), QString("ready"));
        const auto made = jobs.result("matte", video);
        QCOMPARE(made.start, .5);
        QCOMPARE(made.end, 1.5);
        auto frames = run(ffmpeg, {"-v", "error", "-i", made.path, "-f", "rawvideo", "-pix_fmt",
                                   "gray", "-"});
        QCOMPARE(frames.size(), qsizetype(160 * 90 * 8)); // 1 s at 8 fps, picture size
        QVERIFY(uchar(frames[45 * 160 + 20]) > 200);
        QVERIFY(uchar(frames[45 * 160 + 140]) < 50);
        QVERIFY2(jobs.status("upscale", video, 180)["status"].toString() == "ready",
                 qPrintable(jobs.status("upscale", video, 180)["error"].toString()));
        const auto sharp = jobs.result("upscale", video, 180);
        QCOMPARE(sharp.rate, 30.);
        frames = run(ffmpeg, {"-v", "error", "-i", sharp.path, "-f", "rawvideo", "-pix_fmt",
                              "rgb24", "-"});
        QCOMPARE(frames.size(), qsizetype(320 * 180 * 3 * 30)); // every frame, twice the size
        const auto *px = reinterpret_cast<const uchar *>(frames.constData());
        QVERIFY(isRed(QColor(px[(90 * 320 + 40) * 3], px[(90 * 320 + 40) * 3 + 1],
                             px[(90 * 320 + 40) * 3 + 2])));
        QVERIFY(isBlue(QColor(px[(90 * 320 + 280) * 3], px[(90 * 320 + 280) * 3 + 1],
                              px[(90 * 320 + 280) * 3 + 2])));
        // Results are per size; a missing model reports the AI pack as unavailable.
        QCOMPARE(jobs.status("upscale", video, 360)["status"].toString(), QString("none"));
        AiJobs none(dir.filePath("ai"), ffmpeg, Editor::executable("ffprobe"), CUTLERY_AI_WORKER,
                    {{"matte", dir.filePath("x.onnx")}});
        QVERIFY(!none.available("matte"));
        QVERIFY(!none.available("upscale"));
        QVERIFY(!none.missing("matte").isEmpty());
#endif
    }
    void waveformPeaksAndCache() {
        PeakAccumulator peaks(2);
        const auto pcm = QByteArray::fromHex("0000004000800000");
        peaks.append(pcm.left(3));
        peaks.append(pcm.mid(3));
        const auto bins = peaks.finish();
        QCOMPARE(bins.size(), 2);
        QCOMPARE(bins[0], .5f);
        QCOMPARE(bins[1], 1.f);
        QTemporaryDir dir;
        const auto wav = dir.filePath("tone.wav");
        run(Editor::executable("ffmpeg"),
            {"-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=8000:duration=1",
             "-c:a", "pcm_s16le", wav});
        Asset a;
        a.id = "tone";
        a.path = wav;
        a.kind = "audio";
        a.hasAudio = true;
        a.duration = 1;
        MediaAnalysis analysis(dir.filePath("cache"), Editor::executable("ffmpeg"));
        analysis.setAssets({a});
        QTRY_COMPARE_WITH_TIMEOUT(analysis.waveform(a.id)["status"].toString(), QString("ready"),
                                  15000);
        const auto data = analysis.waveform(a.id);
        QVERIFY(data["peaks"].toList().size() >= 99);
        QVERIFY(data["peaks"].toList().first().toDouble() > .1);
        MediaAnalysis cached(dir.filePath("cache"), "");
        cached.setAssets({a});
        QCOMPARE(cached.waveform(a.id), data);
        QFile f(wav);
        QVERIFY(f.open(QIODevice::Append));
        f.write("changed");
        f.close();
        cached.setAssets({a});
        QCOMPARE(cached.waveform(a.id)["status"].toString(), QString("unavailable"));
        QVERIFY(cached.waveform(a.id)["peaks"].toList().isEmpty());
    }
    void asynchronousJobs() {
        QTemporaryDir dir;
        const auto file = dir.filePath("image with spaces.png");
        QImage image(160, 90, QImage::Format_RGB32);
        image.fill(Qt::green);
        QVERIFY(image.save(file));
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(file)});
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["importing"].toBool(), 15000);
        QCOMPARE(editor.project().assets.size(), 1);
        editor.addAsset(editor.project().assets.first().id, 0);
        editor.setClip("duration", 30);
        const auto output = dir.filePath("export.webm");
        editor.exportVideo(QUrl::fromLocalFile(output), "webm");
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 30000);
        QVERIFY2(QFileInfo::exists(output), qPrintable(editor.state()["error"].toString()));
        const auto originalSize = QFileInfo(output).size();
        editor.exportVideo(QUrl::fromLocalFile(output), "webm");
        QVERIFY(editor.state()["error"].toString().contains("already exists"));
        QCOMPARE(QFileInfo(output).size(), originalSize);
        editor.clearError();
        // Live playback starts immediately, survives an edit by restarting, and stops at the end.
        editor.play();
        QVERIFY(editor.playing());
        QTRY_VERIFY_WITH_TIMEOUT(editor.playbackFrame() > 0, 10000);
        editor.setClip("opacity", .5);
        QVERIFY(editor.state()["playing"].toBool());
        QTRY_VERIFY_WITH_TIMEOUT(editor.playing(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["playing"].toBool(), 15000);
        QVERIFY2(editor.state()["error"].toString().isEmpty(),
                 qPrintable(editor.state()["error"].toString()));
        QCOMPARE(editor.state()["playhead"].toLongLong(), qint64(29));
        editor.seek(5);
        editor.play();
        QTRY_VERIFY_WITH_TIMEOUT(editor.playbackFrame() > 5, 10000);
        editor.pause();
        QVERIFY(!editor.playing());
        QVERIFY(editor.state()["playhead"].toLongLong() > 5);
        const auto cancelled = dir.filePath("cancelled.mp4");
        editor.exportVideo(QUrl::fromLocalFile(cancelled), "mpeg4");
        editor.cancelJob();
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 15000);
        QVERIFY(!QFileInfo::exists(cancelled));
        QCOMPARE(QDir(dir.path()).entryList({".cutlery-*"}, QDir::Files | QDir::Hidden).size(), 0);
        QVERIFY(editor.save(QUrl::fromLocalFile(dir.filePath("jobs.cutlery"))));
    }
    void windowedRender() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        const auto source = dir.filePath("moving.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "testsrc2=s=160x90:r=30:d=4", "-f",
                     "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=4", "-c:v",
                     "ffv1", "-threads", "1", "-c:a", "pcm_s16le", "-shortest", source});
        Project p;
        p.width = 160;
        p.height = 90;
        Asset a;
        a.id = "moving";
        a.path = source;
        a.name = "Moving";
        a.kind = "video";
        a.duration = 4;
        a.hasAudio = true;
        p.assets.push_back(a);
        Clip c;
        c.id = "forward";
        c.assetId = a.id;
        c.sourceIn = Time(1, 2);
        c.duration = 45;
        c.fadeIn = .5;
        p.clips.push_back(c);
        c.id = "reversed";
        c.start = 45;
        c.duration = 30;
        c.sourceIn = Time(1);
        c.speed = Time(2);
        c.reverse = true;
        c.fadeIn = 0;
        c.fadeOut = .5;
        p.clips.push_back(c);
        c = {};
        c.id = "overlay";
        c.assetId = a.id;
        c.track = 1;
        c.start = 10;
        c.duration = 35;
        c.sourceIn = Time(2);
        c.scale = .5;
        c.x = .2;
        c.fadeOut = .4;
        p.clips.push_back(c);
        QCOMPARE(p.duration(), qint64(75));
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const RenderOptions &options, double seek) {
            const auto plan = compileRender(p, dir.filePath("work"), 160, 90, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage image;
            image.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", seek)), "PNG");
            return image.convertToFormat(QImage::Format_RGB32);
        };
        auto difference = [](const QImage &l, const QImage &r) {
            qint64 sum = 0;
            for (int y = 0; y < 90; ++y)
                for (int x = 0; x < 160; ++x) {
                    const QColor a = l.pixelColor(x, y), b = r.pixelColor(x, y);
                    sum += std::abs(a.red() - b.red()) + std::abs(a.green() - b.green()) +
                           std::abs(a.blue() - b.blue());
                }
            return double(sum) / (160 * 90 * 3);
        };
        QVector<QImage> sourceFrames;
        for (int i = 0; i < 120; ++i) {
            QImage image;
            image.loadFromData(run(ffmpeg, {"-v", "error", "-i", source, "-vf",
                                            QString("select=eq(n\\,%1)").arg(i), "-frames:v",
                                            "1", "-f", "image2pipe", "-c:v", "png", "pipe:1"}),
                               "PNG");
            sourceFrames << image.convertToFormat(QImage::Format_RGB32);
        }
        auto nearestSource = [&](const QImage &image) {
            int best = -1;
            double score = 1e9;
            for (int i = 0; i < sourceFrames.size(); ++i)
                if (const auto d = difference(image, sourceFrames[i]); d < score)
                    score = d, best = i;
            return best;
        };
        // A one-frame window must show the same picture as seeking the whole timeline.
        for (qint64 frame : {0, 5, 10, 25, 44, 45, 52, 59, 74}) {
            RenderOptions full;
            full.audio = false;
            const auto reference = still(full, frameTime(frame, 30, 1).seconds());
            RenderOptions window = full;
            window.from = frame;
            window.to = frame + 1;
            const auto windowed = still(window, 0);
            QVERIFY(!reference.isNull() && !windowed.isNull());
            if (frame < 45) {
                QVERIFY2(difference(reference, windowed) <= 2,
                         qPrintable(QString("frame %1 differs by %2")
                                        .arg(frame)
                                        .arg(difference(reference, windowed))));
            } else {
                // Reversed clips: FFmpeg reassigns reversed timestamps from the decoded range, so
                // the window may select an adjacent source frame.
                const int expected = nearestSource(reference), actual = nearestSource(windowed);
                QVERIFY2(std::abs(expected - actual) <= 1,
                         qPrintable(QString("frame %1 shows source %2, export %3")
                                        .arg(frame)
                                        .arg(actual)
                                        .arg(expected)));
            }
        }
        // Streamed playback output: exact raw frame and PCM sizes for a window.
        RenderOptions stream;
        stream.from = 30;
        stream.audio = false;
        stream.realtime = true;
        auto plan = compileRender(p, dir.filePath("work"), 160, 90, stream);
        QFile g(graph);
        QVERIFY(g.open(QIODevice::WriteOnly | QIODevice::Truncate));
        g.write(plan.graph.toUtf8());
        g.close();
        QElapsedTimer timer;
        timer.start();
        QCOMPARE(run(ffmpeg, streamArguments(plan, graph, true)).size(),
                 qsizetype(45) * (160 * 90 + 2 * 80 * 45));
        // 45 frames paced to real time take about 1.5 s.
        QVERIFY(timer.elapsed() > 1000);
        stream.audio = true;
        stream.video = false;
        stream.realtime = false;
        plan = compileRender(p, dir.filePath("work"), 160, 90, stream);
        QVERIFY(g.open(QIODevice::WriteOnly | QIODevice::Truncate));
        g.write(plan.graph.toUtf8());
        g.close();
        const auto pcm = run(ffmpeg, streamArguments(plan, graph, false));
        QCOMPARE(pcm.size(), qsizetype(48000 * 3 / 2) * 4);
        RenderOptions outside;
        outside.from = 75;
        QVERIFY_EXCEPTION_THROWN(compileRender(p, dir.filePath("work"), 160, 90, outside),
                                 std::runtime_error);
    }
    void actualRender() {
        const auto ffmpeg = Editor::executable("ffmpeg"), probe = Editor::executable("ffprobe");
        QVERIFY2(!ffmpeg.isEmpty() && !probe.isEmpty(),
                 "FFmpeg and ffprobe are required for integration tests");
        QTemporaryDir dir;
        const auto red = dir.filePath("red's source.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=red:s=160x90:r=30:d=2", "-f",
                     "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=2", "-c:v",
                     "ffv1", "-threads", "1", "-c:a", "pcm_s16le", "-shortest", red});
        const auto blue = dir.filePath("blue image.png");
        QImage img(160, 90, QImage::Format_RGB32);
        img.fill(Qt::blue);
        QVERIFY(img.save(blue));
        Project p;
        p.width = 160;
        p.height = 90;
        Asset a;
        a.id = "red";
        a.path = red;
        a.name = "Red";
        a.kind = "video";
        a.duration = 2;
        a.hasAudio = true;
        p.assets.push_back(a);
        a.id = "blue";
        a.path = blue;
        a.kind = "image";
        a.duration = 5;
        a.hasAudio = false;
        p.assets.push_back(a);
        Clip c;
        c.id = "base";
        c.assetId = "red";
        c.duration = 60;
        p.clips.push_back(c);
        c.id = "overlay";
        c.assetId = "blue";
        c.track = 1;
        c.start = 15;
        c.duration = 15;
        c.scale = .5;
        p.clips.push_back(c);
        auto plan = compileRender(p, dir.path(), 160, 90);
        const auto graph = dir.filePath("graph.txt");
        QFile file(graph);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(plan.graph.toUtf8());
        file.close();
        const auto output = dir.filePath("result.mp4");
        run(ffmpeg, renderArguments(plan, graph, output, "mpeg4"));
        auto metadata =
            QJsonDocument::fromJson(run(probe, {"-v", "error", "-count_frames", "-show_streams",
                                                "-of", "json", output}))
                .object();
        const auto streams = metadata["streams"].toArray();
        QCOMPARE(streams.size(), 2);
        QCOMPARE(streams[0].toObject()["nb_read_frames"].toString(), QString("60"));
        auto frame = [&](double sec) {
            QImage im;
            im.loadFromData(
                run(ffmpeg, {"-v", "error", "-ss", QString::number(sec), "-i", output, "-frames:v",
                             "1", "-threads", "1", "-f", "image2pipe", "-c:v", "png", "pipe:1"}),
                "PNG");
            return im;
        };
        auto redFrame = frame(.1), blueFrame = frame(.7);
        QVERIFY(!redFrame.isNull() && !blueFrame.isNull());
        QVERIFY(redFrame.pixelColor(80, 45).red() > 180);
        QVERIFY(blueFrame.pixelColor(80, 45).blue() > 180);
        QVERIFY(blueFrame.pixelColor(10, 10).red() > 180);
        QVERIFY(frame(1.5).pixelColor(80, 45).red() > 180);
        const auto pcm = run(ffmpeg, {"-v", "error", "-i", output, "-map", "0:a:0", "-f", "s16le",
                                      "-acodec", "pcm_s16le", "pipe:1"});
        QVERIFY(pcm.size() > 100000);
        bool sound = false;
        for (char x : pcm)
            if (x != 0) {
                sound = true;
                break;
            }
        QVERIFY(sound);
        auto renderVariant = [&](const Project &project, const QString &name) {
            const auto plan = compileRender(project, dir.path(), 160, 90);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            const auto out = dir.filePath(name + ".mp4");
            run(ffmpeg, renderArguments(plan, graph, out, "mpeg4"));
            return out;
        };
        auto energy = [&](const QString &path) {
            const auto data = run(ffmpeg, {"-v", "error", "-i", path, "-map", "0:a:0", "-f",
                                           "s16le", "-acodec", "pcm_s16le", "pipe:1"});
            qint64 sum = 0;
            for (int i = 0; i + 1 < data.size(); i += 2) {
                const auto sample = qint16(quint8(data[i]) | (quint16(quint8(data[i + 1])) << 8));
                sum += std::abs(int(sample));
            }
            return sum;
        };
        auto picture = [&](const QString &path) {
            QImage image;
            image.loadFromData(
                run(ffmpeg, {"-v", "error", "-ss", "0.7", "-i", path, "-frames:v", "1", "-threads",
                             "1", "-f", "image2pipe", "-c:v", "png", "pipe:1"}),
                "PNG");
            return image;
        };
        auto hidden = p;
        hidden.trackSettings[1].hidden = true;
        const auto hiddenFile = renderVariant(hidden, "hidden");
        QVERIFY(picture(hiddenFile).pixelColor(80, 45).red() > 180);
        QVERIFY(energy(hiddenFile) > 100000);
        auto muted = p;
        muted.trackSettings[0].muted = true;
        QVERIFY(energy(renderVariant(muted, "muted")) < 1000);
        auto solo = p;
        solo.trackSettings[1].solo = true;
        QVERIFY(energy(renderVariant(solo, "solo")) < 1000);
        auto audioOnly = p;
        audioOnly.clips.resize(1);
        audioOnly.clips[0].audioOnly = true;
        const auto audioFile = renderVariant(audioOnly, "audio-only");
        QVERIFY(picture(audioFile).pixelColor(80, 45).red() < 10);
        QVERIFY(energy(audioFile) > 100000);
        FrameProvider frames;
        Editor ed(&frames);
        const auto sourceProject = dir.filePath("source.cutlery");
        saveProject(p, sourceProject);
        QVERIFY(ed.openProject(QUrl::fromLocalFile(sourceProject)));
        ed.select("base");
        ed.detachAudio();
        QCOMPARE(ed.project().tracks, 4);
        QVERIFY(ed.project().clips[0].muted);
        QVERIFY(ed.project().clips.last().audioOnly);
        QCOMPARE(ed.project().clips.last().sourceIn, p.clips[0].sourceIn);
        ed.undo();
        QCOMPARE(ed.project().json(), p.json());
        ed.save(QUrl::fromLocalFile(sourceProject));
        RenderOptions stillOptions;
        stillOptions.audio = false;
        plan = compileRender(p, dir.path(), 160, 90, stillOptions);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(plan.graph.toUtf8());
        file.close();
        QImage preview;
        preview.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", .7)), "PNG");
        QVERIFY(!preview.isNull());
        QVERIFY(preview.pixelColor(80, 45).blue() > 180);
    }
};
QTEST_MAIN(EngineTest)
#include "test_engine.moc"
