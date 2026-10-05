#include "Editor.h"
#include "AiJobs.h"
#include "Captions.h"
#include "KeyboardShortcuts.h"
#include "MediaAnalysis.h"
#include "Project.h"
#include "RenderGraph.h"
#include "Scopes.h"
#include "SoundLibrary.h"
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
    void subtitleFormats() {
        // WebVTT: header, a note, identifiers, short and long times, cue settings, tags and
        // entities.
        const auto vtt = parseVtt(QString(QChar(0xfeff)) +
                                  "WEBVTT - demo\r\nKind: captions\r\n\r\nNOTE written by hand\n\n"
                                  "intro\n00:01.000 --> 00:02.500 align:start line:80%\n"
                                  "<v Ann><b>Hi</b> &amp; welcome</v>\n\n"
                                  "01:00:00.250 --> 01:00:01.000\nfirst\nsecond\n\n");
        QCOMPARE(vtt.size(), 2);
        QCOMPARE(vtt[0].start, 1.);
        QCOMPARE(vtt[0].end, 2.5);
        QCOMPARE(vtt[0].text, QString("Hi & welcome"));
        QCOMPARE(vtt[1].start, 3600.25);
        QCOMPARE(vtt[1].text, QString("first\nsecond"));
        QVERIFY_EXCEPTION_THROWN(parseVtt("1\n00:00:01,000 --> 00:00:02,000\nx"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(parseVtt("WEBVTT\n\nnot a time\ntext"), std::runtime_error);
        // ASS: the Format line decides the field order; commas in the text, override tags,
        // line breaks and comments.
        const auto ass = parseAss("[Script Info]\nTitle: x\n\n[V4+ Styles]\nFormat: Name\n"
                                  "Style: Default\n\n[Events]\n"
                                  "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
                                  "Comment: 0,0:00:00.00,0:00:09.00,Default,,0,0,0,,ignored\n"
                                  "Dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,later\n"
                                  "Dialogue: 0,0:00:01.50,0:00:02.25,Default,,0,0,0,,{\\i1}Well,{\\i0} yes\\Nand\\hno\n");
        QCOMPARE(ass.size(), 2);
        QCOMPARE(ass[0].start, 1.5);
        QCOMPARE(ass[0].end, 2.25);
        QCOMPARE(ass[0].text, QString("Well, yes\nand no"));
        QCOMPARE(ass[1].text, QString("later"));
        QVERIFY_EXCEPTION_THROWN(parseAss("[Events]\nDialogue: 0,1:00,2:00,x"), std::runtime_error);
        QVERIFY(parseSubtitles("1\n00:00:01,000 --> 00:00:02,000\nx\n", "SRT").size() == 1);

        // Through the editor: each format round-trips the captions' timing and text.
        for (const auto &suffix : {QString("vtt"), QString("ass"), QString("srt")}) {
            FrameProvider frames;
            Editor e(&frames);
            QTemporaryDir dir;
            const auto input = dir.filePath("in." + suffix);
            QFile f(input);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(writeSubtitles({{0.5, 1.5, "Hello, {world} <&>"}, {2, 3.25, "two\nlines"}}, suffix)
                        .toUtf8());
            f.close();
            e.importSrt(QUrl::fromLocalFile(input));
            QVERIFY2(e.state()["error"].toString().isEmpty(), qPrintable(e.state()["error"].toString()));
            QCOMPARE(e.project().clips.size(), 2);
            QCOMPARE(e.project().clips[0].start, qint64(15));
            QCOMPARE(e.project().clips[0].duration, qint64(30));
            QCOMPARE(e.project().clips[0].text, QString("Hello, {world} <&>"));
            QCOMPARE(e.project().clips[1].text, QString("two\nlines"));
            const auto output = dir.filePath("out." + suffix);
            QVERIFY(e.exportSrt(QUrl::fromLocalFile(output)));
            const auto back = parseSubtitles(readUtf8File(output), suffix);
            QCOMPARE(back.size(), 2);
            QCOMPARE(back[1].start, 2.);
            QVERIFY(std::abs(back[1].end - 98 / 30.) < 0.011); // 3.25 s rounds to frame 98 at 30 fps
            QCOMPARE(back[0].text, QString("Hello, {world} <&>"));
            // FFmpeg reads the file too (players and platforms use the same formats).
            const auto ffmpeg = Editor::executable("ffmpeg");
            if (!ffmpeg.isEmpty()) {
                const auto converted = QString::fromUtf8(
                    run(ffmpeg, {"-v", "error", "-i", output, "-f", "srt", "pipe:1"}));
                QVERIFY2(converted.contains("00:00:02,000 --> 00:00:03,2") && converted.contains("lines"),
                         qPrintable(converted));
            }
        }
        // The ASS style uses the canvas size and the caption's font.
        const auto styled = writeSubtitles({{0, 1, "x"}}, "ass", 1080, 1920, "Inter, Bold", 64);
        QVERIFY(styled.contains("PlayResX: 1080\nPlayResY: 1920"));
        QVERIFY(styled.contains("Style: Default,Inter Bold,64,"));
        QVERIFY(styled.contains("Dialogue: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,x\n"));
        QVERIFY(writeSubtitles({{0, 1, "x"}}, "vtt").startsWith("WEBVTT\n\n00:00:00.000 --> 00:00:01.000\nx\n"));
    }
    void videoScopes() {
        // Left half black, right half pure red.
        QImage picture(640, 360, QImage::Format_RGB32);
        picture.fill(Qt::black);
        for (int y = 0; y < 360; ++y)
            for (int x = 320; x < 640; ++x)
                picture.setPixel(x, y, qRgb(255, 0, 0));
        auto bright = [](const QImage &i, int x, int y) { return qGray(i.pixel(x, y)); };
        // Histogram: levels at black (left) and full red (right edge), little in the middle.
        const auto histogram = renderScope(picture, "histogram");
        QCOMPARE(histogram.size(), QSize(256, 128));
        QVERIFY(QColor(histogram.pixel(255, 100)).red() > 100);
        QVERIFY(bright(histogram, 0, 100) > 60);
        QVERIFY(bright(histogram, 128, 100) < 40);
        // Waveform: black at the bottom on the left; red's luma (about 21 %) on the right.
        const auto waveform = renderScope(picture, "waveform");
        QCOMPARE(waveform.size(), QSize(256, 128));
        QVERIFY(QColor(waveform.pixel(60, 127)).green() > 150);
        const int redRow = 127 - qRound(0.2126 * 127);
        QVERIFY(QColor(waveform.pixel(200, redRow)).green() > 150);
        QVERIFY(QColor(waveform.pixel(60, redRow)).green() < 60);
        QVERIFY(QColor(waveform.pixel(200, 10)).green() < 60);
        // Vectorscope: black in the centre, red towards its target (right of up-left... Cr up,
        // Cb slightly left), drawn in red.
        const auto vectors = renderScope(picture, "vectorscope");
        QCOMPARE(vectors.size(), QSize(192, 192));
        QVERIFY(bright(vectors, 96, 96) > 40);
        const double radius = 96 - 6;
        const QPoint red(qRound(96 + (-0.1146 * 255) / 128 * radius), qRound(96 - (0.5 * 255) / 128 * radius));
        const QColor at(vectors.pixel(red));
        QVERIFY2(at.red() > 150 && at.red() > at.green() + 60, qPrintable(at.name()));
        // A grey picture keeps the vectorscope empty away from the centre.
        QImage grey(64, 64, QImage::Format_RGB32);
        grey.fill(QColor(128, 128, 128));
        QVERIFY(bright(renderScope(grey, "vectorscope"), red.x(), red.y()) < 40);
        // Unknown kinds and empty pictures give an empty scope.
        QVERIFY(!renderScope({}, "waveform").isNull());
        QCOMPARE(renderScope(picture, "nonsense").size(), QSize(256, 128));
        // Through the frame provider, which the viewer uses.
        FrameProvider frames;
        frames.frame = picture;
        QSize size;
        QCOMPARE(frames.requestImage("scope/histogram/still/1", &size, {}), histogram);
        QCOMPARE(size, QSize(256, 128));
        frames.live = grey;
        QCOMPARE(frames.requestImage("scope/vectorscope/live/2", &size, {}), renderScope(grey, "vectorscope"));
        QCOMPARE(frames.requestImage("7", &size, {}), picture);
    }
    void soundEffects() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        constexpr int rate = 48000;
        // Mean absolute level of a channel (0 left, 1 right) between two times.
        auto level = [](const QVector<float> &x, int channel, double from, double to) {
            double sum = 0;
            qsizetype n = 0;
            for (qsizetype i = qsizetype(from * rate); i < qsizetype(to * rate) && 2 * i + 1 < x.size(); ++i, ++n)
                sum += std::abs(x[2 * i + channel]);
            return n ? sum / n : 0.;
        };
        // Times where the sound jumps from quiet to loud (key and button hits).
        auto hits = [&](const QVector<float> &x) {
            QVector<double> times;
            const double window = 0.004;
            double before = 0;
            for (double t = 0; t + window < x.size() / 2. / rate; t += window) {
                const double now = level(x, 0, t, t + window) + level(x, 1, t, t + window);
                if (now > 0.05 && now > 4 * before && (times.isEmpty() || t - times.last() > 0.03))
                    times << t;
                before = now;
            }
            return times;
        };
        const auto library = soundLibrary(dir.filePath("generated"), {});
        QStringList ids;
        for (const auto &s : library) {
            QVERIFY(s.builtIn && !s.name.isEmpty() && !s.licence.isEmpty());
            ids << s.id;
            const auto samples = synthesizeSound(s.id, rate);
            QCOMPARE(samples.size(), qsizetype(std::round(s.seconds * rate)) * 2);
            float peak = 0;
            for (const auto v : samples)
                peak = std::max(peak, std::abs(v));
            QVERIFY2(std::abs(peak - 0.7f) < 0.01f, qPrintable(s.id));
            QCOMPARE(synthesizeSound(s.id, rate), samples); // the same every time
        }
        QCOMPARE(ids, (QStringList{"click", "double-click", "typing-short", "typing", "typing-long",
                                   "swoosh", "swoosh-slow"}));
        QVERIFY(synthesizeSound("nope").isEmpty());
        // A click: the press at the start, a softer release, then silence.
        const auto click = synthesizeSound("click", rate);
        const auto clickHits = hits(click);
        QVERIFY2(clickHits.size() == 2 && clickHits[0] < 0.01 && std::abs(clickHits[1] - 0.09) < 0.015,
                 qPrintable(QString::number(clickHits.size())));
        QVERIFY(level(click, 0, 0.2, 0.25) < 0.001);
        const auto doubled = hits(synthesizeSound("double-click", rate));
        QCOMPARE(doubled.size(), 4);
        QVERIFY(std::abs(doubled[2] - doubled[0] - 0.16) < 0.01);
        // Typing: between about 4 and 12 key presses a second (each press has a release).
        const auto typing = hits(synthesizeSound("typing", rate));
        QVERIFY2(typing.size() >= 2 * 5 * 3 && typing.size() <= 2 * 5 * 12, qPrintable(QString::number(typing.size())));
        // A swoosh rises and falls, moves from left to right, and is brighter in the middle.
        const auto swoosh = synthesizeSound("swoosh", rate);
        QVERIFY(level(swoosh, 0, 0.2, 0.3) > 3 * level(swoosh, 0, 0, 0.02));
        QVERIFY(level(swoosh, 0, 0.6, 0.7) < level(swoosh, 0, 0.2, 0.3) / 3);
        QVERIFY(level(swoosh, 0, 0.05, 0.15) > 1.5 * level(swoosh, 1, 0.05, 0.15));
        QVERIFY(level(swoosh, 1, 0.5, 0.6) > 1.5 * level(swoosh, 0, 0.5, 0.6));

        // Files: written once as WAV that FFmpeg reads at the right length.
        auto sound = library[0];
        ensureSoundFile(sound);
        QVERIFY(QFileInfo(sound.path).size() > 1000);
        const auto modified = QFileInfo(sound.path).lastModified();
        ensureSoundFile(sound);
        QCOMPARE(QFileInfo(sound.path).lastModified(), modified);
        const auto pcm = run(ffmpeg, {"-v", "error", "-i", sound.path, "-f", "f32le", "-ac", "2", "-ar", "48000", "pipe:1"});
        QCOMPARE(pcm.size(), click.size() * 4);

        // A recorded pack: entries need an id, name, licence and a plain file name that exists.
        const auto pack = dir.filePath("pack");
        QVERIFY(QDir().mkpath(pack));
        QVERIFY(QFile::copy(sound.path, pack + "/rec.wav"));
        {
            QFile f(pack + "/sounds.json");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(R"([{"id":"rec","file":"rec.wav","name":"Recorded click","category":"Clicks","seconds":0.25,"licence":"CC0 1.0","source":"Test"},
                        {"id":"gone","file":"gone.wav","name":"Missing","licence":"CC0 1.0"},
                        {"id":"up","file":"../rec.wav","name":"Outside","licence":"CC0 1.0"},
                        {"id":"free","file":"rec.wav","name":"No licence"}])");
        }
        const auto withPack = soundLibrary(dir.filePath("generated"), pack);
        QCOMPARE(withPack.size(), library.size() + 1);
        QCOMPARE(withPack.last().id, QString("pack:rec"));
        QCOMPARE(withPack.last().licence, QString("CC0 1.0"));
        QVERIFY(!withPack.last().builtIn);

        // In the editor: at the playhead on a free track, once; and a swoosh at each transition.
        qputenv("CUTLERY_SOUNDS_DIR", pack.toUtf8());
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        QCOMPARE(editor.sounds().size(), library.size() + 1);
        QImage still(160, 90, QImage::Format_RGB32);
        still.fill(Qt::darkGreen);
        QVERIFY(still.save(dir.filePath("still.png")));
        editor.importMedia({QUrl::fromLocalFile(dir.filePath("still.png"))});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        const auto image = editor.project().assets.first().id;
        editor.addAsset(image);
        editor.addAsset(image);
        editor.addAsset(image);
        QCOMPARE(editor.project().clips.size(), size_t(3));
        editor.seek(30);
        editor.addSound("click");
        const auto added = editor.project().clips.back();
        QCOMPARE(added.start, 30);
        QCOMPARE(added.duration, 7); // 0.25 s
        QVERIFY(added.track != 0); // track 0 is taken by the pictures
        QCOMPARE(editor.project().asset(added.assetId)->kind, QString("audio"));
        QVERIFY(editor.soundFile("click").isLocalFile());
        editor.addSound("click");
        QVERIFY(editor.state()["error"].toString().contains("already"));
        editor.clearError();
        editor.addSound("pack:rec");
        QCOMPARE(editor.project().assets.size(), size_t(3));
        editor.undo();
        editor.undo();
        QCOMPARE(editor.project().clips.size(), size_t(3));
        editor.addSoundAtTransitions("swoosh");
        QVERIFY(editor.state()["error"].toString().contains("No transitions"));
        editor.clearError();
        const auto second = editor.project().clips[1], third = editor.project().clips[2];
        editor.select(second.id);
        editor.setClip("transition", "fade");
        editor.select(third.id);
        editor.setClip("transition", "fade");
        editor.addSoundAtTransitions("swoosh");
        QVERIFY2(editor.state()["error"].toString().isEmpty(), qPrintable(editor.state()["error"].toString()));
        QCOMPARE(editor.project().clips.size(), size_t(5));
        const qint64 lead = qRound64(0.32 * 0.7 * 30);
        QCOMPARE(editor.project().clips[3].start, second.start - lead);
        QCOMPARE(editor.project().clips[4].start, third.start - lead);
        editor.addSoundAtTransitions("swoosh");
        QVERIFY(editor.state()["error"].toString().contains("already"));
        editor.clearError();
        // The swoosh is heard in the rendered mix.
        RenderOptions options;
        options.video = false;
        const auto plan = compileRender(editor.project(), dir.filePath("work"), 160, 90, options);
        const auto graph = dir.filePath("graph.txt");
        {
            QFile g(graph);
            QVERIFY(g.open(QIODevice::WriteOnly));
            g.write(plan.graph.toUtf8());
        }
        QStringList args{"-v", "error"};
        args += plan.inputs;
        args << "-filter_complex_script" << graph << "-map" << "[aout]" << "-f" << "f32le" << "-ac" << "2" << "-ar" << "48000" << "pipe:1";
        const auto mixed = run(ffmpeg, args);
        QVector<float> mix(mixed.size() / 4);
        std::memcpy(mix.data(), mixed.constData(), size_t(mix.size()) * 4);
        const double cut = second.start / 30.;
        QVERIFY(level(mix, 0, cut - 0.05, cut + 0.05) > 20 * std::max(1e-5, level(mix, 0, 0.5, 1)));
        qunsetenv("CUTLERY_SOUNDS_DIR");
    }
    void recordedSoundPack() {
        // The pack Get-Sounds.ps1 downloads (CI sets CUTLERY_TEST_SOUNDS to its folder).
        const auto pack = qEnvironmentVariable("CUTLERY_TEST_SOUNDS");
        if (pack.isEmpty())
            QSKIP("Recorded sound pack not available");
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        QVector<Sound> recorded;
        for (const auto &s : soundLibrary(dir.path(), pack))
            if (!s.builtIn)
                recorded << s;
        QFile manifest(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/tools/sound-pack.json");
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        QCOMPARE(recorded.size(), QJsonDocument::fromJson(manifest.readAll())["sounds"].toArray().size());
        for (const auto &s : recorded) {
            QVERIFY2(s.licence.startsWith("CC0") && !s.source.isEmpty(), qPrintable(s.id));
            // Decodes at the stated length, loudest at the stated moment (keyboard: anywhere).
            const auto pcm = run(ffmpeg, {"-v", "error", "-i", s.path, "-ac", "1", "-ar", "8000", "-f", "f32le", "pipe:1"});
            const auto n = pcm.size() / 4;
            QVERIFY2(std::abs(n / 8000. - s.seconds) < 0.05, qPrintable(s.id));
            const auto *x = reinterpret_cast<const float *>(pcm.constData());
            double best = 0, at = 0;
            for (qsizetype i = 0; i + 80 <= n; i += 40) {
                double e = 0;
                for (int k = 0; k < 80; ++k)
                    e += x[i + k] * x[i + k];
                if (e > best) {
                    best = e;
                    at = (i + 40) / 8000.;
                }
            }
            QVERIFY2(best > 0.01, qPrintable(s.id));
            if (s.category != "Keyboard")
                QVERIFY2(std::abs(at - s.peak) < 0.03, qPrintable(QString("%1 %2").arg(s.id).arg(at)));
        }
        // A recorded whoosh at a transition, loudest at the cut.
        qputenv("CUTLERY_SOUNDS_DIR", pack.toUtf8());
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.addTitle();
        editor.addTitle();
        auto clips = editor.project().clips;
        QCOMPARE(clips.size(), size_t(2));
        editor.select(clips[1].id);
        editor.setClip("start", clips[0].start + clips[0].duration);
        editor.setClip("track", clips[0].track);
        editor.setClip("transition", "fade");
        editor.addSoundAtTransitions("pack:elements-whoosh");
        QVERIFY2(editor.state()["error"].toString().isEmpty(), qPrintable(editor.state()["error"].toString()));
        const auto cut = editor.project().clip(clips[1].id)->start;
        QCOMPARE(editor.project().clips.back().start, cut - qRound64(0.165 * 30));
        qunsetenv("CUTLERY_SOUNDS_DIR");
    }
    void recentProjectsAndBackups() {
        QTemporaryDir dir;
        const auto path = dir.filePath("talk.cutlery");
        {
            FrameProvider frames;
            Editor e(&frames);
            e.addTitle();
            QVERIFY(e.save(QUrl::fromLocalFile(path)));
            // The first save has nothing to keep.
            QVERIFY(e.backups().isEmpty());
            e.addTitle();
            QVERIFY(e.save());
            e.addTitle();
            QVERIFY(e.save());
            const auto versions = e.backups();
            QCOMPARE(versions.size(), 2);
            // Newest first: the version with two titles, then the one with one.
            const auto older = versions[1].toMap()["file"].toString();
            QCOMPARE(loadProject(older).clips.size(), size_t(1));
            QCOMPARE(loadProject(versions[0].toMap()["file"].toString()).clips.size(), size_t(2));
            const auto recent = e.state()["recent"].toList();
            QVERIFY(!recent.isEmpty());
            QCOMPARE(recent[0].toMap()["path"].toString(), QFileInfo(path).absoluteFilePath());
            QCOMPARE(recent[0].toMap()["name"].toString(), QString("talk"));
            // Restoring puts the old version back and keeps the current one as a version.
            QVERIFY(e.restoreBackup(older));
            QCOMPARE(e.project().clips.size(), size_t(1));
            QCOMPARE(loadProject(path).clips.size(), size_t(1));
            QCOMPARE(e.backups().size(), 3);
            QCOMPARE(loadProject(e.backups()[0].toMap()["file"].toString()).clips.size(), size_t(3));
            // Only this project's backups can be restored.
            QVERIFY(!e.restoreBackup(path));
            QVERIFY(e.state()["error"].toString().contains("not a backup"));
            e.clearError();
            // At most 20 versions are kept.
            for (int i = 0; i < 22; ++i) {
                e.addTitle();
                QVERIFY(e.save());
            }
            QCOMPARE(e.backups().size(), 20);
        }
        {
            // The list survives a restart; a missing project is reported and dropped.
            FrameProvider frames;
            Editor e(&frames);
            QCOMPARE(e.state()["recent"].toList()[0].toMap()["path"].toString(), QFileInfo(path).absoluteFilePath());
            QVERIFY(e.openRecent(QFileInfo(path).absoluteFilePath()));
            QVERIFY(QFile::rename(path, path + ".moved"));
            QVERIFY(!e.openRecent(QFileInfo(path).absoluteFilePath()));
            QVERIFY(e.state()["error"].toString().contains("no longer there"));
            for (const auto &r : e.state()["recent"].toList())
                QVERIFY(r.toMap()["path"].toString() != QFileInfo(path).absoluteFilePath());
        }
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
        QCOMPARE(keys.bindings().size(), 44);
        QVERIFY(!keys.assign("play", "Ctrl+B"));
        QVERIFY(keys.error().contains("Already assigned"));
        QVERIFY(!keys.assign("play", "Ctrl+NotARealKey"));
        QVERIFY(keys.assign("split", ""));
        QVERIFY(keys.assign("play", "Ctrl+B"));
        KeyboardShortcuts loaded(path);
        QCOMPARE(loaded.bindings(), keys.bindings());
        QVERIFY(loaded.reset());
        auto play = [](const KeyboardShortcuts &k) {
            for (const auto &b : k.bindings())
                if (b.toMap()["id"] == "play")
                    return b.toMap()["sequence"].toString();
            return QString();
        };
        QCOMPARE(play(loaded), QString("Space"));
        KeyboardShortcuts failed(dir.path());
        QVERIFY(!failed.assign("play", "Ctrl+J"));
        QCOMPARE(play(failed), QString("Space"));
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
        // Every transition renders, and some frame inside it differs from both clips' colours
        // (a zoom, for instance, shows only the outgoing clip at its middle).
        for (const auto &[type, name] : transitionTypes()) {
            auto t = p;
            t.clips[1].transition = type;
            bool mixed = false;
            for (qint64 f : {29, 31, 33}) {
                const auto frame = still(t, f, true);
                QVERIFY2(!frame.isNull(), qPrintable(type));
                int reds = 0, blues = 0;
                for (int y = 5; y < 90; y += 10)
                    for (int x = 5; x < 160; x += 10) {
                        const auto c = frame.pixelColor(x, y);
                        reds += c.red() > 200 && c.blue() < 40;
                        blues += c.blue() > 200 && c.red() < 40;
                    }
                mixed = mixed || (reds < 144 && blues < 144);
            }
            QVERIFY2(mixed, qPrintable(type));
        }
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
    void eyeContact() {
#if defined(CUTLERY_AI_WORKER) && defined(CUTLERY_TEST_MODELS)
        const QString models = CUTLERY_TEST_MODELS;
        for (const auto *name :
             {"face_detection_short_range.onnx", "face_landmark.onnx", "iris_landmark.onnx"})
            if (!QFileInfo::exists(models + "/" + name))
                QSKIP("The face models are not in the test models folder");
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // Half a second of a presenter photo (public domain, see tests/fixtures/README.md):
        // looking into the camera, and the same face looking to the side.
        auto video = [&](const QString &name) {
            const auto out = dir.filePath(name + ".mkv");
            run(ffmpeg, {"-v", "error", "-loop", "1", "-i",
                         QString(CUTLERY_SOURCE_DIR) + "/tests/fixtures/face-" + name + ".jpg",
                         "-t", "0.5", "-r", "10", "-c:v", "ffv1", out});
            return out;
        };
        // The worker's report: the shifts applied to both eyes in the last frame, in eye widths.
        auto correct = [&](const QString &input, const QString &output) {
            const auto report = output + ".txt";
            run(CUTLERY_AI_WORKER,
                {"eyecontact", "--ffmpeg", ffmpeg, "--model", models + "/face_landmark.onnx",
                 "--input", input, "--output", output, "--source", "960x540", "--rate", "10/1",
                 "--cpu", "1", "--report", report});
            QFile f(report);
            if (!f.open(QIODevice::ReadOnly))
                throw std::runtime_error("No eye-contact report");
            const auto lines = QString::fromUtf8(f.readAll()).trimmed().split('\n');
            const auto last = lines.last().split(' ');
            if (lines.size() != 5 || last.size() != 6 || last[1] != "1")
                throw std::runtime_error(("Unexpected report: " + lines.join('|')).toStdString());
            return std::array<double, 4>{last[2].toDouble(), last[3].toDouble(),
                                         last[4].toDouble(), last[5].toDouble()};
        };
        // Already looking into the camera: left alone.
        const auto straight = correct(video("straight"), dir.filePath("straight.mov"));
        for (double v : straight)
            QVERIFY2(std::abs(v) < 0.02, qPrintable(QString::number(v)));
        // Looking to the side: both eyes turn the same way, and a second pass finds at most
        // two thirds of the first correction left.
        const auto side = correct(video("side"), dir.filePath("side.mov"));
        QVERIFY2(std::abs(side[0]) > 0.04 && std::abs(side[2]) > 0.04 && side[0] * side[2] > 0,
                 qPrintable(QString("%1 %2").arg(side[0]).arg(side[2])));
        const auto again = correct(dir.filePath("side.mov"), dir.filePath("again.mov"));
        QVERIFY2(std::abs(again[0]) < 0.67 * std::abs(side[0]) &&
                     std::abs(again[2]) < 0.67 * std::abs(side[2]),
                 qPrintable(QString("%1 %2").arg(again[0]).arg(again[2])));

        // Through the editor: the corrected picture replaces the source in an export, and only
        // around the eyes.
        qputenv("CUTLERY_AI_WORKER", CUTLERY_AI_WORKER);
        qputenv("CUTLERY_AI_MODELS", models.toUtf8());
        FrameProvider frames;
        Editor editor(&frames);
        qunsetenv("CUTLERY_AI_WORKER");
        qunsetenv("CUTLERY_AI_MODELS");
        QCOMPARE(editor.state()["aiMissing"].toMap()["eyecontact"].toString(), QString());
        editor.configure(960, 540, 10, 1);
        editor.importMedia({QUrl::fromLocalFile(dir.filePath("side.mkv"))});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        editor.select(editor.project().clips.first().id);
        editor.setClip("eyeContact", true);
        editor.runAi("eyecontact");
        QTRY_COMPARE_WITH_TIMEOUT(
            editor.state()["selected"].toMap()["eyeContactInfo"].toMap()["status"].toString(),
            QString("ready"), 120000);
        QVERIFY(editor.state()["selected"].toMap()["eyeContactInfo"].toMap()["covered"].toBool());
        const auto out = dir.filePath("export.mov");
        editor.exportWith(QUrl::fromLocalFile(out), {{"format", "prores"}});
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 60000);
        QVERIFY2(QFileInfo::exists(out), qPrintable(editor.state()["error"].toString()));
        auto still = [&](const QString &file) {
            QImage image;
            image.loadFromData(run(ffmpeg, {"-v", "error", "-i", file, "-frames:v", "1", "-f",
                                            "image2pipe", "-c:v", "png", "-"}),
                               "PNG");
            return image.convertToFormat(QImage::Format_RGB32);
        };
        const auto before = still(dir.filePath("side.mkv")), after = still(out);
        auto difference = [&](QRect r) {
            double sum = 0;
            for (int y = r.top(); y <= r.bottom(); ++y)
                for (int x = r.left(); x <= r.right(); ++x)
                    sum += std::abs(qGray(before.pixel(x, y)) - qGray(after.pixel(x, y)));
            return sum / (r.width() * r.height());
        };
        // The eyes sit around y = 190 in the fixture; the mouth and background stay.
        const double eyes = difference({340, 160, 280, 60}), mouth = difference({380, 360, 200, 60}),
                     corner = difference({10, 10, 100, 100});
        QVERIFY2(eyes > 1.5 && mouth < 1.5 && corner < 1.5,
                 qPrintable(QString("eyes %1 mouth %2 corner %3").arg(eyes).arg(mouth).arg(corner)));
#else
        QSKIP("Needs the AI worker and the AI pack's models (CUTLERY_TEST_MODELS)");
#endif
    }
    void followFace() {
#if defined(CUTLERY_AI_WORKER) && defined(CUTLERY_TEST_MODELS)
        const QString models = CUTLERY_TEST_MODELS;
        if (!QFileInfo::exists(models + "/face_detection_short_range.onnx"))
            QSKIP("The face models are not in the test models folder");
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // The fixture face (about 13 % of the frame wide) moving from left to right over 2 s on
        // a grey 1280 × 720 frame.
        const auto source = dir.filePath("moving.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=gray:s=1280x720:r=25:d=2", "-i",
                     QString(CUTLERY_SOURCE_DIR) + "/tests/fixtures/face-straight.jpg",
                     "-filter_complex", "[1]scale=480:-2[f];[0][f]overlay=x='100+t*300':y=100",
                     "-c:v", "ffv1", source});
        qputenv("CUTLERY_AI_WORKER", CUTLERY_AI_WORKER);
        qputenv("CUTLERY_AI_MODELS", models.toUtf8());
        FrameProvider frames;
        Editor editor(&frames);
        qunsetenv("CUTLERY_AI_WORKER");
        qunsetenv("CUTLERY_AI_MODELS");
        editor.configure(1280, 720, 25, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        editor.seek(0);
        editor.addEffect("blur");
        const auto area = editor.state()["selectedId"].toString();
        editor.setClip("duration", 50);
        // Start roughly over the face; the face's centre at t = 0 is near x = 0.27, y = 0.33.
        editor.setClipValues({{"x", -0.2}, {"y", -0.15}});
        editor.followFace();
        QTRY_COMPARE_WITH_TIMEOUT(editor.state()["follow"].toMap()["status"].toString(),
                                  QString("done"), 120000);
        const auto *c = editor.project().clip(area);
        QVERIFY(c->keyframes.value("x").size() >= 10);
        // The face's centre moves 300 px/s = 0.234 of the width per second.
        const double x0 = c->valueAt("x", 3), x1 = c->valueAt("x", 40);
        QVERIFY2(std::abs((x1 - x0) - 0.234 * 37 / 25.) < 0.04,
                 qPrintable(QString("%1 → %2").arg(x0).arg(x1)));
        QVERIFY2(std::abs(c->valueAt("y", 3) - c->valueAt("y", 40)) < 0.02, "steady height");
        // Sized to the face (about 7 % of the width) with room.
        QVERIFY2(c->effectWidth > 0.07 && c->effectWidth < 0.3,
                 qPrintable(QString::number(c->effectWidth)));
        // One undo step removes it.
        editor.undo();
        QVERIFY(editor.project().clip(area)->keyframes.isEmpty());
        // Nothing to follow without a video below.
        editor.newProject();
        editor.addTitle();
        editor.followFace();
        QVERIFY(editor.state()["error"].toString().contains("video clip"));
#else
        QSKIP("Needs the AI worker and the AI pack's models (CUTLERY_TEST_MODELS)");
#endif
    }
    void reframeToVertical() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // Red 16:9 video with a green bar in the middle, a small picture in a corner and a title.
        const auto source = dir.filePath("wide.mkv"), corner = dir.filePath("corner.png");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i",
                     "color=c=red:s=1280x720:r=25:d=2,drawbox=x=600:y=0:w=80:h=720:c=lime:t=fill",
                     "-c:v", "ffv1", source});
        QImage blue(64, 64, QImage::Format_RGB32);
        blue.fill(Qt::blue);
        QVERIFY(blue.save(corner));
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(1280, 720, 25, 1);
        editor.reframe(720, 1280);
        QVERIFY(editor.state()["error"].toString().contains("empty"));
        editor.importMedia({QUrl::fromLocalFile(source), QUrl::fromLocalFile(corner)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 2, 15000);
        const auto &assets = editor.project().assets;
        const auto video = assets[0].kind == "video" ? assets[0].id : assets[1].id,
                   image = assets[0].kind == "video" ? assets[1].id : assets[0].id;
        editor.seek(0);
        editor.addAsset(video);
        const auto wide = editor.state()["selectedId"].toString();
        editor.seek(0);
        editor.addAsset(image, 1);
        const auto small = editor.state()["selectedId"].toString();
        editor.setClipValues({{"scale", 0.2}, {"x", 0.3}, {"y", -0.3}});
        editor.addTitle();
        const auto title = editor.state()["selectedId"].toString();
        editor.reframe(721, 1280);
        QVERIFY(editor.state()["error"].toString().contains("even"));

        editor.reframe(720, 1280);
        QCOMPARE(editor.project().width, 720);
        QCOMPARE(editor.project().height, 1280);
        // 16:9 fits 720 × 405 in the tall canvas; zoomed by 1280 / 405 it fills it.
        QVERIFY(std::abs(editor.project().clip(wide)->scale - 1280. / 405) < 0.001);
        QCOMPARE(editor.project().clip(small)->scale, 0.2);
        QCOMPARE(editor.project().clip(small)->x, 0.3);
        QCOMPARE(editor.project().clip(title)->scale, 1.);
        QTRY_VERIFY_WITH_TIMEOUT(editor.state()["reframe"].toMap()["status"] == "done", 120000);
        QVERIFY(editor.state()["reframe"].toMap()["clips"].toStringList().contains(wide));

        // The picture covers the tall frame: red at the edges, the green bar in the middle.
        RenderOptions options;
        options.audio = false;
        options.from = 10;
        options.to = 11;
        auto project = editor.project();
        project.clips.erase(std::remove_if(project.clips.begin(), project.clips.end(),
                                           [&](const Clip &c) { return c.id != wide; }),
                            project.clips.end());
        const auto plan = compileRender(project, dir.filePath("work"), 180, 320, options);
        const auto graph = dir.filePath("graph.txt");
        QFile g(graph);
        QVERIFY(g.open(QIODevice::WriteOnly));
        g.write(plan.graph.toUtf8());
        g.close();
        QImage still;
        still.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
        QCOMPARE(still.size(), QSize(180, 320));
        for (const auto &point : {QPoint(2, 2), QPoint(177, 317), QPoint(2, 317)}) {
            const auto c = still.pixelColor(point);
            QVERIFY2(c.red() > 200 && c.green() < 60, qPrintable(c.name()));
        }
        const auto middle = still.pixelColor(90, 160);
        QVERIFY2(middle.green() > 200 && middle.red() < 60, qPrintable(middle.name()));
        // One undo step brings back the wide canvas and the picture.
        editor.undo();
        QCOMPARE(editor.project().width, 1280);
        QCOMPARE(editor.project().clip(wide)->scale, 1.);
    }
    void reframeFollowsFace() {
#if defined(CUTLERY_AI_WORKER) && defined(CUTLERY_TEST_MODELS)
        const QString models = CUTLERY_TEST_MODELS;
        if (!QFileInfo::exists(models + "/face_detection_short_range.onnx"))
            QSKIP("The face models are not in the test models folder");
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // The face moves from left to right, 300 px/s across 1280 px.
        const auto source = dir.filePath("moving.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=gray:s=1280x720:r=25:d=2", "-i",
                     QString(CUTLERY_SOURCE_DIR) + "/tests/fixtures/face-straight.jpg",
                     "-filter_complex", "[1]scale=480:-2[f];[0][f]overlay=x='100+t*300':y=100",
                     "-c:v", "ffv1", source});
        qputenv("CUTLERY_AI_WORKER", CUTLERY_AI_WORKER);
        qputenv("CUTLERY_AI_MODELS", models.toUtf8());
        FrameProvider frames;
        Editor editor(&frames);
        qunsetenv("CUTLERY_AI_WORKER");
        qunsetenv("CUTLERY_AI_MODELS");
        editor.configure(1280, 720, 25, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        const auto id = editor.state()["selectedId"].toString();
        editor.reframe(720, 1280);
        QCOMPARE(editor.state()["reframe"].toMap()["status"].toString(), QString("analysing"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.state()["reframe"].toMap()["status"].toString(),
                                  QString("done"), 120000);
        QCOMPARE(editor.state()["reframe"].toMap()["faces"].toInt(), 1);
        const auto *c = editor.project().clip(id);
        QVERIFY(c->keyframes.value("x").size() >= 3);
        // The picture is 3.16 canvas widths wide, so the face's 0.234 of the source width per
        // second moves the picture 0.74 canvas widths per second the other way.
        const double x0 = c->valueAt("x", 15), x1 = c->valueAt("x", 35);
        QVERIFY2(std::abs((x1 - x0) + 0.234 * 3.16 * 20 / 25.) < 0.1,
                 qPrintable(QString("%1 → %2").arg(x0).arg(x1)));
        // The face sits in the middle of the tall frame: at 1 s its centre is at about 0.57 of
        // the source width, so the picture moves 0.07 × 3.16 to the left.
        QVERIFY2(std::abs(c->valueAt("x", 25) + 0.22) < 0.12,
                 qPrintable(QString::number(c->valueAt("x", 25))));
        // Reframing and following are separate undo steps; both undo back to the wide frame.
        editor.undo();
        QVERIFY(!editor.project().clip(id)->keyframes.contains("x"));
        editor.undo();
        QCOMPARE(editor.project().width, 1280);
#else
        QSKIP("Needs the AI worker and the AI pack's models (CUTLERY_TEST_MODELS)");
#endif
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
        jobs.start("upscale", video, .5, 1.5, "180");
        QVERIFY(jobs.busy());
        QCOMPARE(jobs.status("upscale", video, "180")["status"].toString(), QString("queued"));
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
        QVERIFY2(jobs.status("upscale", video, "180")["status"].toString() == "ready",
                 qPrintable(jobs.status("upscale", video, "180")["error"].toString()));
        const auto sharp = jobs.result("upscale", video, "180");
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
        QCOMPARE(jobs.status("upscale", video, "360")["status"].toString(), QString("none"));
        AiJobs none(dir.filePath("ai"), ffmpeg, Editor::executable("ffprobe"), CUTLERY_AI_WORKER,
                    {{"matte", dir.filePath("x.onnx")}});
        QVERIFY(!none.available("matte"));
        QVERIFY(!none.available("upscale"));
        QVERIFY(!none.missing("matte").isEmpty());
#endif
    }
    void automaticCaptions() {
        // SRT parsing: CRLF, BOM, multi-line text.
        const auto cues = parseSrt(QString(QChar(0xfeff)) +
                                   "1\r\n00:00:01,000 --> 00:00:01,500\r\nbefore\r\n\r\n"
                                   "2\n00:00:02.200 --> 00:00:03,000\nfirst\nline\n\n");
        QCOMPARE(cues.size(), 2);
        QCOMPARE(cues[1].start, 2.2);
        QCOMPARE(cues[1].text, QString("first\nline"));
        QVERIFY_EXCEPTION_THROWN(parseSrt("1\nnot a time\ntext"), std::runtime_error);
        QVERIFY(parseSrt("\n\n").isEmpty());

        // Placement: a 30 fps project; speech from a source file used by three clips.
        Project p;
        p.fpsN = 30;
        Asset a;
        a.id = "talk";
        a.kind = "video";
        a.hasAudio = true;
        a.duration = 60;
        p.assets = {a};
        Clip first; // timeline 1 s..4 s plays source 2 s..5 s
        first.id = "first";
        first.assetId = "talk";
        first.start = 30;
        first.duration = 90;
        first.sourceIn = Time(2);
        Clip fast = first; // timeline 5 s..6 s plays source 10 s..12 s at 2x
        fast.id = "fast";
        fast.start = 150;
        fast.duration = 30;
        fast.sourceIn = Time(10);
        fast.speed = Time(2);
        Clip quiet = first; // muted: no captions
        quiet.id = "quiet";
        quiet.start = 300;
        quiet.muted = true;
        Clip echo = first; // the same speech on another track: no duplicates
        echo.id = "echo";
        echo.track = 1;
        p.clips = {first, fast, quiet, echo};
        QHash<QString, QVector<Cue>> transcripts{
            {"talk",
             {{1.0, 1.5, "cut away"},   // before the trim
              {2.5, 3.5, "hello"},      // timeline 1.5 s..2.5 s
              {4.5, 5.5, "half"},       // half outside the trim: kept, clipped at 4 s
              {4.8, 6.0, "mostly out"}, // mostly outside: dropped
              {10.0, 11.0, "quick"}}}}; // timeline 5 s..5.5 s
        auto clips = captionClips(p, transcripts, 2);
        QCOMPARE(clips.size(), 3);
        QCOMPARE(clips[0].text, QString("hello"));
        QCOMPARE(clips[0].start, qint64(45));
        QCOMPARE(clips[0].duration, qint64(30));
        QCOMPARE(clips[1].text, QString("half"));
        QCOMPARE(clips[1].start + clips[1].duration, qint64(120));
        QCOMPARE(clips[2].text, QString("quick"));
        QCOMPARE(clips[2].start, qint64(150));
        QCOMPARE(clips[2].duration, qint64(15));
        QCOMPARE(clips[2].track, 2);
        // Overlapping cues never overlap on the track.
        transcripts["talk"] = {{2.5, 3.5, "a"}, {3.2, 4.0, "b"}};
        clips = captionClips(p, transcripts, 2);
        QCOMPARE(clips.size(), 2);
        QCOMPARE(clips[0].start + clips[0].duration, clips[1].start);
        // Reversed clips are not captioned.
        p.clips = {first};
        p.clips[0].reverse = true;
        QVERIFY(captionClips(p, transcripts, 2).isEmpty());

#if defined(CUTLERY_AI_WORKER) && defined(CUTLERY_WHISPER_CLI)
        // End to end with whisper.cpp's untrained test model: audio is extracted and recognised,
        // and an empty result reports that no speech was found.
        const auto ffmpeg = Editor::executable("ffmpeg");
        QTemporaryDir dir;
        const auto clip = dir.filePath("tone.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=gray:s=160x90:r=30:d=3", "-f",
                     "lavfi", "-i", "sine=f=300:d=3", "-c:v", "ffv1", "-c:a", "pcm_s16le",
                     "-shortest", clip});
        QDir().mkpath(dir.filePath("models"));
        QVERIFY(QFile::copy(CUTLERY_SOURCE_DIR "/tests/fixtures/whisper-for-tests-tiny.bin",
                            dir.filePath("models/ggml-large-v3-turbo-q5_0.bin")));
        qputenv("CUTLERY_AI_WORKER", CUTLERY_AI_WORKER);
        qputenv("CUTLERY_WHISPER", CUTLERY_WHISPER_CLI);
        qputenv("CUTLERY_AI_MODELS", dir.filePath("models").toUtf8());
        FrameProvider frames;
        Editor editor(&frames);
        qunsetenv("CUTLERY_AI_WORKER");
        qunsetenv("CUTLERY_WHISPER");
        qunsetenv("CUTLERY_AI_MODELS");
        QVERIFY2(editor.state()["aiMissing"].toMap()["transcribe"].toString().isEmpty(),
                 qPrintable(editor.state()["aiMissing"].toMap()["transcribe"].toString()));
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(clip)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        const int tracks = editor.project().tracks;
        editor.generateCaptions("en");
        QVERIFY(editor.state()["captions"].toMap()["running"].toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["captions"].toMap()["running"].toBool(), 60000);
        QVERIFY2(editor.state()["error"].toString().isEmpty(),
                 qPrintable(editor.state()["error"].toString()));
        QCOMPARE(editor.state()["status"].toString(), QString("No speech found"));
        QCOMPARE(editor.project().tracks, tracks + 1);
        QCOMPARE(editor.project().trackSettings.last().name, QString(Editor::captionTrackName));
        // A second run reuses the cached transcript and the caption track.
        editor.generateCaptions("en");
        QVERIFY(!editor.state()["captions"].toMap()["running"].toBool());
        QCOMPARE(editor.project().tracks, tracks + 1);
#endif
    }
    void pausesAndLoudness() {
        // Cutting ranges out of a clip closes the gaps on its track only.
        Project p;
        p.fpsN = 30;
        Asset a;
        a.id = "talk";
        a.kind = "video";
        a.hasAudio = true;
        a.duration = 60;
        p.assets = {a};
        Clip talk;
        talk.id = "talk";
        talk.assetId = "talk";
        talk.duration = 300;
        talk.sourceIn = Time(1);
        Clip next = talk;
        next.id = "next";
        next.start = 300;
        next.duration = 30;
        Clip above = talk; // another track keeps its timing
        above.id = "above";
        above.track = 1;
        above.start = 200;
        above.duration = 30;
        p.clips = {talk, next, above};
        auto cut = p;
        QCOMPARE(cut.cutRanges("talk", {{100, 150}, {30, 60}, {280, 400}}), qint64(100));
        QCOMPARE(cut.clips.size(), size_t(5));
        auto on = [&](int track) {
            QVector<Clip> list;
            for (const auto &c : cut.clips)
                if (c.track == track)
                    list << c;
            std::sort(list.begin(), list.end(),
                      [](const Clip &x, const Clip &y) { return x.start < y.start; });
            return list;
        };
        const auto pieces = on(0);
        QCOMPARE(pieces.size(), 4);
        QCOMPARE(pieces[0].start, qint64(0));
        QCOMPARE(pieces[0].duration, qint64(30));
        QCOMPARE(pieces[1].start, qint64(30)); // local frame 60: source 1 s + 2 s
        QCOMPARE(pieces[1].sourceIn, Time(3));
        QCOMPARE(pieces[1].duration, qint64(40));
        QCOMPARE(pieces[2].start, qint64(70));
        QCOMPARE(pieces[2].duration, qint64(130));
        QCOMPARE(pieces[3].id, QString("next"));
        QCOMPARE(pieces[3].start, qint64(200));
        QCOMPARE(on(1)[0].start, qint64(200));
        // A cut from the very start removes the head.
        cut = p;
        QCOMPARE(cut.cutRanges("talk", {{0, 10}}), qint64(10));
        QCOMPARE(cut.clip("talk"), nullptr);
        QCOMPARE(on(0)[0].sourceIn, Time(1) + frameTime(10, 30, 1));
        // Detached audio is linked; an unrelated clip of the same media is not.
        auto detached = talk;
        detached.id = "audio";
        detached.track = 2;
        detached.audioOnly = true;
        p.clips << detached;
        QCOMPARE(p.linkedClips("talk"), QStringList{"audio"});
        QVERIFY(p.linkedClips("next").isEmpty());

        // Loudness meter summary parsing.
        QCOMPARE(parseIntegratedLoudness("[Parsed_ebur128_0 @ 0x1] Summary:\n\n  Integrated "
                                         "loudness:\n    I:         -23.4 LUFS\n    Threshold: "
                                         "-33.7 LUFS\n"),
                 -23.4);
        QVERIFY(std::isnan(parseIntegratedLoudness("nothing")));
        QCOMPARE(parseTruePeak("  True peak:\n    Peak:        -3.2 dBFS\n"), -3.2);
        QVERIFY(std::isinf(parseTruePeak("  True peak:\n    Peak:        -inf dBFS\n")));
        QVERIFY(std::isnan(parseTruePeak("nothing")));
        // Playback meter peaks per channel, for 16-bit and float samples.
        const qint16 pcm[] = {16384, -8192, -32768, 100};
        const auto [left, right] = pcmPeaks(reinterpret_cast<const char *>(pcm), sizeof pcm, false);
        QVERIFY(std::abs(left - 1.0) < 1e-3 && std::abs(right - 0.25) < 1e-3);
        const float pcmFloat[] = {0.5f, -0.75f};
        const auto [fl, fr] =
            pcmPeaks(reinterpret_cast<const char *>(pcmFloat), sizeof pcmFloat, true);
        QCOMPARE(fl, 0.5);
        QCOMPARE(fr, 0.75);

        // End to end: a tone, 1.5 s of silence, a tone; then a loudness-normalised export.
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        const auto source = dir.filePath("speech.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=gray:s=160x90:r=30:d=3.5", "-f",
                     "lavfi", "-i",
                     "sine=f=300:d=1,volume=0.3[a];anullsrc=r=44100:cl=mono,atrim=duration=1.5[b];"
                     "sine=f=300:d=1,volume=0.3[c];[a][b][c]concat=n=3:v=0:a=1",
                     "-c:v", "ffv1", "-c:a", "pcm_s16le", "-shortest", source});
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        const auto id = editor.project().clips.first().id;
        editor.select(id);
        QVERIFY(editor.state()["selected"].toMap()["hasAudio"].toBool());
        editor.findPauses(-40, 0.7);
        QCOMPARE(editor.state()["pauses"].toMap()["status"].toString(), QString("finding"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.state()["pauses"].toMap()["status"].toString(),
                                  QString("ready"), 15000);
        const auto found = editor.state()["pauses"].toMap();
        QCOMPARE(found["count"].toInt(), 1);
        // 1.5 s of silence minus 0.12 s kept on each side.
        QVERIFY2(std::abs(found["seconds"].toDouble() - 1.26) < 0.1,
                 qPrintable(found["seconds"].toString()));
        editor.removePauses();
        QCOMPARE(editor.project().clips.size(), size_t(2));
        qint64 total = 0;
        for (const auto &c : editor.project().clips)
            total += c.duration;
        QVERIFY2(std::abs(total - (105 - 38)) <= 2, qPrintable(QString::number(total)));
        QCOMPARE(editor.state()["pauses"].toMap()["status"].toString(), QString("idle"));
        editor.undo();
        QCOMPARE(editor.project().clips.size(), size_t(1));

        // Measuring the mix in the editor: FFmpeg's sine has amplitude 1/8, so the mono tone at 0.3
        // measures -33 LUFS and, panned to both channels at -3 dB, peaks at -31.5 dBFS. An edit
        // then makes the result stale.
        editor.analyzeLoudness();
        QCOMPARE(editor.state()["loudness"].toMap()["status"].toString(), QString("measuring"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.state()["loudness"].toMap()["status"].toString(),
                                  QString("ready"), 30000);
        const auto mix = editor.state()["loudness"].toMap();
        QVERIFY2(std::abs(mix["integrated"].toDouble() + 33) < 1.5,
                 qPrintable(mix["integrated"].toString()));
        QVERIFY2(std::abs(mix["peak"].toDouble() + 31.5) < 1.5, qPrintable(mix["peak"].toString()));
        editor.select(editor.project().clips.first().id);
        editor.setClip("volume", 0.5);
        QCOMPARE(editor.state()["loudness"].toMap()["status"].toString(), QString("stale"));
        editor.undo();

        // Export normalised to -14 LUFS: measured afterwards, it is within a decibel.
        const auto out = dir.filePath("loud.mp4");
        editor.exportWith(QUrl::fromLocalFile(out),
                          {{"format", "mpeg4"}, {"quality", "small"}, {"loudness", -14}});
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 60000);
        QVERIFY2(QFileInfo::exists(out), qPrintable(editor.state()["error"].toString()));
        QVERIFY2(editor.state()["status"].toString().contains("→ -14 LUFS"),
                 qPrintable(editor.state()["status"].toString()));
        QProcess meter;
        meter.start(ffmpeg, {"-hide_banner", "-nostats", "-i", out, "-map", "0:a", "-af",
                             "ebur128=framelog=quiet", "-f", "null", "-"});
        QVERIFY(meter.waitForFinished(30000));
        const double loudness = parseIntegratedLoudness(meter.readAllStandardError());
        QVERIFY2(std::abs(loudness + 14) < 1, qPrintable(QString::number(loudness)));
        // Out-of-range targets are refused.
        editor.exportWith(QUrl::fromLocalFile(dir.filePath("bad.mp4")),
                          {{"format", "mpeg4"}, {"loudness", -2}});
        QVERIFY(editor.state()["error"].toString().contains("LUFS"));
    }
    void karaokeCaptions() {
        // Words into lines: pauses, sentence ends, length, and punctuation tokens.
        const QVector<Cue> words{{0.0, 0.3, " Hello"},  {0.35, 0.6, " world"}, {0.6, 0.62, "."},
                                 {0.7, 0.9, " This"},   {0.95, 1.2, " is"},    {1.25, 1.5, " fine"},
                                 {2.5, 2.8, " Later"}};
        const auto lines = groupWords(words);
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[0].text, QString("Hello world."));
        QCOMPARE(lines[0].wordStarts, (QVector<double>{0.0, 0.35}));
        QCOMPARE(lines[1].text, QString("This is fine"));
        QCOMPARE(lines[2].text, QString("Later")); // after a 1 s pause
        QCOMPARE(groupWords({{0, 1, "aaaa"}, {1, 2, "bbbb"}, {2, 3, "cccc"}}, 9).size(), 2);

        // Placement keeps word timing in clip-local frames.
        Project p;
        p.width = 320;
        p.height = 180;
        p.fpsN = 30;
        Asset a;
        a.id = "talk";
        a.kind = "video";
        a.hasAudio = true;
        a.duration = 60;
        p.assets = {a};
        Clip talk;
        talk.id = "talk";
        talk.assetId = "talk";
        talk.start = 30;
        talk.duration = 300;
        p.clips = {talk};
        auto clips = captionClips(p, {{"talk", lines}}, 1, "karaoke");
        QCOMPARE(clips.size(), 3);
        QCOMPARE(clips[1].start, qint64(30 + 21));
        QCOMPARE(clips[1].wordStarts, (QVector<qint64>{0, 8, 17}));
        QCOMPARE(clips[1].captionStyle, QString("karaoke"));
        QVERIFY(clips[1].timedWords());
        // Splitting a timed caption gives each half its own words.
        Project split = p;
        split.clips = {clips[1]};
        QVERIFY(split.split(clips[1].id, clips[1].start + 10));
        QCOMPARE(split.clips[0].text, QString("This is"));
        QCOMPARE(split.clips[1].text, QString("fine"));
        QCOMPARE(split.clips[1].wordStarts, QVector<qint64>{7});
        // Editing the text to another word count drops back to a plain caption.
        auto edited = clips[1];
        edited.text = "Something else entirely here";
        QVERIFY(!edited.timedWords());
        // Saved and loaded.
        Project saved = p;
        saved.clips << clips[1];
        const auto loaded = Project::fromJson(saved.json(), {});
        QCOMPARE(loaded.clips[1].wordStarts, clips[1].wordStarts);
        QCOMPARE(loaded.clips[1].captionStyle, QString("karaoke"));
        QVERIFY(saved.json()["schemaVersion"].toInt() >= 8);

        // Rendering: the highlight follows the spoken word.
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        Project r;
        r.width = 320;
        r.height = 180;
        r.fpsN = 30;
        Clip caption;
        caption.id = "caption";
        caption.duration = 60;
        caption.text = "WWW MMM"; // one line at this size
        caption.fontSize = 40;
        caption.y = 0;
        caption.captionStyle = "karaoke";
        caption.highlightColor = "#ffd23f";
        caption.textColor = "#ffffff";
        caption.wordStarts = {0, 30};
        r.clips = {caption};
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Project &project, qint64 frame) {
            RenderOptions options;
            options.audio = false;
            options.from = frame;
            options.to = frame + 1;
            const auto plan = compileRender(project, dir.filePath("work"), 320, 180, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage image;
            image.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return image.convertToFormat(QImage::Format_RGB32);
        };
        // Yellow pixels (highlight) and white pixels (other words) on each half.
        auto count = [](const QImage &image, bool left, bool yellow) {
            int n = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = left ? 0 : image.width() / 2; x < (left ? image.width() / 2 : image.width()); ++x) {
                    const QColor c = image.pixelColor(x, y);
                    if (yellow ? (c.red() > 200 && c.green() > 170 && c.blue() < 120)
                               : (c.red() > 220 && c.green() > 220 && c.blue() > 220))
                        ++n;
                }
            return n;
        };
        // Mostly on one side: glyph edges may cross the middle by a pixel or two.
        auto mostly = [](int here, int there) { return here > 50 && there * 20 < here; };
        auto image = still(r, 10); // first word
        QVERIFY2(mostly(count(image, true, true), count(image, false, true)),
                 qPrintable(QString("%1 %2").arg(count(image, true, true)).arg(count(image, false, true))));
        QVERIFY(mostly(count(image, false, false), count(image, true, false)));
        image = still(r, 40); // second word
        QVERIFY(mostly(count(image, false, true), count(image, true, true)));
        QVERIFY(mostly(count(image, true, false), count(image, false, false)));
        // One word at a time: only the current word, in the highlight colour.
        r.clips[0].captionStyle = "word";
        image = still(r, 40);
        QVERIFY(count(image, true, false) + count(image, false, false) == 0);
        QVERIFY(count(image, true, true) + count(image, false, true) > 50);
        // Without matching timing the caption renders plainly (white only).
        r.clips[0].text = "WW MM KK";
        image = still(r, 40);
        QVERIFY(count(image, true, true) + count(image, false, true) == 0);
        QVERIFY(count(image, true, false) + count(image, false, false) > 50);
    }
    void textStyles() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        QImage grey(320, 180, QImage::Format_RGB32);
        grey.fill(QColor(128, 128, 128));
        QVERIFY(grey.save(dir.filePath("grey.png")));
        Project p;
        p.width = 320;
        p.height = 180;
        Asset a;
        a.id = "grey";
        a.path = dir.filePath("grey.png");
        a.kind = "image";
        a.duration = 5;
        a.width = 320;
        a.height = 180;
        p.assets = {a};
        Clip bg;
        bg.id = "bg";
        bg.assetId = "grey";
        bg.duration = 30;
        Clip title;
        title.id = "t";
        title.track = 1;
        title.duration = 30;
        title.text = "Hi";
        title.fontSize = 60;
        p.clips = {bg, title};
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Clip &t) {
            auto project = p;
            project.clips[1] = t;
            RenderOptions options;
            options.audio = false;
            options.from = 5;
            options.to = 6;
            const auto plan = compileRender(project, dir.filePath("work"), 320, 180, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage out;
            out.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return out.convertToFormat(QImage::Format_RGB32);
        };
        // Columns that contain text-coloured (near white) pixels.
        auto inkColumns = [](const QImage &i, auto match) {
            int lo = i.width(), hi = -1, count = 0;
            for (int x = 0; x < i.width(); ++x)
                for (int y = 0; y < i.height(); ++y)
                    if (match(QColor(i.pixel(x, y)))) {
                        lo = std::min(lo, x);
                        hi = std::max(hi, x);
                        ++count;
                        break;
                    }
            return std::tuple{lo, hi, count};
        };
        auto white = [](const QColor &c) { return c.red() > 220 && c.green() > 220 && c.blue() > 220; };
        const auto [cl, ch, cn] = inkColumns(still(title), white);
        QVERIFY2(cn > 10 && std::abs((cl + ch) / 2 - 160) < 12, qPrintable(QString("%1 %2").arg(cl).arg(ch)));
        auto left = title;
        left.align = "left";
        const auto [ll, lh, ln] = inkColumns(still(left), white);
        QVERIFY2(ll < cl - 60 && ll < 40, qPrintable(QString::number(ll)));
        auto right = title;
        right.align = "right";
        const auto [rl, rh, rn] = inkColumns(still(right), white);
        QVERIFY2(rh > ch + 60 && rh > 280, qPrintable(QString::number(rh)));
        // Letter spacing widens the text.
        auto spaced = title;
        spaced.letterSpacing = 0.5;
        const auto [sl, sh, sn] = inkColumns(still(spaced), white);
        QVERIFY2(sh - sl > ch - cl + 10, qPrintable(QString("%1 vs %2").arg(sh - sl).arg(ch - cl)));
        // A yellow outline and a black box behind the line.
        auto styled = title;
        styled.outline = 0.1;
        styled.outlineColor = "#ffd23f";
        auto yellow = [](const QColor &c) {
            return c.red() > 200 && c.green() > 170 && c.blue() < 120;
        };
        QVERIFY(std::get<2>(inkColumns(still(styled), yellow)) > 10);
        QCOMPARE(std::get<2>(inkColumns(still(title), yellow)), 0);
        styled.background = 1;
        const auto boxed = still(styled);
        // Just left of the first letter, inside the box: black instead of grey.
        const QColor beside(boxed.pixel(cl - 11, 90));
        QVERIFY2(beside.red() < 40, qPrintable(beside.name()));
        QVERIFY(qGray(boxed.pixel(5, 5)) > 100); // outside the box
        // No shadow: no dark pixels around plain text on grey.
        auto flat = title;
        flat.textShadow = 0;
        QCOMPARE(std::get<2>(inkColumns(still(flat), [](const QColor &c) { return c.red() < 80; })), 0);
        QVERIFY(std::get<2>(inkColumns(still(title), [](const QColor &c) { return c.red() < 80; })) > 0);
        // Saved only when not the default; validated.
        p.clips[1] = styled;
        p.clips[1].italic = true;
        p.clips[1].align = "right";
        const auto json = p.json();
        const auto saved = json["clips"].toArray()[1].toObject();
        QCOMPARE(saved["outlineColor"].toString(), QString("#ffd23f"));
        QVERIFY(!json["clips"].toArray()[0].toObject().contains("align"));
        const auto loaded = Project::fromJson(json, {}).clips[1];
        QCOMPARE(loaded.align, QString("right"));
        QVERIFY(loaded.italic && loaded.bold);
        QCOMPARE(loaded.background, 1.);
        auto bad = json;
        auto clips = bad["clips"].toArray();
        auto o = clips[1].toObject();
        o["align"] = "justify";
        clips[1] = o;
        bad["clips"] = clips;
        QVERIFY_EXCEPTION_THROWN(Project::fromJson(bad, {}), std::runtime_error);

        // A font file added to Cutlery is copied to the data folder and usable by family name.
        QString fontFile;
        for (const auto &folder : {qEnvironmentVariable("WINDIR") + "/Fonts",
                                   QString("/usr/share/fonts/truetype/dejavu")})
            for (const auto &f : QDir(folder).entryInfoList({"*.ttf"}, QDir::Files))
                if (fontFile.isEmpty())
                    fontFile = f.absoluteFilePath();
        if (fontFile.isEmpty())
            QSKIP("No font file on this machine");
        const auto copy = dir.filePath("Test Font.ttf");
        QVERIFY(QFile::copy(fontFile, copy));
        FrameProvider frames;
        Editor editor(&frames);
        const auto family = editor.addFont(QUrl::fromLocalFile(copy));
        QVERIFY2(!family.isEmpty(), qPrintable(editor.state()["error"].toString()));
        const auto stored = editor.state()["dataPath"].toString() + "/fonts/Test Font.ttf";
        QVERIFY(QFileInfo::exists(stored));
        QVERIFY(editor.fontFamilies().contains(family));
        QFile::remove(stored);
        QVERIFY(editor.addFont(QUrl::fromLocalFile(dir.filePath("grey.png"))).isEmpty());
        QVERIFY(editor.state()["error"].toString().contains(".ttf"));
    }
    void shapes() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        QImage grey(320, 180, QImage::Format_RGB32);
        grey.fill(QColor(128, 128, 128));
        QVERIFY(grey.save(dir.filePath("grey.png")));
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(320, 180, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(dir.filePath("grey.png"))});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        editor.seek(0);
        editor.addGraphic("arrow");
        const auto arrow = editor.state()["selectedId"].toString();
        QCOMPARE(editor.state()["selected"].toMap()["graphic"].toString(), QString("arrow"));
        // The preview frame fits the shape.
        const auto bounds = editor.clipBounds(arrow);
        QVERIFY2(std::abs(bounds["width"].toDouble() - 0.3) < 0.01 &&
                     std::abs(bounds["height"].toDouble() - 0.12) < 0.01,
                 qPrintable(QString("%1 %2").arg(bounds["width"].toDouble()).arg(bounds["height"].toDouble())));
        const auto graph = dir.filePath("graph.txt");
        auto still = [&]() {
            RenderOptions options;
            options.audio = false;
            options.from = 5;
            options.to = 6;
            const auto plan = compileRender(editor.project(), dir.filePath("work"), 320, 180, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage out;
            out.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return out.convertToFormat(QImage::Format_RGB32);
        };
        auto red = [](const QImage &i, int x, int y) {
            const QColor c(i.pixel(x, y));
            return c.red() > 200 && c.green() < 130 && c.blue() < 130;
        };
        auto image = still();
        QVERIFY(red(image, 160, 90));   // shaft
        QVERIFY(red(image, 200, 90));   // head
        QVERIFY(!red(image, 160, 80));  // above the shaft, beside the head: background
        QVERIFY(!red(image, 160, 120)); // below the arrow
        // Rotated a quarter turn, it points down.
        editor.setClip("rotation", 90);
        image = still();
        QVERIFY(red(image, 160, 120));
        QVERIFY(!red(image, 200, 90));
        editor.undo();
        // An outline circle leaves its centre clear.
        editor.setClip("graphic", "ellipse");
        editor.setClipValues({{"fillColor", "#00000000"}, {"strokeColor", "#ff5a5f"},
                              {"stroke", 0.02}, {"graphicWidth", 0.4}, {"graphicHeight", 0.6}});
        image = still();
        QVERIFY(!red(image, 160, 90));
        QVERIFY(red(image, 160, 90 - 54)); // top of the ring: 0.6 × 180 / 2
        // A speech bubble shows its text inside on a white body.
        editor.remove(false);
        editor.addGraphic("bubble");
        const auto bubble = editor.project().clip(editor.state()["selectedId"].toString());
        QCOMPARE(bubble->text, QString("Hello!"));
        image = still();
        int dark = 0, bright = 0;
        for (int x = 120; x < 200; ++x) {
            const int g = qGray(image.pixel(x, 85));
            dark += g < 60;
            bright += g > 240;
        }
        QVERIFY2(dark > 3 && bright > 20, qPrintable(QString("%1 %2").arg(dark).arg(bright)));
        // Shapes are not captions, and the project validates them.
        QVERIFY(!editor.exportSrt(QUrl::fromLocalFile(dir.filePath("none.srt"))));
        auto json = editor.project().json();
        QCOMPARE(Project::fromJson(json, {}).clips.back().graphic, QString("bubble"));
        auto clips = json["clips"].toArray();
        auto o = clips.last().toObject();
        o["graphic"] = "star";
        clips[clips.size() - 1] = o;
        json["clips"] = clips;
        QVERIFY_EXCEPTION_THROWN(Project::fromJson(json, {}), std::runtime_error);
        editor.addGraphic("hexagon");
        QVERIFY(editor.state()["error"].toString().contains("shape"));
    }
    void voiceOverWithoutMicrophone() {
        FrameProvider frames;
        Editor editor(&frames);
        const auto voice = editor.state()["voiceOver"].toMap();
        QCOMPARE(voice["recording"].toBool(), false);
        if (voice["available"].toBool())
            QSKIP("This machine has a microphone; recording is checked by hand");
        editor.startVoiceOver();
        QVERIFY(editor.state()["error"].toString().contains("No microphone"));
        QCOMPARE(editor.state()["voiceOver"].toMap()["recording"].toBool(), false);
        editor.stopVoiceOver(); // nothing to stop
    }
    void markersAndRange() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        const auto source = dir.filePath("clip.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=gray:s=160x90:r=30:d=3", "-f",
                     "lavfi", "-i", "sine=d=3", "-c:v", "ffv1", "-c:a", "pcm_s16le", "-shortest",
                     source});
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        // Markers stay sorted; toggling at the same frame removes one.
        for (qint64 f : {60, 15, 40}) {
            editor.seek(f);
            editor.toggleMarker();
        }
        auto frames_ = [&] {
            QVector<qint64> out;
            for (const auto &m : editor.project().markers)
                out << m.frame;
            return out;
        };
        QCOMPARE(frames_(), (QVector<qint64>{15, 40, 60}));
        editor.seek(40);
        editor.toggleMarker();
        QCOMPARE(frames_(), (QVector<qint64>{15, 60}));
        editor.undo();
        QCOMPARE(frames_(), (QVector<qint64>{15, 40, 60}));
        QCOMPARE(editor.adjacentMarker(true), qint64(60));
        QCOMPARE(editor.adjacentMarker(false), qint64(15));
        editor.seek(70);
        QCOMPARE(editor.adjacentMarker(true), qint64(-1));
        editor.setMarker(0, "name", "Intro");
        editor.setMarker(0, "color", "#ff5a5f");
        QCOMPARE(editor.project().markers[0].name, QString("Intro"));
        editor.setMarker(0, "color", "not a colour");
        QVERIFY(editor.state()["error"].toString().contains("marker"));
        editor.clearError();
        QCOMPARE(editor.project().markers[0].color, QString("#ff5a5f"));
        QCOMPARE(editor.state()["markers"].toList().size(), 3);
        editor.removeMarker(2);
        QCOMPARE(editor.project().markers.size(), 2);

        // Export of the whole timeline needs no range; in/out needs one.
        editor.exportWith(QUrl::fromLocalFile(dir.filePath("none.wav")),
                          {{"format", "wav"}, {"range", "inout"}});
        QVERIFY(editor.state()["error"].toString().contains("in or out"));
        editor.clearError();
        editor.seek(30);
        editor.setInPoint();
        editor.seek(59);
        editor.setOutPoint();
        QCOMPARE(editor.project().inPoint, qint64(30));
        QCOMPARE(editor.project().outPoint, qint64(60));
        // An out point before the in point clears the in point.
        editor.seek(10);
        editor.setOutPoint();
        QCOMPARE(editor.project().inPoint, qint64(-1));
        editor.undo();
        QCOMPARE(editor.project().inPoint, qint64(30));
        // The in/out export is exactly one second, video and audio.
        editor.exportWith(QUrl::fromLocalFile(dir.filePath("range.mp4")),
                          {{"format", "mpeg4"}, {"quality", "small"}, {"range", "inout"}});
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 60000);
        QVERIFY2(QFileInfo::exists(dir.filePath("range.mp4")),
                 qPrintable(editor.state()["error"].toString()));
        const auto probe = QString::fromUtf8(
            run(Editor::executable("ffprobe"),
                {"-v", "error", "-show_entries", "stream=codec_type,duration,nb_frames", "-of",
                 "compact", dir.filePath("range.mp4")}));
        QVERIFY2(probe.contains("nb_frames=30"), qPrintable(probe));
        const auto audio = QRegularExpression("codec_type=audio\\|duration=([0-9.]+)").match(probe);
        QVERIFY2(std::abs(audio.captured(1).toDouble() - 1) < 0.05, qPrintable(probe));

        // Saved with the project and validated.
        auto json = editor.project().json();
        const auto loaded = Project::fromJson(json, {});
        QCOMPARE(loaded.markers, editor.project().markers);
        QCOMPARE(loaded.inPoint, qint64(30));
        QCOMPARE(loaded.outPoint, qint64(60));
        json["inPoint"] = "80";
        QVERIFY_EXCEPTION_THROWN(Project::fromJson(json, {}), std::runtime_error);
        editor.clearInOut();
        QCOMPARE(editor.project().inPoint, qint64(-1));
        QVERIFY(!editor.project().json().contains("inPoint"));
    }
    void collectProject() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // Two media files with the same name in different folders, and a LUT.
        QVERIFY(QDir(dir.path()).mkpath("a") && QDir(dir.path()).mkpath("b"));
        for (const auto &[folder, colour] : {std::pair{QString("a"), QColor(Qt::red)},
                                             std::pair{QString("b"), QColor(Qt::blue)}}) {
            QImage image(160, 90, QImage::Format_RGB32);
            image.fill(colour);
            QVERIFY(image.save(dir.filePath(folder + "/shot.png")));
        }
        const auto lut = dir.filePath("look.cube");
        {
            QFile f(lut);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("LUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
        }
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(dir.filePath("a/shot.png")),
                            QUrl::fromLocalFile(dir.filePath("b/shot.png"))});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 2, 15000);
        editor.addAsset(editor.project().assets[0].id);
        editor.addAsset(editor.project().assets[1].id, 1);
        editor.select(editor.project().clips.first().id);
        editor.setClip("lut", QUrl::fromLocalFile(lut));
        // A title in a font added to Cutlery takes the font file along.
        QString fontFile;
        for (const auto &folder : {qEnvironmentVariable("WINDIR") + "/Fonts",
                                   QString("/usr/share/fonts/truetype/dejavu")})
            for (const auto &f : QDir(folder).entryInfoList({"*.ttf"}, QDir::Files))
                if (fontFile.isEmpty())
                    fontFile = f.absoluteFilePath();
        QString family;
        if (!fontFile.isEmpty()) {
            QVERIFY(QFile::copy(fontFile, dir.filePath("Collect Font.ttf")));
            family = editor.addFont(QUrl::fromLocalFile(dir.filePath("Collect Font.ttf")));
            QVERIFY(!family.isEmpty());
            editor.addTitle();
            editor.setClip("fontFamily", family);
        }
        // Only into an empty folder.
        editor.collectProject(QUrl::fromLocalFile(dir.path()));
        QVERIFY(editor.state()["error"].toString().contains("empty"));
        editor.clearError();
        const auto target = dir.filePath("Archive");
        editor.collectProject(QUrl::fromLocalFile(target));
        QTRY_COMPARE_WITH_TIMEOUT(editor.state()["collect"].toMap()["status"].toString(),
                                  QString("done"), 30000);
        QCOMPARE(editor.state()["collect"].toMap()["path"].toString(),
                 target + "/Archive.cutlery");
        QVERIFY(QFileInfo::exists(target + "/media/shot.png"));
        QVERIFY(QFileInfo::exists(target + "/media/shot-2.png"));
        QVERIFY(QFileInfo::exists(target + "/luts/look.cube"));
        if (!family.isEmpty()) {
            QVERIFY(QFileInfo::exists(target + "/fonts/Collect Font.ttf"));
            QFile::remove(editor.state()["dataPath"].toString() + "/fonts/Collect Font.ttf");
        }
        // The copy works on its own: the originals can go away.
        QVERIFY(QDir(dir.filePath("a")).removeRecursively());
        QVERIFY(QDir(dir.filePath("b")).removeRecursively());
        QVERIFY(QFile::remove(lut));
        const auto collected = loadProject(target + "/Archive.cutlery");
        QCOMPARE(collected.assets.size(), 2);
        for (const auto &a : collected.assets)
            QVERIFY2(QFileInfo(a.path).isFile() && a.path.startsWith(target), qPrintable(a.path));
        QVERIFY(QImage(collected.assets[0].path).pixelColor(1, 1) !=
                QImage(collected.assets[1].path).pixelColor(1, 1));
        QCOMPARE(collected.clips.first().lut, target + "/luts/look.cube");
        QFile file(target + "/Archive.cutlery");
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto text = QString::fromUtf8(file.readAll());
        QVERIFY2(text.contains("\"media/shot.png\"") && !text.contains(dir.path()),
                 "Paths in a collected project are relative");
        // Missing media stops collecting.
        editor.collectProject(QUrl::fromLocalFile(dir.filePath("Again")));
        QVERIFY(editor.state()["error"].toString().contains("Missing media"));
    }
    void variableFrameRate() {
        QVERIFY(isVariableRate(30, 68. / 3));
        QVERIFY(!isVariableRate(30, 29.9));
        QVERIFY(!isVariableRate(30, 0));
        QCOMPARE(standardRate(29.8), 30000. / 1001);
        QCOMPARE(standardRate(25.3), 25.);
        QCOMPARE(standardRate(12), 12.);
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // 1.5 s at 30 fps, then 1.5 s at 15 fps.
        const auto source = dir.filePath("phone.mp4");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=gray:s=160x90:r=30:d=3", "-f",
                     "lavfi", "-i", "sine=d=3", "-vf",
                     "setpts='if(lt(N,45),N/30,1.5+(N-45)/15)/TB'", "-fps_mode", "vfr", "-c:v",
                     "mpeg4", "-c:a", "aac", "-shortest", source});
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        const auto asset = editor.project().assets.first();
        QVERIFY(asset.variableRate);
        QVERIFY2(std::abs(asset.frameRate - 68. / 3) < 0.5, qPrintable(QString::number(asset.frameRate)));
        editor.addAsset(asset.id);
        const auto clip = editor.project().clips.first();
        editor.select(clip.id);
        QVERIFY(editor.state()["selected"].toMap()["variableRate"].toBool());
        QVERIFY(Project::fromJson(editor.project().json(), {}).assets.first().variableRate);
        editor.conformFrameRate();
        QCOMPARE(editor.state()["conform"].toMap()["status"].toString(), QString("converting"));
        QTRY_VERIFY_WITH_TIMEOUT(
            editor.project().assets.first().path.endsWith("-cfr.mov") && !editor.state()["busy"].toBool(),
            60000);
        QCOMPARE(editor.state()["conform"].toMap()["status"].toString(), QString("done"));
        const auto conformed = editor.project().assets.first();
        QCOMPARE(conformed.id, asset.id);
        QVERIFY(!conformed.variableRate);
        QVERIFY(conformed.hasAudio);
        QVERIFY(std::abs(conformed.duration - asset.duration) < 0.1);
        QCOMPARE(editor.project().clips.first().duration, clip.duration);
        QVERIFY(QFileInfo::exists(source)); // the original stays
        // Stills are never variable-rate.
        QImage image(64, 64, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(dir.filePath("still.png")));
        editor.importMedia({QUrl::fromLocalFile(dir.filePath("still.png"))});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 2, 15000);
        QCOMPARE(editor.project().assets.last().frameRate, 0.);
    }
    void imageSequences() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // Frames 7–18 of a half-transparent animation in a folder with a % sign, plus unrelated
        // files and a gap after 18.
        const auto folder = dir.filePath("100% render");
        QVERIFY(QDir().mkpath(folder));
        for (int i = 7; i <= 18; ++i) {
            QImage frame(161, 90, QImage::Format_ARGB32);
            frame.fill(Qt::transparent);
            for (int y = 0; y < 90; ++y)
                for (int x = 0; x < 80; ++x)
                    frame.setPixelColor(x, y, QColor(255, 0, 0));
            QVERIFY(frame.save(folder + QString("/shot_%1.png").arg(i, 4, 10, QChar('0'))));
        }
        QVERIFY(QImage(8, 8, QImage::Format_RGB32).save(folder + "/shot_0020.png"));
        QVERIFY(QImage(8, 8, QImage::Format_RGB32).save(folder + "/other_0001.png"));
        const auto info = Editor::imageSequence(folder + "/shot_0010.png");
        QCOMPARE(info["start"].toLongLong(), qint64(7));
        QCOMPARE(info["count"].toLongLong(), qint64(12));
        QCOMPARE(info["pattern"].toString(), dir.filePath("100%% render") + "/shot_%04d.png");
        QVERIFY(Editor::imageSequence(folder + "/other_0001.png").isEmpty()); // a single frame
        QVERIFY(Editor::imageSequence(dir.filePath("plain.png")).isEmpty());

        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importImageSequence(QUrl::fromLocalFile(folder + "/shot_0012.png"), 12);
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 30000);
        const auto a = editor.project().assets.first();
        QCOMPARE(a.kind, QString("video"));
        QVERIFY2(std::abs(a.duration - 1) < 0.05, qPrintable(QString::number(a.duration)));
        QCOMPARE(a.width, 160); // even size for ProRes
        // Transparency survives: the right half of a frame has no alpha.
        const auto probe = QString::fromUtf8(run(Editor::executable("ffprobe"),
            {"-v", "error", "-show_entries", "stream=pix_fmt,nb_frames", "-of", "compact", a.path}));
        QVERIFY2(probe.contains("yuva444p") && probe.contains("nb_frames=12"), qPrintable(probe));
        editor.importImageSequence(QUrl::fromLocalFile(dir.filePath("none_0001.png")), 12);
        QVERIFY(editor.state()["error"].toString().contains("frame number"));
    }
    void soundTools() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // 2 s: a 60 Hz hum, a 1 kHz tone and a 10 kHz tone, then 2 s of faint noise and a quiet
        // 1 kHz tone, so dynamics can be compared.
        const auto source = dir.filePath("sound.wav");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i",
                     "sine=f=60:d=4,volume=2[a];sine=f=1000:d=4,volume=2[b];"
                     "sine=f=10000:d=4,volume=2[c];anoisesrc=d=4:a=0.003:c=white[n];"
                     "[a][b][c][n]amix=inputs=4:normalize=0,"
                     "volume='if(lt(t,2),1,0.05)':eval=frame",
                     "-ar", "48000", "-ac", "2", source});
        Project p;
        p.width = 160;
        p.height = 90;
        Asset a;
        a.id = "s";
        a.path = source;
        a.kind = "audio";
        a.duration = 4;
        a.hasAudio = true;
        p.assets = {a};
        Clip c;
        c.id = "c";
        c.assetId = "s";
        c.duration = 120;
        p.clips = {c};
        const auto graph = dir.filePath("graph.txt");
        // The rendered mix as a WAV file.
        auto render = [&](const Clip &clip, const QString &name) {
            auto project = p;
            project.clips[0] = clip;
            RenderOptions options;
            options.video = false;
            const auto plan = compileRender(project, dir.filePath("work"), 160, 90, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            const auto out = dir.filePath(name + ".wav");
            QStringList args{"-v", "error", "-y"};
            args += plan.inputs;
            args << "-filter_complex_script" << graph << "-map" << "[aout]" << out;
            run(ffmpeg, args);
            return out;
        };
        // Mean level in dB of a band and time range of a file.
        auto level = [&](const QString &file, const QString &band, double from, double to) {
            QProcess p;
            p.start(ffmpeg, {"-hide_banner", "-nostats", "-ss", QString::number(from), "-t",
                             QString::number(to - from), "-i", file, "-af",
                             band + (band.isEmpty() ? "" : ",") + "volumedetect", "-f", "null",
                             "-"});
            p.waitForFinished(30000);
            const auto m = QRegularExpression("mean_volume: (-?[0-9.]+) dB")
                               .match(QString::fromUtf8(p.readAllStandardError()));
            return m.hasMatch() ? m.captured(1).toDouble() : -999.;
        };
        const auto plain = render(c, "plain");
        const double hum = level(plain, "lowpass=f=120", 0.3, 1.8),
                     tone = level(plain, "bandpass=f=1000:w=200", 0.3, 1.8),
                     hiss = level(plain, "highpass=f=7000", 0.3, 1.8);
        QVERIFY(hum > -30 && tone > -30 && hiss > -30);
        {
            auto x = c;
            x.lowCut = 200;
            const auto f = render(x, "lowcut");
            QVERIFY2(level(f, "lowpass=f=120", 0.3, 1.8) < hum - 10, "low cut removes the hum");
            QVERIFY(std::abs(level(f, "bandpass=f=1000:w=200", 0.3, 1.8) - tone) < 1.5);
        }
        {
            auto x = c;
            x.eqLow = 12;
            x.eqHigh = -12;
            const auto f = render(x, "eq");
            QVERIFY2(level(f, "lowpass=f=120", 0.3, 1.8) > hum + 6, "bass boost");
            QVERIFY2(level(f, "highpass=f=7000", 0.3, 1.8) < hiss - 6, "treble cut");
            // The presence band, on a 2.5 kHz tone.
            const auto presence = dir.filePath("presence.wav");
            run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "sine=f=2500:d=4,volume=2", "-ar",
                         "48000", "-ac", "2", presence});
            const auto original = p.assets[0].path;
            p.assets[0].path = presence;
            x = c;
            x.eqMid = 9;
            QVERIFY2(level(render(x, "mid"), "", 0.3, 1.8) > level(render(c, "flat"), "", 0.3, 1.8) + 6,
                     "presence boost");
            p.assets[0].path = original;
        }
        {
            // Compression narrows the gap between the loud and the quiet half.
            auto x = c;
            x.compressor = 1;
            const auto f = render(x, "comp");
            const double before = level(plain, "", 0.3, 1.8) - level(plain, "", 2.3, 3.8),
                         after = level(f, "", 0.3, 1.8) - level(f, "", 2.3, 3.8);
            QVERIFY2(after < before - 5, qPrintable(QString("%1 → %2").arg(before).arg(after)));
        }
        {
            // A strong gate pushes the quiet half down further.
            auto x = c;
            x.gate = 1;
            const auto f = render(x, "gate");
            QVERIFY2(level(f, "", 2.3, 3.8) < level(plain, "", 2.3, 3.8) - 6, "gate");
            QVERIFY(std::abs(level(f, "", 0.3, 1.8) - level(plain, "", 0.3, 1.8)) < 2);
        }
        {
            // Noise reduction lowers steady hiss.
            const auto hissy = dir.filePath("hiss-source.wav");
            run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "anoisesrc=d=4:a=0.006:c=white", "-ar",
                         "48000", "-ac", "2", hissy});
            const auto original = p.assets[0].path;
            p.assets[0].path = hissy;
            auto x = c;
            x.denoise = 1;
            const double noisy = level(render(c, "hiss"), "", 1, 3.5),
                         cleaned = level(render(x, "denoise"), "", 1, 3.5);
            QVERIFY2(cleaned < noisy - 6, qPrintable(QString("%1 → %2").arg(noisy).arg(cleaned)));
            p.assets[0].path = original;
            x.deess = 1;
            render(x, "deess"); // renders
        }
        // Stored only when set; validated.
        auto x = c;
        x.compressor = 0.5;
        x.eqHigh = -3;
        p.clips = {x};
        auto json = p.json();
        QVERIFY(!json["clips"].toArray()[0].toObject().contains("gate"));
        const auto back = Project::fromJson(json, {}).clips[0];
        QCOMPARE(back.compressor, 0.5);
        QCOMPARE(back.eqHigh, -3.);
        auto clips = json["clips"].toArray();
        auto o = clips[0].toObject();
        o["eqLow"] = 20;
        clips[0] = o;
        json["clips"] = clips;
        QVERIFY_EXCEPTION_THROWN(Project::fromJson(json, {}), std::runtime_error);
    }
    void shuttlePlayback() {
        // Faster playback paces real time faster and keeps sound in step without pitch change.
        QTemporaryDir dir;
        {
            QFile silence(dir.filePath("sound.wav"));
            QVERIFY(silence.open(QIODevice::WriteOnly)); // only the graph is compiled
        }
        Project p;
        p.width = 160;
        p.height = 90;
        Asset a;
        a.id = "s";
        a.path = dir.filePath("sound.wav");
        a.kind = "audio";
        a.duration = 10;
        a.hasAudio = true;
        p.assets = {a};
        Clip c;
        c.id = "c";
        c.assetId = "s";
        c.duration = 90;
        p.clips = {c};
        RenderOptions o;
        o.realtime = true;
        o.rate = 4;
        o.video = false;
        auto graph = compileRender(p, dir.path(), 160, 90, o).graph;
        QVERIFY2(graph.contains("atempo=2.000000000,atempo=2.000000000,arealtime"), qPrintable(graph));
        o.video = true;
        o.audio = false;
        graph = compileRender(p, dir.path(), 160, 90, o).graph;
        QVERIFY2(graph.contains("realtime=speed=4.000000000"), qPrintable(graph));
        o.rate = 1;
        graph = compileRender(p, dir.path(), 160, 90, o).graph;
        QVERIFY(graph.contains(",realtime[vout]") && !graph.contains("speed="));

        // Backward shuttle steps the playhead back ten times a second, faster on each press.
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.addTitle();
        editor.setClip("duration", 300);
        editor.seek(250);
        editor.shuttle(false);
        QCOMPARE(editor.playbackRate(), -1.);
        QTRY_VERIFY_WITH_TIMEOUT(editor.state()["playhead"].toLongLong() <= 244, 2000);
        editor.shuttle(false);
        QCOMPARE(editor.playbackRate(), -2.);
        const auto before = editor.state()["playhead"].toLongLong();
        QTRY_VERIFY_WITH_TIMEOUT(editor.state()["playhead"].toLongLong() <= before - 12, 2000);
        editor.pause();
        QCOMPARE(editor.playbackRate(), 1.);
        const auto stopped = editor.state()["playhead"].toLongLong();
        QTest::qWait(300);
        QCOMPARE(editor.state()["playhead"].toLongLong(), stopped);
        // It stops at the start.
        editor.seek(3);
        editor.shuttle(false);
        QTRY_COMPARE_WITH_TIMEOUT(editor.playbackRate(), 1., 2000);
        QCOMPARE(editor.state()["playhead"].toLongLong(), qint64(0));
    }
    void smoothSlowMotion() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // A white bar moving 8 px per frame over black: at half speed, an in-between frame shows
        // the bar's trailing part fully (repeat) or half (blend).
        const auto source = dir.filePath("flicker.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=black:s=128x72:r=30:d=1", "-f", "lavfi", "-i",
                     "color=white:s=16x72:r=30:d=1", "-filter_complex", "[0][1]overlay=x=8*n:y=0", "-c:v", "ffv1", source});
        Project p;
        p.width = 128;
        p.height = 72;
        Asset a;
        a.id = "v";
        a.path = source;
        a.kind = "video";
        a.duration = 1;
        a.width = 128;
        a.height = 72;
        p.assets = {a};
        Clip c;
        c.id = "c";
        c.assetId = "v";
        c.speed = Time(1, 2);
        c.duration = 40;
        p.clips = {c};
        const auto graph = dir.filePath("graph.txt");
        auto grey = [&](const QString &mode, qint64 frame, int x) {
            auto project = p;
            project.clips[0].slowMotion = mode;
            RenderOptions options;
            options.audio = false;
            options.from = frame;
            options.to = frame + 1;
            const auto plan = compileRender(project, dir.filePath("work"), 128, 72, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage out;
            out.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return qGray(out.pixel(x, 36));
        };
        // While the bar passes x = 64, repeated frames are only black or white; blending
        // produces grey in-between frames.
        int repeated = 0, blended = 0;
        for (qint64 f = 8; f < 24; ++f) {
            const int r = grey("", f, 64), b = grey("blend", f, 64);
            repeated += (r > 70 && r < 185);
            blended += (b > 70 && b < 185);
        }
        QCOMPARE(repeated, 0);
        QVERIFY2(blended >= 1, qPrintable(QString::number(blended)));
        // Optical flow renders and the setting round-trips.
        const int flow = grey("flow", 12, 64);
        QVERIFY(flow >= 0 && flow <= 255);
        p.clips[0].slowMotion = "blend";
        QCOMPARE(Project::fromJson(p.json(), {}).clips[0].slowMotion, QString("blend"));
        auto bad = p.json();
        auto clips = bad["clips"].toArray();
        auto o = clips[0].toObject();
        o["slowMotion"] = "warp";
        clips[0] = o;
        bad["clips"] = clips;
        QVERIFY_EXCEPTION_THROWN(Project::fromJson(bad, {}), std::runtime_error);
    }
    void styleEffects() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // Still colour bars, and a white bar moving 8 px per frame over black.
        const auto bars = dir.filePath("bars.mkv"), moving = dir.filePath("moving.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "smptebars=s=128x72:r=30:d=2", "-c:v",
                     "ffv1", bars});
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=black:s=128x72:r=30:d=1", "-f",
                     "lavfi", "-i", "color=white:s=16x72:r=30:d=1", "-filter_complex",
                     "[0][1]overlay=x=8*n:y=0", "-c:v", "ffv1", moving});
        Project p;
        p.width = 128;
        p.height = 72;
        for (const auto &[id, path, seconds] :
             {std::tuple{QString("bars"), bars, 2.}, std::tuple{QString("moving"), moving, 1.}}) {
            Asset a;
            a.id = id;
            a.path = path;
            a.kind = "video";
            a.duration = seconds;
            a.width = 128;
            a.height = 72;
            p.assets << a;
        }
        Clip clip;
        clip.id = "c";
        clip.assetId = "bars";
        clip.duration = 60;
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Clip &c, qint64 frame) {
            auto project = p;
            project.clips = {c};
            RenderOptions options;
            options.audio = false;
            options.from = frame;
            options.to = frame + 1;
            const auto plan = compileRender(project, dir.filePath("work"), 128, 72, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage out;
            out.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return out.convertToFormat(QImage::Format_RGB32);
        };
        // Mean absolute difference per channel between two pictures.
        auto difference = [](const QImage &a, const QImage &b) {
            double sum = 0;
            for (int y = 0; y < a.height(); ++y)
                for (int x = 0; x < a.width(); ++x) {
                    const QColor u(a.pixel(x, y)), v(b.pixel(x, y));
                    sum += std::abs(u.red() - v.red()) + std::abs(u.green() - v.green()) +
                           std::abs(u.blue() - v.blue());
                }
            return sum / (3. * a.width() * a.height());
        };
        auto with = [&](auto change) {
            auto c = clip;
            change(c);
            return c;
        };
        const auto plain = still(clip, 10);
        QVERIFY(difference(plain, still(clip, 40)) < 1);
        // Shake: the still picture moves over time.
        const auto shake = with([](Clip &c) {
            c.fx = "shake";
            c.fxStrength = 1;
        });
        const double moved = difference(still(shake, 10), still(shake, 40));
        QVERIFY2(moved > 4, qPrintable(QString::number(moved)));
        // Glitch: some frames have their colour channels shifted apart, others are untouched,
        // and the same frame looks the same every time.
        const auto glitch = with([](Clip &c) {
            c.fx = "glitch";
            c.fxStrength = 1;
        });
        int shifted = 0, untouched = 0;
        for (qint64 f = 1; f < 60; f += 3) {
            const double d = difference(still(glitch, f), plain);
            shifted += d > 3;
            untouched += d < 1;
        }
        QVERIFY2(shifted >= 1 && untouched >= 1,
                 qPrintable(QString("%1 %2").arg(shifted).arg(untouched)));
        QCOMPARE(compileRender([&] { auto q = p; q.clips = {glitch}; return q; }(), "w", 128, 72, {}).graph,
                 compileRender([&] { auto q = p; q.clips = {glitch}; return q; }(), "w", 128, 72, {}).graph);
        // VHS: smeared colours and dark scanlines every third row.
        const auto vhs = still(with([](Clip &c) {
                                   c.fx = "vhs";
                                   c.fxStrength = 1;
                               }),
                               10);
        QVERIFY(difference(vhs, plain) > 5);
        double lines = 0, rows = 0;
        for (int x = 0; x < 16; ++x) // the light grey bar
            for (int y = 3; y < 30; ++y)
                (y % 3 == 0 ? lines : rows) += qGray(vhs.pixel(x, y)) / (y % 3 == 0 ? 1. : 2.);
        QVERIFY2(lines < rows - 16 * 9 * 15, qPrintable(QString("%1 %2").arg(lines).arg(rows)));
        // Old film: sepia on the light grey bar.
        const auto film = QColor(still(with([](Clip &c) {
                                           c.fx = "film";
                                           c.fxStrength = 1;
                                       }),
                                       10)
                                     .pixel(8, 20));
        const QColor grey(plain.pixel(8, 20));
        QVERIFY(std::abs(grey.red() - grey.blue()) < 10);
        QVERIFY2(film.red() > film.blue() + 30, qPrintable(film.name()));
        // No strength, no effect.
        QVERIFY(difference(still(with([](Clip &c) {
                                     c.fx = "film";
                                     c.fxStrength = 0;
                                 }),
                                 10),
                           plain) < 1);
        // Stabilize renders and leaves a still picture about where it was.
        const auto steady = still(with([](Clip &c) { c.stabilize = true; }), 10);
        QVERIFY2(difference(steady, plain) < 8, qPrintable(QString::number(difference(steady, plain))));
        // Motion blur: as the bar passes x = 64, frames show grey instead of only black or white,
        // also in single-frame previews.
        auto bar = clip;
        bar.assetId = "moving";
        bar.duration = 30;
        int sharp = 0, smeared = 0;
        for (qint64 f = 6; f < 12; ++f) {
            const int g0 = qGray(still(bar, f).pixel(64, 36)),
                      g1 = qGray(still(with([&](Clip &c) {
                                           c = bar;
                                           c.motionBlur = 1;
                                       }),
                                       f)
                                     .pixel(64, 36));
            sharp += g0 > 40 && g0 < 215;
            smeared += g1 > 40 && g1 < 215;
        }
        QCOMPARE(sharp, 0);
        QVERIFY2(smeared >= 2, qPrintable(QString::number(smeared)));

        // Saved only when set; invalid values are refused.
        auto saved = p;
        saved.clips = {with([](Clip &c) {
            c.fx = "vhs";
            c.fxStrength = 0.7;
            c.motionBlur = 0.4;
            c.stabilize = true;
            c.reverb = 0.3;
            c.echo = 0.2;
        })};
        const auto json = saved.json();
        const auto loaded = Project::fromJson(json, {}).clips[0];
        QCOMPARE(loaded.fx, QString("vhs"));
        QCOMPARE(loaded.fxStrength, 0.7);
        QCOMPARE(loaded.motionBlur, 0.4);
        QVERIFY(loaded.stabilize);
        QCOMPARE(loaded.reverb, 0.3);
        QCOMPARE(loaded.echo, 0.2);
        saved.clips = {clip};
        const auto bare = saved.json()["clips"].toArray()[0].toObject();
        QVERIFY(!bare.contains("fx") && !bare.contains("fxStrength") && !bare.contains("stabilize") &&
                !bare.contains("reverb"));
        for (const auto &[key, value] : {std::pair{QString("fx"), QJsonValue("wobble")},
                                         std::pair{QString("fxStrength"), QJsonValue(2)},
                                         std::pair{QString("echo"), QJsonValue(-1)}}) {
            auto bad = json;
            auto clips = bad["clips"].toArray();
            auto o = clips[0].toObject();
            o[key] = value;
            clips[0] = o;
            bad["clips"] = clips;
            QVERIFY_EXCEPTION_THROWN(Project::fromJson(bad, {}), std::runtime_error);
        }
    }
    void reverbAndEcho() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // One short click, then silence.
        const auto source = dir.filePath("click.wav");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "aevalsrc='if(lt(t,0.02),0.8*sin(2*PI*1000*t),0)':d=2",
                     "-ar", "48000", "-ac", "2", source});
        Project p;
        p.width = 128;
        p.height = 72;
        Asset a;
        a.id = "s";
        a.path = source;
        a.kind = "audio";
        a.duration = 2;
        a.hasAudio = true;
        p.assets = {a};
        Clip c;
        c.id = "c";
        c.assetId = "s";
        c.duration = 60;
        const auto graph = dir.filePath("graph.txt");
        auto render = [&](const Clip &clip, const QString &name) {
            auto project = p;
            project.clips = {clip};
            RenderOptions options;
            options.video = false;
            const auto plan = compileRender(project, dir.filePath("work"), 128, 72, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            const auto out = dir.filePath(name + ".wav");
            QStringList args{"-v", "error", "-y"};
            args += plan.inputs;
            args << "-filter_complex_script" << graph << "-map" << "[aout]" << out;
            run(ffmpeg, args);
            return out;
        };
        auto peak = [&](const QString &file, double from, double to) {
            QProcess p;
            p.start(ffmpeg, {"-hide_banner", "-nostats", "-ss", QString::number(from), "-t",
                             QString::number(to - from), "-i", file, "-af", "volumedetect", "-f",
                             "null", "-"});
            p.waitForFinished(30000);
            const auto m = QRegularExpression("max_volume: (-?[0-9.]+|-inf) dB")
                               .match(QString::fromUtf8(p.readAllStandardError()));
            return !m.hasMatch() || m.captured(1) == "-inf" ? -999. : m.captured(1).toDouble();
        };
        const auto plain = render(c, "plain");
        QVERIFY(peak(plain, 0, 0.02) > -10);
        QVERIFY(peak(plain, 0.05, 0.15) < -60);
        auto x = c;
        x.reverb = 1;
        const auto room = render(x, "reverb");
        QVERIFY2(peak(room, 0.05, 0.15) > -30, "reflections after the click");
        QVERIFY(peak(room, 0.5, 1) < -60);
        x = c;
        x.echo = 1;
        const auto echo = render(x, "echo");
        QVERIFY2(peak(echo, 0.31, 0.36) > -20, "first repeat");
        QVERIFY2(peak(echo, 0.63, 0.68) > -25, "second repeat");
        QVERIFY(peak(echo, 0.1, 0.3) < -60);
        // Pan: fully left silences the right channel and keeps the left one.
        x = c;
        x.pan = -1;
        const auto panned = render(x, "pan");
        const auto right = dir.filePath("right.wav"), left = dir.filePath("left.wav");
        run(ffmpeg, {"-v", "error", "-y", "-i", panned, "-af", "pan=mono|c0=c1", right});
        run(ffmpeg, {"-v", "error", "-y", "-i", panned, "-af", "pan=mono|c0=c0", left});
        QVERIFY(peak(right, 0, 0.05) < -80);
        QVERIFY(std::abs(peak(left, 0, 0.05) - peak(plain, 0, 0.05)) < 1);
        x.pan = 0.5;
        const auto half = render(x, "half");
        run(ffmpeg, {"-v", "error", "-y", "-i", half, "-af", "pan=mono|c0=c0", left});
        QVERIFY(std::abs(peak(left, 0, 0.05) - (peak(plain, 0, 0.05) - 6.02)) < 1);
        // Animated pan on a steady tone: from left at the start to right at the end.
        const auto tone = dir.filePath("tone.wav");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "sine=f=440:d=2", "-ar", "48000", "-ac", "2", tone});
        p.assets[0].path = tone;
        x = c;
        x.keyframes["pan"] = {{0, -1, false}, {59, 1, false}};
        const auto sweep = render(x, "sweep");
        run(ffmpeg, {"-v", "error", "-y", "-i", sweep, "-af", "pan=mono|c0=c1", right});
        run(ffmpeg, {"-v", "error", "-y", "-i", sweep, "-af", "pan=mono|c0=c0", left});
        // (FFmpeg's sine peaks at -18 dBFS.)
        QVERIFY2(peak(left, 0.02, 0.1) > peak(right, 0.02, 0.1) + 15,
                 qPrintable(QString("%1 %2").arg(peak(right, 0.02, 0.1)).arg(peak(left, 0.02, 0.1))));
        QVERIFY(peak(right, 1.9, 1.98) > peak(left, 1.9, 1.98) + 15);
        QVERIFY(std::abs(peak(left, 0.95, 1.05) - peak(right, 0.95, 1.05)) < 3);
    }
    void beatDetection() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // Drum-like hits at a given tempo from `first` seconds on, over steady noise, with an
        // off-beat hi-hat, decoded the way the editor does.
        auto track = [&](double bpm, double first, double seconds) {
            const auto period = 60 / bpm;
            const auto path = dir.filePath(QString("beat%1.wav").arg(bpm));
            const auto kick = QString("if(gte(t,%1),exp(-25*mod(t-%1,%2))*sin(2*PI*70*mod(t-%1,%2)),0)")
                                  .arg(first)
                                  .arg(period);
            const auto hat = QString("if(gte(t,%1),0.15*exp(-80*mod(t-%1,%2))*sin(2*PI*7000*t),0)")
                                 .arg(first + period / 2)
                                 .arg(period);
            run(ffmpeg, {"-v", "error", "-y", "-f", "lavfi", "-i",
                         QString("aevalsrc='0.8*%1+%2':s=44100:d=%3[a];anoisesrc=a=0.02:d=%3:r=44100:seed=7[n];"
                                 "[a][n]amix=inputs=2:normalize=0")
                             .arg(kick, hat)
                             .arg(seconds),
                         "-ac", "2", path});
            const auto pcm = run(ffmpeg, {"-v", "error", "-i", path, "-ac", "1", "-ar", "11025",
                                          "-f", "f32le", "pipe:1"});
            QVector<float> samples(pcm.size() / 4);
            std::memcpy(samples.data(), pcm.constData(), size_t(samples.size()) * 4);
            return std::pair{path, samples};
        };
        for (const auto &[tempo, first] : {std::pair{120., 0.5}, std::pair{97., 1.2}, std::pair{174., 0.3}}) {
            const auto samples = track(tempo, first, 12).second;
            double bpm = 0;
            const auto beats = detectBeats(samples, 11025, &bpm);
            QVERIFY2(std::abs(bpm - tempo) < tempo * 0.02, qPrintable(QString("%1 → %2").arg(tempo).arg(bpm)));
            const double period = 60 / tempo;
            const int expected = int((12 - first) / period);
            QVERIFY2(std::abs(beats.size() - expected) <= 2,
                     qPrintable(QString("%1: %2 of %3").arg(tempo).arg(beats.size()).arg(expected)));
            double worst = 0;
            for (const auto t : beats) {
                const double n = std::round((t - first) / period);
                worst = std::max(worst, std::abs(t - first - n * period));
            }
            QVERIFY2(worst < 0.03, qPrintable(QString("%1: %2 s off").arg(tempo).arg(worst)));
            QVERIFY(beats.first() > first - 0.05);
        }
        // Silence and very short sound have no beats.
        QVERIFY(detectBeats(QVector<float>(11025 * 5, 0.f), 11025).isEmpty());
        QVERIFY(detectBeats(QVector<float>(1000, 0.5f), 11025).isEmpty());

        // In the editor: markers on every 2nd beat of a clip that starts at 1 s and skips the
        // first 2 s of the music, and clips snap to them.
        const auto music = track(120, 0.5, 12).first;
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(music)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        const auto id = editor.project().clips.first().id;
        editor.select(id);
        editor.setClip("duration", 240);
        editor.setClip("sourceIn", 2.0);
        editor.setClip("start", 30);
        QCOMPARE(editor.project().clips.first().start, 30);
        QCOMPARE(editor.project().clips.first().sourceIn.seconds(), 2.);
        editor.markBeats(2);
        QTRY_VERIFY_WITH_TIMEOUT(editor.state()["beats"].toMap()["status"] == "done", 30000);
        const auto markers = editor.project().markers;
        QVERIFY2(markers.size() >= 4, qPrintable(QString::number(markers.size())));
        // Source beats at 0.5 + k/2 s; from 2 s on that is 2.5 s, 3.5 s... every 2nd beat, so
        // timeline frames 30 + 15 + 30k (± 1).
        for (const auto &m : markers) {
            const auto offset = (m.frame - 45) % 30;
            QVERIFY2(m.frame >= 30 && (offset <= 1 || offset >= 29), qPrintable(QString::number(m.frame)));
        }
        QCOMPARE(editor.state()["beats"].toMap()["bpm"].toInt(), 120);
        QCOMPARE(editor.snap(markers[1].frame + 3, 5, {}, 0), markers[1].frame);
        editor.undo();
        QVERIFY(editor.project().markers.isEmpty());
        QVERIFY(editor.state()["error"].toString().isEmpty());
    }
    void freezeFrame() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // 2 s whose picture brightens frame by frame (grey level = 4 x frame), with sound.
        const auto source = dir.filePath("ramp.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=black:s=160x90:r=30:d=2,geq=lum='4*N':cb=128:cr=128",
                     "-f", "lavfi", "-i", "sine=f=440:d=2:sample_rate=48000", "-c:v", "ffv1", "-c:a",
                     "pcm_s16le", "-shortest", source});
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        auto clips = editor.project().clips;
        QCOMPARE(clips.size(), size_t(1));
        const auto id = clips[0].id;
        editor.select(id);
        editor.detachAudio();
        QCOMPARE(editor.project().clips.size(), size_t(2));
        editor.select(id);
        editor.setClip("scale", 0.5);
        editor.setClip("temperature", 0.4);
        editor.seek(20);
        editor.freezeFrame(1);
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().clips.size() == 5, 15000);
        QVERIFY2(editor.state()["error"].toString().isEmpty(), qPrintable(editor.state()["error"].toString()));
        const auto &p = editor.project();
        const auto *still = p.clip(editor.state()["selectedId"].toString());
        QVERIFY(still && still->name == "Freeze frame");
        QCOMPARE(still->start, 20);
        QCOMPARE(still->duration, 30);
        QCOMPARE(still->scale, 0.5);
        QCOMPARE(still->temperature, 0.4);
        QCOMPARE(p.asset(still->assetId)->kind, QString("image"));
        // The picture is the one at the playhead: grey level of frame 20.
        QImage image(p.asset(still->assetId)->path);
        QVERIFY(std::abs(qGray(image.pixel(80, 45)) - 80) < 8);
        // The rest of the clip and of its detached audio now starts after the still.
        int after = 0;
        for (const auto &c : p.clips)
            if (c.id != still->id && c.start == 50) {
                ++after;
                QCOMPARE(c.duration, 40);
                QVERIFY(std::abs(c.sourceIn.seconds() - 20 / 30.) < 1e-6);
            }
        QCOMPARE(after, 2);
        QCOMPARE(p.duration(), 90);
        editor.undo();
        QCOMPARE(editor.project().clips.size(), size_t(2));
        // Not outside the clip.
        editor.select(id);
        editor.setClip("start", 30);
        editor.seek(10);
        editor.freezeFrame(1);
        QVERIFY(editor.state()["error"].toString().contains("playhead"));
    }
    void anchorPointAndSpeedRange() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        QImage red(320, 180, QImage::Format_RGB32);
        red.fill(QColor(220, 0, 0));
        QVERIFY(red.save(dir.filePath("red.png")));
        Project p;
        p.width = 320;
        p.height = 180;
        Asset a;
        a.id = "red";
        a.path = dir.filePath("red.png");
        a.kind = "image";
        a.duration = 5;
        a.width = 320;
        a.height = 180;
        p.assets = {a};
        Clip c;
        c.id = "c";
        c.assetId = "red";
        c.duration = 30;
        c.scale = 0.5;
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Clip &clip, qint64 frame) {
            auto project = p;
            project.clips = {clip};
            RenderOptions options;
            options.audio = false;
            options.from = frame;
            options.to = frame + 1;
            const auto plan = compileRender(project, dir.filePath("work"), 320, 180, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage out;
            out.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return out.convertToFormat(QImage::Format_RGB32);
        };
        auto isRed = [](const QImage &i, int x, int y) { return QColor(i.pixel(x, y)).red() > 150; };
        // Centre anchor: half size in the middle.
        auto centred = still(c, 5);
        QVERIFY(!isRed(centred, 10, 10) && isRed(centred, 160, 90));
        // Top-left anchor: the top-left corner stays at the canvas corner.
        auto topLeft = c;
        topLeft.anchorX = topLeft.anchorY = 0;
        auto corner = still(topLeft, 5);
        QVERIFY(isRed(corner, 5, 5) && isRed(corner, 150, 80) && !isRed(corner, 175, 95));
        // Animated: zooming out towards the bottom-right corner keeps that corner in place.
        auto zoom = c;
        zoom.anchorX = zoom.anchorY = 1;
        zoom.scale = 1;
        zoom.keyframes["scale"] = {{0, 1, false}, {20, 0.5, false}};
        auto end = still(zoom, 25);
        QVERIFY(isRed(end, 314, 174) && isRed(end, 170, 100) && !isRed(end, 150, 80));
        auto start = still(zoom, 0);
        QVERIFY(isRed(start, 5, 5) && isRed(start, 314, 174));
        // The shift itself, with rotation: a quarter turn around the top-left corner.
        QSizeF base(320, 180);
        auto shift = Project::anchorShift(topLeft, base, 1, 90);
        // Centre offset (160, 90) from the anchor turns to (-90, 160).
        QVERIFY(std::abs(shift.x() - (-160 - 90)) < 1e-6 && std::abs(shift.y() - (-90 + 160)) < 1e-6);
        QCOMPARE(Project::anchorShift(c, base, 0.3, 45), QPointF());
        // Saved only when moved; refused outside the picture.
        p.clips = {topLeft};
        const auto json = p.json();
        QCOMPARE(Project::fromJson(json, {}).clips[0].anchorX, 0.);
        p.clips = {c};
        QVERIFY(!p.json()["clips"].toArray()[0].toObject().contains("anchor"));
        auto bad = json;
        auto clips = bad["clips"].toArray();
        auto o = clips[0].toObject();
        o["anchor"] = QJsonArray{1.5, 0};
        clips[0] = o;
        bad["clips"] = clips;
        QVERIFY_EXCEPTION_THROWN(Project::fromJson(bad, {}), std::runtime_error);

        // Speed from 0.1x to 10x: a 10x clip shows every tenth source frame and its sound is
        // a tenth as long.
        const auto ramp = dir.filePath("ramp.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=black:s=320x180:r=30:d=4,geq=lum='2*N':cb=128:cr=128",
                     "-f", "lavfi", "-i", "sine=f=440:d=4:sample_rate=48000", "-c:v", "ffv1", "-c:a",
                     "pcm_s16le", "-shortest", ramp});
        Asset v;
        v.id = "ramp";
        v.path = ramp;
        v.kind = "video";
        v.duration = 4;
        v.width = 320;
        v.height = 180;
        v.hasAudio = true;
        p.assets = {v};
        Clip fast;
        fast.id = "fast";
        fast.assetId = "ramp";
        fast.speed = Time(10, 1);
        fast.duration = 12;
        const auto frame = still(fast, 5);
        QVERIFY2(std::abs(qGray(frame.pixel(160, 90)) - 100) < 10, qPrintable(QString::number(qGray(frame.pixel(160, 90)))));
        p.clips = {fast};
        RenderOptions sound;
        sound.video = false;
        auto plan = compileRender(p, dir.filePath("work"), 320, 180, sound);
        {
            QFile g(graph);
            QVERIFY(g.open(QIODevice::WriteOnly | QIODevice::Truncate));
            g.write(plan.graph.toUtf8());
        }
        QCOMPARE(run(ffmpeg, streamArguments(plan, graph, false)).size(), qsizetype(48000 * 4 * 12 / 30));
        Clip slow = fast;
        slow.speed = Time(1, 10);
        slow.duration = 30;
        p.clips = {slow};
        p.validate();
        slow.speed = Time(1, 20);
        p.clips = {slow};
        QVERIFY_EXCEPTION_THROWN(p.validate(), std::runtime_error);
    }
    void gifAndSvgOverlays() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // A 1 s GIF that alternates red and green four times a second.
        const auto gif = dir.filePath("blink.gif");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i",
                     "color=black:s=64x64:r=4:d=1,format=rgb24,geq=r='255*eq(mod(N,2),0)':g='255*eq(mod(N,2),1)':b=0",
                     gif});
        // An SVG: a blue circle on nothing, twice as wide as tall.
        const auto svg = dir.filePath("badge.svg");
        {
            QFile f(svg);
            QVERIFY(f.open(QIODevice::WriteOnly));
            // A plain string: moc reads "//" inside raw strings as a comment.
            f.write("<svg xmlns=\"http:/" "/www.w3.org/2000/svg\" viewBox=\"0 0 100 50\">"
                    "<circle cx=\"50\" cy=\"25\" r=\"20\" fill=\"#0000ff\"/></svg>");
        }
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(gif), QUrl::fromLocalFile(svg)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 2, 15000);
        QVERIFY2(editor.state()["error"].toString().isEmpty(), qPrintable(editor.state()["error"].toString()));
        const auto &assets = editor.project().assets;
        const auto blink = std::find_if(assets.begin(), assets.end(), [](const Asset &a) { return a.name == "blink.gif"; });
        const auto badge = std::find_if(assets.begin(), assets.end(), [](const Asset &a) { return a.kind == "image"; });
        QVERIFY(blink != assets.end() && badge != assets.end());
        QVERIFY(blink->loops && blink->kind == "video");
        QVERIFY(std::abs(blink->duration - 1) < 0.1);
        // The SVG became a sharp transparent picture keeping its name and shape.
        QCOMPARE(badge->name, QString("badge.png"));
        QImage picture(badge->path);
        QCOMPARE(picture.size(), QSize(2048, 1024));
        QVERIFY(qAlpha(picture.pixel(5, 5)) == 0);
        const QColor centre = picture.pixelColor(1024, 512);
        QVERIFY(centre.blue() > 240 && centre.alpha() == 255);
        QVERIFY_EXCEPTION_THROWN(rasterizeSvg(gif, dir.path()), std::runtime_error);
        // A looping clip can be longer than its GIF and keeps alternating.
        editor.addAsset(blink->id);
        const auto id = editor.project().clips.back().id;
        editor.select(id);
        editor.setClip("duration", 120);
        QCOMPARE(editor.project().clip(id)->duration, 120);
        QCOMPARE(editor.trimBounds(id)["last"].toLongLong() > 120, true);
        const auto json = editor.project().json();
        QVERIFY(Project::fromJson(json, {}).assets[0].loops || Project::fromJson(json, {}).assets[1].loops);
        auto colour = [&](qint64 frame) {
            RenderOptions options;
            options.audio = false;
            options.from = frame;
            options.to = frame + 1;
            const auto plan = compileRender(editor.project(), dir.filePath("work"), 160, 90, options);
            const auto graph = dir.filePath("graph.txt");
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage out;
            out.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return out.pixelColor(80, 45);
        };
        // 4 frames a second: picture n = floor(t * 4), red when even.
        for (const auto &[frame, red] : {std::pair{qint64(61), true}, std::pair{qint64(69), false},
                                         std::pair{qint64(106), true}, std::pair{qint64(113), false}}) {
            const auto c = colour(frame);
            QVERIFY2(red ? (c.red() > 200 && c.green() < 60) : (c.green() > 200 && c.red() < 60),
                     qPrintable(QString("frame %1: %2").arg(frame).arg(c.name())));
        }
    }
    void slipRollAndSlide() {
        // Three touching clips from a 10 s source: A 0-30 (source 1 s), B 30-60 (source 3 s),
        // C 60-90 (source 5 s).
        Project p;
        Asset a;
        a.id = "v";
        a.path = "v.mkv";
        a.kind = "video";
        a.duration = 10;
        a.width = 160;
        a.height = 90;
        a.hasAudio = true;
        p.assets = {a};
        auto make = [](const QString &id, qint64 start, double in) {
            Clip c;
            c.id = id;
            c.assetId = "v";
            c.start = start;
            c.duration = 30;
            c.sourceIn = Time(qRound64(in * 1000), 1000);
            return c;
        };
        p.clips = {make("a", 0, 1), make("b", 30, 3), make("c", 60, 5)};
        auto at = [](const Project &q, const QString &id) { return *q.clip(id); };
        // Slip: B shows half a second later; nothing moves.
        auto slipped = p;
        slipped.slip("b", 15);
        slipped.validate();
        QCOMPARE(at(slipped, "b").sourceIn.seconds(), 3.5);
        QCOMPARE(at(slipped, "b").start, 30);
        QCOMPARE(at(slipped, "a").sourceIn.seconds(), 1.);
        // Not before the start of the source.
        auto early = p;
        QVERIFY_EXCEPTION_THROWN(early.slip("a", -40), std::runtime_error);
        // Roll: the A|B cut moves 6 frames later.
        auto rolled = p;
        rolled.roll("a", 6);
        rolled.validate();
        QCOMPARE(at(rolled, "a").duration, 36);
        QCOMPARE(at(rolled, "b").start, 36);
        QCOMPARE(at(rolled, "b").duration, 24);
        QCOMPARE(at(rolled, "b").sourceIn.seconds(), 3.2);
        QCOMPARE(rolled.duration(), 90);
        QVERIFY_EXCEPTION_THROWN(rolled.roll("c", 5), std::runtime_error); // nothing after C
        // Slide: B moves 3 frames later; A grows, C shrinks at its start; B keeps its source.
        auto slid = p;
        slid.slide("b", 3);
        slid.validate();
        QCOMPARE(at(slid, "a").duration, 33);
        QCOMPARE(at(slid, "b").start, 33);
        QCOMPARE(at(slid, "b").sourceIn.seconds(), 3.);
        QCOMPARE(at(slid, "c").start, 63);
        QCOMPARE(at(slid, "c").duration, 27);
        QCOMPARE(at(slid, "c").sourceIn.seconds(), 5.1);
        QCOMPARE(slid.duration(), 90);
        // Sliding past a neighbour's whole length is refused.
        auto far = p;
        QVERIFY_EXCEPTION_THROWN(far.slide("b", 30), std::runtime_error);
        // Without neighbours a clip slides into free space only.
        Project gap = p;
        gap.clips = {make("a", 0, 1), make("b", 40, 3), make("c", 80, 5)};
        auto moved = gap;
        moved.slide("b", -5);
        QCOMPARE(at(moved, "b").start, 35);
        QVERIFY_EXCEPTION_THROWN(gap.slide("b", -15), std::runtime_error);

        // Through the editor: one undo step each; detached audio slips along.
        FrameProvider frames;
        Editor e(&frames);
        QTemporaryDir dir;
        const auto source = dir.filePath("tone.mkv");
        run(Editor::executable("ffmpeg"), {"-v", "error", "-f", "lavfi", "-i", "color=gray:s=160x90:r=30:d=4", "-f", "lavfi",
                                           "-i", "sine=d=4", "-c:v", "ffv1", "-c:a", "pcm_s16le", "-shortest", source});
        e.configure(160, 90, 30, 1);
        e.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(e.project().assets.size() == 1, 15000);
        e.addAsset(e.project().assets.first().id);
        const auto id = e.project().clips.first().id;
        e.select(id);
        e.setClip("duration", 60);
        e.setClip("sourceIn", 1.0);
        e.detachAudio();
        QCOMPARE(e.project().clips.size(), size_t(2));
        e.slipClip(id, 15);
        QVERIFY2(e.state()["error"].toString().isEmpty(), qPrintable(e.state()["error"].toString()));
        for (const auto &c : e.project().clips)
            QCOMPARE(c.sourceIn.seconds(), 1.5);
        e.undo();
        for (const auto &c : e.project().clips)
            QCOMPARE(c.sourceIn.seconds(), 1.);
        e.slipClip(id, 200); // past the end of the 4 s source
        QVERIFY(!e.state()["error"].toString().isEmpty());
        QCOMPARE(e.project().clip(id)->sourceIn.seconds(), 1.);
    }
    void linkedPictureAndSound() {
        QTemporaryDir dir;
        const auto source = dir.filePath("talk.mkv");
        run(Editor::executable("ffmpeg"), {"-v", "error", "-f", "lavfi", "-i", "color=gray:s=160x90:r=30:d=4", "-f", "lavfi",
                                           "-i", "sine=d=4", "-c:v", "ffv1", "-c:a", "pcm_s16le", "-shortest", source});
        FrameProvider frames;
        Editor e(&frames);
        e.configure(160, 90, 30, 1);
        e.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(e.project().assets.size() == 1, 15000);
        e.addAsset(e.project().assets.first().id);
        const auto video = e.project().clips.first().id;
        e.select(video);
        e.detachAudio();
        const auto audio = e.state()["selectedId"].toString();
        QVERIFY(audio != video);
        QCOMPARE(e.project().linkedClips(video), QStringList{audio});
        QVERIFY(!e.project().clip(video)->link.isEmpty());
        // Moving or trimming either one takes the other along.
        e.moveClip(video, 45, e.project().clip(video)->track);
        QCOMPARE(e.project().clip(audio)->start, 45);
        e.trimClip(audio, 50, 90);
        QCOMPARE(e.project().clip(video)->start, 50);
        QCOMPARE(e.project().clip(video)->duration, 40);
        // A slip of the picture alone keeps the pair linked (sound offset on purpose).
        auto p = e.project();
        p.clip(video)->sourceIn = Time(1, 2);
        QCOMPARE(p.linkedClips(video), QStringList{audio});
        // Splitting both makes two linked pairs.
        e.seek(70);
        e.select(video);
        e.split();
        e.select(audio);
        e.split();
        QCOMPARE(e.project().clips.size(), size_t(4));
        for (const auto &c : e.project().clips)
            QCOMPARE(e.project().linkedClips(c.id).size(), 1);
        // Saved and loaded with the project.
        const auto loaded = Project::fromJson(e.project().json(), {});
        QCOMPARE(loaded.linkedClips(video), QStringList{audio});
        // Unlinked: they move on their own.
        e.select(video);
        e.unlinkClip();
        QVERIFY(e.project().linkedClips(video).isEmpty());
        QVERIFY(e.project().linkedClips(audio).isEmpty());
        e.moveClip(video, 100, e.project().clip(video)->track);
        QCOMPARE(e.project().clip(audio)->start, 50);
        e.select(video);
        e.unlinkClip();
        QVERIFY(e.state()["error"].toString().contains("not linked"));
    }
    void multipleSelectionAndGroups() {
        FrameProvider frames;
        Editor e(&frames);
        e.addTitle();
        e.addTitle();
        e.addTitle();
        auto ids = QStringList{};
        for (const auto &c : e.project().clips)
            ids << c.id;
        QCOMPARE(ids.size(), 3);
        // Spread the titles over two tracks: A at 0, B at 200 (track 0), C at 100 (track 1).
        e.addTrack();
        e.moveClip(ids[0], 0, 0);
        e.moveClip(ids[1], 200, 0);
        e.moveClip(ids[2], 100, 1);
        // Ctrl+click selection: A and C.
        e.select(ids[0]);
        e.toggleSelect(ids[2]);
        QCOMPARE(e.selection().size(), 2);
        QCOMPARE(e.state()["selectedIds"].toStringList().size(), 2);
        // Moving A by 30 frames moves C by 30 too, on its own track.
        e.moveClip(ids[0], 30, 0);
        QCOMPARE(e.project().clip(ids[0])->start, 30);
        QCOMPARE(e.project().clip(ids[2])->start, 130);
        QCOMPARE(e.project().clip(ids[2])->track, 1);
        QCOMPARE(e.project().clip(ids[1])->start, 200);
        // One undo step for all of them.
        e.undo();
        QCOMPARE(e.project().clip(ids[0])->start, 0);
        QCOMPARE(e.project().clip(ids[2])->start, 100);
        // Toggling again removes it.
        e.toggleSelect(ids[2]);
        QCOMPARE(e.selection(), QStringList{ids[0]});
        // Group A and C: selecting either brings both; the group survives saving.
        e.toggleSelect(ids[2]);
        e.groupSelection();
        e.select(ids[1]);
        QCOMPARE(e.selection(), QStringList{ids[1]});
        e.select(ids[2]);
        QCOMPARE(e.selection().size(), 2);
        QVERIFY(e.selection().contains(ids[0]));
        const auto loaded = Project::fromJson(e.project().json(), {});
        QCOMPARE(loaded.clip(ids[0])->group, loaded.clip(ids[2])->group);
        QVERIFY(!loaded.clip(ids[0])->group.isEmpty());
        QVERIFY(loaded.clip(ids[1])->group.isEmpty());
        // Ctrl+click on a grouped clip toggles the whole group.
        e.select(ids[1]);
        e.toggleSelect(ids[0]);
        QCOMPARE(e.selection().size(), 3);
        e.toggleSelect(ids[2]);
        QCOMPARE(e.selection(), QStringList{ids[1]});
        // Delete removes the whole group.
        e.select(ids[0]);
        e.remove(false);
        QCOMPARE(e.project().clips.size(), size_t(1));
        e.undo();
        QCOMPARE(e.project().clips.size(), size_t(3));
        // Ungroup, select all, and a group needs two clips.
        e.select(ids[0]);
        e.ungroupSelection();
        e.select(ids[0]);
        QCOMPARE(e.selection(), QStringList{ids[0]});
        e.groupSelection();
        QVERIFY(e.state()["error"].toString().contains("at least two"));
        e.clearError();
        e.selectAll();
        QCOMPARE(e.selection().size(), 3);
        // An area: frames 50-150 on track 1 touch only C; tracks 0-1 from 0 to 120 touch A and C.
        e.selectArea(50, 150, 1, 1, false);
        QCOMPARE(e.selection(), QStringList{ids[2]});
        e.selectArea(0, 120, 0, 1, false);
        QCOMPARE(e.selection().size(), 2);
        QVERIFY(e.selection().contains(ids[0]) && e.selection().contains(ids[2]));
        e.selectArea(190, 260, 0, 0, true); // adds B
        QCOMPARE(e.selection().size(), 3);
        // Copy A and C, paste at 300: they keep their distance and tracks; the copies are
        // selected; a group among copied clips becomes a new group of the copies.
        e.select(ids[0]);
        e.toggleSelect(ids[2]);
        e.groupSelection();
        e.select(ids[0]);
        e.copy();
        e.seek(300);
        const auto before = e.project().clips.size();
        e.paste();
        QCOMPARE(e.project().clips.size(), before + 2);
        const auto pasted = e.selection();
        QCOMPARE(pasted.size(), 2);
        const auto *pa = e.project().clip(pasted[0]), *pc = e.project().clip(pasted[1]);
        QCOMPARE(std::min(pa->start, pc->start), e.state()["playhead"].toLongLong());
        QCOMPARE(std::abs(pc->start - pa->start), 100);
        QCOMPARE(pa->group, pc->group);
        QVERIFY(pa->group != e.project().clip(ids[0])->group);
        e.undo();
        QCOMPARE(e.project().clips.size(), before);
    }
    void titlesThatBuildUp() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        Project p;
        p.width = 320;
        p.height = 180;
        Clip t;
        t.id = "t";
        t.text = "AB CD";
        t.fontSize = 120;
        t.duration = 60;
        auto lit = [&](const Clip &clip, qint64 frame) {
            auto project = p;
            project.clips = {clip};
            RenderOptions options;
            options.audio = false;
            options.from = frame;
            options.to = frame + 1;
            const auto plan = compileRender(project, dir.filePath("work"), 320, 180, options);
            const auto graph = dir.filePath("graph.txt");
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage out;
            out.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            int count = 0;
            for (int y = 0; y < out.height(); ++y)
                for (int x = 0; x < out.width(); ++x)
                    count += qGray(out.pixel(x, y)) > 128;
            return count;
        };
        const int whole = lit(t, 10);
        QVERIFY(whole > 500);
        // Typewriter over 1 s: one of four characters every quarter second.
        auto typed = t;
        typed.textAnimation = "typewriter";
        typed.textAnimationTime = 1;
        const int one = lit(typed, 3), two = lit(typed, 10), three = lit(typed, 20), all = lit(typed, 40);
        QVERIFY2(0 < one && one < two && two < three && three < all,
                 qPrintable(QString("%1 %2 %3 %4").arg(one).arg(two).arg(three).arg(all)));
        QVERIFY2(std::abs(all - whole) < whole / 20, qPrintable(QString("%1 vs %2").arg(all).arg(whole)));
        // Word by word over 1 s: "AB" for the first half, then everything.
        auto words = typed;
        words.textAnimation = "words";
        QVERIFY(std::abs(lit(words, 5) - two) < two / 10);
        QVERIFY(std::abs(lit(words, 20) - whole) < whole / 20);
        // Saved, and checked.
        p.clips = {typed};
        QCOMPARE(Project::fromJson(p.json(), {}).clips[0].textAnimation, QString("typewriter"));
        auto bad = p;
        bad.clips[0].textAnimation = "explode";
        QVERIFY_EXCEPTION_THROWN(bad.validate(), std::runtime_error);
    }
    void sceneDetection() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // Three shots of 1 s (red, green, blue) with sound, and a 0.2 s white flash in the last.
        const auto source = dir.filePath("shots.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i",
                     "color=red:s=160x90:r=30:d=1[a];color=lime:s=160x90:r=30:d=1[b];"
                     "color=blue:s=160x90:r=30:d=0.4[c];color=white:s=160x90:r=30:d=0.2[d];"
                     "color=blue:s=160x90:r=30:d=0.4[e];[a][b][c][d][e]concat=n=5:v=1:a=0",
                     "-f", "lavfi", "-i", "sine=d=3", "-c:v", "ffv1", "-c:a", "pcm_s16le",
                     "-shortest", source});
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        const auto id = editor.project().clips.first().id;
        editor.select(id);
        editor.detachAudio();
        QCOMPARE(editor.project().clips.size(), size_t(2));
        editor.select(id);
        QVERIFY(editor.state()["selected"].toMap()["video"].toBool());
        editor.splitAtScenes(0.5);
        QCOMPARE(editor.state()["scenes"].toMap()["status"].toString(), QString("finding"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.state()["scenes"].toMap()["status"].toString(),
                                  QString("done"), 30000);
        QCOMPARE(editor.state()["scenes"].toMap()["count"].toInt(), 2);
        // Video and detached audio are both cut at 1 s and 2 s; the flash is too short to split.
        QCOMPARE(editor.project().clips.size(), size_t(6));
        QVector<qint64> videoStarts, audioStarts;
        for (const auto &c : editor.project().clips)
            (c.audioOnly ? audioStarts : videoStarts) << c.start;
        std::sort(videoStarts.begin(), videoStarts.end());
        std::sort(audioStarts.begin(), audioStarts.end());
        QCOMPARE(videoStarts, (QVector<qint64>{0, 30, 60}));
        QCOMPARE(audioStarts, videoStarts);
        QCOMPARE(editor.project().clip(id)->duration, qint64(30));
        QCOMPARE(editor.project().clip(id)->start, qint64(0));
        editor.undo();
        QCOMPARE(editor.project().clips.size(), size_t(2));
        // Titles have no shots.
        editor.addTitle();
        editor.splitAtScenes();
        QVERIFY(editor.state()["error"].toString().contains("video clip"));
    }
    void clipboardAndAudioExport() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        const auto source = dir.filePath("clip.mkv");
        run(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "color=c=gray:s=160x90:r=30:d=2", "-f",
                     "lavfi", "-i", "sine=f=440:d=2", "-c:v", "ffv1", "-c:a", "pcm_s16le",
                     "-shortest", source});
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        const auto first = editor.project().clips.first().id;
        editor.select(first);
        editor.setClipValues({{"temperature", 0.4}, {"vignette", 0.3}, {"scale", 0.5},
                              {"volume", 0.5}});

        // Copy and paste at the playhead: a new clip with the same settings.
        QVERIFY(editor.state()["clipboard"].toString().isEmpty());
        editor.copy();
        QCOMPARE(editor.state()["clipboard"].toString(), editor.project().clips.first().name);
        // The playhead is inside the original, so the copy goes to the next track up.
        editor.seek(30);
        editor.paste();
        QCOMPARE(editor.project().clips.size(), size_t(2));
        const auto pasted = editor.project().clips.last();
        QVERIFY(pasted.id != first);
        QCOMPARE(pasted.start, qint64(30));
        QCOMPARE(pasted.track, 1);
        QCOMPARE(pasted.temperature, 0.4);
        QCOMPARE(editor.state()["selectedId"].toString(), pasted.id);
        editor.undo();
        QCOMPARE(editor.project().clips.size(), size_t(1));

        // Attributes onto another clip: "look" leaves position and volume alone.
        editor.addTitle();
        const auto title = editor.project().clips.last().id;
        editor.select(title);
        editor.pasteAttributes("look");
        auto *t = editor.project().clip(title);
        QCOMPARE(t->temperature, 0.4);
        QCOMPARE(t->vignette, 0.3);
        QCOMPARE(t->scale, 1.);
        editor.pasteAttributes("all");
        t = editor.project().clip(title);
        QCOMPARE(t->scale, 0.5);
        QCOMPARE(t->volume, 0.5);
        editor.undo();
        QCOMPARE(editor.project().clip(title)->scale, 1.);

        // Into a new project: the media comes along.
        editor.newProject();
        QVERIFY(editor.project().assets.empty());
        editor.paste();
        QCOMPARE(editor.project().assets.size(), size_t(1));
        QCOMPARE(editor.project().clips.size(), size_t(1));
        QCOMPARE(editor.project().clips.first().vignette, 0.3);

        // Audio-only export in three formats: no video stream, the timeline's length.
        for (const auto &format : {QString("mp3"), QString("m4a"), QString("wav")}) {
            const auto out = dir.filePath("sound." + format);
            editor.exportWith(QUrl::fromLocalFile(out), {{"format", format}, {"quality", "max"}});
            QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 60000);
            QVERIFY2(QFileInfo::exists(out), qPrintable(editor.state()["error"].toString()));
            const auto probe =
                QString::fromUtf8(run(Editor::executable("ffprobe"),
                                      {"-v", "error", "-show_entries",
                                       "stream=codec_type,codec_name,sample_rate,channels:format="
                                       "duration",
                                       "-of", "compact", out}));
            QVERIFY2(!probe.contains("codec_type=video") && probe.contains("codec_type=audio") &&
                         probe.contains("sample_rate=48000") && probe.contains("channels=2"),
                     qPrintable(probe));
            const auto duration = QRegularExpression("duration=([0-9.]+)").match(probe);
            QVERIFY2(std::abs(duration.captured(1).toDouble() - 2) < 0.1, qPrintable(probe));
        }
        QCOMPARE(editor.exportPreview({{"format", "wav"}})["audio"].toBool(), true);
        QCOMPARE(editor.exportPreview({{"format", "h264"}})["audio"].toBool(), false);
        editor.exportWith(QUrl::fromLocalFile(dir.filePath("wrong.mp4")), {{"format", "mp3"}});
        QVERIFY(editor.state()["error"].toString().contains(".mp3"));

        // The export queue: each job keeps the timeline as it was when queued, so editing on
        // (here: deleting the clip) does not change what the queued exports contain.
        const auto queued1 = dir.filePath("queue-1.wav"), second = dir.filePath("queue-2.mp3"),
                   dropped = dir.filePath("queue-3.wav");
        editor.queueExport(QUrl::fromLocalFile(queued1), {{"format", "wav"}});
        editor.queueExport(QUrl::fromLocalFile(second), {{"format", "mp3"}});
        editor.queueExport(QUrl::fromLocalFile(second), {{"format", "mp3"}});
        QVERIFY(editor.state()["error"].toString().contains("queue already"));
        editor.queueExport(QUrl::fromLocalFile(dir.filePath("queue.mp4")), {{"format", "wav"}});
        QVERIFY(editor.state()["error"].toString().contains(".wav"));
        editor.queueExport(QUrl::fromLocalFile(dropped), {{"format", "wav"}});
        auto queue = editor.state()["exportQueue"].toList();
        QCOMPARE(queue.size(), 3);
        QCOMPARE(queue[0].toMap()["status"].toString(), QString("exporting"));
        QCOMPARE(queue[1].toMap()["status"].toString(), QString("waiting"));
        QCOMPARE(queue[1].toMap()["file"].toString(), QString("queue-2.mp3"));
        editor.removeQueued(0); // running: stays
        editor.removeQueued(2);
        QCOMPARE(editor.state()["exportQueue"].toList().size(), 2);
        editor.select(editor.project().clips.first().id);
        editor.remove();
        QVERIFY(editor.project().clips.empty());
        QTRY_VERIFY_WITH_TIMEOUT(
            [&] {
                const auto q = editor.state()["exportQueue"].toList();
                return q[0].toMap()["status"] == "done" && q[1].toMap()["status"] == "done";
            }(),
            60000);
        QVERIFY(!QFileInfo::exists(dropped));
        for (const auto &out : {queued1, second}) {
            const auto probe = QString::fromUtf8(run(Editor::executable("ffprobe"),
                                                     {"-v", "error", "-show_entries",
                                                      "format=duration", "-of", "compact", out}));
            const auto duration = QRegularExpression("duration=([0-9.]+)").match(probe);
            QVERIFY2(std::abs(duration.captured(1).toDouble() - 2) < 0.1, qPrintable(probe));
        }
    }
    void colourAndLook() {
        QCOMPARE(filterPath("C:/a b/it's,[x];y=z.cube"),
                 QString("C\\\\:/a b/it\\\\\\'s\\,\\[x\\]\\;y\\\\=z.cube"));
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        auto image = [&](const QString &name, QColor colour, bool hole = false) {
            QImage i(320, 180, QImage::Format_ARGB32);
            i.fill(colour);
            if (hole) // a transparent left half
                for (int y = 0; y < 180; ++y)
                    for (int x = 0; x < 160; ++x)
                        i.setPixelColor(x, y, Qt::transparent);
            const auto path = dir.filePath(name);
            if (!i.save(path))
                throw std::runtime_error("Cannot save test image");
            return path;
        };
        Project p;
        p.width = 320;
        p.height = 180;
        for (const auto &[id, path] :
             {std::pair{QString("grey"), image("grey.png", QColor(128, 128, 128))},
              std::pair{QString("red"), image("red.png", QColor(200, 0, 0))},
              std::pair{QString("cut"), image("cut.png", QColor(128, 128, 128), true)}}) {
            Asset a;
            a.id = id;
            a.path = path;
            a.kind = "image";
            a.duration = 5;
            a.width = 320;
            a.height = 180;
            p.assets << a;
        }
        Clip clip;
        clip.id = "c";
        clip.assetId = "grey";
        clip.duration = 30;
        p.clips = {clip};
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Clip &c) {
            auto project = p;
            project.clips[project.clips.size() - 1] = c;
            RenderOptions options;
            options.audio = false;
            options.from = 5;
            options.to = 6;
            const auto plan = compileRender(project, dir.filePath("work"), 320, 180, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage out;
            out.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return out.convertToFormat(QImage::Format_RGB32);
        };
        auto at = [](const QImage &i, int x = 160, int y = 90) { return QColor(i.pixel(x, y)); };
        auto with = [&](auto change) {
            auto c = clip;
            change(c);
            return still(c);
        };
        const auto plain = at(still(clip));
        QVERIFY(std::abs(plain.red() - 128) < 6 && std::abs(plain.blue() - 128) < 6);
        auto warm = at(with([](Clip &c) { c.temperature = 1; }));
        QVERIFY2(warm.red() > warm.blue() + 20, qPrintable(warm.name()));
        auto cool = at(with([](Clip &c) { c.temperature = -1; }));
        QVERIFY2(cool.blue() > cool.red() + 20, qPrintable(cool.name()));
        auto magenta = at(with([](Clip &c) { c.tint = 1; }));
        QVERIFY2(magenta.green() < magenta.red() - 15, qPrintable(magenta.name()));
        auto green = at(with([](Clip &c) { c.tint = -1; }));
        QVERIFY2(green.green() > green.red() + 15, qPrintable(green.name()));
        auto lifted = at(with([](Clip &c) { c.shadows = 1; }));
        QVERIFY2(lifted.red() > plain.red() + 8, qPrintable(lifted.name()));
        auto lowered = at(with([](Clip &c) { c.highlights = -1; }));
        QVERIFY2(lowered.red() < plain.red() - 8, qPrintable(lowered.name()));
        auto glowing = at(with([](Clip &c) { c.glow = 1; }));
        QVERIFY2(glowing.red() > plain.red() + 30, qPrintable(glowing.name()));
        const auto vignette = with([](Clip &c) { c.vignette = 1; });
        QVERIFY(qGray(vignette.pixel(2, 2)) < qGray(vignette.pixel(160, 90)) - 40);
        const auto grain = with([](Clip &c) { c.grain = 1; });
        int lo = 255, hi = 0;
        for (int x = 100; x < 220; ++x) {
            lo = std::min(lo, qGray(grain.pixel(x, 90)));
            hi = std::max(hi, qGray(grain.pixel(x, 90)));
        }
        QVERIFY2(hi - lo > 20, qPrintable(QString::number(hi - lo)));
        auto sharp = at(with([](Clip &c) { c.sharpen = 1; }));
        QVERIFY(std::abs(sharp.red() - plain.red()) < 6); // a flat picture stays flat
        // A LUT that maps everything to blue, from a folder with awkward characters.
        QVERIFY(QDir(dir.path()).mkpath("odd dir;it's,[x]"));
        const auto lutPath = dir.filePath("odd dir;it's,[x]/blue.cube");
        {
            QFile lut(lutPath);
            QVERIFY(lut.open(QIODevice::WriteOnly));
            lut.write("TITLE \"blue\"\nLUT_3D_SIZE 2\n");
            for (int i = 0; i < 8; ++i)
                lut.write("0 0 1\n");
        }
        auto blue = at(with([&](Clip &c) { c.lut = lutPath; }));
        QVERIFY2(blue.blue() > 240 && blue.red() < 15, qPrintable(blue.name()));
        auto half = at(with([&](Clip &c) {
            c.lut = lutPath;
            c.lutStrength = 0.5;
        }));
        QVERIFY2(std::abs(half.blue() - 191) < 12 && std::abs(half.red() - 64) < 12,
                 qPrintable(half.name()));
        auto missing = at(with([&](Clip &c) { c.lut = dir.filePath("gone.cube"); }));
        QVERIFY(std::abs(missing.red() - plain.red()) < 6);
        // Alpha survives every look filter: the red clip below shows through the hole.
        Clip below = clip;
        below.id = "below";
        below.assetId = "red";
        p.clips = {below, clip};
        p.clips[1].track = 1;
        auto cutout = p.clips[1];
        cutout.assetId = "cut";
        cutout.vignette = cutout.grain = cutout.glow = cutout.sharpen = 1;
        cutout.temperature = cutout.vibrance = cutout.shadows = 0.5;
        cutout.lut = lutPath;
        cutout.lutStrength = 0.5;
        const auto layered = still(cutout);
        const auto hole = at(layered, 80, 90);
        QVERIFY2(hole.red() > 150 && hole.blue() < 60, qPrintable(hole.name()));
        QVERIFY(at(layered, 240, 90).blue() > 120);

        // Saved with the project: values only when set, the LUT relative to the project.
        p.clips[1] = cutout;
        const auto json = p.json(dir.path());
        QCOMPARE(json["schemaVersion"].toInt(), 11);
        const auto saved = json["clips"].toArray()[1].toObject();
        QCOMPARE(saved["lut"].toString(), QString("odd dir;it's,[x]/blue.cube"));
        QVERIFY(!json["clips"].toArray()[0].toObject().contains("vignette"));
        const auto loaded = Project::fromJson(json, dir.path());
        QCOMPARE(loaded.clips[1].lut, QDir::cleanPath(lutPath));
        QCOMPARE(loaded.clips[1].grain, 1.);
        QCOMPARE(loaded.clips[1].temperature, 0.5);
        auto bad = json;
        auto clips = bad["clips"].toArray();
        auto first = clips[0].toObject();
        first["tint"] = 2;
        clips[0] = first;
        bad["clips"] = clips;
        QVERIFY_EXCEPTION_THROWN(Project::fromJson(bad, dir.path()), std::runtime_error);
    }
    void blurAndMosaic() {
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        // A fine black/white checkerboard: blurring turns it grey, pixelating makes flat blocks.
        const auto board = dir.filePath("board.png");
        QImage checker(320, 180, QImage::Format_RGB32);
        for (int y = 0; y < 180; ++y)
            for (int x = 0; x < 320; ++x)
                checker.setPixelColor(x, y, ((x / 2 + y / 2) % 2) ? Qt::white : Qt::black);
        QVERIFY(checker.save(board));
        Project p;
        p.width = 320;
        p.height = 180;
        p.fpsN = 30;
        Asset a;
        a.id = "board";
        a.path = board;
        a.kind = "image";
        a.duration = 5;
        a.width = 320;
        a.height = 180;
        p.assets = {a};
        Clip bg;
        bg.id = "bg";
        bg.assetId = "board";
        bg.duration = 60;
        Clip area;
        area.id = "area";
        area.effect = "blur";
        area.track = 1;
        area.duration = 60;
        area.effectWidth = 0.5;
        area.effectHeight = 0.5;
        p.clips = {bg, area};
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Project &project, qint64 frame) {
            RenderOptions options;
            options.audio = false;
            options.from = frame;
            options.to = frame + 1;
            const auto plan = compileRender(project, dir.filePath("work"), 320, 180, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage image;
            image.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return image.convertToFormat(QImage::Format_RGB32);
        };
        // Spread of grey levels in a 16×16 patch around a point: high for the checkerboard.
        auto spread = [](const QImage &image, int cx, int cy) {
            int lo = 255, hi = 0;
            for (int y = cy - 8; y < cy + 8; ++y)
                for (int x = cx - 8; x < cx + 8; ++x) {
                    const int v = qGray(image.pixel(x, y));
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            return hi - lo;
        };
        auto image = still(p, 10);
        QVERIFY2(spread(image, 160, 90) < 40, qPrintable(QString::number(spread(image, 160, 90))));
        QVERIFY(std::abs(qGray(image.pixel(160, 90)) - 128) < 30);
        QVERIFY(spread(image, 20, 20) > 200); // outside the area
        // Mosaic: flat blocks larger than the checker squares.
        auto mosaic = p;
        mosaic.clips[1].effect = "pixelate";
        image = still(mosaic, 10);
        int flat = 0;
        for (int y = 60; y < 120; y += 4)
            for (int x = 100; x < 220; x += 4)
                flat += image.pixel(x, y) == image.pixel(x + 1, y) &&
                        image.pixel(x, y) == image.pixel(x, y + 1);
        QVERIFY2(flat > 400, qPrintable(QString::number(flat)));
        QVERIFY(spread(image, 20, 20) > 200);
        // The area follows keyframed position: from the centre to the right edge.
        auto moving = p;
        moving.clips[1].keyframes["x"] = {{0, 0, false}, {30, 0.25, false}};
        image = still(moving, 0);
        QVERIFY(spread(image, 160, 90) < 40 && spread(image, 300, 90) > 200);
        image = still(moving, 30);
        QVERIFY(spread(image, 300, 90) < 40 && spread(image, 100, 90) > 200);
        // Outside its time, the area does nothing.
        auto later = p;
        later.clips[1].start = 30;
        later.clips[1].duration = 30;
        QVERIFY(spread(still(later, 10), 160, 90) > 200);
        QVERIFY(spread(still(later, 40), 160, 90) < 40);
        // Clip-wide blur of the picture itself.
        auto soft = p;
        soft.clips.removeLast();
        soft.clips[0].blur = 0.5;
        QVERIFY(spread(still(soft, 10), 20, 20) < 40);
        // Model: saved, bounded, sized for the preview frame, never in transitions or SRT.
        const auto loaded = Project::fromJson(mosaic.json(), {});
        QCOMPARE(loaded.clips[1].effect, QString("pixelate"));
        QCOMPARE(loaded.clips[1].effectWidth, 0.5);
        QVERIFY(mosaic.json()["schemaVersion"].toInt() >= 9);
        auto invalid = p;
        invalid.clips[1].effect = "swirl";
        QVERIFY_EXCEPTION_THROWN(invalid.validate(), std::runtime_error);
        QCOMPARE(p.pictureSize(p.clips[1], 320, 180), QSizeF(160, 90));
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(320, 180, 30, 1);
        editor.addEffect("pixelate");
        QCOMPARE(editor.project().clips.size(), size_t(1));
        const auto id = editor.project().clips.first().id;
        QCOMPARE(editor.state()["selected"].toMap()["effect"].toString(), QString("pixelate"));
        QVERIFY(std::abs(editor.clipBounds(id)["width"].toDouble() - 0.3) < 1e-9);
        QVERIFY(!editor.exportSrt(QUrl::fromLocalFile(dir.filePath("none.srt"))));
        editor.addEffect("swirl");
        QVERIFY(editor.state()["error"].toString().contains("Unknown effect"));
    }
    void titleTemplates() {
        Clip lower;
        lower.id = "lower";
        lower.duration = 90;
        lower.titleStyle = "lowerThird";
        lower.text = "Tim Example\nFounder, Cutlery";
        lower.accentColor = "#00ff00";
        lower.textColor = "#ffffff";
        lower.fadeIn = 0;
        // Layout: lower left inside the title-safe area, tight around the text.
        auto plate = titlePlate(lower, 1920, 1080, 1080);
        QVERIFY(!plate.image.isNull());
        QCOMPARE(plate.position.x(), 115);
        QCOMPARE(plate.position.y() + plate.image.height(), 929);
        QVERIFY(plate.image.width() < 1920 * 0.65 && plate.image.height() < 400);
        auto card = lower;
        card.titleStyle = "titleCard";
        const auto centred = titlePlate(card, 1920, 1080, 1080);
        QVERIFY(std::abs(centred.position.x() + centred.image.width() / 2 - 960) <= 2);
        auto plain = lower;
        plain.titleStyle.clear();
        QVERIFY(titlePlate(plain, 1920, 1080, 1080).image.isNull());
        // Text scales with the clip's scale.
        auto big = lower;
        big.scale = 2;
        QVERIFY(titlePlate(big, 1920, 1080, 1080).image.height() > plate.image.height() * 1.8);

        // Rendering over a blue background.
        const auto ffmpeg = Editor::executable("ffmpeg");
        QVERIFY2(!ffmpeg.isEmpty(), "FFmpeg is required for integration tests");
        QTemporaryDir dir;
        const auto background = dir.filePath("blue.png");
        QImage blue(320, 180, QImage::Format_RGB32);
        blue.fill(Qt::blue);
        QVERIFY(blue.save(background));
        Project p;
        p.width = 320;
        p.height = 180;
        p.fpsN = 30;
        Asset a;
        a.id = "bg";
        a.path = background;
        a.kind = "image";
        a.duration = 5;
        a.width = 320;
        a.height = 180;
        p.assets = {a};
        Clip bg;
        bg.id = "bg";
        bg.assetId = "bg";
        bg.duration = 90;
        lower.track = 1;
        lower.fontSize = 20; // proportionate in this 180-pixel-high project
        p.clips = {bg, lower};
        const auto graph = dir.filePath("graph.txt");
        auto still = [&](const Project &project, qint64 frame) {
            RenderOptions options;
            options.audio = false;
            options.from = frame;
            options.to = frame + 1;
            const auto plan = compileRender(project, dir.filePath("work"), 320, 180, options);
            QFile g(graph);
            if (!g.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error("Cannot write graph");
            g.write(plan.graph.toUtf8());
            g.close();
            QImage image;
            image.loadFromData(run(ffmpeg, renderArguments(plan, graph, {}, "", 0)), "PNG");
            return image.convertToFormat(QImage::Format_RGB32);
        };
        plate = titlePlate(lower, 320, 180, 180);
        const QPoint barPoint = plate.position + QPoint(1, plate.image.height() / 2),
                     platePoint = plate.position + QPoint(plate.image.width() - 10, plate.image.height() / 2);
        auto image = still(p, 45); // settled
        QColor bar = image.pixelColor(barPoint), dark = image.pixelColor(platePoint);
        QVERIFY2(bar.green() > 200 && bar.red() < 60 && bar.blue() < 60, qPrintable(bar.name()));
        QVERIFY2(dark.blue() < 90 && dark.red() < 60, qPrintable(dark.name()));
        image = still(p, 0); // still outside, sliding in from the left
        QVERIFY2(image.pixelColor(platePoint).blue() > 200, qPrintable(image.pixelColor(platePoint).name()));
        // Moved with x/y.
        auto moved = p;
        moved.clips[1].y = -0.5;
        image = still(moved, 45);
        QVERIFY(image.pixelColor(barPoint).blue() > 200);
        QVERIFY2(image.pixelColor(barPoint - QPoint(0, 90)).green() > 200,
                 qPrintable(image.pixelColor(barPoint - QPoint(0, 90)).name()));

        // Model and editor.
        const auto loaded = Project::fromJson(p.json(), {});
        QCOMPARE(loaded.clips[1].titleStyle, QString("lowerThird"));
        QCOMPARE(loaded.clips[1].accentColor, QString("#00ff00"));
        QVERIFY(p.json()["schemaVersion"].toInt() >= 10);
        auto invalid = p;
        invalid.clips[1].titleStyle = "banner";
        QVERIFY_EXCEPTION_THROWN(invalid.validate(), std::runtime_error);
        FrameProvider frames;
        Editor editor(&frames);
        editor.configure(320, 180, 30, 1);
        editor.addTitleTemplate("lowerThird");
        const auto id = editor.project().clips.first().id;
        QCOMPARE(editor.state()["selected"].toMap()["titleStyle"].toString(), QString("lowerThird"));
        const auto bounds = editor.clipBounds(id);
        const auto expected = titlePlate(*editor.project().clip(id), 320, 180, 180);
        QVERIFY(std::abs(bounds["x"].toDouble() - expected.position.x() / 320.) < 1e-9);
        QVERIFY(std::abs(bounds["width"].toDouble() - expected.image.width() / 320.) < 1e-9);
        editor.addTitleTemplate("banner");
        QVERIFY(editor.state()["error"].toString().contains("Unknown title template"));
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
