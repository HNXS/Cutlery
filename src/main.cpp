#include "Editor.h"
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("HNXS");
    app.setApplicationName("Cutlery");
    app.setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Basic");
    auto *frames = new cutlery::FrameProvider;
    cutlery::Editor editor(frames);
    QLockFile lock(editor.state()["dataPath"].toString() + "/session.lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        fprintf(stderr, "Cutlery is already running, or the data folder is not writable.\n");
        delete frames;
        return 2;
    }
    QQmlApplicationEngine engine;
    engine.addImageProvider("frames", frames);
    engine.rootContext()->setContextProperty("editor", &editor);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Cutlery", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    const auto args = app.arguments();
    QTemporaryDir demo;
    if (args.contains("--demo")) {
        const auto file = demo.filePath("Cutlery demo.png");
        QImage image(1280, 720, QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const double a = double(x) / 1280, b = double(y) / 720;
                image.setPixelColor(x, y,
                                    QColor::fromRgbF(.04 + .13 * a, .12 + .20 * b, .18 + .15 * a));
            }
        image.save(file);
        cutlery::Project p;
        p.name = "A story in the making";
        p.width = 1280;
        p.height = 720;
        cutlery::Asset a;
        a.id = "demo";
        a.path = file;
        a.name = "Gradient study";
        a.kind = "image";
        a.duration = 8;
        p.assets.push_back(a);
        cutlery::Clip c;
        c.id = "background";
        c.assetId = a.id;
        c.name = a.name;
        c.duration = 240;
        p.clips.push_back(c);
        c = {};
        c.id = "title";
        c.name = "Opening title";
        c.text = "MAKE THE CUT.";
        c.track = 2;
        c.start = 15;
        c.duration = 180;
        c.fontSize = 88;
        p.clips.push_back(c);
        const auto project = demo.filePath("Demo.cutlery");
        cutlery::saveProject(p, project);
        editor.openProject(QUrl::fromLocalFile(project));
        editor.select("title");
        editor.seek(60);
    } else if (args.size() > 1 && !args[1].startsWith("--"))
        editor.openProject(QUrl::fromLocalFile(args[1]));
    const auto shot = args.indexOf("--screenshot");
    if (shot >= 0 && shot + 1 < args.size())
        QTimer::singleShot(5000, &app, [&] {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            const bool ok = window && window->grabWindow().save(args[shot + 1]);
            app.exit(ok ? 0 : 3);
        });
    if (args.contains("--smoke-test"))
        QTimer::singleShot(1500, &app, &QCoreApplication::quit);
    return app.exec();
}
