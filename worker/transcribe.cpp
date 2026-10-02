// Task "transcribe": speech to subtitles for a range of a media file's audio with whisper.cpp.
//
// FFmpeg extracts the range as 16 kHz mono PCM; whisper-cli (a separate, pinned whisper.cpp
// build) transcribes it into SRT with one cue per word, optionally skipping
// non-speech with a Silero VAD model, which keeps Whisper from inventing text during silence and
// music. Cue times start at zero for the first source time of the range.
//
// whisper-cli uses a GPU through Vulkan when the build and driver allow it, and retries on the
// CPU if the GPU run fails.
//
//   cutlery-ai transcribe --ffmpeg F --whisper W --model M --input IN --output OUT.srt
//                         [--vad V] [--start S] [--duration D] [--language auto|de|en|...]
#include "common.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QThread>

int worker::transcribe(const QHash<QString, QString> &o) {
    const QString ffmpeg = o.value("ffmpeg"), whisper = o.value("whisper"),
                  model = o.value("model"), input = o.value("input"), output = o.value("output"),
                  vad = o.value("vad"), language = o.value("language", "auto");
    const double start = o.value("start", "0").toDouble(),
                 duration = o.value("duration", "-1").toDouble();
    static const QRegularExpression code("^(auto|[a-z]{2,3})$");
    if (ffmpeg.isEmpty() || whisper.isEmpty() || model.isEmpty() || input.isEmpty() ||
        output.isEmpty() || !code.match(language).hasMatch())
        return fail("usage: transcribe --ffmpeg F --whisper W --model M --input IN --output OUT "
                    "[--vad V] [--start S] [--duration D] [--language L]");
    const auto base = output + ".work";
    const auto wav = base + ".wav";
    struct Cleanup {
        QString wav, srt;
        ~Cleanup() {
            QFile::remove(wav);
            QFile::remove(srt);
        }
    } cleanup{wav, base + ".srt"};

    QProcess extract;
    extract.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QStringList args{"-hide_banner", "-nostdin", "-v", "error", "-y", "-ss", num(start)};
    if (duration > 0)
        args << "-t" << num(duration);
    args << "-i" << input << "-map" << "0:a:0" << "-vn" << "-ac" << "1" << "-ar" << "16000"
         << "-c:a" << "pcm_s16le" << wav;
    extract.start(ffmpeg, args);
    if (!extract.waitForFinished(-1) || extract.exitStatus() != QProcess::NormalExit ||
        extract.exitCode() != 0)
        return fail("cannot extract the audio");
    progress(2, 100);

    // Whisper scales with physical cores up to about eight; hyper-threads and more threads than
    // free cores only slow it. Machines with 8+ logical processors usually have two per core.
    const int logical = QThread::idealThreadCount();
    // One cue per word ("-ml 1" with word splits): the editor groups words into caption lines
    // and keeps each word's start for highlighting.
    QStringList w{"-m", model, "-f", wav, "-l", language, "-osrt", "-of", base, "-ml", "1",
                  "-sow", "-pp", "-t",
                  QString::number(std::clamp(logical >= 8 ? logical / 2 : logical, 1, 8))};
    if (!vad.isEmpty() && QFileInfo(vad).isFile())
        w << "--vad" << "-vm" << vad;
    std::printf("device cpu\n");
    std::fflush(stdout);
    // whisper-cli reports the GPU it uses and "progress = N%" on stderr; keep the tail for error
    // messages. Returns whether it succeeded and whether it ran on the GPU.
    static const QRegularExpression percent("progress\\s*=\\s*(\\d+)%"),
        gpu("whisper_backend_init_gpu: using (.+) backend");
    QByteArray log;
    bool started = false;
    auto run = [&](const QStringList &args, bool &onGpu) {
        QProcess speech;
        QFile::remove(base + ".srt");
        log.clear();
        onGpu = false;
        speech.start(whisper, args);
        if (!speech.waitForStarted())
            return false;
        started = true;
        int last = 2;
        QString text;
        while (speech.state() != QProcess::NotRunning) {
            speech.waitForReadyRead(1000);
            log += speech.readAllStandardError();
            speech.readAllStandardOutput();
            text = QString::fromUtf8(log);
            if (!onGpu && gpu.match(text).hasMatch()) {
                onGpu = true;
                std::printf("device gpu\n");
                std::fflush(stdout);
            }
            for (auto it = percent.globalMatch(text); it.hasNext();) {
                const int p = std::clamp(2 + it.next().captured(1).toInt() * 97 / 100, 2, 99);
                if (p > last)
                    progress(last = p, 100);
            }
            if (log.size() > 8192)
                log = log.right(4096);
        }
        speech.waitForFinished(-1);
        log += speech.readAllStandardError();
        return speech.exitStatus() == QProcess::NormalExit && speech.exitCode() == 0;
    };
    bool onGpu = false;
    bool ok = run(w, onGpu);
    if (!ok && onGpu) {
        // A GPU driver can fail where the CPU works: try again without the GPU.
        std::printf("device cpu\n");
        std::fflush(stdout);
        ok = run(QStringList(w) << "-ng", onGpu);
    }
    if (!ok) {
        if (!started)
            return fail("cannot start whisper-cli");
        const auto lines = QString::fromUtf8(log).trimmed().split('\n');
        return fail("whisper-cli failed: " + lines.mid(std::max<qsizetype>(0, lines.size() - 2))
                                                  .join(' '));
    }
    QFile::remove(output);
    if (!QFile::exists(base + ".srt") || !QFile::copy(base + ".srt", output))
        return fail("whisper-cli wrote no subtitles");
    progress(100, 100);
    return 0;
}
