#pragma once
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRectF>
#include <QVector>

namespace cutlery {
// Follows a patch of a picture from frame to frame by normalised cross-correlation on grey
// frames: a coarse search at half size around where its motion predicts it, refined at full
// size to a fraction of a pixel. The patch's look adapts slowly so turning and lighting changes
// are followed; a poor match means the patch is lost (covered or out of the picture).
class PatchTracker {
  public:
    // `patch` in pixels of frames `width` × `height` (8-bit grey, rows packed). Patches over 64
    // pixels are tracked by their middle. False when the patch has too little detail to track.
    bool start(const uchar *frame, int width, int height, QRectF patch);
    // The patch's centre in the next frame; false (and no move) once it is lost.
    bool next(const uchar *frame);
    QPointF position() const {
        return m_position;
    }
    double score() const {
        return m_score;
    }

  private:
    int m_width = 0, m_height = 0, m_tw = 0, m_th = 0;
    QPointF m_position, m_velocity;
    double m_score = 1;
    QVector<float> m_template; // zero mean
    double m_templateNorm = 0;
    void setTemplate(const QVector<float> &patch);
    QVector<float> sample(const uchar *frame, QPointF centre) const;
    double match(const uchar *frame, int stride, int width, int height, const float *tmpl,
                 int tw, int th, double tnorm, int x, int y) const;
};

// Tracks a patch of a video file forwards and backwards from one moment with FFmpeg decoding
// small grey frames `rate` times per second of source. Points are in fractions of the source
// picture, sorted by time.
class Tracker final : public QObject {
    Q_OBJECT
  public:
    struct Point {
        double time, x, y, score;
    };
    struct Request {
        QString path;
        int width = 0, height = 0; // of the source picture, for its aspect
        double from = 0, start = 0, to = 0, rate = 25;
        QRectF patch; // fractions of the source picture at `start`
    };
    Tracker(QString ffmpeg, QObject *parent = nullptr);
    ~Tracker() override;
    void start(const Request &request);
    void cancel();
    bool busy() const {
        return m_running;
    }
    double progress() const;
  signals:
    void progressChanged();
    // `error` is empty on success; the points cover the part that could be tracked.
    void finished(const QVector<cutlery::Tracker::Point> &points, const QString &error);

  private:
    QString m_ffmpeg;
    Request m_request;
    bool m_running = false;
    int m_aw = 0, m_ah = 0;     // analysis frame size
    qint64 m_k0 = 0, m_k1 = 0;  // frame indices from the start: k0 ≤ 0 ≤ k1
    qint64 m_done = 0;          // frames processed
    bool m_backward = false;    // second pass
    qint64 m_chunkFirst = 0, m_chunkCount = 0;
    QByteArray m_buffer;
    QVector<QByteArray> m_chunkFrames;
    QByteArray m_firstFrame;
    PatchTracker m_patch;
    bool m_lost = false;
    QVector<Point> m_points;
    QPointer<QProcess> m_process;
    void runChunk(qint64 first, qint64 count);
    void frame(qint64 k, const QByteArray &data);
    void chunkDone(bool ok);
    void finish(const QString &error);
};
} // namespace cutlery
