#include "format.hpp"

#include <cmath>
#include <cstdint>

namespace bh {

void append_fixed2(std::string& out, double v) {
    // prod + err is exactly v * 100 (fma gives the exact rounding error).
    double prod = v * 100.0;
    double err = std::fma(v, 100.0, -prod);
    double lower = std::floor(prod);

    double n;
    if (prod - lower == 0.5) {
        // prod sits exactly on a half; the exact value's side is given by err.
        if (err > 0.0) {
            n = lower + 1.0;
        } else if (err < 0.0) {
            n = lower;
        } else {
            n = std::fmod(lower, 2.0) == 0.0 ? lower : lower + 1.0;
        }
    } else {
        // Rounding is monotonic, so prod and the exact value cannot straddle
        // a representable half: rounding prod gives the same integer.
        n = std::nearbyint(prod);
    }

    int64_t units = static_cast<int64_t>(n);
    if (units < 0) {
        out.push_back('-');
        units = -units;
    }
    int64_t whole = units / 100;
    int frac = static_cast<int>(units % 100);

    char digits[24];
    int len = 0;
    do {
        digits[len++] = static_cast<char>('0' + whole % 10);
        whole /= 10;
    } while (whole > 0);
    while (len > 0) out.push_back(digits[--len]);
    out.push_back('.');
    out.push_back(static_cast<char>('0' + frac / 10));
    out.push_back(static_cast<char>('0' + frac % 10));
}

}  // namespace bh
