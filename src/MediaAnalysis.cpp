#include "MediaAnalysis.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <memory>

namespace cutlery {
namespace {
// In-place radix-2 FFT; the size is a power of two.
void fft(std::vector<std::complex<double>> &a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const auto w = std::polar(1., -2 * std::numbers::pi / double(len));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> u(1);
            for (size_t k = 0; k < len / 2; ++k, u *= w) {
                const auto x = a[i + k], y = a[i + k + len / 2] * u;
                a[i + k] = x + y;
                a[i + k + len / 2] = x - y;
            }
        }
    }
}
} // namespace
QVector<double> detectBeats(const QVector<float> &samples, int rate, double *bpm) {
    if (bpm)
        *bpm = 0;
    // Windows of about 46 ms, every 12 ms at 11025 Hz.
    const int size = rate > 16000 ? 1024 : 512, hop = size / 4;
    const qsizetype frames = samples.size() < size ? 0 : (samples.size() - size) / hop + 1;
    const double frameRate = double(rate) / hop;
    if (frames < 2 * frameRate)
        return {};
    // Onset strength: the summed increase of log-compressed spectral magnitudes.
    std::vector<double> window(size), previous(size / 2, 0.), onset(frames, 0.);
    for (int i = 0; i < size; ++i)
        window[i] = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / size);
    std::vector<std::complex<double>> buffer(size);
    for (qsizetype f = 0; f < frames; ++f) {
        for (int i = 0; i < size; ++i)
            buffer[i] = samples[f * hop + i] * window[i];
        fft(buffer);
        double flux = 0;
        for (int k = 1; k < size / 2; ++k) {
            const double m = std::log1p(100 * std::abs(buffer[k]));
            flux += std::max(0., m - previous[k]);
            previous[k] = m;
        }
        onset[f] = f == 0 ? 0 : flux;
    }
    // Remove the slow trend (about ±0.2 s), keep the rises, and normalise.
    const int reach = int(0.2 * frameRate);
    std::vector<double> prefix(frames + 1, 0.);
    for (qsizetype f = 0; f < frames; ++f)
        prefix[f + 1] = prefix[f] + onset[f];
    std::vector<double> envelope(frames);
    double energy = 0;
    for (qsizetype f = 0; f < frames; ++f) {
        const qsizetype a = std::max<qsizetype>(0, f - reach), b = std::min(frames, f + reach + 1);
        envelope[f] = std::max(0., onset[f] - (prefix[b] - prefix[a]) / double(b - a));
        energy += envelope[f] * envelope[f];
    }
    const double rms = std::sqrt(energy / frames);
    if (rms <= 1e-9)
        return {};
    for (auto &e : envelope)
        e /= rms;
    // Tempo: the autocorrelation lag between 60 and 200 BPM, weighted towards 120 BPM.
    const int shortest = int(std::floor(frameRate * 60 / 200)), longest = int(std::ceil(frameRate));
    std::vector<double> correlation(longest + 2, 0.);
    for (int lag = shortest - 1; lag <= longest + 1 && lag < frames; ++lag) {
        double sum = 0;
        for (qsizetype f = lag; f < frames; ++f)
            sum += envelope[f] * envelope[f - lag];
        correlation[lag] = sum / double(frames - lag);
    }
    int best = 0;
    double bestScore = 0;
    for (int lag = shortest; lag <= longest && lag + 1 < frames; ++lag) {
        const double octaves = std::log2(lag / (frameRate * 0.5));
        const double score = correlation[lag] * std::exp(-0.5 * octaves * octaves);
        if (score > bestScore) {
            bestScore = score;
            best = lag;
        }
    }
    if (best == 0)
        return {};
    // A fractional period from the peak's neighbours.
    double period = best;
    {
        const double l = correlation[best - 1], c = correlation[best], r = correlation[best + 1];
        const double curve = l - 2 * c + r;
        if (curve < 0)
            period += std::clamp(0.5 * (l - r) / curve, -0.5, 0.5);
    }
    if (bpm)
        *bpm = 60 * frameRate / period;
    // Beats: the best chain of strong onsets about one period apart.
    const double tightness = 100;
    std::vector<double> score(frames);
    std::vector<qsizetype> back(frames, -1);
    for (qsizetype f = 0; f < frames; ++f) {
        double from = 0;
        const qsizetype lo = f - qsizetype(std::round(2 * period)),
                        hi = f - qsizetype(std::round(period / 2));
        for (qsizetype j = std::max<qsizetype>(0, lo); j <= hi; ++j) {
            const double gap = std::log((f - j) / period);
            const double value = score[j] - tightness * gap * gap;
            if (back[f] < 0 || value > from) {
                from = value;
                back[f] = j;
            }
        }
        score[f] = envelope[f] + (back[f] >= 0 ? std::max(0., from) : 0.);
        if (back[f] >= 0 && from <= 0)
            back[f] = -1;
    }
    // The chain ends at the best score in the last period.
    qsizetype last = std::max<qsizetype>(0, frames - qsizetype(std::ceil(period)));
    for (qsizetype f = last; f < frames; ++f)
        if (score[f] > score[last])
            last = f;
    std::vector<qsizetype> chain;
    for (qsizetype f = last; f >= 0; f = back[f])
        chain.push_back(f);
    std::reverse(chain.begin(), chain.end());
    // Each beat moves to the strongest onset close by (within about 35 ms).
    std::vector<double> strength(chain.size());
    for (size_t i = 0; i < chain.size(); ++i) {
        const qsizetype f = chain[i];
        for (qsizetype g = std::max<qsizetype>(0, f - 3); g <= std::min(frames - 1, f + 3); ++g)
            if (envelope[g] > envelope[chain[i]] || (envelope[g] == envelope[chain[i]] && g == f))
                chain[i] = g;
        strength[i] = envelope[chain[i]];
    }
    // Leading and trailing beats much weaker than the typical one (silence, noise, intros and
    // fade-outs) are dropped.
    auto sorted = strength;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const double typical = sorted.empty() ? 0 : sorted[sorted.size() / 2];
    auto weak = [&](size_t i) { return strength[i] < std::max(0.5, 0.2 * typical); };
    size_t from = 0, to = chain.size();
    while (from < to && weak(from))
        ++from;
    while (to > from && weak(to - 1))
        --to;
    // The onset of frame f is centred about two thirds into its window.
    QVector<double> beats;
    for (size_t i = from; i < to; ++i)
        beats << (chain[i] * hop + size * 2. / 3) / rate;
    return beats;
}
PeakAccumulator::PeakAccumulator(qint64 samplesPerPeak)
    : m_binSamples(std::max(qint64(1), samplesPerPeak)) {}
void PeakAccumulator::append(const QByteArray &bytes) {
    QByteArray data = m_tail + bytes;
    const auto even = data.size() / 2 * 2;
    for (qsizetype i = 0; i < even; i += 2) {
        const auto sample =
            qFromLittleEndian<qint16>(reinterpret_cast<const uchar *>(data.constData() + i));
        m_peak = std::max(m_peak, float(std::abs(int(sample))) / 32768.f);
        if (++m_count == m_binSamples) {
            if (m_peaks.size() < 6001)
                m_peaks.push_back(m_peak);
            m_count = 0;
            m_peak = 0;
        }
    }
    m_tail = data.mid(even);
}
QVector<float> PeakAccumulator::finish() {
    if (m_count && m_peaks.size() < 6001)
        m_peaks.push_back(m_peak);
    m_count = 0;
    return m_peaks;
}
MediaAnalysis::MediaAnalysis(QString cache, QString ffmpeg, QObject *parent)
    : QObject(parent), m_cache(std::move(cache)), m_ffmpeg(std::move(ffmpeg)) {
    QDir().mkpath(m_cache);
}
MediaAnalysis::~MediaAnalysis() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1500);
    }
}
QString MediaAnalysis::fingerprint(const Asset &a) {
    const QFileInfo f(a.path);
    const auto data = f.canonicalFilePath().toUtf8() + '\0' + QByteArray::number(f.size()) + '\0' +
                      QByteArray::number(f.lastModified().toMSecsSinceEpoch()) + '\0' +
                      QByteArray::number(a.duration, 'g', 17) + "peak-v1";
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
bool MediaAnalysis::load(const QString &id, const QString &key) {
    QFile f(m_cache + '/' + key + ".json");
    if (!f.open(QIODevice::ReadOnly) || f.size() > 100000)
        return false;
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
    const auto a = o["peaks"].toArray();
    const auto step = o["step"].toDouble();
    if (o["version"].toInt() != 1 || a.isEmpty() || a.size() > 6001 || !std::isfinite(step) ||
        step <= 0)
        return false;
    QVector<float> peaks;
    for (const auto &v : a) {
        const auto n = v.toDouble(-1);
        if (!std::isfinite(n) || n < 0 || n > 1)
            return false;
        peaks.push_back(float(n));
    }
    m_waves[id] = {key, "ready", peaks, step};
    return true;
}
void MediaAnalysis::save(const QString &id) {
    const auto w = m_waves.value(id);
    QJsonArray peaks;
    for (float n : w.peaks)
        peaks.append(n);
    QSaveFile f(m_cache + '/' + w.key + ".json");
    if (!f.open(QIODevice::WriteOnly))
        return;
    const auto data = QJsonDocument(QJsonObject{{"version", 1}, {"step", w.step}, {"peaks", peaks}})
                          .toJson(QJsonDocument::Compact);
    if (f.write(data) == data.size())
        f.commit();
}
void MediaAnalysis::setAssets(const QVector<Asset> &assets) {
    QHash<QString, Asset> nextAssets;
    bool updated = false;
    for (const auto &a : assets) {
        if (!a.hasAudio)
            continue;
        nextAssets.insert(a.id, a);
        const auto key = fingerprint(a);
        if (m_waves.contains(a.id) && m_waves.value(a.id).key == key)
            continue;
        updated = true;
        m_queue.removeAll(a.id);
        if (!load(a.id, key)) {
            m_waves[a.id] = {key, "queued", {}, 0};
            m_queue.push_back(a.id);
        }
    }
    for (const auto &id : m_waves.keys())
        if (!nextAssets.contains(id)) {
            m_waves.remove(id);
            m_queue.removeAll(id);
            updated = true;
        }
    m_assets = std::move(nextAssets);
    if (updated)
        emit changed();
    next();
}
QVariantMap MediaAnalysis::waveform(const QString &id) const {
    const auto w = m_waves.value(id);
    QVariantList peaks;
    peaks.reserve(w.peaks.size());
    for (float n : w.peaks)
        peaks.push_back(n);
    return {{"status", w.status}, {"step", w.step}, {"peaks", peaks}};
}
void MediaAnalysis::setPaused(bool paused) {
    m_paused = paused;
    if (!paused)
        next();
}
void MediaAnalysis::next() {
    if (m_process || m_paused)
        return;
    while (!m_queue.empty() && !m_assets.contains(m_queue.first()))
        m_queue.removeFirst();
    if (m_queue.empty())
        return;
    const auto id = m_queue.takeFirst();
    const auto a = m_assets.value(id);
    const auto key = m_waves.value(id).key;
    if (a.duration <= 0 || a.duration > 86400 || !QFileInfo(a.path).isFile() ||
        m_ffmpeg.isEmpty()) {
        m_waves[id].status = "unavailable";
        emit changed();
        QTimer::singleShot(0, this, &MediaAnalysis::next);
        return;
    }
    const qint64 bin = std::max(qint64(80), qint64(std::ceil(a.duration * 8000 / 6000)));
    auto peaks = std::make_shared<PeakAccumulator>(bin);
    auto *p = new QProcess(this);
    m_process = p;
    m_current = id;
    m_waves[id].status = "reading";
    connect(p, &QProcess::readyReadStandardOutput, this,
            [p, peaks] { peaks->append(p->readAllStandardOutput()); });
    connect(p, &QProcess::readyReadStandardError, this, [p] { p->readAllStandardError(); });
    auto complete = [this, p, peaks, id, key, bin](bool success) {
        peaks->append(p->readAllStandardOutput());
        m_process = nullptr;
        m_current.clear();
        p->deleteLater();
        if (m_waves.contains(id) && m_waves.value(id).key == key) {
            auto data = peaks->finish();
            m_waves[id] = {key, success && !data.empty() ? "ready" : "unavailable",
                           success ? data : QVector<float>{}, double(bin) / 8000};
            if (success && !data.empty())
                save(id);
            emit changed();
        }
        QTimer::singleShot(0, this, &MediaAnalysis::next);
    };
    connect(p, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        complete(code == 0 && status == QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            complete(false);
    });
    QTimer::singleShot(120000, p, [p] {
        if (p->state() != QProcess::NotRunning)
            p->kill();
    });
    p->start(m_ffmpeg, {"-hide_banner", "-nostdin", "-v", "error", "-threads", "1",
                        "-protocol_whitelist", "file,pipe", "-i", a.path, "-vn", "-ac", "1", "-ar",
                        "8000", "-f", "s16le", "pipe:1"});
    emit changed();
}
} // namespace cutlery
