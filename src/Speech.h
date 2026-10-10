#pragma once
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QVector>

namespace cutlery {
// Offline text-to-speech with Piper (a separate process, shipped in the AI pack) and its VITS
// voices (an .onnx model with its .onnx.json settings). Each spoken text is a WAV file in the
// cache, named after the voice, speed and text, so the same request is not spoken twice.
class Speech final : public QObject {
    Q_OBJECT
  public:
    struct Voice {
        QString id;       // file name without .onnx, e.g. "de_DE-thorsten-medium"
        QString name;     // "Thorsten"
        QString language; // "German"
        QString path;
        int rate = 22050;
    };
    struct Result {
        QString request, path, error;
        double seconds = 0;
    };
    Speech(QString cacheDir, QString piper, QString voicesDir, QObject *parent = nullptr);
    ~Speech() override;
    // The voices in the voices folder (read again when it changes).
    QVector<Voice> voices() const;
    // Why speech is unavailable, or empty.
    QString missing() const;
    // Queues speaking `text` (titles' *highlight* marks and line breaks are dropped) with a
    // voice at `speed` (0.5–2, 1 normal). Returns the request's id; finished() follows.
    QString speak(const QString &text, const QString &voiceId, double speed);
    bool busy() const {
        return m_process || !m_queue.isEmpty();
    }
    // Plain words of a title as they are spoken.
    static QString spokenText(const QString &text);
    // Length of a PCM WAV file in seconds, 0 when it is not one.
    static double wavSeconds(const QString &path);
  signals:
    void finished(const cutlery::Speech::Result &result);
    void changed();

  private:
    struct Job {
        QString request, text, voice, path;
        double speed = 1;
    };
    QString m_cache, m_piper, m_voicesDir;
    QVector<Job> m_queue;
    QPointer<QProcess> m_process;
    int m_serial = 0;
    void next();
};
} // namespace cutlery
