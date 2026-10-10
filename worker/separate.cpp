// Task "separate": voice and music of a range of a media file's sound, with an MDX-Net model
// (UVR-MDX-NET-Inst_HQ_3 from the Ultimate Vocal Remover project) that predicts the music
// without the voice from its spectrogram.
//
// FFmpeg decodes the range to 44.1 kHz stereo. It is cut into segments of 255 STFT hops
// (n_fft 6144, hop 1024, 3072 frequency bins) with half a window of context on each side, as in
// UVR's MDX separation; the model's spectrogram goes back through the inverse STFT. The music is
// scaled by the model's compensation and the voice is the rest of the mix, so the two add up to
// the original exactly. The output is a 4-channel FLAC at 44.1 kHz: music left and right, then
// voice left and right, starting at the range's first source time.
//
//   cutlery-ai separate --ffmpeg F --model M --input IN --output OUT.flac [--start S]
//                       [--duration D] [--cpu 1]
#include "common.h"
#include <QFile>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

namespace {
using Complex = std::complex<float>;
constexpr int nFft = 6144, hop = 1024, dimF = 3072, dimT = 256, bins = nFft / 2 + 1;
constexpr int chunk = hop * (dimT - 1), trim = nFft / 2, gen = chunk - 2 * trim;
constexpr float compensate = 1.022f;

// Mixed-radix (2 and 3) FFT, recursive decimation in time; n = 6144 = 3 × 2^11.
class Fft {
  public:
    explicit Fft(int n) : m_n(n), m_twiddle(n) {
        for (int k = 0; k < n; ++k)
            m_twiddle[k] = std::polar(1.f, float(-2 * std::numbers::pi * k / n));
        m_scratch.resize(n);
    }
    // In place; `inverse` without the 1/n scale.
    void run(std::vector<Complex> &x, bool inverse) {
        transform(x.data(), m_scratch.data(), m_n, 1, inverse);
    }

  private:
    int m_n;
    std::vector<Complex> m_twiddle, m_scratch;
    Complex tw(int k, bool inverse) const {
        const auto t = m_twiddle[k % m_n];
        return inverse ? std::conj(t) : t;
    }
    // x holds n values at stride `stride` of the full array (in place, contiguous here);
    // `stride` is the twiddle step for this level.
    void transform(Complex *x, Complex *tmp, int n, int stride, bool inverse) {
        if (n == 1)
            return;
        const int radix = n % 3 == 0 ? 3 : 2, m = n / radix;
        // Split into `radix` interleaved sub-sequences, transform each.
        for (int r = 0; r < radix; ++r)
            for (int i = 0; i < m; ++i)
                tmp[r * m + i] = x[i * radix + r];
        for (int r = 0; r < radix; ++r)
            transform(tmp + r * m, x + r * m, m, stride * radix, inverse);
        if (radix == 2) {
            for (int k = 0; k < m; ++k) {
                const auto t = tw(k * stride, inverse) * tmp[m + k];
                x[k] = tmp[k] + t;
                x[k + m] = tmp[k] - t;
            }
        } else {
            const Complex w1 = tw(m_n / 3, inverse), w2 = tw(2 * m_n / 3, inverse);
            for (int k = 0; k < m; ++k) {
                const auto a = tmp[k], b = tw(k * stride, inverse) * tmp[m + k],
                           c = tw(2 * k * stride, inverse) * tmp[2 * m + k];
                x[k] = a + b + c;
                x[k + m] = a + w1 * b + w2 * c;
                x[k + 2 * m] = a + w2 * b + w1 * c;
            }
        }
    }
};

// torch.stft / istft with center=True (reflect padding) and a periodic Hann window.
struct Stft {
    Fft fft{nFft};
    std::vector<float> window = [] {
        std::vector<float> w(nFft);
        for (int i = 0; i < nFft; ++i)
            w[i] = float(0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / nFft));
        return w;
    }();
    std::vector<Complex> buffer = std::vector<Complex>(nFft);
    // `signal` has `chunk` samples; out[f * dimT + t] for f < dimF.
    void forward(const float *signal, std::vector<Complex> &out) {
        out.assign(size_t(dimF) * dimT, {});
        auto at = [&](int i) {
            // Reflect padding by nFft / 2 on both sides.
            i -= trim;
            if (i < 0)
                i = -i;
            if (i >= chunk)
                i = 2 * (chunk - 1) - i;
            return signal[i];
        };
        for (int t = 0; t < dimT; ++t) {
            for (int n = 0; n < nFft; ++n)
                buffer[n] = {at(t * hop + n) * window[n], 0};
            fft.run(buffer, false);
            for (int f = 0; f < dimF; ++f)
                out[size_t(f) * dimT + t] = buffer[f];
        }
    }
    // Spectrogram (dimF bins, the rest zero) back to `chunk` samples.
    void inverse(const std::vector<Complex> &spec, float *signal) {
        std::vector<float> sum(size_t(chunk) + nFft, 0), norm(size_t(chunk) + nFft, 0);
        for (int t = 0; t < dimT; ++t) {
            for (int f = 0; f < nFft; ++f)
                buffer[f] = {};
            for (int f = 0; f < dimF; ++f) {
                buffer[f] = spec[size_t(f) * dimT + t];
                if (f > 0)
                    buffer[nFft - f] = std::conj(buffer[f]);
            }
            fft.run(buffer, true);
            for (int n = 0; n < nFft; ++n) {
                sum[size_t(t) * hop + n] += buffer[n].real() / nFft * window[n];
                norm[size_t(t) * hop + n] += window[n] * window[n];
            }
        }
        for (int i = 0; i < chunk; ++i) {
            const float w = norm[size_t(i) + trim];
            signal[i] = w > 1e-11f ? sum[size_t(i) + trim] / w : 0;
        }
    }
};

// Reads up to `frames` stereo float frames; fewer only at the end of the stream.
qint64 readFrames(QProcess &p, float *data, qint64 frames) {
    const qint64 size = frames * 2 * qint64(sizeof(float));
    qint64 done = 0;
    auto *bytes = reinterpret_cast<char *>(data);
    while (done < size) {
        const auto n = p.read(bytes + done, size - done);
        if (n < 0)
            break;
        done += n;
        if (done < size && !p.waitForReadyRead(300000) && p.bytesAvailable() == 0)
            break;
    }
    return done / (2 * qint64(sizeof(float)));
}
} // namespace

int worker::separate(const QHash<QString, QString> &o) {
    const QString ffmpeg = o.value("ffmpeg"), model = o.value("model"), input = o.value("input"),
                  output = o.value("output");
    const double start = o.value("start", "0").toDouble(),
                 duration = o.value("duration", "-1").toDouble();
    if (ffmpeg.isEmpty() || model.isEmpty() || input.isEmpty() || output.isEmpty())
        return fail("usage: separate --ffmpeg F --model M --input IN --output OUT.flac "
                    "[--start S] [--duration D] [--cpu 1]");
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "cutlery-ai");
    auto session = openModel(env, model, o.value("cpu") == "1");
    Ort::AllocatorWithDefaultOptions allocator;
    const auto inName = session->GetInputNameAllocated(0, allocator),
               outName = session->GetOutputNameAllocated(0, allocator);

    QProcess decode;
    decode.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QStringList args{"-hide_banner", "-nostdin", "-v", "error", "-ss", num(start)};
    if (duration > 0)
        args << "-t" << num(duration);
    args << "-i" << input << "-map" << "0:a:0" << "-vn" << "-ac" << "2" << "-ar" << "44100"
         << "-f" << "f32le" << "pipe:1";
    decode.start(ffmpeg, args);
    if (!decode.waitForStarted())
        return fail("cannot start FFmpeg");
    QProcess encode;
    encode.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    QFile::remove(output);
    encode.start(ffmpeg, {"-hide_banner", "-nostdin", "-v", "error", "-y", "-f", "f32le", "-ar",
                          "44100", "-ch_layout", "quad", "-i", "pipe:0", "-c:a", "flac", "-sample_fmt",
                          "s32", "-f", "flac", output});
    if (!encode.waitForStarted())
        return fail("cannot start FFmpeg");

    const qint64 expected = duration > 0 ? qint64(duration * 44100) : 0;
    // The padded signal: half a window of silence, the sound, silence after its end.
    std::vector<float> buffer(size_t(trim) * 2, 0.f); // stereo interleaved
    qint64 read = 0, written = 0;
    bool ended = false;
    Stft stft;
    std::vector<float> left(chunk), right(chunk), music(size_t(chunk) * 2);
    std::vector<Complex> specL, specR;
    std::vector<float> tensor(size_t(4) * dimF * dimT);
    const std::array<int64_t, 4> shape{1, 4, dimF, dimT};
    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<float> out(size_t(gen) * 4);
    progress(0, 1000);
    while (true) {
        // Fill one segment (chunk frames) from the front of the buffer.
        while (!ended && qint64(buffer.size() / 2) < chunk) {
            const qint64 want = chunk - qint64(buffer.size() / 2);
            const size_t at = buffer.size();
            buffer.resize(at + size_t(want) * 2);
            const auto got = readFrames(decode, buffer.data() + at, want);
            buffer.resize(at + size_t(got) * 2);
            read += got;
            if (got < want)
                ended = true;
        }
        if (written >= read && ended)
            break;
        buffer.resize(std::max(buffer.size(), size_t(chunk) * 2), 0.f);
        for (int i = 0; i < chunk; ++i) {
            left[i] = buffer[size_t(i) * 2];
            right[i] = buffer[size_t(i) * 2 + 1];
        }
        stft.forward(left.data(), specL);
        stft.forward(right.data(), specR);
        const size_t plane = size_t(dimF) * dimT;
        for (size_t i = 0; i < plane; ++i) {
            tensor[i] = specL[i].real();
            tensor[plane + i] = specL[i].imag();
            tensor[2 * plane + i] = specR[i].real();
            tensor[3 * plane + i] = specR[i].imag();
        }
        // The lowest three bins carry no music for the model (as in UVR).
        for (int c = 0; c < 4; ++c)
            for (int f = 0; f < 3; ++f)
                std::fill_n(tensor.begin() + c * plane + size_t(f) * dimT, dimT, 0.f);
        auto in = Ort::Value::CreateTensor<float>(memory, tensor.data(), tensor.size(),
                                                  shape.data(), shape.size());
        const char *inNames[] = {inName.get()}, *outNames[] = {outName.get()};
        auto result = session->Run(Ort::RunOptions{nullptr}, inNames, &in, 1, outNames, 1);
        const float *pred = result[0].GetTensorData<float>();
        for (size_t i = 0; i < plane; ++i) {
            specL[i] = {pred[i], pred[plane + i]};
            specR[i] = {pred[2 * plane + i], pred[3 * plane + i]};
        }
        stft.inverse(specL, left.data());
        stft.inverse(specR, right.data());
        // The middle of the segment: original frames [written, written + gen).
        const qint64 count = std::min<qint64>(gen, read - written);
        for (qint64 i = 0; i < count; ++i) {
            const size_t s = size_t(trim + i);
            const float ml = left[s] * compensate, mr = right[s] * compensate;
            const float xl = buffer[s * 2], xr = buffer[s * 2 + 1];
            out[size_t(i) * 4] = ml;
            out[size_t(i) * 4 + 1] = mr;
            out[size_t(i) * 4 + 2] = xl - ml;
            out[size_t(i) * 4 + 3] = xr - mr;
        }
        if (count > 0 && !writeAll(encode, reinterpret_cast<const char *>(out.data()),
                                   count * 4 * qint64(sizeof(float))))
            return fail("cannot write the separated sound");
        written += count;
        // Next segment: gen frames further.
        buffer.erase(buffer.begin(), buffer.begin() + std::min(buffer.size(), size_t(gen) * 2));
        const qint64 total = std::max(expected, read);
        progress(std::min<qint64>(999, total > 0 ? written * 1000 / total : 0), 1000);
    }
    decode.waitForFinished(-1);
    encode.closeWriteChannel();
    if (!encode.waitForFinished(-1) || encode.exitStatus() != QProcess::NormalExit ||
        encode.exitCode() != 0)
        return fail("cannot encode the separated sound");
    if (read == 0)
        return fail("the range has no sound");
    progress(1000, 1000);
    return 0;
}
