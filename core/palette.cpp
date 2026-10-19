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
    double hue_low[3], hue_high[3], chroma[3];
};

Family uniform(double low, double high, double chroma) {
    return {{low, low, low}, {high, high, high}, {chroma, chroma, chroma}};
}

Family family(Theme t, Season season) {
    // Seasons change only the Temperate mountain palette (SPECIFICATIONS.md,
    // Seasons). Other biomes keep their family in every season.
    if (t == Theme::kTemperate && season == Season::kWinter) {
        return {{195.0, 200.0, 205.0}, {255.0, 260.0, 265.0}, {0.7, 0.7, 0.7}};  // bluish gray
    }
    if (t == Theme::kTemperate && season == Season::kFall) {
        // Orange front ridge (its fill fades to gold, see gradient_bottom); the
        // distant ridges keep the cool summer blues.
        return {{30.0, 188.0, 210.0}, {60.0, 232.0, 240.0}, {1.35, 1.0, 1.0}};
    }
    switch (t) {
        case Theme::kAlpine: return uniform(215.0, 275.0, 1.0);  // cool slate blue
        // Bluish teal in front, shifting to soft blue with distance (aerial
        // perspective). Less violet than alpine's slate.
        case Theme::kTemperate: return {{175.0, 188.0, 210.0}, {205.0, 232.0, 240.0}, {1.0, 1.0, 1.0}};
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
                out.push_back(oklch(lerp(l0, l1, li, 8), f.chroma[band] * lerp(c0, c1, ci, 4), lerp(f.hue_low[band], f.hue_high[band], hi, 8)));
    return out;
}

// 16 entries: index = lightness (2 bits) | hue (2 bits).
std::vector<uint32_t> shades16(const Family& f, int band, double l0, double l1, double c) {
    std::vector<uint32_t> out;
    for (int li = 0; li < 4; ++li)
        for (int hi = 0; hi < 4; ++hi)
            out.push_back(oklch(lerp(l0, l1, li, 4), f.chroma[band] * c, lerp(f.hue_low[band], f.hue_high[band], hi, 4)));
    return out;
}

}  // namespace

namespace {

struct Tables {
    std::vector<uint32_t> ids[3];
};

Tables make_tables(Theme t, Season s) {
    Family f = family(t, s);
    return {{
        shades256(f, 0, 0.30, 0.48, 0.030, 0.075),  // front: darkest band
        shades256(f, 1, 0.51, 0.65, 0.025, 0.065),  // middle
        shades16(f, 2, 0.68, 0.77, 0.040),          // far: lightest band
    }};
}

}  // namespace

const std::vector<uint32_t>& palette(Theme theme, Season season, int id) {
    static const std::vector<Tables> tables = [] {
        std::vector<Tables> out;
        for (Theme t : kThemes)
            for (Season s : kSeasons) out.push_back(make_tables(t, s));
        return out;
    }();
    return tables[static_cast<size_t>(theme) * 4 + static_cast<size_t>(season)].ids[id];
}

bool front_gradient(Theme theme, Season season) {
    return theme == Theme::kTemperate && season == Season::kFall;
}

const std::vector<uint32_t>& gradient_bottom(Theme theme, Season season) {
    // Paired golden shades for the fall front ridge: same index grid as the
    // front palette, hue moved from orange (30-60) to gold (72-92), slightly lighter.
    static const std::vector<uint32_t> fall = [] {
        std::vector<uint32_t> out;
        for (int li = 0; li < 8; ++li)
            for (int hi = 0; hi < 8; ++hi)
                for (int ci = 0; ci < 4; ++ci)
                    out.push_back(oklch(lerp(0.30, 0.48, li, 8) + 0.10, 1.35 * lerp(0.030, 0.075, ci, 4),
                                        lerp(72.0, 92.0, hi, 8)));
        return out;
    }();
    static const std::vector<uint32_t> none;
    return front_gradient(theme, season) ? fall : none;
}

}  // namespace bh

namespace bh {

namespace {

// Hill color family for a biome and season. Spring, summer, and fall are green
// families tinted by the biome; winter is snowy tints (pale blue-whites and
// lavender-whites), still varied by the color bits.
struct HillFamily {
    double hue_low, hue_high, chroma;
    double light[3][2];  // lightness range per palette: single, front, back
};

HillFamily hill_family(Theme t, Season s) {
    if (s == Season::kWinter) return {195.0, 290.0, 0.6, {{0.80, 0.94}, {0.79, 0.85}, {0.87, 0.94}}};
    double light[3][2] = {{0.30, 0.46}, {0.26, 0.34}, {0.36, 0.46}};
    HillFamily f{110.0, 150.0, 1.0, {}};
    switch (t) {
        case Theme::kAlpine: f = {140.0, 175.0, 1.0, {}}; break;     // blue-green
        case Theme::kTemperate: f = {100.0, 170.0, 1.0, {}}; break;  // fresh green
        case Theme::kTundra: f = {95.0, 135.0, 0.7, {}}; break;      // muted olive-green
        case Theme::kDesert: f = {70.0, 125.0, 0.9, {}}; break;      // olive
    }
    for (int i = 0; i < 3; ++i) {
        f.light[i][0] = light[i][0];
        f.light[i][1] = light[i][1];
    }
    return f;
}

std::vector<uint32_t> hill_grid(const HillFamily& f, int id, int nl, int nh, int nc) {
    std::vector<uint32_t> out;
    for (int li = 0; li < nl; ++li)
        for (int hi = 0; hi < nh; ++hi)
            for (int ci = 0; ci < nc; ++ci) {
                double c = nc == 1 ? 0.070 : lerp(0.045, 0.105, ci, nc);
                out.push_back(oklch(lerp(f.light[id][0], f.light[id][1], li, nl), f.chroma * c,
                                    lerp(f.hue_low, f.hue_high, hi, nh)));
            }
    return out;
}

struct HillTables {
    std::vector<uint32_t> ids[3];
};

}  // namespace

const std::vector<uint32_t>& hill_palette(Theme theme, Season season, int id) {
    static const std::vector<HillTables> tables = [] {
        std::vector<HillTables> out;
        for (Theme t : kThemes)
            for (Season s : kSeasons) {
                HillFamily f = hill_family(t, s);
                out.push_back({{hill_grid(f, 0, 8, 8, 4), hill_grid(f, 1, 2, 2, 1), hill_grid(f, 2, 4, 4, 1)}});
            }
        return out;
    }();
    return tables[static_cast<size_t>(theme) * 4 + static_cast<size_t>(season)].ids[id];
}

}  // namespace bh
