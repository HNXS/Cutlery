#pragma once
#include "Project.h"
#include <QHash>
#include <QVector>

namespace cutlery {
// A subtitle cue in seconds.
struct Cue {
    double start = 0, end = 0;
    QString text;
};
// SubRip (SRT) cues in file order. Throws on malformed timing; an empty text yields no cues.
QVector<Cue> parseSrt(QString text);
// Caption clips for spoken words: each audible clip shows the cues of its media's transcript
// (`transcripts`: cues in source seconds by asset id) that fall inside its trim, moved and
// stretched by its position and speed. Reversed and muted clips are skipped, as are cues that
// mostly overlap one already placed (the same speech on two tracks). Clips go to `track`.
QVector<Clip> captionClips(const Project &, const QHash<QString, QVector<Cue>> &transcripts,
                           int track);
// Clips that play audible sound from a media file, which automatic captions transcribe.
bool speaks(const Project &, const Clip &);
} // namespace cutlery
