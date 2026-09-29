#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace cutlery {
// Exact rational edit time. The bounded project validator keeps all supported
// operations well inside int64. Overflow is still rejected at the arithmetic API.
struct Time {
    std::int64_t n = 0, d = 1;
    Time(std::int64_t numerator = 0, std::int64_t denominator = 1) {
        if (denominator <= 0 || numerator == std::numeric_limits<std::int64_t>::min())
            throw std::invalid_argument("Invalid rational time");
        const auto g = std::gcd(numerator, denominator);
        n = numerator / g;
        d = denominator / g;
    }
    double seconds() const {
        return double(n) / double(d);
    }
    static std::int64_t mul(std::int64_t a, std::int64_t b) {
        constexpr auto limit = std::numeric_limits<std::int64_t>::max();
        if (a == std::numeric_limits<std::int64_t>::min() ||
            b == std::numeric_limits<std::int64_t>::min())
            throw std::overflow_error("Time overflow");
        const auto x = a < 0 ? -a : a, y = b < 0 ? -b : b;
        if (y != 0 && x > limit / y)
            throw std::overflow_error("Time overflow");
        return a * b;
    }
    Time operator*(Time b) const {
        auto g1 = std::gcd(n, b.d), g2 = std::gcd(b.n, d);
        return {mul(n / g1, b.n / g2), mul(d / g2, b.d / g1)};
    }
    Time operator+(Time b) const {
        const auto g = std::gcd(d, b.d), a1 = mul(n, b.d / g), b1 = mul(b.n, d / g);
        if ((b1 > 0 && a1 > std::numeric_limits<std::int64_t>::max() - b1) ||
            (b1 < 0 && a1 < std::numeric_limits<std::int64_t>::min() - b1))
            throw std::overflow_error("Time overflow");
        return {a1 + b1, mul(d, b.d / g)};
    }
    bool operator==(const Time &) const = default;
};
inline Time frameTime(std::int64_t frame, int fpsN, int fpsD) {
    return Time(frame, fpsN) * Time(fpsD);
}
} // namespace cutlery
