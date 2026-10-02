#pragma once
#include <QImage>
#include <QString>

namespace cutlery {
// Video scopes of a picture, as images for the viewer:
//   "histogram": red, green, blue and luma levels (shadows left, highlights right), 256x128;
//   "waveform": luma per picture column (black at the bottom, white at the top), 256x128;
//   "vectorscope": colour hue and saturation (BT.709 Cb/Cr) with the 75 % colour-bar targets
//   and the skin-tone line, 192x192.
// An unknown kind or an empty picture gives an empty scope of the right size.
QImage renderScope(const QImage &picture, const QString &kind);
} // namespace cutlery
