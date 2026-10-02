#pragma once
#include "RenderGraph.h"
#include <QAudioSink>
#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSize>
#include <QTimer>
#include <QVideoSink>
#include <deque>
#include <memory>

class QTemporaryDir;

namespace cutlery {
// Peak amplitude (0..1) of the left and right channel of interleaved stereo PCM, 16-bit signed or
// 32-bit float.
std::pair<double, double> pcmPeaks(const char *data, qsizetype bytes, bool floatSamples);
// Live timeline playback. FFmpeg renders the timeline from the playhead into two pipes, paced to
// real time: raw yuv420p video and 48 kHz stereo PCM. Audio output is the master clock; video
// frames are shown when the clock reaches them, and late frames are skipped rather than delaying
// sound. Without an audio device the wall clock is used. Nothing is rendered to disk first.
class Playback final : public QObject {
    Q_OBJECT
  public:
    struct Request {
        QString ffmpeg;
        RenderPlan video, audio;
        QString videoGraph, audioGraph;
        int fpsN = 30, fpsD = 1;
        qint64 from = 0; // first timeline frame of both plans
        double rate = 1; // timeline seconds per second (the plans are paced to match)
        std::shared_ptr<QTemporaryDir> work; // graph files and title images, kept while running
    };
    explicit Playback(QObject *parent = nullptr);
    ~Playback() override;
    void setVideoSink(QVideoSink *);
    // Shows a still frame in the viewer (used for rendered previews while stopped).
    void showImage(const QImage &);
    void start(const Request &);
    void stop();
    bool active() const {
        return m_active;
    }
    // Timeline frame currently on screen.
    qint64 frame() const {
        return m_frame;
    }
    bool clockStarted() const {
        return m_clockStarted;
    }
    // Level meter of the sound being played, in dBFS per channel (−90 when silent), with peaks
    // held and falling back by about 20 dB per second.
    std::pair<double, double> levels() const {
        return {m_levels[0], m_levels[1]};
    }
  signals:
    void frameChanged();
    void finished();
    void failed(const QString &message);

  private:
    double m_levels[2] = {-90, -90};
    QElapsedTimer m_levelClock;
    struct Frame {
        qint64 index = 0;
        QByteArray data;
    };
    QPointer<QVideoSink> m_sink;
    QProcess *m_video = nullptr, *m_audio = nullptr;
    QAudioSink *m_output = nullptr;
    QIODevice *m_outputDevice = nullptr;
    QElapsedTimer m_wallClock;
    double m_clockBase = 0; // seconds already played before the current clock source
    int m_audioFrameBytes = 4;
    bool m_floatAudio = false, m_hasDevice = false;
    QTimer m_tick;
    Request m_request;
    QByteArray m_videoPending, m_audioPending, m_log;
    std::deque<Frame> m_frames;
    qint64 m_nextIndex = 0, m_frame = 0;
    bool m_active = false, m_clockStarted = false, m_videoDone = false, m_audioDone = false;
    qsizetype frameBytes() const;
    QProcess *launch(const QStringList &arguments, bool video);
    void readVideo();
    void readAudio();
    void processFinished(QProcess *, int code, QProcess::ExitStatus);
    void startClock();
    void releaseOutput();
    double clock() const;
    void tick();
    void present(const QByteArray &);
    void release();
};
} // namespace cutlery
