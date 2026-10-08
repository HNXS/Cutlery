#pragma once
#include "Project.h"
#include <QHash>
#include <QImage>
#include <QPoint>
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
// Media derived from a video asset by the AI worker, e.g. a person matte (grayscale video at
// `rate` frames per second). Its time zero is source time `start`; it covers source seconds
// [start, end).
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
    // Live playback speed (1, 2 or 4): real-time pacing runs this much faster, and the sound is
    // sped up without changing its pitch.
    double rate = 1;
    // Export: high-quality (Lanczos, accurate) scaling and the encoder's pixel format.
    bool highQuality = false;
    QString pixelFormat = "yuv420p";
    // Filters appended to the picture after its pixel format (export only, e.g. a GIF palette).
    QString videoTail;
    // The picture starts transparent instead of black, for exports with an alpha channel.
    bool transparent = false;
    // Master audio: gain before the output limiter (loudness normalisation) and the limiter's
    // ceiling as linear amplitude. `measureLoudness` replaces the limiter with an EBU R128 meter
    // whose summary FFmpeg logs at the "info" level (see parseIntegratedLoudness).
    double gainDb = 0, limit = 0.95;
    bool measureLoudness = false;
    // Mattes by asset id, used by clips with AI cutout.
    QHash<QString, MatteSource> mattes;
    // AI-upscaled pictures by asset id, used instead of the source by clips with AI upscale.
    // Same timing as the source from time `start` (`rate` is unused).
    QHash<QString, MatteSource> upscaled;
    // Eye-contact pictures by asset id, used instead of the source (and of an upscale) by clips
    // with eye contact. Same timing as `upscaled`.
    QHash<QString, MatteSource> eyeContact;
};
// A title template (Clip::titleStyle) drawn tightly on a transparent image, and where its top-left
// corner sits on a width × height canvas before the clip's x/y offset; text sizes follow the
// clip's scale. Empty for plain titles.
struct TitlePlate {
    QImage image;
    QPoint position;
};
TitlePlate titlePlate(const Clip &, int width, int height, int projectHeight);
// A title's text as shown: words between asterisks (*like this*) are drawn in the highlight
// colour, and the asterisks are left out.
QString shownTitleText(const QString &text);
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
// Integrated loudness (LUFS) from an EBU R128 meter summary in FFmpeg's log; NaN when absent.
double parseIntegratedLoudness(const QString &log);
// True peak (dBTP) from the same summary; NaN when absent.
double parseTruePeak(const QString &log);
// A file path as an option value inside a filter graph, with every special character escaped.
QString filterPath(const QString &path);
// Audio-only measuring pass for a plan compiled with `measureLoudness`.
QStringList measureArguments(const RenderPlan &, const QString &graphFile);
QStringList streamArguments(const RenderPlan &, const QString &graphFile, bool video,
                            bool floatAudio = false);
} // namespace cutlery
