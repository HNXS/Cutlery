// Task "faces": where the faces are in a video range, for blur and mosaic areas that follow one.
//
// FFmpeg decodes the range at `--rate` frames per second (8 by default) at the given size.
// MediaPipe's short-range face detector runs on the whole frame and on half-overlapping tiles of
// 60 % and 30 % of the shorter side, so faces down to about 3 % of the frame height are found;
// the detections are merged by non-maximum suppression.
//
// The output is text, one line per face: "time x y width height score", with the time in seconds
// from the start of the range and the box in fractions of the frame (x, y the centre). A frame
// without faces writes "time" alone, so the times of empty frames are known too.
//
//   cutlery-ai faces --ffmpeg F --model face_detection_short_range.onnx --input IN --output OUT
//                    --source WxH [--rate R] [--start S] [--duration D] [--cpu 1]
#include "face.h"
#include <QFile>

int worker::faces(const QHash<QString, QString> &o) {
    using namespace worker::face;
    const QString ffmpeg = o.value("ffmpeg"), model = o.value("model"), input = o.value("input"),
                  output = o.value("output");
    const double start = o.value("start", "0").toDouble(),
                 duration = o.value("duration", "-1").toDouble(),
                 rate = o.value("rate", "8").toDouble();
    int width = 0, height = 0;
    if (ffmpeg.isEmpty() || model.isEmpty() || input.isEmpty() || output.isEmpty() ||
        !size(o.value("source"), width, height) || !(rate > 0 && rate <= 60))
        return fail("usage: faces --ffmpeg F --model M --input IN --output OUT --source WxH "
                    "[--rate R] [--start S] [--duration D]");
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "cutlery-ai");
    Detector detector(env, model, o.value("cpu") == "1");

    QProcess decoder;
    decoder.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QStringList args{"-hide_banner", "-nostdin", "-v", "error", "-ss", num(start)};
    if (duration > 0)
        args << "-t" << num(duration);
    args << "-i" << input << "-map" << "0:v:0" << "-an" << "-vf"
         << QString("fps=%1,scale=%2:%3:flags=bicubic,format=rgb24").arg(num(rate)).arg(width).arg(height)
         << "-f" << "rawvideo" << "pipe:1";
    decoder.start(ffmpeg, args);
    if (!decoder.waitForStarted())
        return fail("cannot start FFmpeg");
    QFile out(output);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return fail("cannot write the output");

    const qint64 total = duration > 0 ? qint64(std::ceil(duration * rate - 1e-6)) : 0;
    Image frame{width, height, std::vector<unsigned char>(size_t(width) * height * 3)};
    qint64 done = 0;
    while (readExactly(decoder, reinterpret_cast<char *>(frame.rgb.data()),
                       qint64(frame.rgb.size()))) {
        // The whole frame, then half-overlapping tiles of 60 % and 30 % of the shorter side for
        // smaller faces.
        auto found = detector.find(frame, {width / 2., height / 2.}, std::max(width, height));
        for (const double fraction : {0.6, 0.3}) {
            const double tile = fraction * std::min(width, height);
            const int columns = int(std::ceil((width - tile) / (tile / 2))) + 1,
                      rows = int(std::ceil((height - tile) / (tile / 2))) + 1;
            for (int row = 0; row < rows; ++row)
                for (int column = 0; column < columns; ++column) {
                    const Point centre{
                        std::min(tile / 2 + column * tile / 2, width - tile / 2),
                        std::min(tile / 2 + row * tile / 2, height - tile / 2)};
                    for (const auto &d : detector.find(frame, centre, tile))
                        found.push_back(d);
                }
        }
        found = Detector::suppress(std::move(found));
        const QString time = num(done / rate);
        if (found.empty())
            out.write((time + "\n").toUtf8());
        for (const auto &d : found)
            out.write(QString("%1 %2 %3 %4 %5 %6\n")
                          .arg(time, num(d.centre.x / width), num(d.centre.y / height),
                               num(d.width / width), num(d.height / height), num(d.score))
                          .toUtf8());
        progress(++done, total);
    }
    decoder.waitForFinished(-1);
    if (decoder.exitStatus() != QProcess::NormalExit || decoder.exitCode() != 0)
        return fail("decoding failed");
    if (done == 0)
        return fail("no video frames in range");
    return out.flush() ? 0 : fail("cannot write the output");
}
