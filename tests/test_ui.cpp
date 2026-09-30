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
            library->setProperty(
                "contentY",
                std::max(0., std::min(i * 84., library->property("contentHeight").toDouble() -
                                                   library->height())));
            QTest::qWait(30);
            auto *tile = findItem(window->contentItem(), "asset-" + editor.project().assets[i].id);
            QVERIFY(tile);
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
