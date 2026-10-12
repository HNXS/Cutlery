#include "ScreenRecorder.h"

#include <algorithm>
#include <cmath>
#include <QFile>
#include <QHash>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <string>
#include <vector>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace cutlery {
ScreenRecorder::ScreenRecorder(QString ffmpeg, QObject *parent)
    : QObject(parent), m_ffmpeg(std::move(ffmpeg)) {
    m_pointerTimer.setInterval(33);
    m_pointerTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_pointerTimer, &QTimer::timeout, this, &ScreenRecorder::samplePointer);
}
void ScreenRecorder::samplePointer() {
    if (!m_clock.isValid() || m_pointer.size() >= 30 * 60 * 60 * 4) // four hours
        return;
    PointerSample s;
    s.time = m_clock.elapsed() / 1000.;
    sampleKeys(s.time);
    if (qEnvironmentVariable("CUTLERY_SCREEN_SOURCE") == "test") {
        s.x = 0.5 + 0.25 * std::cos(s.time);
        s.y = 0.5 + 0.25 * std::sin(s.time);
        s.buttons = s.time - std::floor(s.time) < 0.1 ? 1 : 0;
    } else {
#ifdef Q_OS_WIN
        POINT at{};
        if (!GetCursorPos(&at))
            return;
        s.x = (at.x - m_area.x() + 0.5) / m_area.width();
        s.y = (at.y - m_area.y() + 0.5) / m_area.height();
        s.buttons = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0) |
                    ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 2 : 0);
#else
        return;
#endif
    }
    m_pointer << s;
}
void ScreenRecorder::sampleKeys(double time) {
    if (!m_captureKeys || m_keys.size() >= 100000)
        return;
    if (qEnvironmentVariable("CUTLERY_SCREEN_SOURCE") == "test") {
        const int second = int(std::floor(time));
        const bool down = time - second >= 0.5 && time - second < 0.6;
        if (down && !m_down.contains(second))
            m_keys << KeyPress{time, second % 2 ? "Ctrl + V" : "Ctrl + C"};
        m_down = down ? QSet<int>{second} : QSet<int>{};
        return;
    }
#ifdef Q_OS_WIN
    auto isDown = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    QSet<int> down;
    for (int vk = 0x08; vk <= 0xFE; ++vk)
        if (isDown(vk))
            down.insert(vk);
    const bool ctrl = isDown(VK_CONTROL), alt = isDown(VK_MENU), shift = isDown(VK_SHIFT),
               win = isDown(VK_LWIN) || isDown(VK_RWIN);
    // AltGr arrives as right Alt with Ctrl: it types characters (@, €, ...).
    const bool altGr = isDown(VK_RMENU) && ctrl;
    // Keys already down when recording starts are not shortcuts made during it.
    if (!m_keysPrimed) {
        m_down = down;
        m_keysPrimed = true;
        return;
    }
    static const QSet<int> modifiers{VK_SHIFT, VK_CONTROL, VK_MENU, VK_LSHIFT, VK_RSHIFT,
                                     VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LWIN,
                                     VK_RWIN, VK_CAPITAL, VK_NUMLOCK, VK_SCROLL, VK_LBUTTON,
                                     VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
    static const QHash<int, QString> named{
        {VK_RETURN, "Enter"}, {VK_TAB, "Tab"}, {VK_ESCAPE, "Esc"}, {VK_SPACE, "Space"},
        {VK_BACK, "Backspace"}, {VK_DELETE, "Delete"}, {VK_INSERT, "Insert"}, {VK_HOME, "Home"},
        {VK_END, "End"}, {VK_PRIOR, "Page Up"}, {VK_NEXT, "Page Down"}, {VK_LEFT, "←"},
        {VK_RIGHT, "→"}, {VK_UP, "↑"}, {VK_DOWN, "↓"}, {VK_SNAPSHOT, "Print"},
        {VK_ADD, "Num +"}, {VK_SUBTRACT, "Num −"}, {VK_MULTIPLY, "Num *"}, {VK_DIVIDE, "Num /"}};
    for (const int vk : down) {
        if (m_down.contains(vk) || modifiers.contains(vk))
            continue;
        QString key;
        bool types = false; // a key that types a character
        if (vk >= VK_F1 && vk <= VK_F24)
            key = QString("F%1").arg(vk - VK_F1 + 1);
        else if (named.contains(vk))
            key = named.value(vk);
        else if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
            key = QChar(vk);
            types = true;
        } else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
            key = QString("Num %1").arg(vk - VK_NUMPAD0);
            types = true;
        } else if (const auto c = MapVirtualKeyW(UINT(vk), MAPVK_VK_TO_CHAR) & 0x7fff; c >= 0x20) {
            key = QChar(char16_t(c)).toUpper();
            types = true;
        } else
            continue;
        const bool command = (ctrl || alt || win) && !altGr;
        const bool special = !types && key != "Space" && key != "Backspace" && key != "Delete";
        const bool standalone = key == "Esc" || (key.startsWith('F') && key.size() > 1 && key[1].isDigit());
        if (!command && !standalone && !(shift && special))
            continue;
        QStringList parts;
        if (ctrl && !altGr)
            parts << "Ctrl";
        if (alt && !altGr)
            parts << "Alt";
        if (win)
            parts << "Win";
        if (shift)
            parts << "Shift";
        parts << key;
        m_keys << KeyPress{time, parts.join(" + ")};
    }
    m_down = down;
#else
    Q_UNUSED(time);
#endif
}
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
    // screens have different scalings; Qt names a screen after its display device. Where the
    // names do not match (Qt's offscreen platform), the monitor of the same number, the primary
    // one first.
    struct Monitor {
        std::wstring device;
        QRect area;
        bool primary = false;
    };
    std::vector<Monitor> monitors;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info)) {
            const auto &r = info.rcMonitor;
            reinterpret_cast<std::vector<Monitor> *>(data)->push_back(
                {info.szDevice, QRect(r.left, r.top, r.right - r.left, r.bottom - r.top),
                 (info.dwFlags & MONITORINFOF_PRIMARY) != 0});
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));
    std::stable_partition(monitors.begin(), monitors.end(), [](const Monitor &m) { return m.primary; });
    const auto name = screens[screen]->name().toStdWString();
    for (const auto &m : monitors)
        if (m.device == name && m.area.isValid())
            return m.area;
    if (size_t(screen) < monitors.size() && monitors[size_t(screen)].area.isValid())
        return monitors[size_t(screen)].area;
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
    m_pointer.clear();
    m_keys.clear();
    m_down.clear();
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
        m_pointerTimer.stop();
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
        m_pointerTimer.stop();
        m_error = "FFmpeg could not be started";
        emit finished({}, m_error);
        emit changed();
    });
    p->start(m_ffmpeg, arguments());
    m_clock.start();
    // A second try starts the video again, and the pointer with it.
    m_pointer.clear();
    m_keys.clear();
    m_down.clear();
    m_keysPrimed = false;
    m_pointerTimer.start();
    samplePointer();
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
