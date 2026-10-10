#pragma once
// A SentencePiece unigram tokenizer that reads the .spm model files of Marian/Opus-MT models: the
// pieces and their scores from the model's protobuf, NFKC normalisation and whitespace handling
// as the "nmt_nfkc" rule does for ordinary text, and the most likely segmentation by Viterbi.
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstring>
#include <limits>
#include <string>

namespace worker {
class SentencePiece {
  public:
    bool load(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        const auto data = file.readAll();
        const auto *p = reinterpret_cast<const uchar *>(data.constData());
        const auto *end = p + data.size();
        m_score.clear();
        m_longest = 1;
        double lowest = 0;
        // ModelProto: field 1 is a repeated SentencePiece {1: piece, 2: score, 3: type}.
        while (p < end) {
            quint64 key = 0;
            if (!varint(p, end, key))
                return false;
            const int field = int(key >> 3), wire = int(key & 7);
            if (wire == 2) {
                quint64 length = 0;
                if (!varint(p, end, length) || length > quint64(end - p))
                    return false;
                if (field == 1)
                    readPiece(p, p + length, lowest);
                p += length;
            } else if (!skip(p, end, wire)) {
                return false;
            }
        }
        m_unknown = float(lowest - 10);
        return !m_score.isEmpty();
    }
    // Pieces of `text`, e.g. "▁Gut", "en", "▁Morgen".
    QStringList encode(const QString &text) const {
        // Normalised, whitespace collapsed, a "▁" before every word.
        const auto words = text.normalized(QString::NormalizationForm_KC)
                               .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (words.isEmpty())
            return {};
        const auto joined = QString(QChar(0x2581)) + words.join(QChar(0x2581));
        const auto chars = joined.toUcs4();
        const int n = int(chars.size());
        // Scores add up in single precision, as in SentencePiece, so near-ties resolve alike.
        QVector<float> best(n + 1, -std::numeric_limits<float>::infinity());
        QVector<int> from(n + 1, -1);
        best[0] = 0;
        for (int i = 0; i < n; ++i) {
            if (best[i] == -std::numeric_limits<float>::infinity())
                continue;
            bool single = false;
            for (int len = 1; len <= m_longest && i + len <= n; ++len) {
                const auto piece = QString::fromUcs4(chars.constData() + i, len);
                const auto it = m_score.constFind(piece);
                if (it == m_score.constEnd())
                    continue;
                single |= len == 1;
                if (best[i] + *it > best[i + len]) {
                    best[i + len] = best[i] + *it;
                    from[i + len] = i;
                }
            }
            // A character no piece covers is one unknown piece.
            if (!single && best[i] + m_unknown > best[i + 1]) {
                best[i + 1] = best[i] + m_unknown;
                from[i + 1] = i;
            }
        }
        QStringList pieces;
        for (int i = n; i > 0; i = from[i]) {
            const auto piece = QString::fromUcs4(chars.constData() + from[i], i - from[i]);
            // Unknown characters in a row make one unknown piece.
            if (!pieces.isEmpty() && !m_score.contains(piece) && !m_score.contains(pieces.first()))
                pieces.first().prepend(piece);
            else
                pieces.prepend(piece);
        }
        return pieces;
    }
    bool contains(const QString &piece) const {
        return m_score.contains(piece);
    }

  private:
    QHash<QString, float> m_score;
    int m_longest = 1;
    float m_unknown = -20;
    static bool varint(const uchar *&p, const uchar *end, quint64 &value) {
        value = 0;
        for (int shift = 0; p < end && shift < 64; shift += 7) {
            const uchar b = *p++;
            value |= quint64(b & 0x7f) << shift;
            if (!(b & 0x80))
                return true;
        }
        return false;
    }
    static bool skip(const uchar *&p, const uchar *end, int wire) {
        quint64 ignored = 0;
        if (wire == 0)
            return varint(p, end, ignored);
        if (wire == 1 || wire == 5) {
            const int size = wire == 1 ? 8 : 4;
            if (end - p < size)
                return false;
            p += size;
            return true;
        }
        if (wire == 2) {
            if (!varint(p, end, ignored) || ignored > quint64(end - p))
                return false;
            p += ignored;
            return true;
        }
        return false;
    }
    void readPiece(const uchar *p, const uchar *end, double &lowest) {
        QString piece;
        float score = 0;
        quint64 type = 1;
        while (p < end) {
            quint64 key = 0;
            if (!varint(p, end, key))
                return;
            const int field = int(key >> 3), wire = int(key & 7);
            if (field == 1 && wire == 2) {
                quint64 length = 0;
                if (!varint(p, end, length) || length > quint64(end - p))
                    return;
                piece = QString::fromUtf8(reinterpret_cast<const char *>(p), qsizetype(length));
                p += length;
            } else if (field == 2 && wire == 5 && end - p >= 4) {
                std::memcpy(&score, p, 4);
                p += 4;
            } else if (field == 3 && wire == 0) {
                if (!varint(p, end, type))
                    return;
            } else if (!skip(p, end, wire)) {
                return;
            }
        }
        // Normal and user-defined pieces take part; unknown, control and byte pieces do not.
        if (type != 1 && type != 4)
            return;
        m_score.insert(piece, score);
        m_longest = std::max(m_longest, int(piece.toUcs4().size()));
        lowest = std::min(lowest, double(score));
    }
};
} // namespace worker
