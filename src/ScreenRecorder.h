#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRect>

class QWindow;

namespace cutlery {
// Screen recording without sound: FFmpeg grabs an area of the desktop (GDI, with the mouse
// pointer) and writes H.264 through Windows Media Foundation, or lossless FFV1 where Media
// Foundation is missing, into a Matroska file that stays readable if recording is cut short.
// For tests, CUTLERY_SCREEN_SOURCE=test records FFmpeg's test pattern instead of the screen.
class ScreenRecorder final : public QObject {
    Q_OBJECT
  public:
    explicit ScreenRecorder(QString ffmpeg, QObject *parent = nullptr);
    ~ScreenRecorder() override;
    // `area` in physical screen pixels (see screenArea()); `fps` 5–60.
    bool start(const QRect &area, int fps, bool cursor, const QString &output);
    // Ends the recording; finished() follows once the file is complete.
    void stop();
    bool recording() const {
        return m_process != nullptr;
    }
    double seconds() const {
        return m_clock.isValid() ? m_clock.elapsed() / 1000. : 0;
    }
    QString error() const {
        return m_error;
    }
    // A screen's area in physical pixels, as the desktop is captured.
    static QRect screenArea(int screen);
    // Keeps a window (the recording controls) out of screen captures, where Windows supports it
    // (Windows 10 2004 and later).
    static void hideFromCapture(QWindow *window);
  signals:
    void finished(const QString &path, const QString &error);
    void changed();

  private:
    QString m_ffmpeg, m_output, m_error;
    QPointer<QProcess> m_process;
    QElapsedTimer m_clock;
    QRect m_area;
    int m_fps = 30;
    bool m_cursor = true, m_lossless = false, m_stopping = false;
    QStringList arguments() const;
    void launch();
};
} // namespace cutlery
