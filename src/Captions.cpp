#include "Captions.h"
#include <QSet>
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
namespace {
double clockSeconds(const QString &h, const QString &m, const QString &sec, const QString &fraction) {
    return h.toInt() * 3600. + m.toInt() * 60. + sec.toInt() +
           fraction.toInt() / std::pow(10., fraction.size());
}
QString cleanText(QString text) {
    text.remove('\r');
    if (text.startsWith(QChar(0xfeff)))
        text.remove(0, 1);
    return text;
}
} // namespace
QVector<Cue> parseVtt(QString text) {
    text = cleanText(text);
    if (!text.startsWith("WEBVTT"))
        throw std::runtime_error("Not a WebVTT file");
    static const QRegularExpression stamp(
        "^(?:(\\d{1,3}):)?(\\d{2}):(\\d{2})\\.(\\d{3})\\s+-->\\s+(?:(\\d{1,3}):)?(\\d{2}):(\\d{2})\\.(\\d{3})");
    static const QRegularExpression tag("<[^>]*>");
    QVector<Cue> cues;
    const auto blocks = text.split(QRegularExpression("\\n\\s*\\n"), Qt::SkipEmptyParts);
    for (const auto &block : blocks) {
        auto lines = block.split('\n');
        while (!lines.isEmpty() && lines.first().trimmed().isEmpty())
            lines.removeFirst();
        if (lines.isEmpty() || lines.first().startsWith("WEBVTT") || lines.first().startsWith("NOTE") ||
            lines.first().startsWith("STYLE") || lines.first().startsWith("REGION"))
            continue;
        // An optional identifier line before the timing.
        auto m = stamp.match(lines.first());
        if (!m.hasMatch() && lines.size() > 1) {
            lines.removeFirst();
            m = stamp.match(lines.first());
        }
        if (!m.hasMatch())
            throw std::runtime_error("Invalid WebVTT timing line");
        lines.removeFirst();
        auto body = lines.join('\n');
        body.remove(tag);
        body.replace("&lt;", "<").replace("&gt;", ">").replace("&nbsp;", " ").replace("&amp;", "&");
        body = body.trimmed();
        if (!body.isEmpty())
            cues.push_back({clockSeconds(m.captured(1), m.captured(2), m.captured(3), m.captured(4)),
                            clockSeconds(m.captured(5), m.captured(6), m.captured(7), m.captured(8)),
                            body});
    }
    return cues;
}
QVector<Cue> parseAss(QString text) {
    text = cleanText(text);
    static const QRegularExpression time("^(\\d+):(\\d{2}):(\\d{2})[.:](\\d{1,3})$");
    static const QRegularExpression overrides("\\{[^}]*\\}");
    QVector<Cue> cues;
    bool events = false;
    QStringList format;
    for (const auto &raw : text.split('\n')) {
        const auto line = raw.trimmed();
        if (line.startsWith('[')) {
            events = line.compare("[Events]", Qt::CaseInsensitive) == 0;
            continue;
        }
        if (!events)
            continue;
        if (line.startsWith("Format:")) {
            format.clear();
            for (const auto &f : line.mid(7).split(','))
                format << f.trimmed().toLower();
            continue;
        }
        if (!line.startsWith("Dialogue:"))
            continue;
        if (format.isEmpty())
            format = QStringList{"layer", "start", "end", "style", "name", "marginl",
                                 "marginr", "marginv", "effect", "text"};
        // The text is the last field and may contain commas.
        const auto fields = line.mid(9).split(',');
        if (fields.size() < format.size())
            throw std::runtime_error("Invalid ASS dialogue line");
        const auto field = [&](const QString &name) {
            const auto i = format.indexOf(name);
            if (i < 0)
                throw std::runtime_error("ASS events without " + name.toStdString());
            return i == format.size() - 1 ? fields.mid(i).join(',') : fields[i].trimmed();
        };
        const auto a = time.match(field("start")), b = time.match(field("end"));
        if (!a.hasMatch() || !b.hasMatch())
            throw std::runtime_error("Invalid ASS time");
        // Escaped braces are text; unescaped ones hold override tags.
        auto body = field("text");
        body.replace("\\{", QChar(0xe000)).replace("\\}", QChar(0xe001));
        body.remove(overrides);
        body.replace(QChar(0xe000), '{').replace(QChar(0xe001), '}');
        body.replace("\\N", "\n").replace("\\n", "\n").replace("\\h", " ");
        body = body.trimmed();
        // Centiseconds in ASS: "0:00:01.50" is 1.5 s.
        auto seconds = [](const QRegularExpressionMatch &m) {
            return clockSeconds(m.captured(1), m.captured(2), m.captured(3), m.captured(4));
        };
        if (!body.isEmpty())
            cues.push_back({seconds(a), seconds(b), body});
    }
    std::stable_sort(cues.begin(), cues.end(), [](const Cue &x, const Cue &y) { return x.start < y.start; });
    return cues;
}
QVector<Cue> parseTxt(QString text, int maxChars) {
    if (text.startsWith(QChar(0xfeff)))
        text.remove(0, 1);
    text.replace("\r\n", "\n").replace('\r', '\n');
    QVector<Cue> cues;
    double clock = 0;
    auto add = [&](const QStringList &words) {
        if (words.isEmpty())
            return;
        auto line = words.join(' ');
        if (line.size() > maxChars) {
            // Two lines, broken at the space nearest the middle.
            int best = -1;
            for (int i = 0; i < line.size(); ++i)
                if (line[i] == ' ' && (best < 0 || std::abs(i - line.size() / 2) <
                                                       std::abs(best - line.size() / 2)))
                    best = i;
            if (best > 0)
                line[best] = '\n';
        }
        const double seconds = std::clamp(words.join(' ').size() / 15.0, 1.5, 7.0);
        cues.push_back({clock, clock + seconds, line, {}});
        clock += seconds;
    };
    for (const auto &raw : text.split('\n')) {
        const auto words = raw.simplified().split(' ', Qt::SkipEmptyParts);
        QStringList part;
        int length = -1;
        for (const auto &word : words) {
            if (!part.isEmpty() && length + 1 + word.size() > 2 * maxChars) {
                add(part);
                part.clear();
                length = -1;
            }
            part << word;
            length += 1 + word.size();
            // A sentence that fills a good part of the caption ends it.
            static const QString ends = ".!?…";
            if (length >= maxChars / 2 && ends.contains(word.back())) {
                add(part);
                part.clear();
                length = -1;
            }
        }
        add(part);
    }
    return cues;
}
QVector<Cue> parseSubtitles(const QString &text, const QString &suffix) {
    const auto kind = suffix.toLower();
    if (kind == "txt")
        return parseTxt(text);
    if (kind == "vtt")
        return parseVtt(text);
    if (kind == "ass" || kind == "ssa")
        return parseAss(text);
    return parseSrt(text);
}
QString writeSubtitles(const QVector<Cue> &cues, const QString &format, int width, int height,
                       const QString &font, int fontSize) {
    auto clock = [](double seconds, int digits, QChar separator, bool shortHours) {
        const qint64 unit = digits == 3 ? 1000 : 100;
        const auto t = qRound64(std::max(0., seconds) * unit);
        return QString("%1:%2:%3%4%5")
            .arg(t / (3600 * unit), shortHours ? 1 : 2, 10, QChar('0'))
            .arg(t / (60 * unit) % 60, 2, 10, QChar('0'))
            .arg(t / unit % 60, 2, 10, QChar('0'))
            .arg(separator)
            .arg(t % unit, digits, 10, QChar('0'));
    };
    QString out;
    if (format == "vtt") {
        out = "WEBVTT\n\n";
        for (const auto &c : cues) {
            auto body = c.text;
            body.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
            // A blank line would end the cue.
            body.replace(QRegularExpression("\\n\\s*\\n"), "\n");
            out += clock(c.start, 3, '.', false) + " --> " + clock(c.end, 3, '.', false) + "\n" +
                   body + "\n\n";
        }
    } else if (format == "ass") {
        out = QString("[Script Info]\nScriptType: v4.00+\nPlayResX: %1\nPlayResY: %2\n"
                      "WrapStyle: 0\nScaledBorderAndShadow: yes\n\n"
                      "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, "
                      "SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, "
                      "StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
                      "Alignment, MarginL, MarginR, MarginV, Encoding\n"
                      "Style: Default,%3,%4,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,0,0,0,0,"
                      "100,100,0,0,1,%5,0,2,%6,%6,%7,1\n\n"
                      "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, "
                      "MarginV, Effect, Text\n")
                  .arg(width)
                  .arg(height)
                  .arg(QString(font).remove(','))
                  .arg(fontSize)
                  .arg(std::max(1, fontSize / 16))
                  .arg(width / 20)
                  .arg(height / 12);
        for (const auto &c : cues) {
            auto body = c.text;
            body.replace("{", "\\{").replace("}", "\\}").replace("\n", "\\N");
            out += "Dialogue: 0," + clock(c.start, 2, '.', true) + "," + clock(c.end, 2, '.', true) +
                   ",Default,,0,0,0,," + body + "\n";
        }
    } else {
        int i = 0;
        for (const auto &c : cues) {
            auto body = c.text;
            body.replace(QRegularExpression("\\n\\s*\\n"), "\n");
            out += QString::number(++i) + "\n" + clock(c.start, 3, ',', false) + " --> " +
                   clock(c.end, 3, ',', false) + "\n" + body + "\n\n";
        }
    }
    return out;
}
bool isFillerWord(const QString &word) {
    // Not "er" or "eh": they are German words ("he", "anyway").
    static const QSet<QString> fillers{"äh", "ähm", "ääh", "äähm", "öh",  "öhm", "ehm", "hm",
                                       "hmm", "mhm", "mm", "uh",   "uhm", "um",  "umm", "erm",
                                       "ahm"};
    QString w;
    for (const auto ch : word.toLower())
        if (ch.isLetter())
            w += ch;
    return fillers.contains(w);
}
QVector<QPair<qint64, qint64>> wordCutRanges(const QVector<qint64> &starts,
                                             const QVector<qint64> &ends, QList<int> chosen,
                                             qint64 length, qint64 pad) {
    std::sort(chosen.begin(), chosen.end());
    chosen.erase(std::unique(chosen.begin(), chosen.end()), chosen.end());
    QVector<QPair<qint64, qint64>> ranges;
    const int count = int(starts.size());
    for (int i = 0; i < chosen.size();) {
        int last = i;
        while (last + 1 < chosen.size() && chosen[last + 1] == chosen[last] + 1)
            ++last;
        const int first = chosen[i], end = chosen[last];
        if (first < 0 || end >= count) {
            i = last + 1;
            continue;
        }
        const qint64 from = starts[first];
        const qint64 to = end + 1 < count ? starts[end + 1] : std::min(length, ends[end] + pad);
        if (to > from)
            ranges.push_back({from, to});
        i = last + 1;
    }
    return ranges;
}
QVector<Cue> groupWords(const QVector<Cue> &words, int maxChars, double pause, double maxSeconds) {
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
            w.end - lines.last().start > maxSeconds ||
            (sentenceEnd.match(lines.last().text).hasMatch() &&
             lines.last().wordStarts.size() >= 2) ||
            (lines.last().text.endsWith(',') && lines.last().text.size() >= 0.6 * maxChars);
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
QString layoutCaption(const QString &text, int lines, int maxChars) {
    if (lines < 2 || text.size() <= maxChars)
        return text;
    // The break that keeps the longer line shortest; on a tie the first line stays longer.
    int best = -1, bestLongest = 0;
    for (int i = 0; i < text.size(); ++i)
        if (text[i] == ' ') {
            const int longest = std::max<int>(i, text.size() - i - 1);
            if (best < 0 || longest < bestLongest || (longest == bestLongest && i > best)) {
                best = i;
                bestLongest = longest;
            }
        }
    if (best < 0)
        return text;
    auto laid = text;
    laid[best] = '\n';
    return laid;
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
