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
RenderPlan compileRender(const Project &p, const QString &work, int width, int height, bool audio) {
    p.validate();
    if (p.clips.empty())
        throw std::runtime_error("The timeline is empty");
    if (width < 64 || height < 64 || width % 2 || height % 2)
        throw std::runtime_error("Invalid render dimensions");
    QDir().mkpath(work);
    RenderPlan r;
    r.duration = p.seconds();
    r.frames = p.duration();
    r.width = width;
    r.height = height;
    const QString fps = QString::number(p.fpsN) + "/" + QString::number(p.fpsD);
    QStringList nodes, audioLabels;
    QString visual = "base";
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
        const bool hasVideo = title || (asset && asset->kind != "audio");
        const bool hasAudio = audio && asset && asset->hasAudio && !c.muted;
        if ((!hasVideo || c.hidden) && !hasAudio)
            continue;
        const double duration = frameTime(c.duration, p.fpsN, p.fpsD).seconds();
        const double start = frameTime(c.start, p.fpsN, p.fpsD).seconds();
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
        if (image)
            r.inputs << "-loop" << "1" << "-framerate" << fps;
        else
            r.inputs << "-ss" << num(c.sourceIn.seconds());
        r.inputs << "-i" << QFileInfo(file).absoluteFilePath();
        const auto id = QString::number(serial++);
        if (hasVideo && !c.hidden) {
            QString f = QString("[%1:v:0]trim=duration=%2,setpts=PTS-STARTPTS")
                            .arg(input)
                            .arg(num(duration * c.speed.seconds()));
            if (c.reverse && !image)
                f += ",reverse";
            f += ",setpts=PTS/" + num(c.speed.seconds()) + ",fps=" + fps;
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
            f += ",setpts=PTS+" + num(start) + "/TB[v" + id + "]";
            nodes << f;
            const QString next = "mix" + id;
            nodes << QString("[%1][v%2]overlay=x=(W-w)/2+%3*W:y=(H-h)/"
                             "2+%4*H:eof_action=pass:repeatlast=0:format=auto:enable='gte(t,%5)*lt("
                             "t,%6)'[%7]")
                         .arg(visual, id, num(c.x), num(c.y), num(start), num(start + duration),
                              next);
            visual = next;
        }
        if (hasAudio) {
            QString a = QString("[%1:a:0]atrim=duration=%2,asetpts=PTS-STARTPTS")
                            .arg(input)
                            .arg(num(duration * c.speed.seconds()));
            if (c.reverse)
                a += ",areverse";
            double speed = c.speed.seconds();
            while (speed > 2) {
                a += ",atempo=2";
                speed /= 2;
            }
            while (speed < 0.5) {
                a += ",atempo=0.5";
                speed *= 2;
            }
            a += ",atempo=" + num(speed);
            a += ",aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo,volume=" +
                 num(c.volume);
            if (c.fadeIn > 0)
                a += ",afade=t=in:d=" + num(std::min(c.fadeIn, duration));
            if (c.fadeOut > 0) {
                auto d = std::min(c.fadeOut, duration);
                a += ",afade=t=out:st=" + num(duration - d) + ":d=" + num(d);
            }
            a += QString(",apad,atrim=duration=%1,adelay=%2S:all=1[a%3]")
                     .arg(num(duration))
                     .arg(qRound64(start * 48000))
                     .arg(id);
            nodes << a;
            audioLabels << "[a" + id + "]";
        }
        ++input;
    }
    nodes << QString("[%1]trim=end_frame=%2,setpts=PTS-STARTPTS,format=yuv420p[vout]")
                 .arg(visual)
                 .arg(r.frames);
    if (audio)
        nodes << audioLabels.join("") +
                     QString("amix=inputs=%1:duration=longest:normalize=0,alimiter=limit=0.95:"
                             "level=0:latency=1,atrim=end_sample=%2[aout]")
                         .arg(audioLabels.size())
                         .arg(qRound64(r.duration * 48000));
    r.graph = nodes.join(";\n");
    return r;
}
QStringList renderArguments(const RenderPlan &r, const QString &graph, const QString &output,
                            const QString &profile, double seek) {
    QStringList a{
        "-hide_banner", "-nostdin", "-y", "-loglevel", "error", "-filter_complex_threads", "1",
        "-threads",     "2"};
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
} // namespace cutlery
