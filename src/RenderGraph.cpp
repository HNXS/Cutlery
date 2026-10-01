#include "RenderGraph.h"
#include "ExportProfiles.h"
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QHash>
#include <QPainterPath>
#include <algorithm>
#include <stdexcept>

namespace cutlery {
static QString num(double v) {
    return QString::number(v, 'f', 9);
}
static bool animatedGeometry(const Clip &c) {
    return c.keyframes.contains("scale") || c.keyframes.contains("x") ||
           c.keyframes.contains("y") || c.keyframes.contains("rotation");
}
// FFmpeg expression for a property over clip-local frame `frame`, matching Clip::valueAt.
static QString curve(const Clip &c, const QString &property, const QString &frame) {
    const auto k = c.keyframes.value(property);
    if (k.isEmpty())
        return num(c.staticValue(property));
    QString expr = num(k.last().value);
    for (auto i = k.size() - 2; i >= 0; --i) {
        const auto u = QString("((%1-%2)/%3)").arg(frame).arg(k[i].frame).arg(
            k[i + 1].frame - k[i].frame);
        const auto eased = k[i].smooth ? QString("(%1*%1*(3-2*%1))").arg(u) : u;
        expr = QString("if(lt(%1,%2),%3+(%4)*%5,%6)")
                   .arg(frame)
                   .arg(k[i + 1].frame)
                   .arg(num(k[i].value), num(k[i + 1].value - k[i].value), eased, expr);
    }
    return QString("if(lt(%1,%2),%3,%4)").arg(frame).arg(k.first().frame).arg(
        num(k.first().value), expr);
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
    // Per-clip facts and transition handles. A transition of d frames into clip c is centred on
    // the cut: `before` frames before it, `after` frames after. The outgoing clip is extended by
    // `after` frames past its end and the incoming clip by `before` frames before its start.
    struct Info {
        const Clip *clip = nullptr;
        const Asset *asset = nullptr;
        bool title = false, image = false, video = false, audio = false;
        qint64 inLength = 0, outLength = 0;
        qint64 vPre = 0, vPost = 0, aPre = 0, aPost = 0;
        int vPrev = -1, vNext = -1;
        QString file;
    };
    auto clips = p.clips;
    std::stable_sort(clips.begin(), clips.end(),
                     [](const auto &a, const auto &b) { return a.track < b.track; });
    QVector<Info> info(clips.size());
    QHash<QString, int> index;
    for (int i = 0; i < clips.size(); ++i) {
        const auto &c = clips[i];
        auto &n = info[i];
        n.clip = &c;
        n.asset = p.asset(c.assetId);
        n.title = c.assetId.isEmpty();
        n.image = n.title || (n.asset && n.asset->kind == "image");
        n.video = o.video && (n.title || (n.asset && n.asset->kind != "audio")) && !c.audioOnly &&
                  !p.trackSettings[c.track].hidden && !c.hidden;
        n.audio = o.audio && n.asset && n.asset->hasAudio && !c.muted && p.audioEnabled(c.track);
        index.insert(c.id, i);
    }
    for (int i = 0; i < clips.size(); ++i) {
        const auto length = p.transitionLength(clips[i]);
        if (!length)
            continue;
        const int prev = index.value(p.previousAdjacent(clips[i])->id);
        const qint64 before = length / 2, after = length - before;
        info[i].inLength = length;
        info[prev].outLength = length;
        if (info[prev].video && info[i].video) {
            info[prev].vPost = after;
            info[i].vPre = before;
            info[prev].vNext = i;
            info[i].vPrev = prev;
        }
        if (info[prev].audio && info[i].audio) {
            info[prev].aPost = after;
            info[i].aPre = before;
        }
    }
    int input = 0, serial = 0;
    const double frame = secs(1), half = frame / 2;
    auto mediaFile = [&](Info &n) {
        if (!n.file.isEmpty())
            return n.file;
        const auto &c = *n.clip;
        if (n.title) {
            n.file = QDir(work).filePath(QString("title-%1.png").arg(serial++));
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
            if (!img.save(n.file))
                throw std::runtime_error("Cannot write title render asset");
        } else {
            n.file = n.asset->path;
            if (!QFileInfo(n.file).isFile())
                throw std::runtime_error(("Missing media: " + n.file).toStdString());
        }
        return n.file;
    };
    // Adds an input seeked to `seek` source seconds (images loop instead) and returns its index.
    auto addInput = [&](Info &n, double seek) {
        const auto file = mediaFile(n);
        r.inputs << "-protocol_whitelist" << "file,pipe";
        if (n.image)
            r.inputs << "-loop" << "1" << "-framerate" << fps;
        else
            r.inputs << "-ss" << num(std::max(0., seek));
        r.inputs << "-i" << QFileInfo(file).absoluteFilePath();
        return input++;
    };
    // Clip-local seconds where source media exists; images and titles never run out.
    auto available = [&](const Info &n) {
        const auto &c = *n.clip;
        if (n.image)
            return std::pair{-1e12, 1e12};
        const double s = c.speed.seconds(), in = c.sourceIn.seconds(), d = secs(c.duration),
                     media = n.asset->duration;
        return c.reverse ? std::pair{d - (media - in) / s, d + in / s}
                         : std::pair{-in / s, (media - in) / s};
    };
    // Picture of clip-local frames [l0, l1), which may reach into transition handles. Missing
    // source frames at either end hold the nearest frame. Output timestamps start at zero.
    auto videoChain = [&](Info &n, qint64 l0, qint64 l1) {
        const auto &c = *n.clip;
        const double s = c.speed.seconds(), d = secs(c.duration);
        const auto [lo, hi] = available(n);
        const double rate = double(p.fpsN) / p.fpsD;
        const qint64 first = qint64(std::ceil(lo * rate - 1e-6)),
                     last = qint64(std::floor(hi * rate + 1e-6));
        qint64 c0 = std::max(l0, first), c1 = std::min(l1, last);
        if (c1 <= c0) {
            c0 = std::clamp(l0, first, std::max(first, last - 1));
            c1 = c0 + 1;
        }
        const qint64 begin = std::min(l0, c0), padStart = std::max<qint64>(0, c0 - l0),
                     padStop = std::max<qint64>(0, l1 - c1) + 1;
        const double seek = c.sourceIn.seconds() + (c.reverse ? d - secs(c1) : secs(c0)) * s;
        const int in = addInput(n, seek);
        QString f = QString("[%1:v:0]trim=duration=%2,setpts=PTS-STARTPTS")
                        .arg(in)
                        .arg(num(secs(c1 - c0) * s));
        if (c.reverse && !n.image)
            f += ",reverse";
        // Sampling 1/8 frame late resolves exact half-frame ties (2x speed, 60 fps sources) the
        // same way regardless of where decoding started, so stills, playback and export agree.
        f += ",setpts=PTS/" + num(s) + "+" + num(frame / 8) + "/TB,fps=" + fps;
        f += QString(",trim=end_frame=%1,tpad=start=%2:stop=%3:start_mode=clone:stop_mode=clone")
                 .arg(c1 - c0)
                 .arg(padStart)
                 .arg(padStop);
        // Exact frame timestamps, clip-local and shifted by the handle so none are negative.
        const qint64 base = n.vPre;
        f += QString(",trim=start_frame=%1:end_frame=%2,settb=%3/%4,setpts=N+%5")
                 .arg(l0 - begin)
                 .arg(l0 - begin + (l1 - l0))
                 .arg(p.fpsD)
                 .arg(p.fpsN)
                 .arg(l0 + base);
        if (c.crop > 0)
            f += QString(",crop=iw*(1-2*%1):ih*(1-2*%1)").arg(num(c.crop));
        const bool moving = animatedGeometry(c);
        // Animated geometry first fits the canvas at scale 1 and is resized per frame below;
        // otherwise the clip is scaled once.
        int w = moving ? width : std::max(2, int(width * c.scale) / 2 * 2),
            h = moving ? height : std::max(2, int(height * c.scale) / 2 * 2);
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
        if (!moving && c.rotation != 0)
            f += QString(",rotate=%1*PI/180:ow=rotw(%1*PI/180):oh=roth(%1*PI/180):c=black@0")
                     .arg(num(c.rotation));
        if (c.keyframes.contains("opacity")) {
            // Opacity changes per frame through runtime commands, one per run of equal values.
            const auto name = QString("colorchannelmixer@op%1").arg(serial++);
            QStringList commands;
            auto value = [&](qint64 f) { return qRound(c.valueAt("opacity", f) * 1000) / 1000.; };
            for (qint64 f = l0; f < l1;) {
                qint64 e = f + 1;
                while (e < l1 && value(e) == value(f))
                    ++e;
                commands << QString("%1-%2 %3 aa %4")
                                .arg(num(std::max(0., secs(f + base) - half)),
                                     num(secs(e + base) - half), name,
                                     num(value(f)));
                f = e;
            }
            f += QString(",sendcmd=c='%1',%2=aa=%3")
                     .arg(commands.join(";"), name, num(value(l0)));
        } else if (c.opacity != 1)
            f += ",colorchannelmixer=aa=" + num(c.opacity);
        const double k = secs(base);
        if (c.fadeIn > 0)
            f += QString(",fade=t=in:st=%1:d=%2:alpha=1").arg(num(k), num(std::min(c.fadeIn, d)));
        if (c.fadeOut > 0) {
            const auto fd = std::min(c.fadeOut, d);
            f += QString(",fade=t=out:st=%1:d=%2:alpha=1").arg(num(k + d - fd), num(fd));
        }
        if (moving) {
            // Animated geometry: rotate within a square that fits any angle, then resize per
            // frame. Overlay positions the changing frame (see overlayPosition). All three are
            // LGPL filters evaluated per frame from the clip-local frame t*fps - base.
            const auto local = QString("(t*%1/%2-%3)").arg(p.fpsN).arg(p.fpsD).arg(base);
            if (c.rotation != 0 || c.keyframes.contains("rotation"))
                f += QString(",rotate=a='(%1)*PI/180':ow='hypot(iw,ih)':oh=ow:c=black@0")
                         .arg(curve(c, "rotation", local));
            const auto size = curve(c, "scale", local);
            f += QString(",scale=w='max(2,trunc(iw*(%1)/2)*2)':h='max(2,trunc(ih*(%1)/2)*2)':"
                         "eval=frame")
                     .arg(size);
        }
        return f + ",setpts=PTS-STARTPTS";
    };
    // `offset` is the clip-local frame shown at overlay time zero, for animated positions.
    auto overlayPosition = [&](const Clip &c, qint64 offset) {
        if (animatedGeometry(c)) {
            const auto local = QString("(t*%1/%2+%3)").arg(p.fpsN).arg(p.fpsD).arg(offset);
            return QString("x='(W-w)/2+(%1)*W':y='(H-h)/2+(%2)*H'")
                .arg(curve(c, "x", local), curve(c, "y", local));
        }
        return QString("x=(W-w)/2+%1*W:y=(H-h)/2+%2*H").arg(num(c.x), num(c.y));
    };
    // Composites a zero-based stream onto the picture for window frames [place, place + length).
    auto composite = [&](const QString &stream, const QString &position, qint64 place,
                         qint64 length) {
        const auto id = QString::number(serial++);
        nodes << stream + QString(",setpts=PTS+%1[v%2]").arg(place).arg(id);
        const QString next = "mix" + id;
        nodes << QString("[%1][v%2]overlay=%3:eof_action=pass:repeatlast=0:format=auto:"
                         "enable='gte(t,%4)*lt(t,%5)'[%6]")
                     .arg(visual, id, position, num(secs(place) - half),
                          num(secs(place + length) - half), next);
        visual = next;
    };
    QVector<bool> emitted(clips.size());
    for (int i = 0; i < clips.size(); ++i) {
        if (!info[i].video || emitted[i])
            continue;
        int head = i;
        while (info[head].vPrev >= 0)
            head = info[head].vPrev;
        QVector<int> group;
        for (int m = head; m >= 0; m = info[m].vNext) {
            group << m;
            emitted[m] = true;
        }
        const auto startOf = [&](int m) { return clips[m].start; };
        const auto endOf = [&](int m) { return clips[m].start + clips[m].duration; };
        // Render from the start of any transition the window begins inside, because xfade
        // cannot start part-way through; likewise finish any transition the window ends inside.
        qint64 gs = std::max(from, startOf(group.first())),
               ge = std::min(to, endOf(group.last()));
        if (gs >= ge)
            continue;
        for (int g = 1; g < group.size(); ++g) {
            const auto m = group[g];
            const qint64 ws = startOf(m) - info[m].vPre, we = startOf(m) + info[m].inLength -
                                                              info[m].vPre;
            if (gs > ws && gs < we)
                gs = ws;
            if (ge > ws && ge < we)
                ge = we;
        }
        QVector<int> members;
        for (int m : group)
            if (std::max(gs, startOf(m) - info[m].vPre) <
                std::min(ge, endOf(m) + info[m].vPost))
                members << m;
        const qint64 visibleStart = std::max(gs, from), visibleEnd = std::min(ge, to);
        if (members.size() == 1) {
            // Inside one clip's own time: an ordinary clip, positioned on the canvas directly.
            auto &n = info[members.first()];
            const auto &c = *n.clip;
            composite(videoChain(n, visibleStart - c.start, visibleEnd - c.start),
                      overlayPosition(c, from - c.start), visibleStart - from,
                      visibleEnd - visibleStart);
            continue;
        }
        // Each member is placed on its own transparent canvas and joined with xfade.
        QString accumulated;
        for (int g = 0; g < members.size(); ++g) {
            auto &n = info[members[g]];
            const auto &c = *n.clip;
            const qint64 r0 = std::max(gs, c.start - n.vPre),
                         r1 = std::min(ge, c.start + c.duration + n.vPost);
            const auto id = QString::number(serial++);
            nodes << QString("color=c=black@0.0:s=%1x%2:r=%3,trim=end_frame=%4,format=rgba[cv%5]")
                         .arg(width)
                         .arg(height)
                         .arg(fps)
                         .arg(r1 - r0)
                         .arg(id);
            nodes << videoChain(n, r0 - c.start, r1 - c.start) + QString("[m%1]").arg(id);
            nodes << QString("[cv%1][m%1]overlay=%2:eof_action=pass:format=auto,settb=%3/%4,"
                             "setpts=PTS-STARTPTS[mc%1]")
                         .arg(id, overlayPosition(c, r0 - c.start))
                         .arg(p.fpsD)
                         .arg(p.fpsN);
            if (g == 0) {
                accumulated = "mc" + id;
                continue;
            }
            const auto joined = "x" + id;
            nodes << QString("[%1][mc%2]xfade=transition=%3:duration=%4:offset=%5[%6]")
                         .arg(accumulated, id, c.transition, num(secs(n.inLength)),
                              num(secs(r0 - gs)), joined);
            accumulated = joined;
        }
        composite(QString("[%1]trim=start_frame=%2:end_frame=%3,setpts=PTS-STARTPTS")
                      .arg(accumulated)
                      .arg(visibleStart - gs)
                      .arg(visibleEnd - gs),
                  "x=0:y=0", visibleStart - from, visibleEnd - visibleStart);
    }
    for (int i = 0; i < clips.size(); ++i) {
        auto &n = info[i];
        if (!n.audio)
            continue;
        const auto &c = *n.clip;
        const qint64 v0 = std::max(from, c.start - n.aPre),
                     v1 = std::min(to, c.start + c.duration + n.aPost);
        if (v0 >= v1)
            continue;
        const double l0 = secs(v0 - c.start), l1 = secs(v1 - c.start);
        const auto [lo, hi] = available(n);
        const double t0 = std::max(l0, lo), t1 = std::min(l1, hi);
        if (t1 <= t0)
            continue; // Handles beyond the source are silent.
        const double s = c.speed.seconds(), d = secs(c.duration), k = secs(n.aPre);
        const int in =
            addInput(n, c.sourceIn.seconds() + (c.reverse ? d - t1 : t0) * s);
        QString a = QString("[%1:a:0]atrim=duration=%2,asetpts=PTS-STARTPTS")
                        .arg(in)
                        .arg(num((t1 - t0) * s));
        if (c.reverse)
            a += ",areverse";
        double tempo = s;
        while (tempo > 2) {
            a += ",atempo=2";
            tempo /= 2;
        }
        while (tempo < 0.5) {
            a += ",atempo=0.5";
            tempo *= 2;
        }
        a += ",atempo=" + num(tempo);
        a += ",aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo,asetpts=PTS+" +
             num(t0 + k) + "/TB";
        if (c.keyframes.contains("volume"))
            // Audio timestamps here are clip-local seconds plus the handle.
            a += QString(",volume=eval=frame:volume='%1'")
                     .arg(curve(c, "volume", QString("((t-%1)*%2/%3)")
                                                 .arg(num(k))
                                                 .arg(p.fpsN)
                                                 .arg(p.fpsD)));
        else
            a += ",volume=" + num(c.volume);
        if (c.fadeIn > 0)
            a += ",afade=t=in:st=" + num(k) + ":d=" + num(std::min(c.fadeIn, d));
        if (c.fadeOut > 0) {
            auto fd = std::min(c.fadeOut, d);
            a += ",afade=t=out:st=" + num(k + d - fd) + ":d=" + num(fd);
        }
        // Equal-power crossfades across transitions.
        if (n.aPre > 0)
            a += ",afade=t=in:st=0:d=" + num(secs(n.inLength)) + ":curve=qsin";
        if (n.aPost > 0) {
            const qint64 before = n.outLength - n.aPost;
            a += ",afade=t=out:st=" + num(k + d - secs(before)) +
                 ":d=" + num(secs(n.outLength)) + ":curve=qsin";
        }
        const auto id = QString::number(serial++);
        a += QString(",asetpts=PTS-STARTPTS,apad,atrim=duration=%1,adelay=%2S:all=1[a%3]")
                 .arg(num(l1 - t0))
                 .arg(qRound64((secs(v0 - from) + t0 - l0) * 48000))
                 .arg(id);
        nodes << a;
        audioLabels << "[a" + id + "]";
    }
    const QString pace = o.realtime ? ",realtime" : "";
    if (o.video)
        nodes << QString("[%1]trim=end_frame=%2,setpts=PTS-STARTPTS,format=%3%4[vout]")
                     .arg(visual)
                     .arg(r.frames)
                     .arg(o.pixelFormat, pace);
    if (audio)
        nodes << audioLabels.join("") +
                     QString("amix=inputs=%1:duration=longest:normalize=0,alimiter=limit=0.95:"
                             "level=0:latency=1,atrim=end_sample=%2%3[aout]")
                         .arg(audioLabels.size())
                         .arg(qRound64(r.duration * 48000))
                         .arg(o.realtime ? ",arealtime" : "");
    r.graph = nodes.join(";\n");
    if (o.highQuality)
        r.graph = "sws_flags=lanczos+accurate_rnd+full_chroma_int;\n" + r.graph;
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
QStringList exportArguments(const RenderPlan &r, const QString &graph, const QString &output,
                            const Encoder &e) {
    QStringList a{"-hide_banner", "-nostdin", "-y", "-loglevel", "error"};
    a += r.inputs;
    a << "-filter_complex_script" << graph << "-map" << "[vout]" << "-map" << "[aout]"
      << "-frames:v" << QString::number(r.frames) << "-t" << num(r.duration);
    a += e.videoArguments;
    a << "-pix_fmt" << e.pixelFormat;
    a += e.audioArguments;
    if (e.extension != "webm")
        a << "-movflags" << "+faststart";
    a << "-progress" << "pipe:1" << output;
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
