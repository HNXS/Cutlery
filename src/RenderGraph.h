#pragma once
#include "Project.h"
#include <QHash>
#include <QStringList>

namespace cutlery {
struct RenderPlan {
    QStringList inputs;
    QString graph;
    double duration = 0;
    qint64 frames = 0;
    int width = 0, height = 0;
};
// A range of timeline frames to compile. Only clips overlapping [from, to) are decoded, and each
// input seeks directly to its first needed source time, so the cost of a preview depends on the
// window length rather than on the playhead position.
// An analysed person matte of a video asset: grayscale video at `rate` frames per second whose
// time zero is source time `start`, covering source seconds [start, end).
struct MatteSource {
    QString path;
    double start = 0, end = 0, rate = 8;
};
struct RenderOptions {
    bool video = true, audio = true;
    qint64 from = 0, to = -1; // -1: timeline end
    // Pace output to wall-clock speed. Live playback reads frames from a pipe; pacing bounds the
    // amount of decoded media waiting in memory.
    bool realtime = false;
    // Export: high-quality (Lanczos, accurate) scaling and the encoder's pixel format.
    bool highQuality = false;
    QString pixelFormat = "yuv420p";
    // Mattes by asset id, used by clips with AI cutout.
    QHash<QString, MatteSource> mattes;
};
struct Encoder;
// The same compiler handles preview stills, live playback and final export.
// FFmpeg is the first CPU reference backend; a D3D11 backend is not implemented.
RenderPlan compileRender(const Project &, const QString &workDir, int width, int height,
                         const RenderOptions & = {});
QStringList renderArguments(const RenderPlan &, const QString &graphFile, const QString &output,
                            const QString &profile, double seek = -1);
QStringList exportArguments(const RenderPlan &, const QString &graphFile, const QString &output,
                            const Encoder &);
// Raw output for live playback on stdout: yuv420p frames, or 48 kHz interleaved stereo PCM
// (signed 16-bit, or 32-bit float when the audio device requires it).
QStringList streamArguments(const RenderPlan &, const QString &graphFile, bool video,
                            bool floatAudio = false);
} // namespace cutlery
