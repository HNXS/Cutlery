#include "SoundLibrary.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <random>
#include <stdexcept>

namespace cutlery {
namespace {
constexpr double pi = std::numbers::pi;
struct BuiltIn {
    const char *id, *name, *category;
    double seconds;
};
// Version suffix of the file names: raise it when a sound's synthesis changes.
constexpr auto version = "v1";
constexpr BuiltIn builtIns[] = {
    {"click", "Mouse click", "Clicks", 0.25},
    {"double-click", "Double click", "Clicks", 0.4},
    {"typing-short", "Keyboard typing (2 s)", "Keyboard", 2},
    {"typing", "Keyboard typing (5 s)", "Keyboard", 5},
    {"typing-long", "Keyboard typing (10 s)", "Keyboard", 10},
    {"swoosh", "Swoosh", "Transitions", 0.7},
    {"swoosh-slow", "Swoosh (slow)", "Transitions", 1.3},
};
// RBJ band-pass biquad, re-tunable while running.
struct BandPass {
    double b0 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void tune(double frequency, double q, int rate) {
        const double w = 2 * pi * frequency / rate, alpha = std::sin(w) / (2 * q),
                     a0 = 1 + alpha;
        b0 = alpha / a0;
        b2 = -alpha / a0;
        a1 = -2 * std::cos(w) / a0;
        a2 = (1 - alpha) / a0;
    }
    double operator()(double x) {
        const double y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};
// The sound of a key or mouse button hitting its stop: a filtered noise snap, a short ring of
// the plastic and a dull thump, panned between the channels.
void hit(std::vector<double> &left, std::vector<double> &right, int rate, double at, double gain,
         double pitch, double pan, double body, std::mt19937 &random) {
    std::uniform_real_distribution<double> noise(-1, 1);
    BandPass snap;
    snap.tune(pitch, 3, rate);
    const auto first = qsizetype(at * rate);
    const auto length = qsizetype(0.06 * rate);
    const double l = std::cos((pan + 1) * pi / 4), r = std::sin((pan + 1) * pi / 4);
    for (qsizetype i = 0; i < length && first + i < qsizetype(left.size()); ++i) {
        const double t = double(i) / rate;
        const double v = 2.5 * snap(noise(random) * std::exp(-t / 0.0012)) +
                         0.35 * std::sin(2 * pi * pitch * 0.47 * t) * std::exp(-t / 0.0035) +
                         body * std::sin(2 * pi * 180 * t) * std::exp(-t / 0.008);
        left[first + i] += gain * l * v;
        right[first + i] += gain * r * v;
    }
}
} // namespace
QVector<Sound> soundLibrary(const QString &generatedDir, const QString &packDir) {
    QVector<Sound> sounds;
    for (const auto &b : builtIns) {
        Sound s;
        s.id = b.id;
        s.name = b.name;
        s.category = b.category;
        s.seconds = b.seconds;
        s.builtIn = true;
        s.licence = "Made by Cutlery: free to use without credit";
        s.source = "Synthesised by Cutlery";
        s.path = QDir(generatedDir).filePath(QString("cutlery-%1-%2.wav").arg(b.id, version));
        // Swooshes are loudest about a third of the way in.
        s.peak = QString(b.id).startsWith("swoosh") ? 0.32 * b.seconds : 0;
        sounds << s;
    }
    QFile manifest(QDir(packDir).filePath("sounds.json"));
    if (packDir.isEmpty() || !manifest.open(QIODevice::ReadOnly) || manifest.size() > 1024 * 1024)
        return sounds;
    auto json = manifest.readAll();
    if (json.startsWith("\xEF\xBB\xBF")) // a byte order mark from PowerShell
        json.remove(0, 3);
    const auto entries = QJsonDocument::fromJson(json).array();
    for (const auto &v : entries) {
        const auto o = v.toObject();
        Sound s;
        s.id = "pack:" + o["id"].toString();
        s.name = o["name"].toString();
        s.category = o["category"].toString("Recorded");
        s.seconds = o["seconds"].toDouble();
        s.peak = std::clamp(o["peak"].toDouble(), 0., s.seconds);
        s.licence = o["licence"].toString();
        s.source = o["source"].toString();
        // Only plain file names inside the pack folder.
        const auto file = o["file"].toString();
        if (o["id"].toString().isEmpty() || s.name.isEmpty() || s.licence.isEmpty() ||
            !(s.seconds > 0 && s.seconds < 3600) ||
            file.isEmpty() || file.contains('/') || file.contains('\\') || file.startsWith('.'))
            continue;
        s.path = QDir(packDir).filePath(file);
        if (QFileInfo(s.path).isFile())
            sounds << s;
    }
    return sounds;
}
QVector<float> synthesizeSound(const QString &id, int rate) {
    const auto *b = std::find_if(std::begin(builtIns), std::end(builtIns),
                                 [&](const BuiltIn &x) { return id == x.id; });
    if (b == std::end(builtIns))
        return {};
    const auto frames = size_t(std::round(b->seconds * rate));
    std::vector<double> left(frames, 0.), right(frames, 0.);
    // Seeded from the id (FNV-1a), so a sound is the same on every machine.
    uint32_t seed = 2166136261u;
    for (const auto ch : id.toUtf8())
        seed = (seed ^ uint8_t(ch)) * 16777619u;
    std::mt19937 random(seed);
    std::uniform_real_distribution<double> unit(0, 1);
    // A mouse button: the press, then a softer release about 80 ms later.
    auto click = [&](double at) {
        hit(left, right, rate, at, 1, 3400, 0, 0.25, random);
        hit(left, right, rate, at + 0.085, 0.45, 3900, 0, 0.1, random);
    };
    if (id == "click")
        click(0.005);
    else if (id == "double-click") {
        click(0.005);
        click(0.165);
    } else if (id.startsWith("typing")) {
        // Keys at an irregular pace with a short pause now and then; every few words the
        // space bar, lower and with more thump.
        double t = 0.02;
        int word = 0;
        while (t < b->seconds - 0.15) {
            const bool space = word >= 4 + int(unit(random) * 4);
            const double pitch = space ? 1300 + 300 * unit(random) : 2400 + 2200 * unit(random);
            const double pan = (unit(random) - 0.5) * 0.6;
            hit(left, right, rate, t, space ? 0.9 : 0.55 + 0.4 * unit(random), pitch, pan,
                space ? 0.6 : 0.2, random);
            hit(left, right, rate, t + 0.045 + 0.04 * unit(random), 0.25, pitch * 1.15, pan, 0.05,
                random);
            word = space ? 0 : word + 1;
            t += 0.07 + 0.13 * unit(random) + (space && unit(random) < 0.3 ? 0.35 : 0);
        }
    } else {
        // Swoosh: noise through a band-pass that sweeps up and back, rising faster than it
        // fades, moving from left to right.
        BandPass high, low;
        std::uniform_real_distribution<double> noise(-1, 1);
        for (size_t i = 0; i < frames; ++i) {
            const double u = double(i) / frames, shape = std::pow(std::sin(pi * std::pow(u, 0.6)), 2);
            if (i % 32 == 0) {
                high.tune(250 * std::pow(18., shape), 2.5, rate);
                low.tune(150 + 500 * shape, 0.8, rate);
            }
            const double n = noise(random);
            const double v = (high(n) + 0.4 * low(n)) * shape;
            const double pan = -0.7 + 1.4 * u;
            left[i] += v * std::cos((pan + 1) * pi / 4);
            right[i] += v * std::sin((pan + 1) * pi / 4);
        }
    }
    double peak = 1e-9;
    for (size_t i = 0; i < frames; ++i)
        peak = std::max({peak, std::abs(left[i]), std::abs(right[i])});
    const double gain = 0.7 / peak;
    QVector<float> out(qsizetype(frames) * 2);
    for (size_t i = 0; i < frames; ++i) {
        // A 2 ms fade at both ends, so nothing clicks where the sound starts or stops.
        const double edge = std::min({1., double(i) / (0.002 * rate), double(frames - 1 - i) / (0.002 * rate)});
        out[qsizetype(i) * 2] = float(left[i] * gain * edge);
        out[qsizetype(i) * 2 + 1] = float(right[i] * gain * edge);
    }
    return out;
}
void ensureSoundFile(const Sound &s) {
    if (!s.builtIn || QFileInfo(s.path).isFile())
        return;
    constexpr int rate = 48000;
    const auto samples = synthesizeSound(s.id, rate);
    if (samples.isEmpty())
        throw std::runtime_error("Unknown sound");
    QDir().mkpath(QFileInfo(s.path).absolutePath());
    QByteArray data(44 + samples.size() * 2, 0);
    auto put32 = [&](int at, quint32 v) { qToLittleEndian(v, data.data() + at); };
    auto put16 = [&](int at, quint16 v) { qToLittleEndian(v, data.data() + at); };
    memcpy(data.data(), "RIFF", 4);
    put32(4, quint32(data.size() - 8));
    memcpy(data.data() + 8, "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1); // PCM
    put16(22, 2);
    put32(24, rate);
    put32(28, rate * 4);
    put16(32, 4);
    put16(34, 16);
    memcpy(data.data() + 36, "data", 4);
    put32(40, quint32(samples.size() * 2));
    for (qsizetype i = 0; i < samples.size(); ++i)
        qToLittleEndian(qint16(std::lround(std::clamp(samples[i], -1.f, 1.f) * 32767)),
                        data.data() + 44 + i * 2);
    QSaveFile f(s.path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
        throw std::runtime_error("Cannot write the sound to " + s.path.toStdString());
}
} // namespace cutlery
