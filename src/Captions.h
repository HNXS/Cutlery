#pragma once
#include "Project.h"
#include <QHash>
#include <QVector>

namespace cutlery {
// A subtitle cue in seconds.
struct Cue {
    double start = 0, end = 0;
    QString text;
    QVector<double> wordStarts; // per word of `text`, when known
};
// Caption lines from one-word cues: at most `maxChars` characters, broken at pauses longer than
// `pause` seconds and after sentence ends. Each line keeps its words' start times.
QVector<Cue> groupWords(const QVector<Cue> &words, int maxChars = 42, double pause = 0.6);
// SubRip (SRT) cues in file order. Throws on malformed timing; an empty text yields no cues.
QVector<Cue> parseSrt(QString text);
// WebVTT cues; headers, notes, styles and regions are skipped, tags and cue settings dropped.
QVector<Cue> parseVtt(QString text);
// Advanced SubStation Alpha (ASS/SSA) dialogue lines; override tags are dropped.
QVector<Cue> parseAss(QString text);
// Cues from a subtitle file's text by its suffix: "srt", "vtt", "ass" or "ssa".
QVector<Cue> parseSubtitles(const QString &text, const QString &suffix);
// A subtitle file for the cues: "srt", "vtt" or "ass". ASS uses the canvas size for its
// coordinates, `fontSize` (canvas pixels) and `font` for the default style.
QString writeSubtitles(const QVector<Cue> &cues, const QString &format, int width = 1920,
                       int height = 1080, const QString &font = "Arial", int fontSize = 48);
// Caption clips for spoken words: each audible clip shows the cues of its media's transcript
// (`transcripts`: cues in source seconds by asset id) that fall inside its trim, moved and
// stretched by its position and speed. Reversed and muted clips are skipped, as are cues that
// mostly overlap one already placed (the same speech on two tracks). Clips go to `track` with
// `style` ("", "karaoke", "word") and their words' timing when the cues have it.
QVector<Clip> captionClips(const Project &, const QHash<QString, QVector<Cue>> &transcripts,
                           int track, const QString &style = {});
// Clips that play audible sound from a media file, which automatic captions transcribe.
bool speaks(const Project &, const Clip &);
} // namespace cutlery
