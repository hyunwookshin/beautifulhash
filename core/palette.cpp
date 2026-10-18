#include "palette.hpp"

#include <algorithm>
#include <cmath>

namespace bh {

namespace {

double to_srgb(double c) {
    c = c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    return std::min(1.0, std::max(0.0, c));
}

uint32_t oklch(double L, double C, double hue_deg) {
    double h = hue_deg * 3.14159265358979323846 / 180.0;
    double a = C * std::cos(h);
    double b = C * std::sin(h);
    double l = L + 0.3963377774 * a + 0.2158037573 * b;
    double m = L - 0.1055613458 * a - 0.0638541728 * b;
    double s = L - 0.0894841775 * a - 1.2914855480 * b;
    l = l * l * l;
    m = m * m * m;
    s = s * s * s;
    double r = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    double g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    double bl = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
    auto byte = [](double c) { return static_cast<uint32_t>(std::lround(to_srgb(c) * 255.0)); };
    return byte(r) << 16 | byte(g) << 8 | byte(bl);
}

double lerp(double a, double b, int i, int n) { return a + (b - a) * i / (n - 1); }

// A biome's color family: one hue range shared by every ridge, with a chroma
// multiplier. Each ridge gets its own lightness band, so depth order is
// preserved, and its bits select a shade within the family.
// Hue ranges are per depth band (front, middle, far), so a biome can shift hue
// with distance; most biomes use one range for all bands.
struct Family {
    double hue_low[3], hue_high[3], chroma;
};

Family uniform(double low, double high, double chroma) {
    return {{low, low, low}, {high, high, high}, chroma};
}

Family family(Theme t) {
    switch (t) {
        case Theme::kAlpine: return uniform(215.0, 275.0, 1.0);  // cool slate blue
        // Bluish teal in front, shifting to soft blue with distance (aerial
        // perspective). Less violet than alpine's slate.
        case Theme::kTemperate: return {{175.0, 188.0, 210.0}, {205.0, 232.0, 240.0}, 1.0};
        case Theme::kTundra: return uniform(285.0, 325.0, 0.9);  // muted purple, cool lavender
        case Theme::kDesert: return uniform(50.0, 85.0, 1.2);    // warm golden, ochre, sand
    }
    return uniform(215.0, 275.0, 1.0);
}

// 256 entries: index = lightness (3 bits) | hue (3 bits) | chroma (2 bits).
std::vector<uint32_t> shades256(const Family& f, int band, double l0, double l1, double c0, double c1) {
    std::vector<uint32_t> out;
    for (int li = 0; li < 8; ++li)
        for (int hi = 0; hi < 8; ++hi)
            for (int ci = 0; ci < 4; ++ci)
                out.push_back(oklch(lerp(l0, l1, li, 8), f.chroma * lerp(c0, c1, ci, 4), lerp(f.hue_low[band], f.hue_high[band], hi, 8)));
    return out;
}

// 16 entries: index = lightness (2 bits) | hue (2 bits).
std::vector<uint32_t> shades16(const Family& f, int band, double l0, double l1, double c) {
    std::vector<uint32_t> out;
    for (int li = 0; li < 4; ++li)
        for (int hi = 0; hi < 4; ++hi)
            out.push_back(oklch(lerp(l0, l1, li, 4), f.chroma * c, lerp(f.hue_low[band], f.hue_high[band], hi, 4)));
    return out;
}

}  // namespace

namespace {

struct Tables {
    std::vector<uint32_t> ids[3];
};

Tables make_tables(Theme t) {
    Family f = family(t);
    return {{
        shades256(f, 0, 0.30, 0.48, 0.030, 0.075),  // front: darkest band
        shades256(f, 1, 0.51, 0.65, 0.025, 0.065),  // middle
        shades16(f, 2, 0.68, 0.77, 0.040),          // far: lightest band
    }};
}

}  // namespace

const std::vector<uint32_t>& palette(Theme theme, int id) {
    static const Tables tables[4] = {
        make_tables(Theme::kAlpine), make_tables(Theme::kTemperate), make_tables(Theme::kTundra),
        make_tables(Theme::kDesert),
    };
    return tables[static_cast<int>(theme)].ids[id];
}

}  // namespace bh
