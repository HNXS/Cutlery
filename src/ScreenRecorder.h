#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRect>
#include <algorithm>
#include <cmath>
#include <QSet>
#include <QTimer>
#include <QVector>

class QWindow;

namespace cutlery {
// A frame that follows the mouse pointer: it stays while the pointer is in its calm middle
// (`calm` of its half-size), then eases after it at `rate` per second, along the chosen axes,
// always inside the screen. Positions are fractions of the screen; the same steps give the
// frame shown while recording and the picture's movement afterwards.
struct FollowCamera {
    double u = .5, v = .5;   // the frame's centre
    double hw = .25, hh = .25; // its half-width and half-height
    double calm = .6, rate = 1.8;
    bool alongX = true, alongY = true;
    // The frame of `height` (a fraction of the screen) and width-to-height `ratio`, on a screen
    // of width-to-height `aspect`, centred at (cu, cv); `speed` 1–10 (0: it stays), `axes`
    // "both", "horizontal" or "vertical".
    void setUp(double cu, double cv, double height, double ratio, double aspect, double speed,
               const QString &axes) {
        double h = std::min(1., height), w = h * ratio / aspect;
        if (w > 1) {
            w = 1;
            h = aspect / ratio;
        }
        hw = w / 2;
        hh = h / 2;
        rate = 0.45 * speed;
        alongX = speed > 0 && axes != "vertical";
        alongY = speed > 0 && axes != "horizontal";
        u = std::clamp(cu, hw, 1 - hw);
        v = std::clamp(cv, hh, 1 - hh);
    }
    // Moves on by `dt` seconds towards the pointer at (pu, pv).
    void step(double pu, double pv, double dt) {
        const double ease = 1 - std::exp(-rate * dt);
        auto target = [&](double c, double pointer, double half) {
            const double zone = calm * half;
            return pointer > c + zone ? pointer - zone : pointer < c - zone ? pointer + zone : c;
        };
        if (alongX)
            u = std::clamp(u + (target(u, pu, hw) - u) * ease, hw, 1 - hw);
        if (alongY)
            v = std::clamp(v + (target(v, pv, hh) - v) * ease, hh, 1 - hh);
    }
};
// Screen recording without sound: FFmpeg grabs an area of the desktop (GDI, with the mouse
// pointer) and writes H.264 through Windows Media Foundation, or lossless FFV1 where Media
// Foundation is missing, into a Matroska file that stays readable if recording is cut short.
// While recording, the mouse pointer is sampled 30 times a second (where it is and which buttons
// are down), for highlights placed over the video afterwards.
// Keyboard shortcuts are noted too: keys pressed with Ctrl, Alt or the Windows key, function
// keys, Escape, and Shift with keys that do not type (arrows, Home, Enter, ...). Plain typing,
// with or without Shift or AltGr, is never noted.
// For tests, CUTLERY_SCREEN_SOURCE=test records FFmpeg's test pattern instead of the screen,
// a pointer that circles the middle and clicks in the first tenth of every second, and
// "Ctrl + C" and "Ctrl + V" pressed in turn half a second after every whole second.
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
    // A shortcut pressed during a recording, e.g. "Ctrl + Shift + S", at seconds from its start.
    struct KeyPress {
        double time = 0;
        QString keys;
    };
    const QVector<KeyPress> &keys() const {
        return m_keys;
    }
    // Whether shortcuts are noted at all (off by default), for the next start().
    void setKeyCapture(bool on) {
        m_captureKeys = on;
    }
    // A frame that follows the mouse while recording, for the next start(): its placement as
    // fractions of the recorded area, its ratio, speed (1–10) and axes; an empty `frame` turns
    // it off. liveFrame() is where it is now, as fractions of the recorded area.
    void setFollow(const QRectF &frame, double ratio, double speed, const QString &axes);
    QRectF liveFrame() const;
    // A screen's area in physical pixels, as the desktop is captured.
    static QRect screenArea(int screen);
    // Keeps a window (the recording controls) out of screen captures, where Windows supports it
    // (Windows 10 2004 and later).
    static void hideFromCapture(QWindow *window);
  signals:
    void finished(const QString &path, const QString &error);
    void changed();
    void liveFrameChanged();

  private:
    QString m_ffmpeg, m_output, m_error;
    QPointer<QProcess> m_process;
    QElapsedTimer m_clock;
    QTimer m_pointerTimer;
    QVector<PointerSample> m_pointer;
    QVector<KeyPress> m_keys;
    QSet<int> m_down; // keys down at the last sample (Windows virtual-key codes)
    bool m_keysPrimed = false, m_captureKeys = false;
    QRectF m_followFrame;
    double m_followRatio = 0, m_followSpeed = 4;
    QString m_followAxes = "both";
    FollowCamera m_camera;
    double m_cameraTime = -1;
    void sampleKeys(double time);
    void samplePointer();
    QRect m_area;
    int m_fps = 30;
    bool m_cursor = true, m_lossless = false, m_stopping = false;
    QStringList arguments() const;
    void launch();
};
} // namespace cutlery
