#include "Speech.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>
#include <QtEndian>

namespace cutlery {
Speech::Speech(QString cacheDir, QString piper, QString voicesDir, QObject *parent)
    : QObject(parent), m_cache(std::move(cacheDir)), m_piper(std::move(piper)),
      m_voicesDir(std::move(voicesDir)) {}
Speech::~Speech() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}
QVector<Speech::Voice> Speech::voices() const {
    static const QHash<QString, QString> languages{
        {"de", "German"},  {"en", "English"}, {"en-us", "English"}, {"en-gb", "English"},
        {"fr", "French"},  {"es", "Spanish"}, {"it", "Italian"},    {"nl", "Dutch"},
        {"pl", "Polish"},  {"pt", "Portuguese"}};
    QVector<Voice> list;
    for (const auto &info : QDir(m_voicesDir).entryInfoList({"*.onnx"}, QDir::Files, QDir::Name)) {
        QFile file(info.filePath() + ".json");
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const auto json = QJsonDocument::fromJson(file.readAll()).object();
        // Piper's settings name the phonemes; other models in the folder have none.
        if (!json.contains("phoneme_id_map"))
            continue;
        Voice v;
        v.id = info.completeBaseName();
        v.path = info.filePath();
        v.rate = json.value("audio").toObject().value("sample_rate").toInt(22050);
        auto name = json.value("dataset").toString();
        if (name.isEmpty())
            name = v.id.section('-', 1, 1);
        if (name.isEmpty())
            name = v.id;
        name.replace('_', ' ');
        v.name = name.left(1).toUpper() + name.mid(1);
        const auto language = json.value("language").toObject();
        v.language = language.value("name_english").toString();
        if (v.language.isEmpty()) {
            const auto code = json.value("espeak").toObject().value("voice").toString();
            v.language = languages.value(code.toLower(), code);
        }
        list << v;
    }
    return list;
}
QString Speech::missing() const {
    if (m_piper.isEmpty() || !QFileInfo(m_piper).isExecutable())
        return "Text to speech needs the AI pack (Piper in its tts folder).";
    if (voices().isEmpty())
        return "Text to speech needs a voice from the AI pack in its models folder.";
    return {};
}
QString Speech::spokenText(const QString &text) {
    auto out = text;
    out.remove('*');
    out.replace(QRegularExpression("\\s+"), " ");
    return out.trimmed();
}
double Speech::wavSeconds(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    const auto data = file.read(1 << 16);
    if (data.size() < 44 || !data.startsWith("RIFF") || data.mid(8, 4) != "WAVE")
        return 0;
    quint32 byteRate = 0;
    for (qsizetype at = 12; at + 8 <= data.size();) {
        const auto id = data.mid(at, 4);
        const quint32 size = qFromLittleEndian<quint32>(data.constData() + at + 4);
        if (id == "fmt " && at + 20 <= data.size())
            byteRate = qFromLittleEndian<quint32>(data.constData() + at + 16);
        if (id == "data") {
            if (!byteRate)
                return 0;
            const qint64 available = file.size() - (at + 8);
            return double(std::min<qint64>(size, available)) / byteRate;
        }
        at += 8 + size + (size & 1);
    }
    return 0;
}
QString Speech::speak(const QString &text, const QString &voiceId, double speed) {
    const auto request = QString::number(++m_serial);
    const auto spoken = spokenText(text).left(5000);
    speed = std::clamp(speed, 0.5, 2.);
    auto fail = [this, request](const QString &error) {
        QTimer::singleShot(0, this, [this, request, error] {
            emit finished({request, {}, error, 0});
        });
        return request;
    };
    if (spoken.isEmpty())
        return fail("There is no text to speak");
    if (const auto why = missing(); !why.isEmpty())
        return fail(why);
    const auto list = voices();
    const auto voice = std::find_if(list.begin(), list.end(),
                                    [&](const Voice &v) { return v.id == voiceId; });
    if (voice == list.end())
        return fail("No such voice: " + voiceId);
    QDir().mkpath(m_cache);
    const auto key = QCryptographicHash::hash(
        (voice->id + '\n' + QString::number(speed, 'f', 3) + '\n' + spoken).toUtf8(),
        QCryptographicHash::Sha1);
    const auto path = m_cache + "/" + QString::fromLatin1(key.toHex().left(20)) + ".wav";
    if (const double seconds = wavSeconds(path); seconds > 0) {
        QTimer::singleShot(0, this, [this, request, path, seconds] {
            emit finished({request, path, {}, seconds});
        });
        return request;
    }
    m_queue << Job{request, spoken, voice->path, path, speed};
    if (!m_process)
        QTimer::singleShot(0, this, &Speech::next);
    emit changed();
    return request;
}
void Speech::next() {
    if (m_process || m_queue.isEmpty())
        return;
    const auto job = m_queue.takeFirst();
    auto *p = new QProcess(this);
    m_process = p;
    // Piper opens its files with narrow paths: the model by its plain file name from its folder,
    // and the sound comes back through standard output.
    const QFileInfo model(job.voice);
    p->setWorkingDirectory(model.absolutePath());
    auto out = std::make_shared<QByteArray>(), err = std::make_shared<QByteArray>();
    connect(p, &QProcess::readyReadStandardOutput, this, [p, out] { *out += p->readAllStandardOutput(); });
    connect(p, &QProcess::readyReadStandardError, this, [p, err] {
        *err += p->readAllStandardError();
        if (err->size() > 8192)
            *err = err->right(4096);
    });
    auto done = [this, p, out, err, job](const QString &failure) {
        if (m_process != p)
            return;
        m_process = nullptr;
        p->deleteLater();
        *out += p->readAllStandardOutput();
        Result r{job.request, {}, failure, 0};
        if (r.error.isEmpty()) {
            QFile file(job.path + ".part");
            if (!out->startsWith("RIFF") || !file.open(QIODevice::WriteOnly) ||
                file.write(*out) != out->size()) {
                r.error = "Piper gave no sound";
            } else {
                file.close();
                QFile::remove(job.path);
                if (!QFile::rename(job.path + ".part", job.path))
                    r.error = "The spoken sound could not be saved";
            }
            QFile::remove(job.path + ".part");
            if (r.error.isEmpty()) {
                r.path = job.path;
                r.seconds = wavSeconds(job.path);
                if (r.seconds <= 0)
                    r.error = "Piper gave no sound";
            }
        }
        emit finished(r);
        emit changed();
        QTimer::singleShot(0, this, &Speech::next);
    };
    connect(p, &QProcess::finished, this, [p, err, done](int code, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit || code != 0) {
            const auto lines = QString::fromUtf8(*err).trimmed().split('\n');
            done("Piper failed: " + (lines.isEmpty() ? QString::number(code) : lines.last()));
        } else {
            done({});
        }
        Q_UNUSED(p);
    });
    connect(p, &QProcess::errorOccurred, this, [done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            done("Piper could not be started");
    });
    QTimer::singleShot(300000, p, [p] {
        if (p->state() != QProcess::NotRunning)
            p->kill();
    });
    p->start(m_piper, {"--quiet", "--model", model.fileName(), "--output_file", "-",
                       "--length_scale", QString::number(1 / job.speed, 'f', 3)});
    // One line: Piper speaks each line into the output file on its own.
    p->write(job.text.toUtf8() + '\n');
    p->closeWriteChannel();
    emit changed();
}
} // namespace cutlery
