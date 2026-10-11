// Task "people": who is in a video or picture, as face descriptors that can be compared, for
// finding the same person again across media.
//
// FFmpeg decodes the range at `--rate` frames per second (2 by default; a picture gives one
// frame). MediaPipe's face detector (BlazeFace, as in the faces task) finds the faces; each one
// of at least 32 pixels is turned upright and scaled by its eyes, nose and mouth to the 112 × 112
// layout SFace expects (a similarity transform fitted to the reference points of the OpenCV
// face recogniser), and SFace (Zhong et al., MobileFaceNet trained with the SFace loss,
// Apache-2.0, from the OpenCV model zoo) gives a 128-number descriptor, scaled to length 1. Two
// faces of the same person have descriptors pointing the same way: their dot product (cosine
// similarity) is above about 0.36.
//
// The output is text, one line per face: "time x y width height score d0 ... d127", the time in
// seconds from the start of the range, the box in fractions of the frame (x, y its centre). A
// frame without faces writes "time" alone.
//
//   cutlery-ai people --ffmpeg F --model face_recognition_sface.onnx --input IN --output OUT
//                     --source WxH [--rate R] [--start S] [--duration D] [--still 1] [--cpu 1]
// face_detection_short_range.onnx sits beside the SFace model. --still reads a picture.
#include "face.h"
#include <QFile>

namespace {
using namespace worker::face;
// Where the eyes, nose tip and mouth centre sit in SFace's 112 × 112 input (OpenCV's reference
// points; the mouth is the middle of its two corners).
constexpr std::array<Point, 4> kReference{
    Point{38.2946, 51.6963}, Point{73.5318, 51.5014}, Point{56.0252, 71.7366}, Point{56.1396, 92.2848}};

// The similarity transform (scale, rotation, shift) taking `from` onto `to` in the least-squares
// sense, as a, b, tx, ty with x' = a x - b y + tx, y' = b x + a y + ty.
std::array<double, 4> similarity(const std::array<Point, 4> &from, const std::array<Point, 4> &to) {
    Point mf, mt;
    for (size_t i = 0; i < from.size(); ++i) {
        mf = mf + from[i] * (1. / from.size());
        mt = mt + to[i] * (1. / to.size());
    }
    double sxx = 0, sxy = 0, norm = 0;
    for (size_t i = 0; i < from.size(); ++i) {
        const Point f = from[i] - mf, t = to[i] - mt;
        sxx += f.x * t.x + f.y * t.y;
        sxy += f.x * t.y - f.y * t.x;
        norm += f.x * f.x + f.y * f.y;
    }
    const double a = norm > 0 ? sxx / norm : 1, b = norm > 0 ? sxy / norm : 0;
    return {a, b, mt.x - (a * mf.x - b * mf.y), mt.y - (b * mf.x + a * mf.y)};
}
} // namespace

int worker::people(const QHash<QString, QString> &o) {
    const QString ffmpeg = o.value("ffmpeg"), model = o.value("model"), input = o.value("input"),
                  output = o.value("output");
    const double start = o.value("start", "0").toDouble(),
                 duration = o.value("duration", "-1").toDouble(),
                 rate = o.value("rate", "2").toDouble();
    const bool still = o.value("still") == "1";
    int width = 0, height = 0;
    if (ffmpeg.isEmpty() || model.isEmpty() || input.isEmpty() || output.isEmpty() ||
        !size(o.value("source"), width, height) || !(rate > 0 && rate <= 30))
        return fail("usage: people --ffmpeg F --model M --input IN --output OUT --source WxH "
                    "[--rate R] [--start S] [--duration D] [--still 1]");
    const auto detectorPath = QFileInfo(model).dir().filePath("face_detection_short_range.onnx");
    if (!QFileInfo(detectorPath).isFile())
        return fail("face_detection_short_range.onnx is missing beside the face model");
    // OpenCV's SFace export lists its weights as graph inputs, which ONNX Runtime warns about
    // once per weight; only errors are worth showing.
    Ort::Env env(ORT_LOGGING_LEVEL_ERROR, "cutlery-ai");
    Detector detector(env, detectorPath, o.value("cpu") == "1");
    auto recogniser = openModel(env, model, o.value("cpu") == "1");
    Ort::AllocatorWithDefaultOptions allocator;
    // The picture input: "data" in OpenCV's export, else the first one.
    std::string inputName = recogniser->GetInputNameAllocated(0, allocator).get();
    for (size_t i = 0; i < recogniser->GetInputCount(); ++i)
        if (std::string(recogniser->GetInputNameAllocated(i, allocator).get()) == "data")
            inputName = "data";
    const std::string outputName = recogniser->GetOutputNameAllocated(0, allocator).get();
    const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    QProcess decoder;
    decoder.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QStringList args{"-hide_banner", "-nostdin", "-v", "error"};
    if (!still) {
        args << "-ss" << num(start);
        if (duration > 0)
            args << "-t" << num(duration);
    }
    args << "-i" << input << "-map" << "0:v:0" << "-an" << "-vf"
         << (still ? QString("scale=%1:%2:flags=bicubic,format=rgb24").arg(width).arg(height)
                   : QString("fps=%1,scale=%2:%3:flags=bicubic,format=rgb24").arg(num(rate)).arg(width).arg(height))
         << "-frames:v" << (still ? "1" : "1000000") << "-f" << "rawvideo" << "pipe:1";
    decoder.start(ffmpeg, args);
    if (!decoder.waitForStarted())
        return fail("cannot start FFmpeg");
    QFile out(output);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return fail("cannot write the output");

    const qint64 total = still ? 1 : duration > 0 ? qint64(std::ceil(duration * rate - 1e-6)) : 0;
    Image frame{width, height, std::vector<unsigned char>(size_t(width) * height * 3)};
    std::vector<float> face(size_t(3) * 112 * 112);
    const std::array<int64_t, 4> shape{1, 3, 112, 112};
    qint64 done = 0;
    while (readExactly(decoder, reinterpret_cast<char *>(frame.rgb.data()),
                       qint64(frame.rgb.size()))) {
        const QString time = num(done / rate);
        int written = 0;
        for (const auto &d : detector.findAll(frame)) {
            if (d.score < 0.7 || std::min(d.width, d.height) < 32)
                continue;
            // Upright and scaled by its eyes, nose and mouth; sampled through the inverse map.
            const std::array<Point, 4> marks{d.keypoints[0], d.keypoints[1], d.keypoints[2],
                                             d.keypoints[3]};
            const auto [a, b, tx, ty] = similarity(marks, kReference);
            const double det = a * a + b * b;
            if (det <= 0)
                continue;
            for (int v = 0; v < 112; ++v)
                for (int u = 0; u < 112; ++u) {
                    const double px = u - tx, py = v - ty;
                    const double x = (a * px + b * py) / det, y = (-b * px + a * py) / det;
                    for (int c = 0; c < 3; ++c)
                        face[size_t(c) * 112 * 112 + size_t(v) * 112 + u] = float(frame.at(x, y, c));
                }
            auto tensor = Ort::Value::CreateTensor<float>(memory, face.data(), face.size(),
                                                          shape.data(), shape.size());
            const char *inNames[] = {inputName.c_str()};
            const char *outNames[] = {outputName.c_str()};
            auto result = recogniser->Run(Ort::RunOptions{nullptr}, inNames, &tensor, 1, outNames, 1);
            const float *v = result[0].GetTensorData<float>();
            const auto count = result[0].GetTensorTypeAndShapeInfo().GetElementCount();
            double length = 0;
            for (size_t i = 0; i < count; ++i)
                length += double(v[i]) * v[i];
            length = std::sqrt(length);
            if (length <= 0)
                continue;
            QStringList line{time, num(d.centre.x / width), num(d.centre.y / height),
                             num(d.width / width), num(d.height / height), num(d.score)};
            for (size_t i = 0; i < count; ++i)
                line << QString::number(v[i] / length, 'f', 5);
            out.write((line.join(' ') + "\n").toUtf8());
            ++written;
        }
        if (!written)
            out.write((time + "\n").toUtf8());
        progress(++done, total);
    }
    decoder.waitForFinished(-1);
    if (decoder.exitStatus() != QProcess::NormalExit || decoder.exitCode() != 0)
        return fail("decoding failed");
    if (done == 0)
        return fail("no video frames in range");
    return out.flush() ? 0 : fail("cannot write the output");
}
