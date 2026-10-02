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
    // Video: h264, hevc, av1, vp9, prores, mpeg4. Audio only: mp3, m4a (AAC), wav.
    QString format = "h264";
    QString quality = "high"; // max, high, balanced, small
    int height = 0;           // 0: project size
    // Target integrated loudness in LUFS (-14 YouTube/streaming, -16 podcasts, -23 EBU R128 TV);
    // 0 keeps the mix as it is.
    double loudness = 0;
};
// One concrete way to produce that format. Candidates are tried in order; hardware encoders are
// only used after a short probe proves they work on this machine.
struct Encoder {
    QString name, label;
    QStringList videoArguments, audioArguments;
    QString pixelFormat = "yuv420p", extension = "mp4";
    bool probe = false;
    bool audioOnly = false; // no video stream; videoArguments and pixelFormat are unused
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
