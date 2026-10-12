#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRect>
#include <QTimer>
#include <QVector>

class QWindow;

namespace cutlery {
// Screen recording without sound: FFmpeg grabs an area of the desktop (GDI, with the mouse
// pointer) and writes H.264 through Windows Media Foundation, or lossless FFV1 where Media
// Foundation is missing, into a Matroska file that stays readable if recording is cut short.
// While recording, the mouse pointer is sampled 30 times a second (where it is and which buttons
// are down), for highlights placed over the video afterwards.
// For tests, CUTLERY_SCREEN_SOURCE=test records FFmpeg's test pattern instead of the screen and
// a pointer that circles the middle and clicks in the first tenth of every second.
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
    // The pointer during a recording: the time in seconds from its start, the position as
    // fractions of the recorded area (outside 0..1 when the pointer is on another screen) and
    // the buttons down (1 left, 2 right).
    struct PointerSample {
        double time = 0, x = 0, y = 0;
        int buttons = 0;
    };
    const QVector<PointerSample> &pointer() const {
        return m_pointer;
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
    QTimer m_pointerTimer;
    QVector<PointerSample> m_pointer;
    void samplePointer();
    QRect m_area;
    int m_fps = 30;
    bool m_cursor = true, m_lossless = false, m_stopping = false;
    QStringList arguments() const;
    void launch();
};
} // namespace cutlery
