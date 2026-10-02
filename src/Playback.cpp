#include "Playback.h"
#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QTemporaryDir>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace cutlery {
namespace {
constexpr int sampleRate = 48000;
// Decoded frames waiting ahead of the clock. Real-time pacing keeps the queue short; the cap only
// guards memory if pacing and the audio clock disagree.
constexpr std::size_t maxQueuedFrames = 90;
QAudioFormat audioFormat(bool floating) {
    QAudioFormat f;
    f.setSampleRate(sampleRate);
    f.setChannelCount(2);
    f.setSampleFormat(floating ? QAudioFormat::Float : QAudioFormat::Int16);
    return f;
}
} // namespace

Playback::Playback(QObject *parent) : QObject(parent) {
    m_tick.setInterval(5);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &Playback::tick);
}
Playback::~Playback() {
    release();
}
void Playback::setVideoSink(QVideoSink *sink) {
    m_sink = sink;
}
void Playback::showImage(const QImage &image) {
    if (!m_sink || image.isNull())
        return;
    const auto rgba = image.convertToFormat(QImage::Format_RGBA8888);
    QVideoFrame frame(QVideoFrameFormat(rgba.size(), QVideoFrameFormat::Format_RGBA8888));
    if (!frame.map(QVideoFrame::WriteOnly))
        return;
    for (int y = 0; y < rgba.height(); ++y)
        std::memcpy(frame.bits(0) + y * frame.bytesPerLine(0), rgba.constScanLine(y),
                    size_t(rgba.width()) * 4);
    frame.unmap();
    m_sink->setVideoFrame(frame);
}
qsizetype Playback::frameBytes() const {
    const auto w = qsizetype(m_request.video.width), h = qsizetype(m_request.video.height);
    return w * h + 2 * (w / 2) * (h / 2);
}
void Playback::start(const Request &request) {
    release();
    m_request = request;
    m_frame = request.from;
    m_nextIndex = 0;
    m_clockBase = 0;
    m_clockStarted = false;
    m_videoDone = request.videoGraph.isEmpty();
    m_audioDone = true;
    m_log.clear();
    m_active = true;
    const auto device = QMediaDevices::defaultAudioOutput();
    m_hasDevice = false;
    if (!device.isNull() && !request.audioGraph.isEmpty()) {
        if (device.isFormatSupported(audioFormat(false)))
            m_hasDevice = true, m_floatAudio = false;
        else if (device.isFormatSupported(audioFormat(true)))
            m_hasDevice = true, m_floatAudio = true;
    }
    m_audioFrameBytes = m_floatAudio ? 8 : 4;
    if (!m_videoDone)
        m_video = launch(streamArguments(request.video, request.videoGraph, true), true);
    // Without a usable output device, sound would be discarded; the wall clock drives video.
    if (m_hasDevice) {
        m_audioDone = false;
        m_audio =
            launch(streamArguments(request.audio, request.audioGraph, false, m_floatAudio), false);
    }
    m_tick.start();
}
QProcess *Playback::launch(const QStringList &arguments, bool video) {
    auto *process = new QProcess(this);
    connect(process, &QProcess::readyReadStandardOutput, this,
            video ? &Playback::readVideo : &Playback::readAudio);
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        m_log += process->readAllStandardError();
        if (m_log.size() > 8000)
            m_log = m_log.right(8000);
    });
    connect(process, &QProcess::finished, this,
            [this, process](int code, QProcess::ExitStatus status) {
                processFinished(process, code, status);
            });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            processFinished(process, -1, QProcess::CrashExit);
    });
    process->start(m_request.ffmpeg, arguments);
    return process;
}
void Playback::readVideo() {
    if (!m_video)
        return;
    m_videoPending += m_video->readAllStandardOutput();
    const auto bytes = frameBytes();
    qsizetype used = 0;
    while (m_videoPending.size() - used >= bytes) {
        m_frames.push_back({m_nextIndex++, m_videoPending.mid(used, bytes)});
        used += bytes;
        if (m_frames.size() > maxQueuedFrames)
            m_frames.pop_front();
    }
    m_videoPending.remove(0, used);
}
std::pair<double, double> pcmPeaks(const char *data, qsizetype bytes, bool floatSamples) {
    double peak[2] = {0, 0};
    if (floatSamples) {
        const auto *s = reinterpret_cast<const float *>(data);
        for (qsizetype i = 0; i + 1 < bytes / qsizetype(sizeof(float)); i += 2)
            for (int c = 0; c < 2; ++c)
                peak[c] = std::max(peak[c], double(std::abs(s[i + c])));
    } else {
        const auto *s = reinterpret_cast<const qint16 *>(data);
        for (qsizetype i = 0; i + 1 < bytes / qsizetype(sizeof(qint16)); i += 2)
            for (int c = 0; c < 2; ++c)
                peak[c] = std::max(peak[c], std::abs(double(s[i + c])) / 32768.);
    }
    return {std::min(1., peak[0]), std::min(1., peak[1])};
}
void Playback::readAudio() {
    if (m_audio)
        m_audioPending += m_audio->readAllStandardOutput();
}
void Playback::processFinished(QProcess *process, int code, QProcess::ExitStatus status) {
    if (process != m_video && process != m_audio)
        return;
    const bool video = process == m_video;
    if (video)
        readVideo();
    else
        readAudio();
    m_log += process->readAllStandardError();
    process->disconnect(this);
    process->deleteLater();
    (video ? m_video : m_audio) = nullptr;
    (video ? m_videoDone : m_audioDone) = true;
    if (code != 0 || status != QProcess::NormalExit) {
        const auto message = QString::fromUtf8(m_log).right(2500);
        release();
        emit failed("Playback failed. " + message);
    }
}
void Playback::startClock() {
    m_clockStarted = true;
    if (m_hasDevice) {
        m_output = new QAudioSink(QMediaDevices::defaultAudioOutput(), audioFormat(m_floatAudio),
                                  this);
        // Enough to ride out scheduling jitter; small enough to keep picture and sound together.
        m_output->setBufferSize(sampleRate * m_audioFrameBytes / 8);
        m_outputDevice = m_output->start();
        if (!m_outputDevice || m_output->error() != QAudio::NoError)
            releaseOutput();
    }
    m_wallClock.start();
}
void Playback::releaseOutput() {
    if (!m_output)
        return;
    m_output->disconnect(this);
    m_output->stop();
    m_output->deleteLater();
    m_output = nullptr;
    m_outputDevice = nullptr;
}
double Playback::clock() const {
    if (m_output)
        return m_clockBase + m_output->processedUSecs() / 1e6;
    return m_clockBase + m_wallClock.nsecsElapsed() / 1e9;
}
void Playback::tick() {
    if (!m_active)
        return;
    if (!m_clockStarted) {
        const bool videoReady = m_videoDone || !m_frames.empty();
        const bool audioReady = m_audioDone || m_audioPending.size() >= sampleRate *
                                                                            m_audioFrameBytes / 8;
        if (!videoReady || !audioReady)
            return;
        startClock();
    }
    if (m_output) {
        auto n = std::min<qsizetype>(m_output->bytesFree(), m_audioPending.size());
        n -= n % m_audioFrameBytes;
        if (n > 0) {
            const auto written = m_outputDevice->write(m_audioPending.constData(), n);
            if (written > 0) {
                // Meter what goes to the device: hold peaks, fall back about 20 dB/s.
                const auto [l, r] = pcmPeaks(m_audioPending.constData(), written, m_floatAudio);
                const double fall =
                    m_levelClock.isValid() ? m_levelClock.restart() / 1000.0 * 20 : 0;
                if (!m_levelClock.isValid())
                    m_levelClock.start();
                const double now[2] = {l, r};
                for (int c = 0; c < 2; ++c)
                    m_levels[c] = std::max(now[c] > 0 ? 20 * std::log10(now[c]) : -90.,
                                           std::max(-90., m_levels[c] - fall));
                m_audioPending.remove(0, written);
            }
        }
        // The device clock stops when sound runs out; continue on the wall clock after the end.
        const bool drained = m_output->state() == QAudio::IdleState ||
                             m_output->bytesFree() >= m_output->bufferSize();
        if (m_audioDone && m_audioPending.isEmpty() && drained) {
            m_clockBase = clock();
            releaseOutput();
            m_wallClock.start();
        }
    }
    const double seconds = clock();
    const qint64 end = m_request.from + m_request.video.frames;
    const qint64 target =
        m_request.from +
        qint64(std::floor(seconds * m_request.rate * m_request.fpsN / m_request.fpsD + 1e-9));
    // Show the newest decoded frame the clock has reached; frames behind it are skipped.
    QByteArray show;
    while (!m_frames.empty() && m_request.from + m_frames.front().index <= target) {
        show = std::move(m_frames.front().data);
        m_frames.pop_front();
    }
    if (!show.isEmpty())
        present(show);
    const auto position = std::clamp(target, m_request.from, end - 1);
    if (position != m_frame) {
        m_frame = position;
        emit frameChanged();
    }
    const bool videoOver = m_videoDone && m_frames.empty();
    if (target >= end && videoOver) {
        release();
        emit finished();
    }
}
void Playback::present(const QByteArray &data) {
    if (!m_sink || data.size() != frameBytes())
        return;
    const int w = m_request.video.width, h = m_request.video.height;
    QVideoFrameFormat format(QSize(w, h), QVideoFrameFormat::Format_YUV420P);
    // FFmpeg's default RGB to YUV conversion: BT.601, limited range.
    format.setColorSpace(QVideoFrameFormat::ColorSpace_BT601);
    format.setColorRange(QVideoFrameFormat::ColorRange_Video);
    QVideoFrame frame(format);
    if (!frame.map(QVideoFrame::WriteOnly))
        return;
    const char *source = data.constData();
    const int widths[] = {w, w / 2, w / 2}, heights[] = {h, h / 2, h / 2};
    for (int plane = 0; plane < 3; ++plane) {
        for (int y = 0; y < heights[plane]; ++y) {
            std::memcpy(frame.bits(plane) + y * frame.bytesPerLine(plane), source,
                        size_t(widths[plane]));
            source += widths[plane];
        }
    }
    frame.unmap();
    m_sink->setVideoFrame(frame);
}
void Playback::stop() {
    release();
}
void Playback::release() {
    m_tick.stop();
    for (auto **p : {&m_video, &m_audio}) {
        if (*p) {
            (*p)->disconnect(this);
            (*p)->kill();
            (*p)->waitForFinished(2000);
            delete *p;
            *p = nullptr;
        }
    }
    releaseOutput();
    m_frames.clear();
    m_videoPending.clear();
    m_audioPending.clear();
    m_levels[0] = m_levels[1] = -90;
    m_levelClock.invalidate();
    m_request.work.reset();
    m_active = false;
    m_clockStarted = false;
}
} // namespace cutlery
