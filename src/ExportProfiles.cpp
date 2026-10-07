#include "ExportProfiles.h"
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <memory>

namespace cutlery {
const QStringList &exportFormats() {
    static const QStringList formats{"h264", "hevc", "av1", "vp9", "prores", "prores4444",
                                     "mpeg4", "gif", "png", "mp3", "m4a", "wav"};
    return formats;
}
const QVector<QPair<QString, double>> &exportFrameRates() {
    static const QVector<QPair<QString, double>> rates{
        {"24000/1001", 24000 / 1001.}, {"24", 24}, {"25", 25}, {"30000/1001", 30000 / 1001.},
        {"30", 30}, {"50", 50}, {"60000/1001", 60000 / 1001.}, {"60", 60}};
    return rates;
}
bool audioFormat(const QString &format) {
    return format == "mp3" || format == "m4a" || format == "wav";
}
QString formatExtension(const QString &format) {
    if (audioFormat(format))
        return format;
    if (format == "vp9")
        return "webm";
    if (format == "prores" || format == "prores4444")
        return "mov";
    if (format == "png")
        return "png";
    if (format == "gif")
        return "gif";
    return "mp4";
}
QSize exportSize(const Project &p, int height) {
    if (height <= 0)
        return {p.width, p.height};
    const int width = std::max(2, int(std::lround(double(p.width) * height / p.height / 2)) * 2);
    return {width, height / 2 * 2};
}
static QVector<Encoder> baseCandidates(const ExportSettings &s, QSize size, double fps) {
    const int q = std::max<qsizetype>(0, QStringList{"max", "high", "balanced", "small"}.indexOf(
                                             s.quality));
    const auto pick = [q](std::initializer_list<int> values) {
        return QString::number(*(values.begin() + q));
    };
    const QString gop = QString::number(std::max(1, int(std::lround(fps * 2))));
    const QStringList aac{"-c:a", "aac", "-b:a", pick({320, 256, 192, 128}) + "k"};
    // Media Foundation encoders take a bitrate; scale it by pixels per second.
    const auto bitrate = [&](double bitsPerPixel) {
        return QString::number(qRound64(size.width() * size.height() * fps * bitsPerPixel / 1000)) +
               "k";
    };
    const double bpp[] = {0.22, 0.13, 0.08, 0.05};
    QVector<Encoder> c;
    auto add = [&](QString name, QString label, QStringList video, bool probe) {
        Encoder e;
        e.name = name;
        e.label = label;
        e.videoArguments = QStringList{"-c:v", name} + video;
        e.audioArguments = aac;
        e.extension = formatExtension(s.format);
        e.probe = probe;
        c << e;
    };
    if (audioFormat(s.format)) {
        Encoder e;
        e.audioOnly = true;
        e.extension = s.format;
        if (s.format == "mp3") {
            e.name = "libmp3lame";
            e.label = "MP3 (LAME)";
            e.audioArguments = {"-c:a", "libmp3lame", "-q:a", pick({0, 2, 4, 6})};
        } else if (s.format == "m4a") {
            e.name = "aac";
            e.label = "AAC";
            e.audioArguments = aac;
        } else {
            e.name = "pcm";
            e.label = "WAV (uncompressed)";
            e.audioArguments = {"-c:a", q == 0 ? "pcm_s24le" : "pcm_s16le"};
        }
        return {e};
    }
    if (s.format == "gif") {
        // An animated GIF: fewer frames a second and one palette made from the whole clip
        // (colours that change little between frames weigh most), with light ordered dithering.
        Encoder e;
        e.name = "gif";
        e.label = "GIF";
        e.extension = "gif";
        e.noAudio = true;
        e.pixelFormat = "rgb24";
        e.videoArguments = {"-c:v", "gif", "-loop", "0"};
        e.videoTail = QString("fps=%1,split[gifa][gifb];[gifa]palettegen=max_colors=%2:stats_mode=diff[gifp];"
                              "[gifb][gifp]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle")
                          .arg(pick({25, 15, 12, 10}), pick({256, 256, 192, 128}));
        return {e};
    }
    if (s.format == "png") {
        // Lossless pictures with transparency, one file per frame.
        Encoder e;
        e.name = "png";
        e.label = "PNG sequence";
        e.extension = "png";
        e.noAudio = true;
        e.alpha = true;
        e.sequence = true;
        e.pixelFormat = "rgba";
        e.videoArguments = {"-c:v", "png", "-compression_level", pick({6, 6, 4, 3})};
        return {e};
    }
    if (s.format == "h264" || s.format == "hevc") {
        const bool h264 = s.format == "h264";
        const QString codec = h264 ? "h264" : "hevc";
        const QStringList tag = h264 ? QStringList{} : QStringList{"-tag:v", "hvc1"};
        add(codec + "_nvenc", "NVIDIA NVENC",
            QStringList{"-preset", "p7", "-tune", "hq", "-rc", "vbr", "-cq",
                        pick({16, 19, 23, 28}), "-b:v", "0", "-g", gop} + tag,
            true);
        QStringList amf{"-quality", "quality", "-rc", "cqp", "-qp_i", pick({16, 19, 23, 28}),
                        "-qp_p", pick({16, 19, 23, 28})};
        if (h264)
            amf << "-qp_b" << pick({16, 19, 23, 28});
        add(codec + "_amf", "AMD AMF", amf + QStringList{"-g", gop} + tag, true);
        add(codec + "_qsv", "Intel Quick Sync",
            QStringList{"-preset", "veryslow", "-global_quality", pick({18, 21, 25, 30}), "-g",
                        gop} + tag,
            true);
        add(codec + "_mf", "Windows Media Foundation",
            QStringList{"-b:v", bitrate(bpp[q] * (h264 ? 1 : 0.65)), "-g", gop} + tag, true);
    } else if (s.format == "av1") {
        add("av1_nvenc", "NVIDIA NVENC",
            {"-preset", "p7", "-tune", "hq", "-rc", "vbr", "-cq", pick({22, 27, 32, 38}), "-b:v",
             "0", "-g", gop},
            true);
        add("av1_amf", "AMD AMF",
            {"-quality", "quality", "-rc", "cqp", "-qp_i", pick({64, 84, 104, 128}), "-qp_p",
             pick({64, 84, 104, 128}), "-g", gop},
            true);
        add("av1_qsv", "Intel Quick Sync",
            {"-preset", "veryslow", "-global_quality", pick({20, 24, 28, 34}), "-g", gop}, true);
        // Software fallback: always available, royalty-free and efficient.
        add("libsvtav1", "SVT-AV1 (software)",
            {"-preset", pick({5, 6, 8, 10}), "-crf", pick({22, 28, 34, 40}), "-g", gop}, false);
    } else if (s.format == "vp9") {
        add("libvpx-vp9", "libvpx VP9 (software)",
            {"-crf", pick({18, 26, 32, 38}), "-b:v", "0", "-deadline", "good", "-cpu-used",
             pick({1, 2, 3, 4}), "-row-mt", "1", "-g", gop},
            false);
        c.last().audioArguments = {"-c:a", "libopus", "-b:a", pick({256, 192, 160, 128}) + "k"};
    } else if (s.format == "prores4444") {
        // ProRes 4444 keeps an alpha channel: transparent where the timeline shows nothing.
        add("prores_ks", "ProRes 4444 with alpha (software)",
            {"-profile:v", "4", "-vendor", "apl0", "-alpha_bits", "16", "-qscale:v",
             pick({4, 6, 9, 12})},
            false);
        c.last().pixelFormat = "yuva444p10le";
        c.last().alpha = true;
        c.last().audioArguments = {"-c:a", "pcm_s16le"};
    } else if (s.format == "prores") {
        add("prores_ks", "ProRes 422 (software)",
            {"-profile:v", pick({3, 3, 2, 1}), "-vendor", "apl0"}, false);
        c.last().pixelFormat = "yuv422p10le";
        c.last().audioArguments = {"-c:a", "pcm_s16le"};
    } else {
        add("mpeg4", "MPEG-4 Part 2 (software)", {"-q:v", pick({2, 3, 5, 8}), "-g", gop}, false);
    }
    return c;
}
QVector<Encoder> encoderCandidates(const ExportSettings &s, QSize size, double fps) {
    auto c = baseCandidates(s, size, fps);
    for (auto &e : c) {
        if (!e.noAudio) {
            if (s.channels == 1)
                e.audioArguments << "-ac" << "1";
            if (s.sampleRate != 48000)
                e.audioArguments << "-ar" << QString::number(s.sampleRate);
        }
        if (s.fps > 0)
            for (const auto &[rate, value] : exportFrameRates())
                if (std::abs(value - s.fps) < 0.01) {
                    e.frameRate = rate;
                    e.frameRateValue = value;
                }
        if (s.bitrate <= 0 || e.name == "prores_ks" || e.sequence || e.audioOnly)
            continue;
        // A set bitrate replaces the quality-based rate control: the quality options are
        // dropped and an average bitrate with a 1.5× peak is asked for.
        static const QStringList quality{"-cq", "-b:v", "-rc", "-qp_i", "-qp_p", "-qp_b",
                                         "-global_quality", "-crf", "-q:v"};
        QStringList kept;
        for (int i = 0; i < e.videoArguments.size(); ++i) {
            if (quality.contains(e.videoArguments[i]) && i + 1 < e.videoArguments.size()) {
                ++i;
                continue;
            }
            kept << e.videoArguments[i];
        }
        if (e.name.endsWith("_nvenc"))
            kept << "-rc" << "vbr";
        else if (e.name.endsWith("_amf"))
            kept << "-rc" << "vbr_peak";
        const auto k = [](double v) { return QString::number(qRound64(v)) + "k"; };
        kept << "-b:v" << k(s.bitrate) << "-maxrate" << k(1.5 * s.bitrate) << "-bufsize"
             << k(2 * s.bitrate);
        e.videoArguments = kept;
    }
    return c;
}
EncoderResolver::EncoderResolver(QString ffmpeg, QObject *parent)
    : QObject(parent), m_ffmpeg(std::move(ffmpeg)) {}
EncoderResolver::~EncoderResolver() {
    cancel();
}
void EncoderResolver::cancel() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1500);
        m_process->deleteLater();
        m_process = nullptr;
    }
}
void EncoderResolver::resolve(const QVector<Encoder> &candidates, QSize size, double fps,
                              std::function<void(const Encoder *)> done) {
    cancel();
    tryNext(std::make_shared<QVector<Encoder>>(candidates), 0, size, fps, std::move(done));
}
void EncoderResolver::tryNext(std::shared_ptr<QVector<Encoder>> candidates, int index, QSize size,
                              double fps, std::function<void(const Encoder *)> done) {
    for (; index < candidates->size(); ++index) {
        const auto &e = candidates->at(index);
        const auto key = e.videoArguments.join(' ') + QString("@%1x%2").arg(size.width()).arg(
                                                          size.height());
        if (!e.probe || m_works.value(key, false))
            return done(&e);
        if (m_works.contains(key))
            continue; // Known not to work on this machine.
        // Encode a few frames at the real size with the real arguments.
        auto *p = new QProcess(this);
        m_process = p;
        QStringList args{"-hide_banner", "-nostdin",   "-v",    "error", "-f",
                         "lavfi",        "-i",
                         QString("color=c=gray:s=%1x%2:r=%3:d=0.3")
                             .arg(size.width())
                             .arg(size.height())
                             .arg(fps, 0, 'f', 3),
                         "-frames:v",    "5",          "-pix_fmt", e.pixelFormat};
        args += e.videoArguments;
        args << "-f" << "null" << "-";
        connect(p, &QProcess::finished, this,
                [=, this](int code, QProcess::ExitStatus status) {
                    m_process = nullptr;
                    p->deleteLater();
                    m_works[key] = code == 0 && status == QProcess::NormalExit;
                    tryNext(candidates, index, size, fps, done);
                });
        connect(p, &QProcess::errorOccurred, this, [=, this](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
                return;
            m_process = nullptr;
            p->deleteLater();
            m_works[key] = false;
            tryNext(candidates, index + 1, size, fps, done);
        });
        QTimer::singleShot(15000, p, [p] {
            if (p->state() != QProcess::NotRunning)
                p->kill();
        });
        p->start(m_ffmpeg, args);
        return;
    }
    done(nullptr);
}
} // namespace cutlery
