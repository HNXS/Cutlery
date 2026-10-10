#pragma once
#include "Project.h"
#include <QObject>
#include <QProcess>
#include <QSize>
#include <QStringList>
#include <functional>

namespace cutlery {
// What the user asks for: a delivery format, a quality level and an output height.
struct ExportSettings {
    // Video: h264, hevc, av1, vp9, prores, prores4444 (with transparency), mpeg4, gif
    // (animated, no sound), png (a numbered picture sequence in a folder, with transparency, no
    // sound). Audio only: mp3, m4a (AAC), wav.
    QString format = "h264";
    QString quality = "high"; // max, high, balanced, small
    int height = 0;           // 0: project size
    // Target integrated loudness in LUFS (-14 YouTube/streaming, -16 podcasts, -23 EBU R128 TV);
    // 0 keeps the mix as it is.
    double loudness = 0;
    // Output frame rate (0: the project's), one of exportFrameRates(); frames are repeated or
    // dropped to reach it. Video bitrate in kbit/s (0: by quality), for encoders that take one.
    double fps = 0;
    int bitrate = 0;
    // Sound: 2 (stereo), 1 (mono) or 6 (5.1, tracks placed by Track::surround) channels, at
    // 48000 or 44100 Hz. MP3 holds at most two.
    int channels = 2, sampleRate = 48000;
    // Picture: "" (8-bit SDR as before), "10bit" (10-bit BT.709 SDR), "pq" (HDR10: BT.2020 with
    // the PQ curve) or "hlg" (BT.2020 hybrid log-gamma). 10-bit and HDR need HEVC, AV1, VP9 or
    // ProRes; the timeline's SDR picture keeps its look, its white at 203 nits.
    QString dynamicRange;
};
// Whether a format can be exported in 10 bits and as HDR.
bool deepColourFormat(const QString &format);
// Frame rates an export can be set to, as FFmpeg rates ("30000/1001") paired with their value.
const QVector<QPair<QString, double>> &exportFrameRates();
// One concrete way to produce that format. Candidates are tried in order; hardware encoders are
// only used after a short probe proves they work on this machine.
struct Encoder {
    QString name, label;
    QStringList videoArguments, audioArguments;
    QString pixelFormat = "yuv420p", extension = "mp4";
    // The render graph's own last pixel format when the tail below converts from it; empty for
    // pixelFormat.
    QString graphPixelFormat;
    bool probe = false;
    bool audioOnly = false; // no video stream; videoArguments and pixelFormat are unused
    bool noAudio = false;   // picture only (GIF)
    int channels = 2;       // of the render graph's mix: 2 or 6 (5.1)
    // Filters after the picture's pixel format, before the encoder (e.g. GIF palette steps);
    // labels inside must not clash with the render graph's.
    QString videoTail;
    // Transparent where nothing is on the timeline (an alpha channel in the output).
    bool alpha = false;
    // Numbered pictures into a folder instead of one file.
    bool sequence = false;
    // Output frame rate for FFmpeg's -r ("30000/1001"); empty keeps the project's.
    QString frameRate;
    double frameRateValue = 0;
};
const QStringList &exportFormats();
QString formatExtension(const QString &format);
bool audioFormat(const QString &format);
QSize exportSize(const Project &, int height);
QVector<Encoder> encoderCandidates(const ExportSettings &, QSize size, double fps);

// Picks the first working candidate without blocking the interface. Results are cached per
// encoder and argument list for the session.
class EncoderResolver final : public QObject {
    Q_OBJECT
  public:
    EncoderResolver(QString ffmpeg, QObject *parent = nullptr);
    ~EncoderResolver() override;
    void resolve(const QVector<Encoder> &candidates, QSize size, double fps,
                 std::function<void(const Encoder *)> done);
    void cancel();
    bool busy() const {
        return m_process != nullptr;
    }

  private:
    QString m_ffmpeg;
    QHash<QString, bool> m_works;
    QProcess *m_process = nullptr;
    void tryNext(std::shared_ptr<QVector<Encoder>> candidates, int index, QSize size, double fps,
                 std::function<void(const Encoder *)> done);
};
} // namespace cutlery
