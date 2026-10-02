#include "Editor.h"
#include "KeyboardShortcuts.h"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
using namespace cutlery;
class UiTest : public QObject {
    Q_OBJECT
    static QQuickItem *findItem(QQuickItem *root, const QString &name) {
        if (root->objectName() == name)
            return root;
        for (auto *child : root->childItems())
            if (auto *found = findItem(child, name))
                return found;
        return nullptr;
    }
    static void drag(QQuickWindow *window, QPoint from, QPoint to) {
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(window, from + QPoint(12, 0), 20);
        QTest::mouseMove(window, from + QPoint(20, 0), 20);
        QTest::mouseMove(window, (from + to) / 2, 20);
        QTest::mouseMove(window, to, 20);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        QTest::qWait(30);
    }
    static QPoint center(QQuickItem *item) {
        return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
    }
  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QQuickStyle::setStyle("Basic");
    }
    void dragDropAndMagnet() {
        QTemporaryDir dir;
        const auto picture = dir.filePath("Image.png"), video = dir.filePath("Video.mp4"),
                   audio = dir.filePath("Audio.wav");
        QImage image(160, 90, QImage::Format_RGB32);
        image.fill(Qt::green);
        QVERIFY(image.save(picture));
        QProcess generate;
        generate.start(Editor::executable("ffmpeg"),
                       {"-v", "error", "-f", "lavfi", "-i",
                        "color=blue:size=160x90:rate=30:duration=2", "-f", "lavfi", "-i",
                        "sine=frequency=440:duration=2", "-c:v", "mpeg4", "-c:a", "aac",
                        "-shortest", video});
        QVERIFY(generate.waitForFinished(15000));
        QCOMPARE(generate.exitCode(), 0);
        generate.start(Editor::executable("ffmpeg"), {"-v", "error", "-f", "lavfi", "-i",
                                                      "sine=frequency=440:duration=2", audio});
        QVERIFY(generate.waitForFinished(15000));
        QCOMPARE(generate.exitCode(), 0);
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.importMedia(
            {QUrl::fromLocalFile(video), QUrl::fromLocalFile(audio), QUrl::fromLocalFile(picture)});
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["importing"].toBool(), 15000);
        QCOMPARE(editor.project().assets.size(), 3);
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &e : errors)
                        warnings << e.toString();
                });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->requestActivate();
        QTest::qWait(150);
        auto *viewport = findItem(window->contentItem(), "trackViewport");
        QVERIFY(viewport);
        auto trackPoint = [&](int track, int frame) {
            return viewport->mapToScene(QPointF(double(frame) / 30 * 48, (2 - track) * 86 + 43))
                .toPoint();
        };
        auto *library = findItem(window->contentItem(), "mediaLibrary");
        QVERIFY(library);
        QStringList clipIds;
        for (int i = 0; i < 3; ++i) {
            auto *tile = findItem(window->contentItem(), "asset-" + editor.project().assets[i].id);
            QVERIFY(tile);
            // Scroll the library so the tile is in view.
            auto *content = qvariant_cast<QQuickItem *>(library->property("contentItem"));
            QVERIFY(content);
            const double top = tile->mapToItem(content, QPointF(0, 0)).y();
            library->setProperty(
                "contentY",
                std::max(0., std::min(top - 10, library->property("contentHeight").toDouble() -
                                                    library->height())));
            QTest::qWait(30);
            drag(window, center(tile), trackPoint(i, 30 * (i + 1)));
            QTRY_COMPARE_WITH_TIMEOUT(editor.project().clips.size(), i + 1, 2000);
            const auto c = editor.project().clips.last();
            clipIds << c.id;
            QCOMPARE(c.track, i);
            QCOMPARE(c.start, qint64(30 * (i + 1)));
        }
        // Every media kind supports moving on its track and between tracks.
        for (int i = 0; i < 3; ++i) {
            auto *clip = findItem(window->contentItem(), "clip-" + clipIds[i]);
            QVERIFY(clip);
            const auto start = center(clip);
            drag(window, start, start + QPoint(48, 0));
            QCOMPARE(editor.project().clips[i].start, qint64(30 * (i + 2)));
        }
        auto *clip = findItem(window->contentItem(), "clip-" + clipIds[0]);
        QVERIFY(clip);
        drag(window, center(clip), center(clip) + QPoint(0, -86));
        QCOMPARE(editor.project().clips[0].track, 1);
        editor.undo();
        QTest::qWait(30);
        // Independent snapping controls must affect actual pointer edits.
        editor.setTrack(0, "snapping", false);
        editor.seek(0);
        QTest::qWait(20);
        clip = findItem(window->contentItem(), "clip-" + clipIds[0]);
        drag(window, center(clip), center(clip) + QPoint(3, 0));
        QCOMPARE(editor.project().clips[0].start, qint64(62));
        editor.setTrack(0, "snapping", true);
        QTest::qWait(20);
        clip = findItem(window->contentItem(), "clip-" + clipIds[0]);
        drag(window, center(clip), center(clip) - QPoint(4, 0));
        QCOMPARE(editor.project().clips[0].start, qint64(60));
        // Escape cancels both a timeline move and a library insertion.
        const auto beforeCancel = editor.project().json();
        clip = findItem(window->contentItem(), "clip-" + clipIds[0]);
        auto from = center(clip), to = from + QPoint(100, 0);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(window, to, 30);
        QTest::keyClick(window, Qt::Key_Escape);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        QCOMPARE(editor.project().json(), beforeCancel);
        auto *tile = findItem(window->contentItem(), "asset-" + editor.project().assets[2].id);
        QVERIFY(tile);
        from = center(tile);
        to = trackPoint(2, 240);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(window, to, 30);
        QTest::keyClick(window, Qt::Key_Escape);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        QTest::qWait(30);
        QCOMPARE(editor.project().json(), beforeCancel);
        editor.setTrack(2, "locked", true);
        QTest::qWait(20);
        tile = findItem(window->contentItem(), "asset-" + editor.project().assets[2].id);
        QVERIFY(tile);
        drag(window, center(tile), trackPoint(2, 300));
        QCOMPARE(editor.project().clips.size(), 3);
        editor.setTrack(2, "locked", false);
        // Add a second clip and enable Magnet with the real track button.
        editor.insertAsset(editor.project().assets[0].id, 0, 240);
        QTest::qWait(40);
        auto *magnet = findItem(window->contentItem(), "magnetic-0");
        QVERIFY(magnet);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(magnet));
        QVERIFY(editor.project().trackSettings[0].magnetic);
        QCOMPARE(editor.project().clips[0].start, qint64(0));
        const auto second = editor.project().clips.last().id;
        clip = findItem(window->contentItem(), "clip-" + second);
        drag(window, center(clip), trackPoint(0, 5));
        QCOMPARE(editor.project().clips.last().start, qint64(0));
        QCOMPARE(editor.project().clips[0].start, qint64(60));
        editor.undo();
        QCOMPARE(editor.project().clips[0].start, qint64(0));
        // OS file drops exercise the native URL MIME path, including timeline placement.
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(picture)});
        auto target = trackPoint(2, 300);
        QDragEnterEvent enter(target, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop(target, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &drop);
        QVERIFY(drop.isAccepted());
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["importing"].toBool(), 15000);
        QCOMPARE(editor.project().clips.size(), 5);
        QCOMPARE(editor.project().clips.last().track, 2);
        QCOMPARE(editor.project().clips.last().start, qint64(300));
        QDragEnterEvent libraryEnter(QPoint(60, 180), Qt::CopyAction, &mime, Qt::LeftButton,
                                     Qt::NoModifier);
        QCoreApplication::sendEvent(window, &libraryEnter);
        QVERIFY(libraryEnter.isAccepted());
        QDropEvent libraryDrop(QPointF(60, 180), Qt::CopyAction, &mime, Qt::LeftButton,
                               Qt::NoModifier);
        QCoreApplication::sendEvent(window, &libraryDrop);
        QVERIFY(libraryDrop.isAccepted());
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["importing"].toBool(), 15000);
        QCOMPARE(editor.project().assets.size(), 5);
        QCOMPARE(editor.project().clips.size(), 5);
        // Video and image clips show filmstrip tiles once background extraction finishes.
        int filmstrips = 0;
        for (const auto &clip : editor.project().clips) {
            const auto *asset = editor.project().asset(clip.assetId);
            auto *strip = findItem(window->contentItem(), "filmstrip-" + clip.id);
            QVERIFY(asset && strip);
            if (asset->kind == "audio") {
                QVERIFY(!strip->isVisible());
                continue;
            }
            ++filmstrips;
            QTRY_VERIFY_WITH_TIMEOUT(strip->isVisible(), 30000);
            QTRY_VERIFY_WITH_TIMEOUT(
                [&] {
                    for (auto *tile : strip->childItems())
                        if (tile->inherits("QQuickImage") && tile->property("status").toInt() == 1)
                            return true;
                    return false;
                }(),
                10000);
        }
        QVERIFY(filmstrips >= 2);
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
        QVERIFY(editor.save(QUrl::fromLocalFile(dir.filePath("drag.cutlery"))));
    }
    void transitionMarkers() {
        QTemporaryDir dir;
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(160, 90, 30, 1);
        editor.addTitle();
        const auto first = editor.project().clips.first().id;
        editor.seek(89);
        editor.addTitle();
        const auto second = editor.project().clips.last().id;
        editor.moveClip(second, 90, editor.project().clips.last().track);
        QCOMPARE(editor.project().clips.last().start, qint64(90));
        QVERIFY(editor.project().previousAdjacent(editor.project().clips.last()));
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &e : errors)
                        warnings << e.toString();
                });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTest::qWait(100);
        QVERIFY(!findItem(window->contentItem(), "transitionMarker-" + first)->isVisible());
        auto *marker = findItem(window->contentItem(), "transitionMarker-" + second);
        QVERIFY(marker && marker->isVisible());
        const auto point =
            marker->mapToScene(QPointF(marker->width() / 2, marker->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE(editor.project().clips.last().transition, QString("fade"));
        QCOMPARE(editor.state()["selectedId"].toString(), second);
        QCOMPARE(editor.project().clips.last().transitionFrames, qint64(15));
        QCOMPARE(editor.project().transitionLength(editor.project().clips.last()), qint64(15));
        auto *type = findItem(window->contentItem(), "transitionType");
        QVERIFY(type && type->isVisible());
        QTRY_COMPARE(type->property("currentText").toString(), QString("Dissolve"));
        editor.setClip("transition", "wipeleft");
        QTRY_COMPARE(type->property("currentText").toString(), QString("Wipe left"));
        editor.undo();
        editor.undo();
        QVERIFY(editor.project().clips.last().transition.isEmpty());
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void keyframeControls() {
        QTemporaryDir dir;
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(160, 90, 30, 1);
        editor.addTitle();
        const auto id = editor.project().clips.first().id;
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &e : errors)
                        warnings << e.toString();
                });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTest::qWait(100);
        auto click = [&](QQuickItem *item) {
            QVERIFY(item && item->isVisible() && item->isEnabled());
            QTest::mouseClick(
                window, Qt::LeftButton, Qt::NoModifier,
                item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        };
        // Inspector buttons may be scrolled out of view; press them directly.
        auto press = [&](QQuickItem *item) {
            QVERIFY(item && item->isVisible() && item->isEnabled());
            QVERIFY(QMetaObject::invokeMethod(item, "clicked"));
        };
        editor.seek(0);
        auto *diamond = findItem(window->contentItem(), "keyframe-scale");
        press(diamond);
        QTRY_COMPARE(editor.project().clips.first().keyframes["scale"].size(), 1);
        QCOMPARE(diamond->property("text").toString(), QString("◆"));
        // With keyframes, editing the value at another frame adds a keyframe there.
        editor.seek(60);
        QCOMPARE(diamond->property("text").toString(), QString("◇"));
        editor.setClip("scale", .5);
        const auto &list = editor.project().clips.first().keyframes["scale"];
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.last().frame, qint64(60));
        QCOMPARE(editor.project().clips.first().scale, 1.);
        editor.seek(30);
        const auto animated = editor.state()["selected"].toMap()["animated"].toMap();
        QCOMPARE(animated["scale"].toDouble(), .75);
        QTRY_VERIFY(findItem(window->contentItem(), "keyframe-" + id + "-60"));
        click(findItem(window->contentItem(), "keyframe-" + id + "-60"));
        QTRY_COMPARE(editor.state()["playhead"].toLongLong(), qint64(60));
        press(findItem(window->contentItem(), "previousKeyframe"));
        QTRY_COMPARE(editor.state()["playhead"].toLongLong(), qint64(0));
        press(diamond);
        QTRY_COMPARE(editor.project().clips.first().keyframes["scale"].size(), 1);
        editor.seek(60);
        press(diamond);
        QTRY_VERIFY(!editor.project().clips.first().keyframes.contains("scale"));
        QCOMPARE(editor.project().clips.first().scale, .5);
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void exportDialog() {
        QTemporaryDir dir;
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(1920, 1080, 30, 1);
        editor.addTitle();
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &e : errors)
                        warnings << e.toString();
                });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        auto *dialog = window->findChild<QObject *>("exportSettings");
        QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        auto *preset = findItem(window->contentItem(), "exportPreset");
        QTRY_VERIFY(preset && preset->isVisible());
        auto choose = [&](const char *name, int index) {
            auto *combo = findItem(window->contentItem(), name);
            QVERIFY(combo);
            combo->setProperty("currentIndex", index);
            QVERIFY(QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, index)));
        };
        choose("exportPreset", 1);
        QCOMPARE(findItem(window->contentItem(), "exportFormat")->property("currentText").toString(),
                 QString("H.264 · MP4 (plays everywhere)"));
        QCOMPARE(findItem(window->contentItem(), "exportHeight")->property("currentText").toString(),
                 QString("4K (2160p)"));
        const auto current = dialog->property("current").toMap();
        QCOMPARE(current["quality"].toString(), QString("max"));
        QCOMPARE(current["loudness"].toDouble(), -14.);
        QCOMPARE(findItem(window->contentItem(), "exportLoudness")->property("currentText").toString(),
                 QString("YouTube & streaming (−14 LUFS)"));
        QCOMPARE(dialog->property("preview").toMap()["width"].toInt(), 3840);
        choose("exportLoudness", 0); // keep the mix: no longer the YouTube preset
        QCOMPARE(dialog->property("current").toMap()["loudness"].toDouble(), 0.);
        QCOMPARE(preset->property("currentIndex").toInt(), 0);
        choose("exportPreset", 1);
        choose("exportQuality", 2);
        QCOMPARE(preset->property("currentIndex").toInt(), 0);
        choose("exportFormat", 4);
        QCOMPARE(dialog->property("preview").toMap()["extension"].toString(), QString("mov"));
        // The range choice appears once an in or out point exists.
        QVERIFY(!findItem(window->contentItem(), "exportRange")->isVisible());
        editor.seek(10);
        editor.setInPoint();
        QTRY_VERIFY(findItem(window->contentItem(), "exportRange")->isVisible());
        editor.toggleMarker();
        QTRY_VERIFY(findItem(window->contentItem(), "marker-0"));
        choose("exportFormat", 6); // MP3: audio only, no resolution
        QCOMPARE(dialog->property("preview").toMap()["extension"].toString(), QString("mp3"));
        QTRY_VERIFY(!findItem(window->contentItem(), "exportHeight")->isEnabled());
        choose("exportFormat", 0);
        QTRY_VERIFY(findItem(window->contentItem(), "exportHeight")->isEnabled());
        auto *result = findItem(window->contentItem(), "loudnessResult");
        QVERIFY(result && findItem(window->contentItem(), "measureLoudness"));
        QVERIFY(result->property("text").toString().contains("Integrated loudness"));
        QVERIFY(findItem(window->contentItem(), "levelMeter"));
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void presenterControls() {
        QTemporaryDir dir;
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(1280, 720, 30, 1);
        editor.addTitle();
        editor.addTitle();
        auto &clips = editor.project().clips;
        const auto top = clips.last().id;
        editor.select(top);
        editor.seek(10);
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &e : errors)
                        warnings << e.toString();
                });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTest::qWait(100);
        auto press = [&](const QString &name) {
            auto *item = findItem(window->contentItem(), name);
            QVERIFY2(item && item->isVisible() && item->isEnabled(), qPrintable(name));
            QVERIFY(QMetaObject::invokeMethod(item, "clicked"));
        };
        auto clip = [&]() { return *editor.project().clip(top); };
        press("place-bottomRight");
        QCOMPARE(clip().scale, .3);
        // 30% of 1280x720 is 384x216; centred 0.03*720 = 21.6 px from the bottom-right corner.
        QVERIFY(std::abs(clip().x - (0.5 - (192 + 21.6) / 1280)) < 1e-9);
        QVERIFY(std::abs(clip().y - (0.5 - (108 + 21.6) / 720)) < 1e-9);
        auto *box = findItem(window->contentItem(), "transformBox");
        QTRY_VERIFY(box && box->isVisible());
        auto *move = findItem(window->contentItem(), "transformMove");
        QVERIFY(move);
        // Drag the overlay left by a quarter of the viewer width.
        const auto canvasWidth = box->property("canvasWidth").toDouble();
        const auto start = move->mapToScene(QPointF(move->width() / 2, move->height() / 2)).toPoint();
        const auto end = start - QPoint(int(canvasWidth / 4), 0);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, start - QPoint(10, 0), 20);
        QTest::mouseMove(window, end, 20);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);
        const double movedX = 0.5 - (192 + 21.6) / 1280 - 0.25;
        QTRY_VERIFY2(std::abs(clip().x - movedX) < 0.01, qPrintable(QString::number(clip().x)));
        QCOMPARE(clip().scale, .3);
        // Drag the bottom-right corner outwards to enlarge.
        auto *corner = findItem(window->contentItem(), "transformCorner3");
        QVERIFY(corner);
        const auto cornerPoint =
            corner->mapToScene(QPointF(corner->width() / 2, corner->height() / 2)).toPoint();
        const auto centre = box->mapToScene(QPointF(box->width() / 2, box->height() / 2)).toPoint();
        const auto outward = cornerPoint + (cornerPoint - centre) / 2;
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, cornerPoint);
        QTest::mouseMove(window, cornerPoint + QPoint(3, 3), 20);
        QTest::mouseMove(window, outward, 20);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, outward);
        QTRY_VERIFY2(clip().scale > .4 && clip().scale < .5, qPrintable(QString::number(clip().scale)));
        editor.undo();
        QCOMPARE(clip().scale, .3);
        // Style controls.
        auto *shape = findItem(window->contentItem(), "overlayShape");
        QVERIFY(shape);
        shape->setProperty("currentIndex", 2);
        QVERIFY(QMetaObject::invokeMethod(shape, "activated", Q_ARG(int, 2)));
        QCOMPARE(clip().shape, QString("circle"));
        auto *key = findItem(window->contentItem(), "chromaKey");
        QVERIFY(key);
        key->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(key, "toggled"));
        QVERIFY(clip().chromaKey);
        press("place-full");
        QCOMPARE(clip().scale, 1.);
        QCOMPARE(clip().x, 0.);
        // A mosaic area: added from the library panel, configured in its own section.
        press("addMosaicArea");
        const auto area = editor.project().clips.last();
        QCOMPARE(area.effect, QString("pixelate"));
        QCOMPARE(editor.state()["selectedId"].toString(), area.id);
        auto *type = findItem(window->contentItem(), "effectType");
        QTRY_VERIFY(type && type->isVisible());
        QCOMPARE(type->property("currentIndex").toInt(), 1);
        type->setProperty("currentIndex", 0);
        QVERIFY(QMetaObject::invokeMethod(type, "activated", Q_ARG(int, 0)));
        QCOMPARE(editor.project().clip(area.id)->effect, QString("blur"));
        QVERIFY(!findItem(window->contentItem(), "overlayShape")->isVisible());
        // A lower third from the library panel; its style can be changed in the inspector.
        press("addLowerThird");
        const auto lower = editor.project().clips.last();
        QCOMPARE(lower.titleStyle, QString("lowerThird"));
        auto *style = findItem(window->contentItem(), "titleStyle");
        QTRY_VERIFY(style && style->isVisible());
        QCOMPARE(style->property("currentIndex").toInt(), 1);
        style->setProperty("currentIndex", 3);
        QVERIFY(QMetaObject::invokeMethod(style, "activated", Q_ARG(int, 3)));
        QCOMPARE(editor.project().clip(lower.id)->titleStyle, QString("titleCard"));
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void lookControls() {
        QTemporaryDir dir;
        QImage picture(160, 90, QImage::Format_RGB32);
        picture.fill(Qt::gray);
        const auto path = dir.filePath("grey.png");
        QVERIFY(picture.save(path));
        const auto lut = dir.filePath("look.cube");
        {
            QFile f(lut);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("LUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
        }
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(path)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        const auto id = editor.project().clips.first().id;
        editor.select(id);
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &e : errors)
                        warnings << e.toString();
                });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        auto *section = findItem(window->contentItem(), "lookSection");
        QTRY_VERIFY(section && section->isVisible());
        auto *preset = findItem(window->contentItem(), "lookPreset");
        QVERIFY(preset);
        auto clip = [&] { return *editor.project().clip(id); };
        QVERIFY(QMetaObject::invokeMethod(preset, "activated", Q_ARG(int, 5))); // Vintage
        QCOMPARE(clip().temperature, .3);
        QCOMPARE(clip().grain, .4);
        QCOMPARE(clip().saturation, .75);
        QCOMPARE(preset->property("currentIndex").toInt(), 0);
        editor.undo();
        QCOMPARE(clip().grain, 0.);
        auto *vignette = findItem(window->contentItem(), "look-vignette");
        QVERIFY(vignette);
        vignette->setProperty("value", 0.5);
        QVERIFY(QMetaObject::invokeMethod(vignette, "moved"));
        QCOMPARE(clip().vignette, .5);
        editor.setClip("lut", QUrl::fromLocalFile(lut));
        QCOMPARE(clip().lut, QDir::cleanPath(lut));
        QTRY_COMPARE(findItem(window->contentItem(), "lutName")->property("text").toString(),
                     QString("LUT: look"));
        QTRY_VERIFY(findItem(window->contentItem(), "lutStrength")->isVisible());
        editor.setClip("lut", dir.filePath("keys.json"));
        QVERIFY(editor.state()["error"].toString().contains(".cube"));
        QCOMPARE(clip().lut, QDir::cleanPath(lut));
        editor.clearError();
        editor.setClip("lut", "");
        QVERIFY(clip().lut.isEmpty());
        // Style effects: a picture has the effect choice, but no motion blur or stabilizing.
        QVERIFY(findItem(window->contentItem(), "effectsSection")->isVisible());
        QVERIFY(!findItem(window->contentItem(), "stabilize")->isVisible());
        auto *fx = findItem(window->contentItem(), "fxChoice");
        QVERIFY(QMetaObject::invokeMethod(fx, "activated", Q_ARG(int, 4))); // Old film
        QCOMPARE(clip().fx, QString("film"));
        QTRY_VERIFY(findItem(window->contentItem(), "fxStrength")->isVisible());
        QTRY_COMPARE(fx->property("currentIndex").toInt(), 4);
        editor.setClip("fx", "wobble");
        QCOMPARE(clip().fx, QString("film"));
        editor.clearError();
        editor.undo();
        QVERIFY(clip().fx.isEmpty());
        // Copy the look and paste it onto a title.
        editor.setClip("temperature", 0.5);
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "copyClip"), "clicked"));
        // Titles have no picture to grade.
        editor.addTitle();
        editor.select(editor.project().clips.last().id);
        QTRY_VERIFY(!section->isVisible());
        auto *pasteLook = findItem(window->contentItem(), "pasteLook");
        QTRY_VERIFY(pasteLook && pasteLook->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(pasteLook, "clicked"));
        QCOMPARE(editor.project().clips.back().temperature, 0.5);
        // A picture without sound has no sound section; titles neither.
        QVERIFY(!findItem(window->contentItem(), "soundSection")->isVisible());
        // Shapes from the library: the shape section replaces the text box for arrows.
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "addShape"), "clicked"));
        QObject *addArrow = nullptr;
        QTRY_VERIFY((addArrow = findItem(window->contentItem(), "addGraphic-arrow")));
        QVERIFY(QMetaObject::invokeMethod(addArrow, "triggered"));
        QCOMPARE(editor.project().clips.back().graphic, QString("arrow"));
        QTRY_VERIFY(findItem(window->contentItem(), "graphicSection")->isVisible());
        QTRY_VERIFY(!findItem(window->contentItem(), "titleText")->isVisible());
        auto *kind = findItem(window->contentItem(), "graphicKind");
        QVERIFY(QMetaObject::invokeMethod(kind, "activated", Q_ARG(int, 2)));
        QCOMPARE(editor.project().clips.back().graphic, QString("bubble"));
        QTRY_VERIFY(findItem(window->contentItem(), "titleText")->isVisible());
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void aiCutoutControls() {
        QTemporaryDir dir;
        const auto video = dir.filePath("speaker.mkv");
        QProcess ffmpeg;
        ffmpeg.start(Editor::executable("ffmpeg"),
                     {"-v", "error", "-f", "lavfi", "-i", "color=c=blue:s=160x90:r=30:d=2", "-vf",
                      "drawbox=x=0:y=0:w=80:h=90:c=red:t=fill", "-c:v", "ffv1", video});
        QVERIFY(ffmpeg.waitForFinished(30000) && ffmpeg.exitCode() == 0);
        // The AI pack with stand-in models.
        QDir().mkpath(dir.filePath("models"));
        QFile::copy(CUTLERY_SOURCE_DIR "/tests/fixtures/red-matte.onnx",
                    dir.filePath("models/u2net_human_seg.onnx"));
        QFile::copy(CUTLERY_SOURCE_DIR "/tests/fixtures/nearest-x2.onnx",
                    dir.filePath("models/realesr-general-x4v3.onnx"));
#ifdef CUTLERY_AI_WORKER
        qputenv("CUTLERY_AI_WORKER", CUTLERY_AI_WORKER);
#else
        qputenv("CUTLERY_AI_WORKER", dir.filePath("missing").toUtf8());
#endif
        qputenv("CUTLERY_AI_MODELS", dir.filePath("models").toUtf8());
        auto *frames = new FrameProvider;
        Editor editor(frames);
        qunsetenv("CUTLERY_AI_WORKER");
        qunsetenv("CUTLERY_AI_MODELS");
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(video)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        QCOMPARE(editor.project().clips.size(), 1);
        const auto id = editor.project().clips.first().id;
        editor.select(id);
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &e : errors)
                        warnings << e.toString();
                });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTest::qWait(100);
        auto *box = findItem(window->contentItem(), "aiCutout");
        QVERIFY(box && box->isVisible());
#ifdef CUTLERY_AI_WORKER
        QVERIFY(box->isEnabled());
        box->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(box, "toggled"));
        QVERIFY(editor.project().clips.first().aiCutout);
        // Turning the cutout on starts the analysis, which ends with the matte covering the clip.
        QTRY_VERIFY_WITH_TIMEOUT(
            editor.state()["selected"].toMap()["cutout"].toMap()["covered"].toBool(), 60000);
        auto *status = findItem(window->contentItem(), "cutoutStatus");
        QVERIFY(status);
        QTRY_COMPARE(status->property("text").toString(), QString("Speaker found ✓"));
        QVERIFY(!findItem(window->contentItem(), "cutoutAnalyze")->isVisible());
        // AI upscale of the 90p clip, to 360p: the same flow.
        auto *sharpen = findItem(window->contentItem(), "aiUpscale");
        QVERIFY(sharpen && sharpen->isVisible() && sharpen->isEnabled());
        sharpen->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(sharpen, "toggled"));
        QVERIFY(editor.project().clips.first().aiUpscale);
        QCOMPARE(editor.state()["selected"].toMap()["upscaleHeight"].toInt(), 360);
        QTRY_VERIFY_WITH_TIMEOUT(
            editor.state()["selected"].toMap()["upscale"].toMap()["covered"].toBool(), 60000);
        QTRY_COMPARE(findItem(window->contentItem(), "aiUpscaleStatus")->property("text").toString(),
                     QString("Sharper picture ready ✓"));
#else
        // Without the AI pack the option is off and explains why.
        QVERIFY(!box->isEnabled());
        QVERIFY(!editor.state()["aiMissing"].toMap()["matte"].toString().isEmpty());
        QVERIFY(!findItem(window->contentItem(), "aiUpscale")->isEnabled());
#endif
        // Automatic captions without speech recognition installed: explained, not startable.
        auto *captions = window->findChild<QObject *>("captionDialog");
        QVERIFY(captions);
        QVERIFY(QMetaObject::invokeMethod(captions, "open"));
        QTRY_VERIFY(window->findChild<QQuickItem *>("captionStart"));
        QVERIFY(!window->findChild<QQuickItem *>("captionStart")->isEnabled());
        QVERIFY(window->findChild<QQuickItem *>("captionStatus")
                    ->property("text")
                    .toString()
                    .contains("missing"));
        QVERIFY(QMetaObject::invokeMethod(captions, "close"));
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void pauseControls() {
        QTemporaryDir dir;
        const auto source = dir.filePath("talk.mkv");
        QProcess ffmpeg;
        ffmpeg.start(Editor::executable("ffmpeg"),
                     {"-v", "error", "-f", "lavfi", "-i", "color=c=gray:s=160x90:r=30:d=3", "-f",
                      "lavfi", "-i",
                      "sine=d=1[a];anullsrc=r=44100:cl=mono,atrim=duration=1[b];sine=d=1[c];"
                      "[a][b][c]concat=n=3:v=0:a=1",
                      "-c:v", "ffv1", "-c:a", "pcm_s16le", "-shortest", source});
        QVERIFY(ffmpeg.waitForFinished(30000) && ffmpeg.exitCode() == 0);
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        editor.select(editor.project().clips.first().id);
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &e : errors)
                        warnings << e.toString();
                });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTest::qWait(100);
        // Sound presets set every sound value in one undo step.
        auto *section = findItem(window->contentItem(), "soundSection");
        QTRY_VERIFY(section && section->isVisible());
        const auto selectedId = editor.project().clips.first().id;
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "soundPreset"), "activated",
                                          Q_ARG(int, 2))); // Clear voice
        QCOMPARE(editor.project().clip(selectedId)->lowCut, 80.);
        QCOMPARE(editor.project().clip(selectedId)->compressor, .5);
        editor.undo();
        QCOMPARE(editor.project().clip(selectedId)->lowCut, 0.);
        auto *open = findItem(window->contentItem(), "removePauses");
        QVERIFY(open && open->isVisible() && open->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(open, "clicked"));
        QTRY_VERIFY(window->findChild<QQuickItem *>("pauseFind"));
        auto *remove = window->findChild<QQuickItem *>("pauseRemove");
        QVERIFY(remove && !remove->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QQuickItem *>("pauseFind"), "clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(remove->isEnabled(), 15000);
        QVERIFY(window->findChild<QQuickItem *>("pauseStatus")
                    ->property("text")
                    .toString()
                    .startsWith("1 pause,"));
        QVERIFY(QMetaObject::invokeMethod(remove, "clicked"));
        QCOMPARE(editor.project().clips.size(), size_t(2));
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void editingAndPlayback() {
        QTemporaryDir dir;
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(160, 90, 30, 1);
        editor.addTitle();
        const auto clipId = editor.project().clips.first().id;
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &e : errors)
                        warnings << e.toString();
                });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->requestActivate();
        QTest::qWait(100);
        auto *timeline = findItem(window->contentItem(), "timelinePanel");
        QVERIFY(timeline);
        timeline->forceActiveFocus();
        auto *trim = findItem(window->contentItem(), "trimStart-" + clipId);
        QVERIFY(trim);
        const auto point =
            trim->mapToScene(QPointF(trim->width() / 2, trim->height() / 2)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, point);
        QTest::mouseMove(window, point + QPoint(48, 0), 50);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, point + QPoint(48, 0));
        QTRY_COMPARE(editor.project().clips[0].start, qint64(30));
        QCOMPARE(editor.project().clips[0].duration, qint64(60));
        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(editor.project().clips[0].start, qint64(0));
        editor.seek(30);
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QCOMPARE(editor.project().clips.size(), 2);
        editor.undo();
        timeline->forceActiveFocus();
        auto *title = findItem(window->contentItem(), "titleText");
        QVERIFY(title);
        title->forceActiveFocus();
        const auto oldText = title->property("text").toString();
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(!editor.state()["busy"].toBool());
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QCOMPARE(editor.project().clips.size(), 1);
        editor.seek(20);
        QVERIFY(title->property("text").toString() != oldText);
        timeline->forceActiveFocus();
        QVERIFY(keys.assign("split", "Ctrl+Shift+B"));
        QTest::qWait(50);
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QCOMPARE(editor.project().clips.size(), 1);
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(editor.project().clips.size(), 2);
        editor.undo();
        timeline->forceActiveFocus();
        editor.seek(0);
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(editor.playing());
        QTRY_VERIFY_WITH_TIMEOUT(editor.playbackFrame() > 0, 10000);
        QTest::keyClick(window, Qt::Key_K);
        QVERIFY(!editor.playing());
        const auto paused = editor.state()["playhead"].toLongLong();
        QVERIFY(paused > 0);
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(editor.playing());
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(!editor.playing());
        QVERIFY(editor.state()["playhead"].toLongLong() >= paused);
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
        QVERIFY(editor.save(QUrl::fromLocalFile(dir.filePath("ui.cutlery"))));
    }
};
QTEST_MAIN(UiTest)
#include "test_ui.moc"
