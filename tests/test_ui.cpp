#include "Editor.h"
#include "KeyboardShortcuts.h"
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
  private slots:
    void editingAndPlayback() {
        QStandardPaths::setTestModeEnabled(true);
        QQuickStyle::setStyle("Basic");
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
        QTRY_VERIFY_WITH_TIMEOUT(!editor.state()["playbackUrl"].toString().isEmpty(), 30000);
        auto *player = window->findChild<QObject *>("previewPlayer");
        QVERIFY(player);
        QTRY_VERIFY_WITH_TIMEOUT(player->property("position").toLongLong() > 0, 10000);
        QTest::keyClick(window, Qt::Key_K);
        QVERIFY(player->property("playbackState").toInt() != 1);
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
        QVERIFY(editor.save(QUrl::fromLocalFile(dir.filePath("ui.cutlery"))));
    }
};
QTEST_MAIN(UiTest)
#include "test_ui.moc"
