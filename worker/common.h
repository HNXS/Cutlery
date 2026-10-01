#pragma once
// Shared helpers for the cutlery-ai worker tasks.
#ifdef _WIN32
// The DirectML header includes windows.h; keep its min/max macros out of std::min/std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <onnxruntime_cxx_api.h>
#ifdef CUTLERY_DIRECTML
#include <dml_provider_factory.h>
#endif
#include <QHash>
#include <QProcess>
#include <QStringList>
#include <algorithm>
#include <cstdio>
#include <memory>

namespace worker {
inline int fail(const QString &message) {
    std::fprintf(stderr, "cutlery-ai: %s\n", qUtf8Printable(message));
    return 1;
}
inline QString num(double v) {
    return QString::number(v, 'f', 6);
}
// "--key value" pairs.
inline QHash<QString, QString> options(int argc, char **argv, int first) {
    QHash<QString, QString> o;
    for (int i = first; i + 1 < argc; i += 2)
        o.insert(QString::fromUtf8(argv[i]).mid(2), QString::fromUtf8(argv[i + 1]));
    return o;
}
inline bool size(const QString &text, int &width, int &height) {
    const auto parts = text.split('x');
    if (parts.size() != 2)
        return false;
    width = parts[0].toInt();
    height = parts[1].toInt();
    return width >= 2 && height >= 2;
}
inline bool readExactly(QProcess &p, char *data, qint64 size) {
    qint64 done = 0;
    while (done < size) {
        const auto n = p.read(data + done, size - done);
        if (n < 0)
            return false;
        done += n;
        if (done < size && !p.waitForReadyRead(300000) && p.bytesAvailable() == 0)
            return false;
    }
    return true;
}
inline bool writeAll(QProcess &p, const char *data, qint64 size) {
    if (p.write(data, size) != size)
        return false;
    while (p.bytesToWrite() > 0)
        if (!p.waitForBytesWritten(300000))
            return false;
    return true;
}
inline void progress(qint64 done, qint64 total) {
    std::printf("progress %lld %lld\n", static_cast<long long>(done),
                static_cast<long long>(std::max(total, done)));
    std::fflush(stdout);
}
// Opens a model on the GPU through DirectML when available (any DirectX 12 GPU), otherwise on
// the CPU. Prints "device gpu" or "device cpu" for the editor.
inline std::unique_ptr<Ort::Session> openModel(Ort::Env &env, const QString &model, bool cpu) {
#ifdef _WIN32
    const auto path = model.toStdWString();
#else
    const auto path = model.toStdString();
#endif
#ifdef CUTLERY_DIRECTML
    if (!cpu) {
        try {
            Ort::SessionOptions gpu;
            gpu.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            // Required by the DirectML provider.
            gpu.DisableMemPattern();
            gpu.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(gpu, 0));
            auto session = std::make_unique<Ort::Session>(env, path.c_str(), gpu);
            std::printf("device gpu\n");
            std::fflush(stdout);
            return session;
        } catch (const Ort::Exception &e) {
            std::fprintf(stderr, "cutlery-ai: DirectML unavailable, using the CPU: %s\n",
                         e.what());
        }
    }
#else
    (void)cpu;
#endif
    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    auto session = std::make_unique<Ort::Session>(env, path.c_str(), options);
    std::printf("device cpu\n");
    std::fflush(stdout);
    return session;
}
int matte(const QHash<QString, QString> &);
int upscale(const QHash<QString, QString> &);
int transcribe(const QHash<QString, QString> &);
} // namespace worker
