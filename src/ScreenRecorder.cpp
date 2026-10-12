#include "ScreenRecorder.h"

#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <string>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace cutlery {
ScreenRecorder::ScreenRecorder(QString ffmpeg, QObject *parent)
    : QObject(parent), m_ffmpeg(std::move(ffmpeg)) {}
ScreenRecorder::~ScreenRecorder() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}
QRect ScreenRecorder::screenArea(int screen) {
    const auto screens = QGuiApplication::screens();
    if (screen < 0 || screen >= screens.size())
        return {};
#ifdef Q_OS_WIN
    // Windows' own monitor rectangle, in the device pixels the desktop is captured in, also when
    // screens have different scalings; Qt names a screen after its display device.
    struct Search {
        std::wstring device;
        QRect found;
    } search{screens[screen]->name().toStdWString(), {}};
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
        auto *s = reinterpret_cast<Search *>(data);
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info) && s->device == info.szDevice) {
            const auto &r = info.rcMonitor;
            s->found = QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    if (search.found.isValid())
        return search.found;
#endif
    const auto g = screens[screen]->geometry();
    const double ratio = screens[screen]->devicePixelRatio();
    return QRect(qRound(g.x() * ratio), qRound(g.y() * ratio), qRound(g.width() * ratio),
                 qRound(g.height() * ratio));
}
void ScreenRecorder::hideFromCapture(QWindow *window) {
#ifdef Q_OS_WIN
    // WDA_EXCLUDEFROMCAPTURE (0x11); older Windows ignore it and the window is recorded.
    if (window)
        SetWindowDisplayAffinity(reinterpret_cast<HWND>(window->winId()), 0x11);
#else
    Q_UNUSED(window);
#endif
}
QStringList ScreenRecorder::arguments() const {
    // Even sizes, as H.264 needs.
    const int width = m_area.width() / 2 * 2, height = m_area.height() / 2 * 2;
    QStringList a{"-hide_banner", "-loglevel", "error", "-y"};
    if (qEnvironmentVariable("CUTLERY_SCREEN_SOURCE") == "test")
        a << "-re" << "-f" << "lavfi" << "-i"
          << QString("testsrc2=size=%1x%2:rate=%3").arg(width).arg(height).arg(m_fps);
    else
        a << "-f" << "gdigrab" << "-framerate" << QString::number(m_fps) << "-draw_mouse"
          << (m_cursor ? "1" : "0") << "-offset_x" << QString::number(m_area.x()) << "-offset_y"
          << QString::number(m_area.y()) << "-video_size" << QString("%1x%2").arg(width).arg(height)
          << "-i" << "desktop";
    a << "-an";
    if (m_lossless)
        a << "-c:v" << "ffv1" << "-level" << "3" << "-slices" << "16" << "-g" << "1"
          << "-pix_fmt" << "yuv420p";
    else
        // About 10 Mbit/s for 1080p, more for larger screens: sharp text in spreadsheets.
        a << "-c:v" << "h264_mf" << "-scenario" << "display_remoting" << "-b:v"
          << QString("%1k").arg(std::max(4000, int(10000.0 * width * height / (1920 * 1080))))
          << "-g" << QString::number(m_fps * 2) << "-pix_fmt" << "nv12";
    a << "-f" << "matroska" << m_output;
    return a;
}
bool ScreenRecorder::start(const QRect &area, int fps, bool cursor, const QString &output) {
    if (m_process)
        return false;
    m_area = area;
    m_fps = std::clamp(fps, 5, 60);
    m_cursor = cursor;
    m_output = output;
#ifdef Q_OS_WIN
    m_lossless = false;
#else
    m_lossless = true; // Media Foundation is Windows only
#endif
    m_error.clear();
    m_stopping = false;
    if (m_area.width() < 16 || m_area.height() < 16) {
        m_error = "There is no such screen";
        return false;
    }
    launch();
    return true;
}
void ScreenRecorder::launch() {
    auto *p = new QProcess(this);
    m_process = p;
    auto log = std::make_shared<QByteArray>();
    connect(p, &QProcess::readyReadStandardError, this, [p, log] {
        *log += p->readAllStandardError();
        if (log->size() > 8192)
            *log = log->right(4096);
    });
    connect(p, &QProcess::finished, this, [this, p, log](int code, QProcess::ExitStatus status) {
        if (m_process != p)
            return;
        m_process = nullptr;
        p->deleteLater();
        const bool early = !m_stopping && m_clock.elapsed() < 3000;
        // Without Media Foundation (Windows N editions) the encoder fails at once: lossless then.
        if (early && !m_lossless && QString::fromUtf8(*log).contains("h264_mf", Qt::CaseInsensitive)) {
            m_lossless = true;
            QFile::remove(m_output);
            launch();
            return;
        }
        const bool saved = QFileInfo(m_output).size() > 0;
        if (!saved || (status != QProcess::NormalExit && !m_stopping) || (code != 0 && !m_stopping)) {
            // FFmpeg's last few lines: the cause comes before "Error opening input files".
            QStringList lines;
            for (const auto &line : QString::fromUtf8(*log).split('\n'))
                if (!line.trimmed().isEmpty())
                    lines << line.trimmed();
            m_error = "Screen recording failed" +
                      (lines.isEmpty() ? QString() : ": " + lines.mid(std::max<qsizetype>(0, lines.size() - 3)).join(" "));
        }
        m_clock.invalidate();
        emit finished(m_error.isEmpty() ? m_output : QString(), m_error);
        emit changed();
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_process != p)
            return;
        m_process = nullptr;
        p->deleteLater();
        m_error = "FFmpeg could not be started";
        emit finished({}, m_error);
        emit changed();
    });
    p->start(m_ffmpeg, arguments());
    m_clock.start();
    emit changed();
}
void ScreenRecorder::stop() {
    if (!m_process || m_stopping)
        return;
    m_stopping = true;
    // "q" ends FFmpeg cleanly, finishing the file; a stuck one is killed after a while.
    m_process->write("q");
    QPointer<QProcess> p = m_process;
    QTimer::singleShot(10000, this, [p] {
        if (p && p->state() != QProcess::NotRunning)
            p->kill();
    });
}
} // namespace cutlery
