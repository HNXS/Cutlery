#include "Captions.h"
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cutlery {
QVector<Cue> parseSrt(QString text) {
    text.replace("\r", "");
    if (text.startsWith(QChar(0xfeff)))
        text.remove(0, 1);
    const auto blocks = text.split(QRegularExpression("\\n\\s*\\n"), Qt::SkipEmptyParts);
    const QRegularExpression stamp("(\\d{1,3}):(\\d{2}):(\\d{2})[,.](\\d{3})\\s*-->\\s*(\\d{1,"
                                   "3}):(\\d{2}):(\\d{2})[,.](\\d{3})");
    QVector<Cue> cues;
    for (const auto &block : blocks) {
        if (block.trimmed().isEmpty())
            continue;
        const auto m = stamp.match(block);
        if (!m.hasMatch())
            throw std::runtime_error("Invalid SRT timing block");
        auto seconds = [&](int i) {
            return m.captured(i).toInt() * 3600. + m.captured(i + 1).toInt() * 60. +
                   m.captured(i + 2).toInt() + m.captured(i + 3).toInt() / 1000.;
        };
        cues.push_back({seconds(1), seconds(5), block.mid(m.capturedEnd()).trimmed()});
    }
    return cues;
}
QVector<Cue> groupWords(const QVector<Cue> &words, int maxChars, double pause) {
    QVector<Cue> lines;
    static const QRegularExpression sentenceEnd("[.!?…]$");
    for (const auto &w : words) {
        const auto text = w.text.simplified();
        if (text.isEmpty())
            continue;
        // Punctuation on its own joins the previous word.
        static const QRegularExpression punctuation("^[,.;:!?…%)\\]»\"']+$");
        if (!lines.isEmpty() && punctuation.match(text).hasMatch()) {
            lines.last().text += text;
            lines.last().end = std::max(lines.last().end, w.end);
            continue;
        }
        const bool breakHere =
            lines.isEmpty() || w.start - lines.last().end > pause ||
            lines.last().text.size() + 1 + text.size() > maxChars ||
            (sentenceEnd.match(lines.last().text).hasMatch() &&
             lines.last().wordStarts.size() >= 2);
        if (breakHere)
            lines.push_back({w.start, w.end, text, {w.start}});
        else {
            lines.last().text += ' ' + text;
            lines.last().end = std::max(lines.last().end, w.end);
            lines.last().wordStarts << w.start;
        }
    }
    return lines;
}
bool speaks(const Project &p, const Clip &c) {
    const auto *a = p.asset(c.assetId);
    return a && a->hasAudio && !c.muted && p.audioEnabled(c.track) && c.volume > 0;
}
QVector<Clip> captionClips(const Project &p, const QHash<QString, QVector<Cue>> &transcripts,
                           int track, const QString &style) {
    struct Placed {
        double start, end;
        QString text;
        QVector<double> words; // timeline seconds
    };
    QVector<Placed> placed;
    auto clips = p.clips;
    std::stable_sort(clips.begin(), clips.end(),
                     [](const Clip &a, const Clip &b) { return a.start < b.start; });
    const double fps = double(p.fpsN) / p.fpsD;
    for (const auto &c : clips) {
        if (!speaks(p, c) || c.reverse || !transcripts.contains(c.assetId))
            continue;
        const double s = c.speed.seconds(), in = c.sourceIn.seconds(),
                     out = in + c.duration / fps * s, start = c.start / fps,
                     end = (c.start + c.duration) / fps;
        for (const auto &cue : transcripts[c.assetId]) {
            const double a = std::max(cue.start, in), b = std::min(cue.end, out);
            // Only cues mostly inside the trim: half-cut sentences read as noise.
            if (b - a < 0.25 || b - a < 0.5 * (cue.end - cue.start) || cue.text.isEmpty())
                continue;
            const double t0 = std::clamp(start + (a - in) / s, start, end),
                         t1 = std::clamp(start + (b - in) / s, start, end);
            const bool duplicate = std::any_of(placed.begin(), placed.end(), [&](const Placed &x) {
                return std::min(x.end, t1) - std::max(x.start, t0) > 0.5 * (t1 - t0);
            });
            QVector<double> words;
            for (double w : cue.wordStarts)
                words << std::clamp(start + (w - in) / s, t0, t1);
            if (!duplicate && t1 > t0)
                placed.push_back({t0, t1, cue.text, words});
        }
    }
    std::sort(placed.begin(), placed.end(),
              [](const Placed &a, const Placed &b) { return a.start < b.start; });
    QVector<Clip> result;
    for (int i = 0; i < placed.size(); ++i) {
        Clip c;
        c.id = newId();
        c.name = "Caption";
        c.track = track;
        c.start = qRound64(placed[i].start * fps);
        qint64 last = qRound64(placed[i].end * fps);
        // Captions on one track must not overlap: end where the next one starts.
        if (i + 1 < placed.size())
            last = std::min(last, qRound64(placed[i + 1].start * fps));
        c.duration = last - c.start;
        c.text = placed[i].text;
        c.fontSize = 48;
        c.y = .32;
        c.captionStyle = style;
        if (placed[i].words.size() == captionWords(c.text).size())
            for (double w : placed[i].words)
                c.wordStarts << std::clamp<qint64>(qRound64(w * fps) - c.start, 0,
                                                   std::max<qint64>(0, last - c.start - 1));
        if (c.duration > 0)
            result.push_back(c);
    }
    return result;
}
} // namespace cutlery
