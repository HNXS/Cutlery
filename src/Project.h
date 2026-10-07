#pragma once
#include "RationalTime.h"
#include <QJsonObject>
#include <QMap>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace cutlery {
QString newId();
struct Track {
    QString name;
    bool locked = false, muted = false, hidden = false, solo = false;
    bool snapping = true, magnetic = false;
    QString id = newId();
};
struct Asset {
    QString id, path, name, kind; // video, audio, image
    double duration = 0;
    int width = 0, height = 0;
    bool hasAudio = false;
    // Video only: the average frame rate, and whether frames arrive at irregular intervals
    // (variable frame rate, typical of phone and screen recordings).
    double frameRate = 0;
    bool variableRate = false;
    // Animated GIFs repeat, so their clips can be any length.
    bool loops = false;
    // The media library folder it is in; empty for the top level.
    QString folder;
    // A nested sequence: the project it holds (absolute media paths). Its picture and sound are
    // rendered by the editor into `path`, a cache file named after this content.
    QJsonObject nested;
    bool isNested() const {
        return !nested.isEmpty();
    }
    // Images and looping animations have no end; their clips are not limited by the source.
    bool endless() const {
        return kind == "image" || loops;
    }
};
// Variable frame rate: the nominal and average rates of a stream differ by more than 1 %.
bool isVariableRate(double nominal, double average);
// A standard frame rate close to `rate` (23.976 … 60), or `rate` itself when none is near.
double standardRate(double rate);
// A property value at a clip-local frame. Smooth keyframes ease in and out towards the next one;
// others interpolate linearly.
struct Keyframe {
    qint64 frame = 0;
    double value = 0;
    bool smooth = true;
    bool operator==(const Keyframe &) const = default;
};
struct Clip {
    QString id, assetId, name;
    // Clips with the same link (picture and its detached sound) move and trim together while
    // they stay aligned; "none" marks a pair the user unlinked.
    QString link;
    // Clips with the same group are selected, moved and deleted together.
    QString group;
    int track = 0;
    qint64 start = 0, duration = 1;
    Time sourceIn, speed{1};
    bool muted = false, hidden = false, reverse = false, flip = false, audioOnly = false;
    double scale = 1, x = 0, y = 0, rotation = 0, opacity = 1, volume = 1;
    // The point of the picture that scaling and rotation keep in place, as fractions of its
    // width and height (0.5, 0.5: the centre).
    double anchorX = 0.5, anchorY = 0.5;
    double brightness = 0, contrast = 1, saturation = 1, crop = 0;
    // Exposure in stops (−3..3): light multiplied by 2^exposure, applied before contrast.
    double exposure = 0;
    // Crop of single edges, as fractions of the source width (left, right) and height (top,
    // bottom), before the equal-edge crop; each pair leaves at least a tenth of the picture.
    double cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;
    // Upside down (flip mirrors left and right).
    bool flipVertical = false;
    // How the picture mixes with what the lower tracks show: "" normal, or one of
    // blendModes() (FFmpeg blend mode names).
    QString blendMode;
    double fadeIn = 0, fadeOut = 0;
    QString text, fontFamily = "Arial", textColor = "#ffffff";
    int fontSize = 72;
    // Text style: weight and slant, alignment ("left", "center", "right"), letter spacing as a
    // fraction of the font size, line spacing as a multiple of the line height, an outline
    // (width as a fraction of the font size), a drop shadow (0..1) and a box behind each line
    // (opacity 0..1).
    bool bold = true, italic = false;
    QString align = "center";
    double letterSpacing = 0, lineSpacing = 1;
    double outline = 0, textShadow = 1, background = 0;
    // A soft glow around the letters, 0..1 (reach about half the font size at 1).
    double textGlow = 0;
    QString textGlowColor = "#ffd23f";
    QString outlineColor = "#000000", backgroundColor = "#000000";
    // Captions with word timing: the clip-local frame each word of `text` (split at white
    // space) starts on. "karaoke" colours the word being spoken in `highlightColor`; "word"
    // shows one word at a time. Without matching timing a caption renders plainly.
    QString captionStyle; // "", "karaoke", "word"
    // Title templates: "lowerThird" (name and role on a plate), "lowerThirdLine" (accent line,
    // no plate), "titleCard" (large centred text on a plate); "" is the plain centred title.
    // The first text line is the name or headline, further lines the role or subtitle.
    QString titleStyle;
    QString accentColor = "#64d8bc";
    // Plain titles can build up: "typewriter" shows one character after another, "words" one
    // word after another, all of them within textAnimationTime seconds from the clip's start.
    QString textAnimation;
    double textAnimationTime = 1.5;
    // A second text colour: the letters fade from textColor at the top to this at the bottom.
    QString gradientColor;
    // Effect area clips (no media, no text) blur or pixelate whatever lower tracks show inside a
    // rectangle of effectWidth × effectHeight canvas fractions at scale 1, centred at x/y.
    // "", "blur", "pixelate" (an area of the picture below), "adjust" (an adjustment layer:
    // the clip's colour and look apply to everything below it, by its opacity)
    QString effect;
    double effectStrength = 0.6, effectWidth = 0.3, effectHeight = 0.2;
    // Graphic clips (no media): a shape of graphicWidth × graphicHeight canvas fractions at
    // scale 1, centred at x/y, filled with fillColor and outlined with strokeColor at `stroke`
    // (fraction of the canvas height). Arrows and lines point right; rotate them. Speech bubbles
    // and boxes show the clip's text inside.
    QString graphic; // "", "rectangle", "ellipse", "arrow", "line", "bubble"
    QString fillColor = "#ffd23f", strokeColor = "#000000";
    double stroke = 0, graphicWidth = 0.3, graphicHeight = 0.2;
    // Gaussian blur of the clip's own picture, 0..1.
    double blur = 0;
    // In-between frames when the clip plays slower than its source: "" repeats frames, "blend"
    // cross-fades neighbours, "flow" interpolates motion (slow to render).
    QString slowMotion;
    // Colour: warmer/cooler, magenta/green, gentle saturation of muted colours, and lifted or
    // lowered shadows and highlights, each −1..1 with 0 unchanged.
    double temperature = 0, tint = 0, vibrance = 0, shadows = 0, highlights = 0;
    // Sound: three-band EQ in dB (−12..12; low shelf 100 Hz, peak 2.5 kHz, high shelf 8 kHz), a
    // low cut in Hz (0 off, up to 300), and 0..1 amounts of compression, noise gate, noise
    // reduction and de-essing.
    double eqLow = 0, eqMid = 0, eqHigh = 0, lowCut = 0;
    double compressor = 0, gate = 0, denoise = 0, deess = 0;
    // Style effect: "", "shake", "glitch", "vhs" or "film", at fxStrength (0..1). Motion blur
    // blends successive frames (0..1); stabilize smooths camera shake.
    QString fx;
    double fxStrength = 0.5, motionBlur = 0;
    bool stabilize = false;
    // How far stabilizing may move the picture (0..1: about 16 to 64 pixels of the source), and
    // whether it zooms in that far so no mirrored edge shows.
    double stabilizeStrength = 0.33;
    bool stabilizeZoom = false;
    // Room reverb and a distinct echo, 0..1.
    double reverb = 0, echo = 0;
    // Pitch in semitones (−12..12) without changing the tempo.
    double pitch = 0;
    double pan = 0; // -1 left .. 1 right
    // Look: 0..1, 0 off.
    double sharpen = 0, glow = 0, vignette = 0, grain = 0;
    // A 3D LUT file (.cube or .3dl) mixed in at lutStrength (0..1). Missing files are skipped.
    QString lut;
    // Tone curves as FFmpeg curve points "x/y x/y ..." (0..1, x rising); empty is unchanged.
    QString curveMaster, curveRed, curveGreen, curveBlue;
    // Selective colour: hue shift (degrees), saturation and lightness (-1..1) of the colours in
    // hslColors ("r y g c b m", any of them; empty means all colours).
    QString hslColors;
    double hslHue = 0, hslSaturation = 0, hslLightness = 0;
    double lutStrength = 1;
    // The look settings above (for copy and paste of attributes).
    static const QStringList &lookProperties();
    QString highlightColor = "#ffd23f";
    QVector<qint64> wordStarts;
    // Word timing usable for the caption style: one start per word of the text.
    bool timedWords() const;
    // Transition from the clip that ends exactly where this one starts on the same track. It is
    // centred on the cut; both clips extend into the other's time using source handles or a held
    // frame, so the timeline length does not change.
    QString transition; // an xfade name from transitionTypes(); empty for a straight cut
    qint64 transitionFrames = 0;
    // Animated properties (see animatableProperties()), sorted by frame. Frames are relative to
    // the clip start and stay attached to the picture when the clip is trimmed or split. A
    // property with keyframes ignores its static value.
    QMap<QString, QVector<Keyframe>> keyframes;
    // Overlay styling for picture-in-picture (presenter) layouts.
    QString shape = "rect"; // rect, rounded, circle (centre square)
    double radius = 0.12;   // rounded corners, fraction of the shorter side
    double border = 0;      // border width, fraction of the canvas height at scale 1
    QString borderColor = "#ffffff";
    double shadow = 0; // soft drop shadow strength, 0..1
    // Soft edge of the overlay's shape, as a fraction of the picture's shorter side (0..0.5).
    double feather = 0;
    // Background removal by colour (green/blue screen).
    bool chromaKey = false;
    QString keyColor = "#00ff00";
    double keySimilarity = 0.25, keyBlend = 0.08;
    // Background removal by brightness: "dark" keys out black, "light" white, within
    // lumaTolerance (0.01..1) and a soft edge of lumaSoftness (0..1).
    QString lumaKey;
    double lumaTolerance = 0.1, lumaSoftness = 0.05;
    // Background removal with the AI person matte of the asset (see AiJobs). Takes precedence
    // over the colour key; without an analysed matte the picture stays as it is.
    bool aiCutout = false;
    // Picture from the AI-upscaled copy of the asset (see AiJobs) when one covers the clip.
    bool aiUpscale = false;
    // Picture from the eye-contact copy of the asset (see AiJobs), which replaces the upscaled
    // one when both are on.
    bool eyeContact = false;
    bool styled() const {
        return shape != "rect" || border > 0 || shadow > 0 || aiCutout || feather > 0;
    }
    double staticValue(const QString &property) const;
    // Property value at a clip-local frame, interpolating keyframes when present.
    double valueAt(const QString &property, double frame) const;
    void shiftKeyframes(qint64 delta);
    void scaleKeyframes(double factor);
};
const QStringList &animatableProperties();
// Clip::graphic values.
const QStringList &graphicKinds();
// A tone curve as FFmpeg's curves filter takes it: 2–16 points "x/y", 0..1, x rising.
bool validCurve(const QString &points);
// Words of a caption text, as used with Clip::wordStarts.
QStringList captionWords(const QString &text);
// Blend modes other than normal: FFmpeg blend names paired with display labels.
const QVector<QPair<QString, QString>> &blendModes();
// Supported transitions: FFmpeg xfade names paired with display labels, plus Cutlery's own
// "spin", "glitch" and "lightleak", which dissolve and add their effect around the cut.
const QVector<QPair<QString, QString>> &transitionTypes();
// A named point on the timeline.
struct Marker {
    qint64 frame = 0;
    QString name, color = "#ffd23f";
    bool operator==(const Marker &) const = default;
};
struct Project {
    QString name = "Untitled";
    // Timeline markers, sorted by frame; at most one per frame.
    QVector<Marker> markers;
    // In and out points of a range for export (frames; -1 unset). The range is [inPoint,
    // outPoint), from the timeline start or to its end when one is unset.
    qint64 inPoint = -1, outPoint = -1;
    int width = 1920, height = 1080, fpsN = 30, fpsD = 1, tracks = 3;
    QVector<Track> trackSettings{{"Track 1"}, {"Track 2"}, {"Track 3"}};
    QVector<Asset> assets;
    // Media library folders, in the order shown; assets name theirs in Asset::folder.
    QStringList folders;
    QVector<Clip> clips;
    qint64 duration() const;
    double seconds() const {
        return frameTime(duration(), fpsN, fpsD).seconds();
    }
    const Asset *asset(const QString &id) const;
    Clip *clip(const QString &id);
    const Clip *clip(const QString &id) const {
        return const_cast<Project *>(this)->clip(id);
    }
    // Size of a clip's picture fitted into a box at scale 1, before styling. Circles use the
    // centre square; equal-edge crop keeps the aspect ratio.
    QSizeF pictureSize(const Clip &c, double boxWidth, double boxHeight) const;
    // How far the anchor point moves the picture's centre, in canvas pixels, for a picture of
    // `base` size at scale 1 shown at `scale` and `rotation` degrees (clockwise).
    static QPointF anchorShift(const Clip &c, QSizeF base, double scale, double rotation);
    // The clip a transition into `c` comes from, or nullptr when `c` does not start at a cut.
    const Clip *previousAdjacent(const Clip &c) const;
    // Effective transition length into `c` in frames (0 when inactive), limited by both clips.
    qint64 transitionLength(const Clip &c) const;
    QJsonObject json(const QString &baseDir = {}) const;
    static Project fromJson(const QJsonObject &json, const QString &baseDir);
    void validate() const;
    bool split(const QString &id, qint64 frame);
    void remove(const QString &id, bool ripple);
    // Cuts clip-local frame ranges out of a clip and closes each gap on its track (ripple), as
    // when removing pauses. Ranges are clamped to the clip; returns the frames removed.
    qint64 cutRanges(const QString &id, QVector<QPair<qint64, qint64>> ranges);
    // Clips on other tracks that play the same media at the same time and range as `id`, such
    // as audio detached from it.
    QStringList linkedClips(const QString &id) const;
    void requireEditable(int track) const;
    bool audioEnabled(int track) const;
    void addTrack(const QString &name = {});
    void removeTrack(int track);
    void trim(const QString &id, qint64 start, qint64 end);
    // Edits that keep the clips around them in place (the caller validates the result):
    // Slip shows a later (positive) or earlier part of the source in the same place and length;
    // detached audio of the clip slips with it.
    void slip(const QString &id, qint64 frames);
    // Roll moves the cut between this clip and the one right after it on its track.
    void roll(const QString &id, qint64 frames);
    // Slide moves the clip; the touching clips before and after it grow or shrink to match.
    void slide(const QString &id, qint64 frames);
    QVector<QString> trackOrder(int track, const QString &exclude = {}) const;
    void packTrack(int track, const QVector<QString> &order);
    qint64 placement(int track, qint64 frame, const QString &exclude = {}) const;
    void move(const QString &id, int track, qint64 frame);
    qint64 snap(qint64 frame, qint64 threshold, const QString &exclude, qint64 playhead,
                qint64 length = 0) const;
};
QString newId();
QString readUtf8File(const QString &path);
void saveProject(const Project &project, const QString &path);
Project loadProject(const QString &path);
} // namespace cutlery
