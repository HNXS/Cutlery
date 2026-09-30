#include "RenderGraph.h"
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <stdexcept>

namespace cutlery {
static QString num(double v) {
    return QString::number(v, 'f', 9);
}
RenderPlan compileRender(const Project &p, const QString &work, int width, int height,
                         const RenderOptions &o) {
    p.validate();
    if (p.clips.empty())
        throw std::runtime_error("The timeline is empty");
    if (width < 64 || height < 64 || width % 2 || height % 2)
        throw std::runtime_error("Invalid render dimensions");
    if (!o.video && !o.audio)
        throw std::runtime_error("Nothing to render");
    const qint64 from = o.from, to = o.to < 0 ? p.duration() : std::min(o.to, p.duration());
    if (from < 0 || from >= to)
        throw std::runtime_error("The render range is outside the timeline");
    const bool audio = o.audio;
    QDir().mkpath(work);
    RenderPlan r;
    r.frames = to - from;
    r.duration = frameTime(r.frames, p.fpsN, p.fpsD).seconds();
    r.width = width;
    r.height = height;
    const QString fps = QString::number(p.fpsN) + "/" + QString::number(p.fpsD);
    const auto secs = [&p](qint64 frames) { return frameTime(frames, p.fpsN, p.fpsD).seconds(); };
    QStringList nodes, audioLabels;
    QString visual = "base";
    if (o.video)
        nodes << QString("color=c=black:s=%1x%2:r=%3:d=%4,trim=end_frame=%5,format=rgba[base]")
                     .arg(width)
                     .arg(height)
                     .arg(fps, num(r.duration))
                     .arg(r.frames);
    if (audio) {
        nodes << QString("anullsrc=r=48000:cl=stereo,atrim=end_sample=%1[asilence]")
                     .arg(qRound64(r.duration * 48000));
        audioLabels << "[asilence]";
    }
    auto clips = p.clips;
    std::stable_sort(clips.begin(), clips.end(),
                     [](const auto &a, const auto &b) { return a.track < b.track; });
    int input = 0, serial = 0;
    for (const auto &c : clips) {
        const auto *asset = p.asset(c.assetId);
        const bool title = c.assetId.isEmpty();
        const bool image = title || (asset && asset->kind == "image");
        const bool hasVideo = o.video && (title || (asset && asset->kind != "audio")) &&
                              !c.audioOnly && !p.trackSettings[c.track].hidden;
        const bool hasAudio =
            audio && asset && asset->hasAudio && !c.muted && p.audioEnabled(c.track);
        if ((!hasVideo || c.hidden) && !hasAudio)
            continue;
        // Visible part of the clip inside the window, in clip-local and window-local time.
        const qint64 visibleStart = std::max(c.start, from),
                     visibleEnd = std::min(c.start + c.duration, to);
        if (visibleStart >= visibleEnd)
            continue;
        const double duration = secs(c.duration), offset = secs(visibleStart - c.start),
                     length = secs(visibleEnd - visibleStart), place = secs(visibleStart - from);
        const double speed = c.speed.seconds();
        // Frame boundaries are compared half a frame early, away from decimal rounding.
        const double half = secs(1) / 2;
        // A reversed clip shows its source backwards, so the window starts later in the source.
        const double sourceStart =
            c.sourceIn.seconds() + (c.reverse ? duration - offset - length : offset) * speed;
        QString file;
        if (title) {
            file = QDir(work).filePath(QString("title-%1.png").arg(serial));
            QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QPainter paint(&img);
            paint.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            QFont font(c.fontFamily);
            font.setPixelSize(std::max(8, qRound(c.fontSize * double(height) / p.height)));
            font.setBold(true);
            paint.setFont(font);
            const QRect rect(width / 15, height / 12, width * 13 / 15, height * 5 / 6);
            paint.setPen(QColor(0, 0, 0, 210));
            paint.drawText(rect.translated(2, 3), Qt::AlignCenter | Qt::TextWordWrap, c.text);
            paint.setPen(QColor(c.textColor));
            paint.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, c.text);
            paint.end();
            if (!img.save(file))
                throw std::runtime_error("Cannot write title render asset");
        } else {
            file = asset->path;
            if (!QFileInfo(file).isFile())
                throw std::runtime_error(("Missing media: " + file).toStdString());
        }
        r.inputs << "-protocol_whitelist" << "file,pipe";
        if (image)
            r.inputs << "-loop" << "1" << "-framerate" << fps;
        else
            r.inputs << "-ss" << num(sourceStart);
        r.inputs << "-i" << QFileInfo(file).absoluteFilePath();
        const auto id = QString::number(serial++);
        if (hasVideo && !c.hidden) {
            QString f = QString("[%1:v:0]trim=duration=%2,setpts=PTS-STARTPTS")
                            .arg(input)
                            .arg(num(length * speed));
            if (c.reverse && !image)
                f += ",reverse";
            // Clip-local timestamps keep fades anchored to the clip, not to the window. Sampling
            // 1/8 frame late resolves exact half-frame ties (2x speed, 60 fps sources) the same way
            // regardless of where decoding started, so stills, playback and export agree.
            f += ",setpts=PTS/" + num(speed) + "+" + num(offset + secs(1) / 8) + "/TB,fps=" + fps;
            if (c.crop > 0)
                f += QString(",crop=iw*(1-2*%1):ih*(1-2*%1)").arg(num(c.crop));
            int w = std::max(2, int(width * c.scale) / 2 * 2),
                h = std::max(2, int(height * c.scale) / 2 * 2);
            f += QString(",scale=%1:%2:force_original_aspect_ratio=decrease,setsar=1,format=rgba")
                     .arg(w)
                     .arg(h);
            if (c.flip)
                f += ",hflip";
            if (c.brightness != 0 || c.contrast != 1) {
                const QString expr = QString("clip((val-128)*%1+128+%2,0,255)")
                                         .arg(num(c.contrast), num(c.brightness * 255));
                f += QString(",lutrgb=r='%1':g='%1':b='%1'").arg(expr);
            }
            if (c.saturation != 1)
                f += ",hue=s=" + num(c.saturation);
            if (c.rotation != 0)
                f += QString(",rotate=%1*PI/180:ow=rotw(%1*PI/180):oh=roth(%1*PI/180):c=none")
                         .arg(num(c.rotation));
            if (c.opacity != 1)
                f += ",colorchannelmixer=aa=" + num(c.opacity);
            if (c.fadeIn > 0)
                f += QString(",fade=t=in:st=0:d=%1:alpha=1").arg(num(std::min(c.fadeIn, duration)));
            if (c.fadeOut > 0) {
                const auto d = std::min(c.fadeOut, duration);
                f += QString(",fade=t=out:st=%1:d=%2:alpha=1").arg(num(duration - d), num(d));
            }
            // setpts truncates; round so e.g. 1/3 s lands on frame 10, not 9.
            f += ",setpts=round(PTS-STARTPTS+" + num(place) + "/TB)[v" + id + "]";
            nodes << f;
            const QString next = "mix" + id;
            nodes << QString("[%1][v%2]overlay=x=(W-w)/2+%3*W:y=(H-h)/"
                             "2+%4*H:eof_action=pass:repeatlast=0:format=auto:enable='gte(t,%5)*lt("
                             "t,%6)'[%7]")
                         .arg(visual, id, num(c.x), num(c.y), num(place - half),
                              num(place + length - half), next);
            visual = next;
        }
        if (hasAudio) {
            QString a = QString("[%1:a:0]atrim=duration=%2,asetpts=PTS-STARTPTS")
                            .arg(input)
                            .arg(num(length * speed));
            if (c.reverse)
                a += ",areverse";
            double tempo = speed;
            while (tempo > 2) {
                a += ",atempo=2";
                tempo /= 2;
            }
            while (tempo < 0.5) {
                a += ",atempo=0.5";
                tempo *= 2;
            }
            a += ",atempo=" + num(tempo);
            a += ",aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo,volume=" +
                 num(c.volume) + ",asetpts=PTS+" + num(offset) + "/TB";
            if (c.fadeIn > 0)
                a += ",afade=t=in:d=" + num(std::min(c.fadeIn, duration));
            if (c.fadeOut > 0) {
                auto d = std::min(c.fadeOut, duration);
                a += ",afade=t=out:st=" + num(duration - d) + ":d=" + num(d);
            }
            a += QString(",asetpts=PTS-STARTPTS,apad,atrim=duration=%1,adelay=%2S:all=1[a%3]")
                     .arg(num(length))
                     .arg(qRound64(place * 48000))
                     .arg(id);
            nodes << a;
            audioLabels << "[a" + id + "]";
        }
        ++input;
    }
    const QString pace = o.realtime ? ",realtime" : "";
    if (o.video)
        nodes << QString("[%1]trim=end_frame=%2,setpts=PTS-STARTPTS,format=yuv420p%3[vout]")
                     .arg(visual)
                     .arg(r.frames)
                     .arg(pace);
    if (audio)
        nodes << audioLabels.join("") +
                     QString("amix=inputs=%1:duration=longest:normalize=0,alimiter=limit=0.95:"
                             "level=0:latency=1,atrim=end_sample=%2%3[aout]")
                         .arg(audioLabels.size())
                         .arg(qRound64(r.duration * 48000))
                         .arg(o.realtime ? ",arealtime" : "");
    r.graph = nodes.join(";\n");
    return r;
}
QStringList renderArguments(const RenderPlan &r, const QString &graph, const QString &output,
                            const QString &profile, double seek) {
    QStringList a{"-hide_banner", "-nostdin", "-y", "-loglevel", "error"};
    // A single still is cheap; keep it from competing with playback or export for every core.
    if (seek >= 0)
        a << "-filter_complex_threads" << "1";
    a += r.inputs;
    a << "-filter_complex_script" << graph << "-map" << "[vout]";
    if (seek >= 0) {
        a << "-ss" << num(seek) << "-frames:v" << "1" << "-c:v" << "png" << "-f" << "image2pipe"
          << "pipe:1";
        return a;
    }
    a << "-map" << "[aout]" << "-frames:v" << QString::number(r.frames) << "-t" << num(r.duration);
    if (profile == "webm")
        a << "-c:v" << "libvpx-vp9" << "-crf" << "30" << "-b:v" << "0" << "-deadline" << "realtime"
          << "-cpu-used" << "6" << "-c:a" << "libopus" << "-b:a" << "160k";
    else if (profile == "h264")
        a << "-c:v" << "h264_mf" << "-b:v" << "12000k" << "-c:a" << "aac" << "-b:a" << "192k"
          << "-movflags" << "+faststart";
    else
        a << "-c:v" << "mpeg4" << "-q:v" << "3" << "-c:a" << "aac" << "-b:a" << "192k"
          << "-movflags" << "+faststart";
    a << "-pix_fmt" << "yuv420p" << "-progress" << "pipe:1" << output;
    return a;
}
QStringList streamArguments(const RenderPlan &r, const QString &graph, bool video,
                            bool floatAudio) {
    QStringList a{"-hide_banner", "-nostdin", "-loglevel", "error"};
    a += r.inputs;
    a << "-filter_complex_script" << graph;
    if (video)
        a << "-map" << "[vout]" << "-frames:v" << QString::number(r.frames) << "-f" << "rawvideo"
          << "-pix_fmt" << "yuv420p";
    else
        a << "-map" << "[aout]" << "-f" << (floatAudio ? "f32le" : "s16le") << "-ar" << "48000" << "-ac" << "2";
    a << "pipe:1";
    return a;
}
} // namespace cutlery
