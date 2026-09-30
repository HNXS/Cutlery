#include "Editor.h"
#include "KeyboardShortcuts.h"
#include "MediaAnalysis.h"
#include "Project.h"
#include "RenderGraph.h"
#include "Thumbnails.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>
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
