#include "Editor.h"
#include "Project.h"
#include "RenderGraph.h"
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
        editor.renderPlayback();
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 30000);
        QVERIFY2(!editor.state()["playbackUrl"].toString().isEmpty(),
                 qPrintable(editor.state()["error"].toString()));
        editor.setClip("opacity", .5);
        QVERIFY(editor.state()["playbackUrl"].toString().isEmpty());
        const auto cancelled = dir.filePath("cancelled.mp4");
        editor.exportVideo(QUrl::fromLocalFile(cancelled), "mpeg4");
        editor.cancelJob();
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 15000);
        QVERIFY(!QFileInfo::exists(cancelled));
        QCOMPARE(QDir(dir.path()).entryList({".cutlery-*"}, QDir::Files | QDir::Hidden).size(), 0);
        QVERIFY(editor.save(QUrl::fromLocalFile(dir.filePath("jobs.cutlery"))));
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
        plan = compileRender(p, dir.path(), 160, 90, false);
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
