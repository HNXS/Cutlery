#include "Editor.h"
#include "KeyboardShortcuts.h"
#include "MediaAnalysis.h"
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
    // Visible controls (buttons, sliders, fields, lists) without a name for screen readers,
    // described by object name or class.
    static QStringList unnamedControls(QQuickItem *root) {
        QStringList missing;
        std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
            if (!item->isVisible() || item->opacity() == 0)
                return;
            const bool control = item->inherits("QQuickAbstractButton") || item->inherits("QQuickSlider") ||
                                 item->inherits("QQuickTextField") || item->inherits("QQuickComboBox") ||
                                 item->inherits("QQuickSpinBox") || item->inherits("QQuickTextArea");
            if (control && item->width() > 0 && item->height() > 0) {
                // What a screen reader announces: Accessible.name, or else a button's text or a
                // field's placeholder. Symbols alone (↺, ▾, ◇) do not count as a name.
                QString name;
                for (auto *child : item->children())
                    if (child->inherits("QQuickAccessibleAttached"))
                        name = child->property("name").toString();
                if (name.trimmed().isEmpty() && item->inherits("QQuickAbstractButton"))
                    name = item->property("text").toString();
                if (name.trimmed().isEmpty() && item->inherits("QQuickTextField"))
                    name = item->property("placeholderText").toString();
                if (!std::any_of(name.begin(), name.end(), [](QChar c) { return c.isLetterOrNumber(); }))
                    missing << (item->objectName().isEmpty() ? QString(item->metaObject()->className()) +
                                                                   "@" + QString::number(int(item->mapToScene({}).x())) +
                                                                   "," + QString::number(int(item->mapToScene({}).y()))
                                                             : item->objectName());
            }
            // The text field inside a list or number box belongs to it.
            if (item->inherits("QQuickComboBox") || item->inherits("QQuickSpinBox"))
                return;
            for (auto *child : item->childItems())
                visit(child);
        };
        visit(root);
        return missing;
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
    // Opens an inspector page: "video" means the picture's Basic page (its own tab for media,
    // Basic for titles and shapes) and "audio" the sound pages (tabs of their own for sound clips).
    static void page(QQuickWindow *window, const QString &tab, const QString &sub = {}) {
        const auto kind = window->property("selectionKind").toString();
        QString t = tab, s = sub;
        if (tab == "video" && kind != "media")
            t = sub.isEmpty() || sub == "basic" ? "basic" : tab, s = {};
        if (tab == "audio" && kind == "audio")
            t = sub.isEmpty() ? "basic" : sub, s = {};
        QVERIFY(QMetaObject::invokeMethod(window, "chooseInspector", Q_ARG(QVariant, t), Q_ARG(QVariant, s)));
        QCoreApplication::processEvents();
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
            return viewport->mapToScene(QPointF(double(frame) / 30 * 48, (2 - track) * 68 + 34))
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
        page(window, "animation");
        auto *type = findItem(window->contentItem(), "transitionType");
        QVERIFY(type && type->isVisible());
        QTRY_COMPARE(type->property("currentText").toString(), QString("Dissolve"));
        editor.setClip("transition", "wipeleft");
        QTRY_COMPARE(type->property("currentText").toString(), QString("Wipe left"));
        // The length in seconds, typed into the value row's box, is stored in frames.
        auto *length = findItem(window->contentItem(), "transitionDuration-box");
        QTRY_VERIFY(length && length->isVisible());
        QCOMPARE(length->property("text").toString(), QString("0.50 s"));
        length->setProperty("text", "1");
        QVERIFY(QMetaObject::invokeMethod(length, "editingFinished"));
        QCOMPARE(editor.project().clips.last().transitionFrames, qint64(30));
        editor.undo();
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
        // Project settings open on the current canvas and rate; a new rate keeps clip times.
        {
            auto *dialog = window->findChild<QObject *>("projectSettings");
            QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
            auto *canvas = window->findChild<QQuickItem *>("projectCanvas");
            auto *rate = window->findChild<QQuickItem *>("projectFrameRate");
            QTRY_VERIFY(canvas && rate && rate->isVisible());
            QCOMPARE(canvas->property("currentText").toString(), QString("160 × 90 · landscape"));
            QCOMPARE(rate->property("currentText").toString(), QString("30"));
            const auto before = editor.project().clips.first().duration;
            rate->setProperty("currentIndex", 7); // 60
            QVERIFY(QMetaObject::invokeMethod(dialog, "accept"));
            QTRY_COMPARE(editor.project().fpsN, 60);
            QCOMPARE(editor.project().clips.first().duration, before * 2);
            QCOMPARE(editor.project().width, 160);
            editor.undo();
            QCOMPARE(editor.project().fpsN, 30);
        }
        // A second open project shows tabs; a click switches, × closes one.
        {
            auto *tabs = findItem(window->contentItem(), "projectTabs");
            QVERIFY(tabs && !tabs->isVisible());
            editor.newProject();
            QTRY_VERIFY(tabs->isVisible());
            QVERIFY(editor.project().clips.empty());
            if (qEnvironmentVariableIsSet("CUTLERY_UI_SHOTS")) {
                QTest::qWait(50);
                window->grabWindow().save(qEnvironmentVariable("CUTLERY_UI_SHOTS") + "/tabs.png");
            }
            auto *first = findItem(window->contentItem(), "projectTab-0");
            QTRY_VERIFY(first);
            QVERIFY(QMetaObject::invokeMethod(first, "clicked"));
            QCOMPARE(editor.project().clips.first().id, id);
            QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "closeProjectTab-1"), "clicked"));
            QTRY_VERIFY(!tabs->isVisible());
            QCOMPARE(editor.project().clips.first().id, id);
        }
        // Edit → History lists the steps; a click goes back to one and forward again.
        {
            auto *dialog = window->findChild<QObject *>("historyDialog");
            QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
            auto *list = window->findChild<QQuickItem *>("historyList");
            QTRY_VERIFY(list && list->isVisible());
            const auto steps = editor.history();
            const int count = steps.size();
            int current = 0;
            while (steps[current].toMap()["offset"].toInt() != 0)
                ++current;
            QVERIFY(current >= 1);
            QTRY_COMPARE(list->property("count").toInt(), count);
            const auto before = editor.project().json();
            QQuickItem *back = nullptr;
            QTRY_VERIFY((back = findItem(list, "historyStep-" + QString::number(current - 1))));
            QVERIFY(QMetaObject::invokeMethod(back, "clicked"));
            QVERIFY(editor.state()["canRedo"].toBool());
            QVERIFY(editor.project().json() != before);
            if (qEnvironmentVariableIsSet("CUTLERY_UI_SHOTS")) {
                QTest::qWait(50);
                window->grabWindow().save(qEnvironmentVariable("CUTLERY_UI_SHOTS") + "/history.png");
            }
            QQuickItem *now = nullptr;
            QTRY_VERIFY((now = findItem(list, "historyStep-" + QString::number(current))));
            QVERIFY(QMetaObject::invokeMethod(now, "clicked"));
            QCOMPARE(editor.project().json(), before);
            QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
            QTRY_VERIFY(!list->isVisible());
            editor.select(id); // the step back went to before the title existed
        }
        // A title's pages, the top bar and the timeline: every control has a name.
        {
            QStringList missing;
            for (const auto &tab : {"text", "basic", "animation", "more"}) {
                page(window, tab);
                QTest::qWait(30);
                missing << unnamedControls(window->contentItem());
            }
            missing.removeDuplicates();
            QVERIFY2(missing.isEmpty(), qPrintable("Without a name: " + missing.join(", ")));
        }
        // Tab moves the keyboard focus from control to control; each one it reaches has a name.
        {
            window->requestActivate();
            QStringList reached;
            for (int i = 0; i < 12; ++i) {
                QTest::keyClick(window, Qt::Key_Tab);
                auto *focus = window->activeFocusItem();
                QVERIFY(focus);
                reached << (focus->objectName().isEmpty() ? QString(focus->metaObject()->className()) : focus->objectName());
            }
            reached.removeDuplicates();
            QVERIFY2(reached.size() >= 6, qPrintable(reached.join(", ")));
        }
        editor.seek(0);
        page(window, "video", "basic");
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
    void libraryFoldersAndSearch() {
        QTemporaryDir dir;
        QList<QUrl> files;
        for (const auto &name : {"Beach sunset.png", "Beach walk.png", "Logo.png"}) {
            QImage image(64, 36, QImage::Format_RGB32);
            image.fill(Qt::green);
            QVERIFY(image.save(dir.filePath(name)));
            files << QUrl::fromLocalFile(dir.filePath(name));
        }
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.importMedia(files);
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 3, 15000);
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
        auto *list = findItem(window->contentItem(), "mediaLibrary");
        QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(), 3);
        QVERIFY(!findItem(window->contentItem(), "missingMedia")->isVisible());
        // Search words narrow the list, in any order and case.
        auto *search = findItem(window->contentItem(), "librarySearch");
        search->setProperty("text", "beach");
        QTRY_COMPARE(list->property("count").toInt(), 2);
        search->setProperty("text", "WALK beach");
        QTRY_COMPARE(list->property("count").toInt(), 1);
        // Usage rights from the dialog; the rights are searchable too.
        QVariant walkAsset;
        for (const auto &a : editor.assets())
            if (a.toMap()["name"].toString().contains("walk", Qt::CaseInsensitive))
                walkAsset = a;
        const auto walk = walkAsset.toMap()["id"].toString();
        auto *rights = window->findChild<QObject *>("rightsDialog");
        QVERIFY(QMetaObject::invokeMethod(rights, "ask", Q_ARG(QVariant, walkAsset)));
        findItem(window->contentItem(), "rightsKind")->setProperty("currentIndex", 3);
        findItem(window->contentItem(), "rightsCredit")->setProperty("text", "Footage: Sam");
        QVERIFY(QMetaObject::invokeMethod(rights, "accept"));
        QCOMPARE(editor.project().asset(walk)->rights, QString("attribution"));
        QCOMPARE(editor.project().asset(walk)->credit, QString("Footage: Sam"));
        search->setProperty("text", "sam");
        QTRY_COMPARE(list->property("count").toInt(), 1);
        search->setProperty("text", "");
        // A new folder is shown at once and empty; media moved into it appears there.
        auto *dialog = window->findChild<QObject *>("folderDialog");
        QVERIFY(QMetaObject::invokeMethod(dialog, "ask", Q_ARG(QVariant, ""), Q_ARG(QVariant, ""),
                                          Q_ARG(QVariant, "")));
        findItem(window->contentItem(), "folderName")->setProperty("text", "Graphics");
        QVERIFY(QMetaObject::invokeMethod(dialog, "accept"));
        QCOMPARE(editor.project().folders, QStringList{"Graphics"});
        auto *view = findItem(window->contentItem(), "libraryView");
        QTRY_COMPARE(view->property("folder").toString(), QString("Graphics"));
        QTRY_COMPARE(list->property("count").toInt(), 0);
        QString logo;
        for (const auto &a : editor.project().assets)
            if (a.name == "Logo.png")
                logo = a.id;
        editor.moveToFolder({logo}, "Graphics");
        QTRY_COMPARE(list->property("count").toInt(), 1);
        QTRY_VERIFY(findItem(window->contentItem(), "asset-" + logo));
        // Back to all media; the kind filter shows images only.
        view->setProperty("currentIndex", 0);
        QTRY_COMPARE(list->property("count").toInt(), 3);
        QTRY_COMPARE(editor.state()["importFolder"].toString(), QString());
        view->setProperty("currentIndex", 2); // audio
        QTRY_COMPARE(list->property("count").toInt(), 0);
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void startScreenAndPreferences() {
        QTemporaryDir dir;
        auto *frames = new FrameProvider;
        Editor editor(frames);
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
        auto *start = findItem(window->contentItem(), "startScreen");
        QVERIFY(start && !start->isVisible());
        // Shown at launch (main.cpp sets this when no project is opened), with the templates.
        editor.addTitle();
        editor.saveTemplate("UI intro");
        editor.newProject();
        window->setProperty("startScreen", true);
        QTRY_VERIFY(start->isVisible());
        QTRY_VERIFY(findItem(window->contentItem(), "startTemplate-0"));
        if (qEnvironmentVariableIsSet("CUTLERY_UI_SHOTS"))
            window->grabWindow().save(qEnvironmentVariable("CUTLERY_UI_SHOTS") + "/start.png");
        {
            const auto missing = unnamedControls(start);
            QVERIFY2(missing.isEmpty(), qPrintable("Without a name: " + missing.join(", ")));
        }
        editor.removeTemplate("UI intro");
        QVERIFY(findItem(window->contentItem(), "startFormat-5"));
        // Choosing "Vertical 9:16" starts a project in that shape and closes the screen.
        auto *vertical = findItem(window->contentItem(), "startFormat-2");
        QVERIFY(vertical);
        QVERIFY(QMetaObject::invokeMethod(vertical, "clicked"));
        QCOMPARE(editor.project().width, 1080);
        QCOMPARE(editor.project().height, 1920);
        QTRY_VERIFY(!start->isVisible());
        QVERIFY(!window->property("startScreen").toBool());
        // The "New project" banner starts one in the shape chosen in the preferences.
        editor.newProject();
        window->setProperty("startScreen", true);
        QTRY_VERIFY(start->isVisible());
        auto *create = findItem(window->contentItem(), "startCreate");
        QTRY_VERIFY(create && create->isVisible() && create->width() > 100);
        QTest::mouseClick(window, Qt::LeftButton, {}, center(create));
        QTRY_VERIFY(!start->isVisible());
        // Preferences: the dialog shows and saves the app-wide settings.
        auto *dialog = window->findChild<QObject *>("preferencesDialog");
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        auto *format = findItem(window->contentItem(), "prefFormat");
        QTRY_VERIFY(format && format->isVisible());
        QCOMPARE(format->property("currentIndex").toInt(), 0); // Full HD
        format->setProperty("currentIndex", 4);                 // square
        findItem(window->contentItem(), "prefBackups")->setProperty("value", 5);
        QVERIFY(QMetaObject::invokeMethod(dialog, "accept"));
        auto prefs = editor.state()["preferences"].toMap();
        QCOMPARE(prefs["width"].toInt(), 1080);
        QCOMPARE(prefs["height"].toInt(), 1080);
        QCOMPARE(prefs["backups"].toInt(), 5);
        editor.setPreferences({{"width", 1920}, {"height", 1080}, {"backups", 20}});
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void nestedSequenceUi() {
        QTemporaryDir dir;
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(320, 180, 25, 1);
        editor.addTitle();
        const auto first = editor.state()["selectedId"].toString();
        editor.seek(200);
        editor.addTitle();
        editor.toggleSelect(first);
        editor.nestSelection();
        QCOMPARE(editor.project().clips.size(), size_t(1));
        const auto nested = editor.project().clips.first().id;
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
        auto *bar = findItem(window->contentItem(), "nestingBar");
        QVERIFY(bar && !bar->isVisible());
        // Double-clicking the nested clip opens it; the bar leads back.
        QQuickItem *clip = nullptr;
        QTRY_VERIFY((clip = findItem(window->contentItem(), "clip-" + nested)));
        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier, center(clip));
        QTRY_VERIFY(bar->isVisible());
        QCOMPARE(editor.project().clips.size(), size_t(2));
        auto *back = findItem(window->contentItem(), "closeNested");
        QVERIFY(QMetaObject::invokeMethod(back, "clicked"));
        QTRY_VERIFY(!bar->isVisible());
        QCOMPARE(editor.project().clips.size(), size_t(1));
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["nestedRendering"].toBool(), 30000);
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void editByText() {
        QTemporaryDir dir;
        const auto source = dir.filePath("talk.mkv");
        QProcess generate;
        generate.start(Editor::executable("ffmpeg"),
                       {"-v", "error", "-f", "lavfi", "-i", "color=c=gray:s=160x90:r=25:d=4", "-f",
                        "lavfi", "-i", "sine=d=4", "-c:v", "ffv1", "-c:a", "pcm_s16le",
                        "-shortest", source});
        QVERIFY(generate.waitForFinished(15000));
        QCOMPARE(generate.exitCode(), 0);
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(160, 90, 25, 1);
        editor.importMedia({QUrl::fromLocalFile(source)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        const auto asset = editor.project().assets.first();
        editor.addAsset(asset.id);
        // The cached transcript the speech recogniser would write (German).
        const auto key = QDir(editor.state()["dataPath"].toString() + "/ai")
                             .filePath(MediaAnalysis::fingerprint(asset) + "-transcribe-v2-de");
        QVERIFY(QDir().mkpath(QFileInfo(key).absolutePath()));
        {
            QFile srt(key + ".srt");
            QVERIFY(srt.open(QIODevice::WriteOnly));
            srt.write("1\n00:00:00,000 --> 00:00:00,800\nHallo\n\n"
                      "2\n00:00:01,000 --> 00:00:01,400\nähm\n\n"
                      "3\n00:00:02,000 --> 00:00:02,600\nalle\n\n"
                      "4\n00:00:03,000 --> 00:00:03,600\nzusammen\n");
            QFile meta(key + ".json");
            QVERIFY(meta.open(QIODevice::WriteOnly));
            meta.write(R"({"start": 0, "end": 4, "rate": 8})");
        }
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
        // Choose German and start: the transcript is there already, so the words show at once.
        QTRY_VERIFY(!window->property("selectionKind").toString().isEmpty());
        page(window, "more");
        auto *language = findItem(window->contentItem(), "wordLanguage");
        QTRY_VERIFY(language && language->isVisible());
        language->setProperty("currentIndex", 1);
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "transcribeClip"), "clicked"));
        QQuickItem *word = nullptr;
        QTRY_VERIFY((word = findItem(window->contentItem(), "word-2")));
        QTRY_VERIFY(findItem(window->contentItem(), "removeFillers")->property("text").toString().contains("1"));
        // Click "alle" and "zusammen", then cut them.
        QVERIFY(QMetaObject::invokeMethod(word, "clicked"));
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "word-3"), "clicked"));
        auto *cut = findItem(window->contentItem(), "cutWords");
        QTRY_VERIFY(cut->isEnabled());
        QVERIFY(cut->property("text").toString().contains("2 words"));
        QVERIFY(QMetaObject::invokeMethod(cut, "clicked"));
        QTRY_VERIFY(!findItem(window->contentItem(), "word-2"));
        qint64 total = 0;
        for (const auto &c : editor.project().clips)
            total += c.duration;
        // Cut from "alle" to 0.15 s after "zusammen" (3.75 s): 2 s of the 4 s clip and 0.24 s stay.
        QCOMPARE(total, qint64(56));
        // Double-clicking a word moves the playhead to it.
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "word-1"), "doubleClicked"));
        QCOMPARE(editor.state()["playhead"].toLongLong(), qint64(25));
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void curvesAndSelectiveColour() {
        QTemporaryDir dir;
        QImage image(64, 36, QImage::Format_RGB32);
        image.fill(QColor("#405070"));
        QVERIFY(image.save(dir.filePath("still.png")));
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(320, 180, 25, 1);
        editor.importMedia({QUrl::fromLocalFile(dir.filePath("still.png"))});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        editor.addAsset(editor.project().assets.first().id);
        const auto id = editor.state()["selectedId"].toString();
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
        QTRY_VERIFY(!window->property("selectionKind").toString().isEmpty());
        page(window, "adjust", "curves");
        auto *curve = findItem(window->contentItem(), "curveEditor");
        QTRY_VERIFY(curve && curve->width() > 50);
        // Scroll the inspector so the curve is on screen.
        for (auto *p = curve->parentItem(); p; p = p->parentItem())
            if (p->property("contentY").isValid() && p->property("contentHeight").toDouble() > p->height()) {
                const double top = curve->mapToItem(p->property("contentItem").value<QQuickItem *>(), QPointF(0, 0)).y();
                p->setProperty("contentY", std::max(0., top - 20));
                break;
            }
        QTest::qWait(50);
        // Click in the middle adds a point; dragging it up brightens the middle tones.
        const auto from = curve->mapToScene(QPointF(curve->width() / 2, curve->height() / 2)).toPoint(),
                   to = curve->mapToScene(QPointF(curve->width() / 2, curve->height() / 4)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(window, (from + to) / 2);
        QTest::mouseMove(window, to);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        QTRY_VERIFY(!editor.project().clip(id)->curveMaster.isEmpty());
        const auto points = editor.project().clip(id)->curveMaster.split(' ');
        QCOMPARE(points.size(), 3);
        QVERIFY2(points[1].split('/')[1].toDouble() > 0.7, qPrintable(points.join(' ')));
        // Another channel; the reset button clears it again.
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "curveChannel-curveBlue"), "clicked"));
        QTRY_COMPARE(curve->property("key").toString(), QString("curveBlue"));
        // Choosing blue for the selective colour change.
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "hslColour-b"), "clicked"));
        QTRY_COMPARE(editor.project().clip(id)->hslColors, QString("b"));
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "hslColour-c"), "clicked"));
        QTRY_COMPARE(editor.project().clip(id)->hslColors, QString("b c"));
        QVERIFY(findItem(window->contentItem(), "autoColour")->isEnabled());
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void textStylesAndAdjustment() {
        QTemporaryDir dir;
        auto *frames = new FrameProvider;
        Editor editor(frames);
        for (const auto &st : editor.textStyles())
            editor.removeTextStyle(st.toMap()["name"].toString());
        editor.addTitle();
        editor.setClipValues({{"fontSize", 110}, {"textColor", "#ff0000"}});
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
        // Save the title's look as a style.
        auto *save = findItem(window->contentItem(), "saveTextStyle");
        QTRY_VERIFY(save && save->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
        auto *name = findItem(window->contentItem(), "styleName");
        QTRY_VERIFY(name && name->isVisible());
        name->setProperty("text", "Red heading");
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject *>("styleDialog"), "accept"));
        QCOMPARE(editor.textStyles().size(), 1);
        // Apply it to a new title from the list.
        editor.addTitle();
        const auto second = editor.state()["selectedId"].toString();
        QCOMPARE(editor.project().clip(second)->fontSize, 72);
        auto *choice = findItem(window->contentItem(), "textStyle");
        QTRY_VERIFY(choice->isEnabled());
        choice->setProperty("currentIndex", 1);
        QVERIFY(QMetaObject::invokeMethod(choice, "activated", Q_ARG(int, 1)));
        QCOMPARE(editor.project().clip(second)->fontSize, 110);
        QCOMPARE(editor.project().clip(second)->textColor, QString("#ff0000"));
        QCOMPARE(choice->property("currentIndex").toInt(), 0);
        editor.removeTextStyle("Red heading");
        // An adjustment layer from the Add tab shows its own inspector section.
        window->setProperty("leftTab", "effects");
        auto *adjust = findItem(window->contentItem(), "addAdjustment");
        QTRY_VERIFY(adjust && adjust->isVisible());
        QVERIFY(QMetaObject::invokeMethod(adjust, "clicked"));
        QCOMPARE(editor.project().clips.last().effect, QString("adjust"));
        page(window, "basic");
        QTRY_VERIFY(findItem(window->contentItem(), "adjustStrength")->isVisible());
        page(window, "adjust", "basic");
        QVERIFY(findItem(window->contentItem(), "lookSection")->isVisible());
        // Layouts need two selected clips; "Full size" one.
        QVERIFY(!findItem(window->contentItem(), "arrange-side")->isEnabled());
        QVERIFY(findItem(window->contentItem(), "arrange-full")->isEnabled());
        editor.toggleSelect(second);
        QTRY_VERIFY(findItem(window->contentItem(), "arrange-side")->isEnabled());
        // The star keeps the selected title's font at the top of the font list.
        const auto adjustment = editor.project().clips.back().id;
        editor.select(second);
        for (const auto &f : editor.state()["fontFavorites"].toStringList())
            editor.toggleFontFavorite(f);
        auto *star = findItem(window->contentItem(), "favoriteFont");
        QTRY_VERIFY(star && star->isVisible());
        const auto family = editor.state()["selected"].toMap()["fontFamily"].toString();
        QVERIFY(QMetaObject::invokeMethod(star, "clicked"));
        QCOMPARE(editor.state()["fontFavorites"].toStringList(), QStringList{family});
        QTRY_COMPARE(star->property("text").toString(), QString("★"));
        editor.toggleFontFavorite(family);
        editor.select(adjustment);
        editor.toggleSelect(second);
        // Saving a layout through its dialog; it is offered in the layout list.
        for (const auto &l : editor.state()["layouts"].toList())
            editor.removeLayout(l.toMap()["name"].toString());
        auto *saveLayout = findItem(window->contentItem(), "saveLayout");
        QTRY_VERIFY(saveLayout && saveLayout->isEnabled());
        auto *layoutDialog = window->findChild<QObject *>("layoutDialog");
        QVERIFY(QMetaObject::invokeMethod(layoutDialog, "open"));
        findItem(window->contentItem(), "layoutName")->setProperty("text", "Pair");
        QVERIFY(QMetaObject::invokeMethod(layoutDialog, "accept"));
        QCOMPARE(editor.state()["layouts"].toList().size(), 1);
        auto *layoutChoice = findItem(window->contentItem(), "layoutChoice");
        QTRY_VERIFY(layoutChoice->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(layoutChoice, "activated", Q_ARG(int, 1)));
        QVERIFY(editor.state()["error"].toString().isEmpty());
        editor.removeLayout("Pair");
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void commandSearch() {
        QTemporaryDir dir;
        auto *frames = new FrameProvider;
        Editor editor(frames);
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
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *popup = window->findChild<QObject *>("commandSearch");
        QVERIFY(popup);
        // Ctrl+K opens it with every command listed.
        QTest::keyClick(window, Qt::Key_K, Qt::ControlModifier);
        QTRY_VERIFY(popup->property("visible").toBool());
        auto *field = findItem(window->contentItem(), "commandSearchField");
        QVERIFY(field);
        QTRY_VERIFY(field->hasActiveFocus());
        auto found = [&] { return popup->property("found").toList(); };
        QVERIFY(found().size() > 40);
        // Words in any order, from menus (with their place) and from keyboard commands.
        field->setProperty("text", "frame export");
        QVERIFY(!found().isEmpty());
        QCOMPARE(found()[0].toMap()["label"].toString(), QString("Export current frame as picture…"));
        QCOMPARE(found()[0].toMap()["where"].toString(), QString("Project"));
        field->setProperty("text", "shuttle forward");
        QCOMPARE(found().size(), 1);
        QCOMPARE(found()[0].toMap()["keys"].toString(), QString("L"));
        field->setProperty("text", "no such thing");
        QVERIFY(found().isEmpty());
        // Enter runs the first match and closes the search.
        field->setProperty("text", "add track");
        const int tracks = editor.project().tracks;
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(editor.project().tracks, tracks + 1);
        // A menu command runs as if chosen from the menu.
        QTest::keyClick(window, Qt::Key_K, Qt::ControlModifier);
        QTRY_VERIFY(popup->property("visible").toBool());
        field->setProperty("text", "preferences");
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(window->findChild<QObject *>("preferencesDialog") &&
                    window->findChild<QObject *>("preferencesDialog")->property("visible").toBool());
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
        choose("exportFormat", 9); // MP3: audio only, no resolution
        QCOMPARE(dialog->property("preview").toMap()["extension"].toString(), QString("mp3"));
        QTRY_VERIFY(!findItem(window->contentItem(), "exportHeight")->isEnabled());
        choose("exportFormat", 0);
        QTRY_VERIFY(findItem(window->contentItem(), "exportHeight")->isEnabled());
        // Frame rate and bitrate: part of the choice, and no longer a preset.
        choose("exportPreset", 1);
        choose("exportFps", 7); // 59.94
        choose("exportBitrate", 3); // 8 Mbit/s
        QCOMPARE(dialog->property("current").toMap()["fps"].toDouble(), 59.94);
        QCOMPARE(dialog->property("current").toMap()["bitrate"].toInt(), 8000);
        QCOMPARE(preset->property("currentIndex").toInt(), 0);
        QCOMPARE(findItem(window->contentItem(), "exportBitrate")->property("currentText").toString(),
                 QString("8 Mbit/s"));
        choose("exportFormat", 4); // ProRes takes no bitrate
        QTRY_VERIFY(!findItem(window->contentItem(), "exportBitrate")->isEnabled());
        // 10-bit and HDR only for formats that can hold them.
        QTRY_VERIFY(findItem(window->contentItem(), "exportDynamicRange")->isEnabled());
        choose("exportDynamicRange", 2);
        QCOMPARE(dialog->property("current").toMap()["dynamicRange"].toString(), QString("pq"));
        choose("exportFormat", 0);
        QTRY_VERIFY(!findItem(window->contentItem(), "exportDynamicRange")->isEnabled());
        QCOMPARE(dialog->property("current").toMap()["dynamicRange"].toString(), QString());
        choose("exportDynamicRange", 0);
        choose("exportFormat", 4);
        QVERIFY(findItem(window->contentItem(), "exportFps")->isEnabled());
        choose("exportFps", 0);
        choose("exportBitrate", 0);
        choose("exportFormat", 0);
        choose("exportSound", 2); // mono, 48 kHz
        QCOMPARE(dialog->property("current").toMap()["channels"].toInt(), 1);
        QCOMPARE(dialog->property("current").toMap()["sampleRate"].toInt(), 48000);
        choose("exportSound", 4); // 5.1 surround
        QCOMPARE(dialog->property("current").toMap()["channels"].toInt(), 6);
        choose("exportSound", 0);
        choose("exportCaptions", 1); // an SRT beside the video
        QCOMPARE(dialog->property("current").toMap()["captions"].toString(), QString("srt"));
        QCOMPARE(preset->property("currentIndex").toInt(), 0);
        choose("exportCaptions", 0);
        auto *result = findItem(window->contentItem(), "loudnessResult");
        QVERIFY(result && findItem(window->contentItem(), "measureLoudness"));
        QVERIFY(result->property("text").toString().contains("Integrated loudness"));
        QVERIFY(findItem(window->contentItem(), "levelMeter"));
        // Export now or add to the queue; queued jobs are listed with their state.
        QVERIFY(findItem(window->contentItem(), "exportNow")->isVisible());
        QVERIFY(findItem(window->contentItem(), "addToQueue")->isVisible());
        editor.queueExport(QUrl::fromLocalFile(dir.filePath("a.wav")), {{"format", "wav"}});
        editor.queueExport(QUrl::fromLocalFile(dir.filePath("b.wav")), {{"format", "wav"}});
        QTRY_VERIFY(findItem(window->contentItem(), "queued-1"));
        auto *removeSecond = findItem(window->contentItem(), "removeQueued-1");
        QVERIFY(removeSecond && removeSecond->isEnabled());
        QVERIFY(!findItem(window->contentItem(), "removeQueued-0")->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(removeSecond, "clicked"));
        QTRY_VERIFY(!findItem(window->contentItem(), "queued-1"));
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["busy"].toBool(), 60000);
        // Reframe for…: the shorter side stays, the canvas takes the new shape.
        QVERIFY(window->findChild<QObject *>("reframe-9x16"));
        QVERIFY(QMetaObject::invokeMethod(window, "reframeTo", Q_ARG(QVariant, 9), Q_ARG(QVariant, 16)));
        QCOMPARE(editor.project().width, 1080);
        QCOMPARE(editor.project().height, 1920);
        QVERIFY(QMetaObject::invokeMethod(window, "reframeTo", Q_ARG(QVariant, 4), Q_ARG(QVariant, 5)));
        QCOMPARE(editor.project().height, 1350);
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
        page(window, "video", "basic");
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
        auto *key = findItem(window->contentItem(), "chromaKey-on");
        QVERIFY(key);
        key->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(key, "toggled"));
        QVERIFY(clip().chromaKey);
        // "Pick" turns the viewer into a colour picker until the next click.
        auto *picker = findItem(window->contentItem(), "keyPicker");
        QVERIFY(picker && !picker->isVisible());
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "pickKeyColor"), "clicked"));
        QTRY_VERIFY(picker->isVisible());
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "pickKeyColor"), "clicked"));
        QTRY_VERIFY(!picker->isVisible());
        // Blend mode and luma key: chosen from their lists; tolerance sliders appear for the key.
        auto *blend = findItem(window->contentItem(), "blendMode");
        QVERIFY(blend && blend->isVisible());
        QCOMPARE(blend->property("currentIndex").toInt(), 0);
        blend->setProperty("currentIndex", 1);
        QVERIFY(QMetaObject::invokeMethod(blend, "activated", Q_ARG(int, 1)));
        QCOMPARE(clip().blendMode, QString("multiply"));
        QVERIFY(!findItem(window->contentItem(), "lumaTolerance"));
        auto *luma = findItem(window->contentItem(), "lumaKey");
        QVERIFY(luma);
        luma->setProperty("currentIndex", 2);
        QVERIFY(QMetaObject::invokeMethod(luma, "activated", Q_ARG(int, 2)));
        QCOMPARE(clip().lumaKey, QString("light"));
        QTRY_VERIFY(findItem(window->contentItem(), "lumaTolerance"));
        auto *upsideDown = findItem(window->contentItem(), "flipVertical");
        QVERIFY(upsideDown);
        upsideDown->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(upsideDown, "toggled"));
        QVERIFY(clip().flipVertical);
        press("place-full");
        QCOMPARE(clip().scale, 1.);
        QCOMPARE(clip().x, 0.);
        // A mosaic area: added from the library panel, configured in its own section.
        window->setProperty("leftTab", "effects");
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
        auto *areaShape = findItem(window->contentItem(), "effectShape");
        QVERIFY(areaShape && areaShape->isVisible());
        areaShape->setProperty("currentIndex", 1);
        QVERIFY(QMetaObject::invokeMethod(areaShape, "activated", Q_ARG(int, 1)));
        QCOMPARE(editor.project().clip(area.id)->effectShape, QString("ellipse"));
        QVERIFY(findItem(window->contentItem(), "effectFeather")->isVisible());
        // Brand kit in the Add tab: colours appear as swatches; the logo needs choosing first.
        for (const auto &c : editor.state()["brandColors"].toStringList())
            editor.removeBrandColor(c);
        editor.setBrandLogo({});
        window->setProperty("leftTab", "stickers");
        auto *brandField = findItem(window->contentItem(), "brandColorField");
        QVERIFY(brandField && brandField->isVisible());
        brandField->setProperty("text", "#123456");
        press("addBrandColor");
        QCOMPARE(editor.state()["brandColors"].toStringList(), QStringList{"#123456"});
        QTRY_VERIFY(findItem(window->contentItem(), "brandColor-#123456"));
        QVERIFY(!findItem(window->contentItem(), "addBrandLogo")->property("enabled").toBool());
        editor.removeBrandColor("#123456");
        QTRY_VERIFY(!findItem(window->contentItem(), "brandColor-#123456"));
        // A lower third from the library panel; its style can be changed in the inspector.
        window->setProperty("leftTab", "text");
        press("addLowerThird");
        const auto lower = editor.project().clips.last();
        QCOMPARE(lower.titleStyle, QString("lowerThird"));
        page(window, "text");
        auto *style = findItem(window->contentItem(), "titleStyle");
        QTRY_VERIFY(style && style->isVisible());
        QCOMPARE(style->property("currentIndex").toInt(), 1);
        if (qEnvironmentVariableIsSet("CUTLERY_UI_SHOTS"))
            window->grabWindow().save(qEnvironmentVariable("CUTLERY_UI_SHOTS") + "/text.png");
        style->setProperty("currentIndex", 4);
        QVERIFY(QMetaObject::invokeMethod(style, "activated", Q_ARG(int, 4)));
        QCOMPARE(editor.project().clip(lower.id)->titleStyle, QString("titleCard"));
        style->setProperty("currentIndex", 6);
        QVERIFY(QMetaObject::invokeMethod(style, "activated", Q_ARG(int, 6)));
        QCOMPARE(editor.project().clip(lower.id)->titleStyle, QString("quote"));
        press("addBanner");
        QCOMPARE(editor.project().clips.last().titleStyle, QString("banner"));
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
    void slipSlideAndRollDrags() {
        QTemporaryDir dir;
        const auto video = dir.filePath("clip.mkv");
        QProcess generate;
        generate.start(Editor::executable("ffmpeg"),
                       {"-v", "error", "-f", "lavfi", "-i", "color=gray:size=160x90:rate=30:duration=4",
                        "-c:v", "ffv1", video});
        QVERIFY(generate.waitForFinished(15000));
        auto *frames = new FrameProvider;
        Editor editor(frames);
        editor.configure(160, 90, 30, 1);
        editor.importMedia({QUrl::fromLocalFile(video)});
        QTRY_VERIFY_WITH_TIMEOUT(editor.project().assets.size() == 1, 15000);
        // Three touching 1 s clips, each starting 1 s into the source.
        const auto asset = editor.project().assets.first().id;
        for (int i = 0; i < 3; ++i) {
            editor.addAsset(asset);
            editor.select(editor.project().clips.back().id);
            editor.setClip("duration", 30);
            editor.setClip("sourceIn", 1.0);
            editor.setClip("start", 30 * i);
        }
        const auto a = editor.project().clips[0].id, b = editor.project().clips[1].id;
        KeyboardShortcuts keys(dir.filePath("keys.json"));
        QQmlApplicationEngine engine;
        engine.addImageProvider("frames", frames);
        engine.rootContext()->setContextProperty("editor", &editor);
        engine.rootContext()->setContextProperty("shortcutSettings", &keys);
        QStringList warnings;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&](const QList<QQmlError> &errors) {
            for (const auto &e : errors)
                warnings << e.toString();
        });
        engine.load(QUrl::fromLocalFile(QString::fromUtf8(CUTLERY_SOURCE_DIR) + "/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->requestActivate();
        QTest::qWait(150);
        // Pressed in the middle: the transition button sits on the top edge of the cut.
        auto altDrag = [&](QQuickItem *item, int dx, Qt::KeyboardModifiers modifiers) {
            const auto from = center(item), to = from + QPoint(dx, 0);
            QTest::mousePress(window, Qt::LeftButton, modifiers, from);
            QTest::mouseMove(window, from + QPoint(dx / 4, 0), 20);
            QTest::mouseMove(window, from + QPoint(dx / 2, 0), 20);
            QTest::mouseMove(window, to, 20);
            QTest::mouseRelease(window, Qt::LeftButton, modifiers, to);
            QTest::qWait(30);
        };
        const double pixelsPerFrame = 48. / 30;
        auto *clipB = findItem(window->contentItem(), "clip-" + b);
        QVERIFY(clipB);
        // Alt+drag on B to the left by 10 frames: B shows 10 frames later in its source.
        altDrag(clipB, -qRound(10 * pixelsPerFrame), Qt::AltModifier);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QTRY_VERIFY(std::abs(editor.project().clip(b)->sourceIn.seconds() - (1 + 10 / 30.)) < 0.05);
        QCOMPARE(editor.project().clip(b)->start, 30);
        // Alt+Shift+drag: B slides 6 frames right; A grows to meet it. (Clip items are made
        // again after each edit.)
        clipB = findItem(window->contentItem(), "clip-" + b);
        QVERIFY(clipB);
        altDrag(clipB, qRound(6 * pixelsPerFrame), Qt::AltModifier | Qt::ShiftModifier);
        QTRY_VERIFY(std::abs(editor.project().clip(b)->start - 36) <= 1);
        QCOMPARE(editor.project().clip(a)->duration, editor.project().clip(b)->start);
        // Alt+drag A's end edge: the A|B cut rolls 4 frames earlier.
        const auto cut = editor.project().clip(b)->start;
        auto *endA = findItem(window->contentItem(), "trimEnd-" + a);
        QVERIFY(endA);
        altDrag(endA, -qRound(4 * pixelsPerFrame), Qt::AltModifier);
        QTRY_VERIFY(std::abs(editor.project().clip(b)->start - (cut - 4)) <= 1);
        QCOMPARE(editor.project().clip(a)->duration, editor.project().clip(b)->start);
        QCOMPARE(editor.project().duration(), 90);
        // Ctrl+click adds C to the selection; dragging A then moves both.
        const auto c = editor.project().clips[2].id;
        editor.select(a);
        auto *clipC = findItem(window->contentItem(), "clip-" + c);
        QVERIFY(clipC);
        QTest::mouseClick(window, Qt::LeftButton, Qt::ControlModifier, center(clipC));
        QTRY_COMPARE(editor.selection().size(), 2);
        const auto startA = editor.project().clip(a)->start, startC = editor.project().clip(c)->start;
        auto *clipA = findItem(window->contentItem(), "clip-" + a);
        QVERIFY(clipA);
        altDrag(clipA, qRound(10 * pixelsPerFrame), Qt::NoModifier);
        QTRY_VERIFY(editor.project().clip(a)->start > startA);
        QCOMPARE(editor.project().clip(c)->start - startC, editor.project().clip(a)->start - startA);
        // A rectangle drawn from the empty track above into track 0 selects the clips it touches.
        editor.select(QString());
        clipA = findItem(window->contentItem(), "clip-" + a);
        auto *clipB2 = findItem(window->contentItem(), "clip-" + b);
        QVERIFY(clipA && clipB2);
        const auto from = clipA->mapToScene(QPointF(10, -40)).toPoint(),
                   to = center(clipB2);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(window, from + QPoint(10, 10), 20);
        QTest::mouseMove(window, (from + to) / 2, 20);
        QTest::mouseMove(window, to, 20);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        QTRY_VERIFY(editor.selection().contains(a) && editor.selection().contains(b));
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
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
        QTRY_VERIFY(!window->property("selectionKind").toString().isEmpty());
        page(window, "adjust", "basic");
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
        page(window, "adjust", "lut");
        QTRY_VERIFY(findItem(window->contentItem(), "lutStrength")->isVisible());
        editor.setClip("lut", dir.filePath("keys.json"));
        QVERIFY(editor.state()["error"].toString().contains(".cube"));
        QCOMPARE(clip().lut, QDir::cleanPath(lut));
        editor.clearError();
        editor.setClip("lut", "");
        QVERIFY(clip().lut.isEmpty());
        // Scopes over the viewer, for the preview still.
        auto *scopes = findItem(window->contentItem(), "scopes");
        QVERIFY(scopes && !scopes->isVisible());
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "toggleScopes"), "clicked"));
        QTRY_VERIFY(scopes->isVisible());
        auto *scopeImage = findItem(window->contentItem(), "scopeImage");
        QTRY_COMPARE(scopeImage->property("implicitWidth").toInt(), 256);
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "scopeKind"), "activated", Q_ARG(int, 2)));
        findItem(window->contentItem(), "scopeKind")->setProperty("currentIndex", 2);
        QTRY_COMPARE(scopeImage->property("implicitWidth").toInt(), 192);
        if (qEnvironmentVariableIsSet("CUTLERY_SCOPE_SHOT"))
            window->grabWindow().save(qEnvironmentVariable("CUTLERY_SCOPE_SHOT"));
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "toggleScopes"), "clicked"));
        QTRY_VERIFY(!scopes->isVisible());
        // Sound effects: the library lists Cutlery's sounds with their licence; Add puts one at
        // the playhead.
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "openSounds"), "clicked"));
        QObject *addClick = nullptr;
        // The list's rows are items under the popup overlay.
        auto *soundList = window->findChild<QQuickItem *>("soundList");
        QTRY_VERIFY(soundList && soundList->property("count").toInt() >= 7);
        QTRY_VERIFY((addClick = findItem(soundList, "addSound-click")));
        QVERIFY(findItem(soundList, "listen-click"));
        const auto before = editor.project().clips.size();
        QVERIFY(QMetaObject::invokeMethod(addClick, "clicked"));
        QCOMPARE(editor.project().clips.size(), before + 1);
        QCOMPARE(editor.project().clips.back().name, QString("Mouse click"));
        QTRY_VERIFY(!window->findChild<QQuickItem *>("soundDialog") ||
                    !window->findChild<QQuickItem *>("soundDialog")->isVisible());
        editor.undo();
        editor.select(id);
        // Style effects: a picture has the effect choice, but no motion blur or stabilizing.
        page(window, "effects");
        QVERIFY(findItem(window->contentItem(), "effectsSection")->isVisible());
        QVERIFY(!findItem(window->contentItem(), "stabilize")->isVisible());
        QVERIFY(!findItem(window->contentItem(), "videoDenoise")->isVisible());
        QVERIFY(!findItem(window->contentItem(), "colorRange")->isVisible());
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
        QVERIFY(QMetaObject::invokeMethod(fx, "activated", Q_ARG(int, 8)));
        QCOMPARE(clip().fx, QString("mirror"));
        editor.undo();
        // Perspective: switching it on pins the corners where they are; sliders move them.
        page(window, "video", "basic");
        auto *pin = findItem(window->contentItem(), "cornerPin-on");
        QTRY_VERIFY(pin && pin->isVisible());
        pin->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(pin, "toggled"));
        QCOMPARE(clip().cornerPin, (QVector<double>{0, 0, 1, 0, 0, 1, 1, 1}));
        auto *corner = findItem(window->contentItem(), "cornerPin-2");
        QTRY_VERIFY(corner && corner->isVisible());
        corner->setProperty("value", 0.8);
        QVERIFY(QMetaObject::invokeMethod(corner, "commit"));
        QCOMPARE(clip().cornerPin[2], 0.8);
        editor.undo();
        editor.undo();
        QVERIFY(clip().cornerPin.isEmpty());
        // Colour wheels: set from the wheel, reset by double-click.
        page(window, "adjust", "wheels");
        auto *wheel = findItem(window->contentItem(), "wheel-gain");
        QTRY_VERIFY(wheel && wheel->isVisible());
        QVERIFY(QMetaObject::invokeMethod(wheel, "set", Q_ARG(QVariant, 2.0), Q_ARG(QVariant, 0.0)));
        QCOMPARE(clip().gainX, 1.); // kept inside the wheel
        QCOMPARE(clip().gainY, 0.);
        editor.undo();
        QCOMPARE(clip().gainX, 0.);
        page(window, "video", "basic");
        auto *tilt = findItem(window->contentItem(), "tilt-tiltY");
        QTRY_VERIFY(tilt && tilt->isVisible());
        tilt->setProperty("value", 30);
        QVERIFY(QMetaObject::invokeMethod(tilt, "moved"));
        QCOMPARE(clip().tiltY, 30.);
        editor.undo();
        // Canvas fill for pictures that do not cover the frame.
        page(window, "video", "canvas");
        auto *fill = findItem(window->contentItem(), "canvasFill");
        QTRY_VERIFY(fill && fill->isVisible());
        QVERIFY(QMetaObject::invokeMethod(fill, "activated", Q_ARG(int, 1)));
        QCOMPARE(clip().canvasFill, QString("blur"));
        QTRY_COMPARE(fill->property("currentIndex").toInt(), 1);
        editor.setClip("canvasFill", "#12ab34");
        QTRY_COMPARE(fill->property("currentText").toString(), QString("#12ab34"));
        editor.undo();
        editor.undo();
        QVERIFY(clip().canvasFill.isEmpty());
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
        // Tabs for a video without sound; the sub-tab chosen is kept when switching back.
        QStringList tabs;
        for (const auto &t : window->property("inspectorTabs").toList())
            tabs << t.toMap()["id"].toString();
        QCOMPARE(tabs, (QStringList{"video", "speed", "animation", "adjust", "effects", "more"}));
        QVERIFY(!findItem(window->contentItem(), "projectDetails")->isVisible());
        auto *speedTab = findItem(window->contentItem(), "inspectorTab-speed");
        QVERIFY(speedTab);
        QVERIFY(QMetaObject::invokeMethod(speedTab, "clicked"));
        QCOMPARE(window->property("inspectorTab").toString(), QString("speed"));
        QTRY_VERIFY(findItem(window->contentItem(), "freezeFrame")->isVisible());
        page(window, "video", "mask");
        page(window, "speed");
        page(window, "video");
        QCOMPARE(window->property("inspectorSub").toString(), QString("mask"));
        // Nothing selected: the project's details.
        editor.select("");
        QTRY_VERIFY(findItem(window->contentItem(), "projectDetails")->isVisible());
        QCOMPARE(window->property("inspectorTabs").toList().size(), 0);
        editor.select(id);
        QTRY_VERIFY(!findItem(window->contentItem(), "projectDetails")->isVisible());
        // Values: typed into the number box (in percent), stepped, and reset with the section.
        page(window, "video", "basic");
        auto *scaleBox = findItem(window->contentItem(), "prop-scale-box");
        QVERIFY(scaleBox && scaleBox->isVisible());
        scaleBox->setProperty("text", "150");
        QVERIFY(QMetaObject::invokeMethod(scaleBox, "editingFinished"));
        QCOMPARE(editor.project().clips.first().scale, 1.5);
        QTRY_COMPARE(scaleBox->property("text").toString(), QString("150%"));
        auto *reset = findItem(window->contentItem(), "transformSection-reset");
        QVERIFY(reset && reset->isVisible());
        QVERIFY(QMetaObject::invokeMethod(reset, "clicked"));
        QCOMPARE(editor.project().clips.first().scale, 1.);
        editor.undo();
        QCOMPARE(editor.project().clips.first().scale, 1.5);
        editor.undo();
        QCOMPARE(editor.project().clips.first().scale, 1.);
        // A section with an on/off box: stabilizing.
        page(window, "video", "enhance");
        auto *stabilizeOn = findItem(window->contentItem(), "stabilize-on");
        QVERIFY(stabilizeOn && stabilizeOn->isVisible());
        stabilizeOn->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(stabilizeOn, "toggled"));
        QVERIFY(editor.project().clips.first().stabilize);
        QTRY_VERIFY(findItem(window->contentItem(), "stabilizeStrength")->isEnabled());
        editor.undo();
        QVERIFY(!editor.project().clips.first().stabilize);
        // The asset panel's tabs: a style effect and a look as tiles for the selected clip; a
        // transition only when the clip follows another.
        auto *effectsTab = findItem(window->contentItem(), "leftTab-effects");
        QVERIFY(effectsTab);
        QVERIFY(QMetaObject::invokeMethod(effectsTab, "clicked"));
        QCOMPARE(window->property("leftTab").toString(), QString("effects"));
        auto *glitch = findItem(window->contentItem(), "fxTile-glitch");
        QTRY_VERIFY(glitch && glitch->isVisible() && glitch->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(glitch, "clicked"));
        QCOMPARE(editor.project().clips.first().fx, QString("glitch"));
        QTRY_VERIFY(glitch->property("checked").toBool());
        editor.undo();
        window->setProperty("leftTab", "filters");
        auto *warm = findItem(window->contentItem(), "lookTile-1");
        QTRY_VERIFY(warm && warm->isVisible() && warm->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(warm, "clicked"));
        QCOMPARE(editor.project().clips.first().temperature, .35);
        editor.undo();
        QCOMPARE(editor.project().clips.first().temperature, 0.);
        window->setProperty("leftTab", "transitions");
        auto *none = findItem(window->contentItem(), "transitionTile-none");
        QTRY_VERIFY(none && none->isVisible());
        QVERIFY(!none->isEnabled());
        // Audio tab: sound effects as tiles; a click adds one at the playhead.
        window->setProperty("leftTab", "audio");
        auto *clickTile = findItem(window->contentItem(), "soundTile-click");
        QTRY_VERIFY(clickTile && clickTile->isVisible());
        const auto clipsBefore = editor.project().clips.size();
        QVERIFY(QMetaObject::invokeMethod(clickTile, "clicked"));
        QCOMPARE(editor.project().clips.size(), clipsBefore + 1);
        QCOMPARE(editor.project().clips.back().name, QString("Mouse click"));
        editor.undo();
        editor.select(editor.project().clips.first().id);
        // The category column scrolls the tab to a category, and back to the top.
        window->setProperty("leftTab", "audio");
        auto *column = findItem(window->contentItem(), "categoryColumn");
        QTRY_VERIFY(column && column->isVisible());
        auto *scroll = findItem(window->contentItem(), "addTabScroll");
        auto *flick = qvariant_cast<QQuickItem *>(scroll->property("contentItem"));
        QVERIFY(flick);
        auto *typing = findItem(window->contentItem(), "cat-sound-Transitions");
        QVERIFY(typing);
        QTRY_VERIFY(typing->mapToItem(flick, QPointF()).y() > flick->height());
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "category-cat-sound-Transitions"), "clicked"));
        QTRY_VERIFY(flick->property("contentY").toDouble() > 0);
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "category-top"), "clicked"));
        QTRY_COMPARE(flick->property("contentY").toDouble(), 0.);
        window->setProperty("leftTab", "text");
        QTRY_VERIFY(!column->isVisible());
        // Media used on the timeline is marked "Added".
        window->setProperty("leftTab", "media");
        auto *added = findItem(window->contentItem(), "added-" + editor.project().clips.first().assetId);
        QTRY_VERIFY(added && added->isVisible());
        // Editing proxies: offered for videos; this one is too small to need one, and the player
        // shows the proxy switch only once a proxy exists.
        const auto proxyAsset = editor.project().clips.first().assetId;
        auto *assetMenu = findItem(window->contentItem(), "asset-" + proxyAsset)->findChild<QObject *>("assetMenu-" + proxyAsset);
        QVERIFY(assetMenu);
        QObject *makeProxy = nullptr;
        for (int i = 0; i < assetMenu->property("count").toInt() && !makeProxy; ++i) {
            QQuickItem *item = nullptr;
            QMetaObject::invokeMethod(assetMenu, "itemAt", Q_RETURN_ARG(QQuickItem *, item), Q_ARG(int, i));
            if (item && item->objectName() == "makeProxy")
                makeProxy = item;
        }
        QVERIFY(makeProxy && makeProxy->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(makeProxy, "triggered"));
        QVERIFY(editor.state()["error"].toString().contains("small enough"));
        editor.clearError();
        QVERIFY(!findItem(window->contentItem(), "toggleProxies")->isVisible());
        // Every visible control has a name for screen readers, on every left tab and inspector
        // page of a selected video.
        {
            QStringList missing;
            for (const auto &tab : {"media", "audio", "text", "stickers", "effects", "transitions", "filters", "layouts"}) {
                window->setProperty("leftTab", tab);
                QTest::qWait(30);
                missing << unnamedControls(window->contentItem());
            }
            window->setProperty("leftTab", "media");
            for (const auto &[tab, sub] : std::initializer_list<std::pair<const char *, const char *>>{
                     {"video", "basic"}, {"video", "cutout"}, {"video", "mask"}, {"video", "canvas"},
                     {"video", "enhance"}, {"speed", ""}, {"animation", ""}, {"adjust", "basic"},
                     {"adjust", "hsl"}, {"adjust", "curves"}, {"adjust", "wheels"}, {"adjust", "lut"},
                     {"effects", ""}, {"more", ""}}) {
                page(window, tab, sub);
                QTest::qWait(30);
                missing << unnamedControls(window->contentItem());
            }
            missing.removeDuplicates();
            QVERIFY2(missing.isEmpty(), qPrintable("Without a name: " + missing.join(", ")));
        }
        if (qEnvironmentVariableIsSet("CUTLERY_UI_SHOTS"))
            for (const auto &tab : {"media", "audio", "text", "stickers", "effects", "transitions", "filters", "layouts"}) {
                window->setProperty("leftTab", tab);
                QTest::qWait(50);
                window->grabWindow().save(qEnvironmentVariable("CUTLERY_UI_SHOTS") + "/left-" + tab + ".png");
            }
        window->setProperty("leftTab", "media");
        // Tools above the timeline: split and mirror the selected clip, each one undo step.
        editor.seek(20);
        auto *split = findItem(window->contentItem(), "toolSplit");
        QTRY_VERIFY(split && split->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(split, "clicked"));
        QCOMPARE(editor.project().clips.size(), 2);
        editor.undo();
        QCOMPARE(editor.project().clips.size(), 1);
        editor.select(id);
        auto *trimEnd = findItem(window->contentItem(), "toolTrimEnd");
        QTRY_VERIFY(trimEnd && trimEnd->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(trimEnd, "clicked"));
        QCOMPARE(editor.project().clips.first().duration, qint64(20));
        editor.undo();
        auto *mirror = findItem(window->contentItem(), "toolMirror");
        QTRY_VERIFY(mirror && mirror->isEnabled());
        mirror->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(mirror, "clicked"));
        QVERIFY(editor.project().clips.first().flip);
        editor.undo();
        QVERIFY(!editor.project().clips.first().flip);
        if (qEnvironmentVariableIsSet("CUTLERY_UI_SHOTS"))
            window->grabWindow().save(qEnvironmentVariable("CUTLERY_UI_SHOTS") + "/window.png");
        // Pictures of every inspector page, for looking at the layout.
        if (qEnvironmentVariableIsSet("CUTLERY_UI_SHOTS")) {
            for (const auto &[tab, sub] : std::initializer_list<std::pair<const char *, const char *>>{
                     {"video", "basic"}, {"video", "cutout"}, {"video", "mask"}, {"video", "canvas"},
                     {"video", "enhance"}, {"speed", ""}, {"animation", ""}, {"adjust", "basic"},
                     {"adjust", "wheels"}, {"effects", ""}, {"more", ""}}) {
                page(window, tab, sub);
                QTest::qWait(50);
                window->grabWindow().save(qEnvironmentVariable("CUTLERY_UI_SHOTS") + "/" + tab + "-" + sub + ".png");
            }
        }
        // Video noise, flicker and source colours for a video; the HDR choice only for HDR.
        page(window, "video", "enhance");
        for (const char *name : {"videoDenoise", "deflicker", "colorRange", "colorMatrix", "lensCorrection"})
            QVERIFY2(findItem(window->contentItem(), name) &&
                         findItem(window->contentItem(), name)->isVisible(),
                     name);
        QVERIFY(!findItem(window->contentItem(), "toneMap")->isVisible());
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "colorRange"), "activated",
                                          Q_ARG(int, 2)));
        QCOMPARE(editor.project().clips.first().colorRange, QString("pc"));
        QTRY_COMPARE(findItem(window->contentItem(), "colorRange")->property("currentIndex").toInt(), 2);
        editor.undo();
        QVERIFY(editor.project().clips.first().colorRange.isEmpty());
        // Lens distortion: typed into its box, one undo step.
        auto *lens = findItem(window->contentItem(), "lensCorrection-box");
        QVERIFY(lens);
        lens->setProperty("text", "40");
        QVERIFY(QMetaObject::invokeMethod(lens, "editingFinished"));
        QCOMPARE(editor.project().clips.first().lensCorrection, 0.4);
        editor.undo();
        QCOMPARE(editor.project().clips.first().lensCorrection, 0.);
        // Free mask: "Draw mask" lets clicks on the preview add points.
        page(window, "video", "mask");
        auto *drawMask = findItem(window->contentItem(), "drawMask");
        QVERIFY(drawMask && drawMask->isVisible());
        QVERIFY(QMetaObject::invokeMethod(drawMask, "clicked"));
        auto *drawer = findItem(window->contentItem(), "maskDrawer");
        QTRY_VERIFY(drawer && drawer->isVisible());
        for (const auto &at : {QPointF(0.2, 0.2), QPointF(0.8, 0.2), QPointF(0.5, 0.8)})
            QTest::mouseClick(window, Qt::LeftButton, {},
                              drawer->mapToScene(QPointF(at.x() * drawer->width(), at.y() * drawer->height()))
                                  .toPoint());
        QTRY_COMPARE(maskPoints(editor.project().clips.first().mask).size(), 3);
        QTRY_VERIFY(findItem(window->contentItem(), "maskSmooth")->isVisible());
        QVERIFY(drawMask->property("text").toString().contains("3 points"));
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "clearMask"), "clicked"));
        QVERIFY(editor.project().clips.first().mask.isEmpty());
        QTRY_VERIFY(!drawer->isVisible());
        page(window, "video", "cutout");
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
        page(window, "video", "enhance");
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
        QTRY_VERIFY(!window->property("selectionKind").toString().isEmpty());
        page(window, "audio", "cleanup");
        auto *section = findItem(window->contentItem(), "soundSection");
        QTRY_VERIFY(section && section->isVisible());
        const auto selectedId = editor.project().clips.first().id;
        QVERIFY(QMetaObject::invokeMethod(findItem(window->contentItem(), "soundPreset"), "activated",
                                          Q_ARG(int, 2))); // Clear voice
        QCOMPARE(editor.project().clip(selectedId)->lowCut, 80.);
        QCOMPARE(editor.project().clip(selectedId)->compressor, .5);
        editor.undo();
        QCOMPARE(editor.project().clip(selectedId)->lowCut, 0.);
        page(window, "more");
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
