#pragma once
#include "Project.h"
#include <QJsonObject>
#include <QStringList>

namespace cutlery {
// The timeline for other editors. Media clips keep their file, trim, position and speed; what
// the format cannot carry (titles, graphics, effects, colour, keyframes, transitions, clips that
// overlap on a track) is left out and described in `lost`, one line per kind of loss.

// OpenTimelineIO (.otio, JSON): every track, gaps between clips, constant speed as a time warp,
// timeline markers. Read by DaVinci Resolve, Premiere Pro (with the OTIO plug-in), Kdenlive and
// others.
QJsonObject otioTimeline(const Project &, QStringList *lost);
// CMX 3600 EDL (.edl): the lowest track with pictures, one event per media clip with the clip's
// file name as a comment; non-drop-frame timecode starting at 01:00:00:00.
QString cmxEdl(const Project &, QStringList *lost);
} // namespace cutlery
