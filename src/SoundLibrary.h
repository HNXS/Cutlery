#pragma once
#include <QString>
#include <QVector>

namespace cutlery {
// A sound effect in the library: Cutlery's own synthesised sounds, written as WAV files on first
// use, and recorded sounds from an optional pack folder described by its sounds.json.
struct Sound {
    QString id, name, category, path, licence, source;
    double seconds = 0;
    bool builtIn = false;
};
// The built-in sounds (paths in `generatedDir`, which may not exist yet) followed by the pack's.
// A pack entry is {"id", "file", "name", "category", "seconds", "licence", "source"}; entries
// whose file is missing are skipped, and a missing or broken sounds.json gives no pack sounds.
QVector<Sound> soundLibrary(const QString &generatedDir, const QString &packDir);
// Interleaved stereo samples of a built-in sound at `rate` Hz, peaking at about -3 dBFS; empty
// for an unknown id. The same id always gives the same samples.
QVector<float> synthesizeSound(const QString &id, int rate = 48000);
// Writes the built-in sound to its path (16-bit stereo WAV) unless it is there already.
// Throws when it cannot be written.
void ensureSoundFile(const Sound &);
} // namespace cutlery
