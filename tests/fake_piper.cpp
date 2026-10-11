// A stand-in for Piper in the tests: reads one line of text and writes a WAV to standard output
// whose length follows the text, a tenth of a second per character times --length_scale, as a
// 1000 Hz tone. Lets dubbing be tested without a voice model.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static void put32(std::vector<unsigned char> &out, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<unsigned char>(v >> (8 * i)));
}
static void put16(std::vector<unsigned char> &out, uint16_t v) {
    out.push_back(static_cast<unsigned char>(v));
    out.push_back(static_cast<unsigned char>(v >> 8));
}

int main(int argc, char **argv) {
    double scale = 1;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--length_scale") == 0)
            scale = std::atof(argv[i + 1]);
    std::string line;
    for (int ch; (ch = std::getchar()) != EOF && ch != '\n';)
        line += static_cast<char>(ch);
    if (line.empty())
        return 1;
    const uint32_t rate = 16000;
    const auto samples = static_cast<uint32_t>(line.size() * 0.1 * scale * rate);
    std::vector<unsigned char> out;
    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    put32(out, 36 + samples * 2);
    out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put32(out, 16);
    put16(out, 1);
    put16(out, 1);
    put32(out, rate);
    put32(out, rate * 2);
    put16(out, 2);
    put16(out, 16);
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    put32(out, samples * 2);
    for (uint32_t i = 0; i < samples; ++i)
        put16(out, static_cast<uint16_t>(static_cast<int16_t>(8000 * std::sin(i * 2 * 3.14159265358979 * 1000 / rate))));
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    return std::fwrite(out.data(), 1, out.size(), stdout) == out.size() ? 0 : 1;
}
