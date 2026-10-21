#include "hills.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "celestial.hpp"
#include "mountains.hpp"
#include "palette.hpp"
#include "partition.hpp"

namespace bh::hills {

const std::vector<HillLayout>& layout(bool two) {
    // {x, crest, left, right, shape, color, palette}
    static const std::vector<HillLayout> one = {
        {8, 7, 7, 7, 2, 8, kHillSingle},  // 39 bits
    };
    static const std::vector<HillLayout> pair = {
        {4, 4, 4, 3, 2, 2, kHillFront},  // 19 bits
        {4, 4, 3, 3, 2, 4, kHillBack},   // 20 bits
    };
    return two ? pair : one;
}

const Params& default_params() {
    static const Params p;
    return p;
}

double steps(int bits) { return static_cast<double>((1u << bits) - 1u); }

double profile(int exponent, double u) {
    double u2 = u * u;
    switch (exponent) {
        case 1: return 1.0 - u;                          // tent
        case 2: return 1.0 - u2;                         // dome
        case 3: return 1.0 - u2 * (3.0 - 2.0 * u);       // sigmoid
        default: return 1.0 - u2 * (3.0 - 2.0 * u2);    // W: dips 12.5% below the edge level
    }
}

Values decode(const Hash256& hash) {
    BitReader in(hash, kHills.begin, kHills.width);
    Values v;
    v.two = in.read(1) == 1;
    for (const HillLayout& l : layout(v.two)) {
        HillValues h;
        h.x = in.read(l.x);
        h.crest = in.read(l.crest);
        h.left = in.read(l.left);
        h.right = in.read(l.right);
        h.shape = in.read(l.shape);
        h.color = in.read(l.color);
        v.hills.push_back(h);
    }
    if (in.remaining() != 0) {
        std::fprintf(stderr, "hills: layout leaves %d bits unread\n", in.remaining());
        std::abort();
    }
    return v;
}

void encode(const Values& values, Hash256& hash) {
    BitWriter out(hash, kHills.begin, kHills.width);
    out.write(1, values.two ? 1 : 0);
    const auto& hills = layout(values.two);
    for (size_t i = 0; i < hills.size(); ++i) {
        const HillLayout& l = hills[i];
        const HillValues& h = values.hills[i];
        out.write(l.x, h.x);
        out.write(l.crest, h.crest);
        out.write(l.left, h.left);
        out.write(l.right, h.right);
        out.write(l.shape, h.shape);
        out.write(l.color, h.color);
    }
}

Interval crest_interval(const Params& p, bool two, int index, double crest_x, const std::vector<Hill>& front) {
    if (!two) return {p.single_min, p.cap};
    if (index == 0) return {p.front_min, p.front_max};
    // Back hill: its crest must rise at least `exposure` above the front hill
    // at the same x, so the back hill is never hidden. The front hill never
    // exceeds front_max, so the interval is never empty.
    double lo = mountains::height_at(front[0].points, crest_x) + p.exposure;
    return {lo, p.cap};
}

Hill build(const Params& p, bool two, int index, const HillValues& v, const std::vector<Hill>& front,
           Theme theme, Season season) {
    const HillLayout& l = layout(two)[index];
    Hill hill;
    HillProfile& f = hill.profile;
    f.crest_x = kCanvasWidth * (p.x_min + (p.x_max - p.x_min) * (v.x / steps(l.x)));
    Interval iv = crest_interval(p, two, index, f.crest_x, front);
    f.crest_h = iv.lo + (iv.hi - iv.lo) * (v.crest / steps(l.crest));
    f.left_h = f.crest_h * (p.edge_min + (p.edge_max - p.edge_min) * (v.left / steps(l.left)));
    f.right_h = f.crest_h * (p.edge_min + (p.edge_max - p.edge_min) * (v.right / steps(l.right)));
    f.exponent = static_cast<int>(v.shape) + 1;

    // Sample each side of the crest uniformly in x.
    double n = static_cast<double>(p.samples);
    for (int i = 0; i <= p.samples; ++i) {
        double u = 1.0 - i / n;  // 1 at the left edge, 0 at the crest
        hill.points.push_back({f.crest_x * (i / n), f.left_h + (f.crest_h - f.left_h) * profile(f.exponent, u)});
    }
    for (int i = 1; i <= p.samples; ++i) {
        double u = i / n;  // 0 at the crest, 1 at the right edge
        hill.points.push_back({f.crest_x + (kCanvasWidth - f.crest_x) * (i / n),
                               f.right_h + (f.crest_h - f.right_h) * profile(f.exponent, u)});
    }
    hill.rgb = hill_palette(theme, season, l.palette)[v.color];
    hill.palette = l.palette;
    return hill;
}

namespace {

// Moves each channel a fraction k toward white (k > 0) or black (k < 0).
uint32_t shade(uint32_t rgb, double k) {
    uint32_t out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        double c = static_cast<double>((rgb >> shift) & 255u);
        c = k > 0.0 ? c + (255.0 - c) * k : c * (1.0 + k);
        out |= static_cast<uint32_t>(std::lround(c)) << shift;
    }
    return out;
}

// Grass teeth along an edge, \/\/\/: valleys on the edge every `spacing`, peaks
// between them raised by `height` times a repeating, irregular pattern, so
// the teeth are uneven but not random. With `cap`, no point rises above it.
std::vector<Point> teeth(const std::vector<Point>& edge, double spacing, double height, const std::vector<Point>* cap) {
    static const int kPattern[6] = {2, 3, 0, 1, 4, 7};
    std::vector<Point> out;
    int steps = static_cast<int>(kCanvasWidth / spacing);
    for (int k = 0; k <= 2 * steps; ++k) {
        double x = 0.5 * spacing * k;
        double h = mountains::height_at(edge, x);
        if (k % 2 == 1) h += height * (0.3 + 0.1 * kPattern[(k / 2) % 6]);
        if (cap) h = std::min(h, mountains::height_at(*cap, x));
        out.push_back({x, h});
    }
    if (out.back().x < kCanvasWidth) out.push_back({kCanvasWidth, mountains::height_at(edge, kCanvasWidth)});
    return out;
}

}  // namespace

void add_bands(Hill& hill, const Sun& sun) {
    // Discrete light bands: lighter at the top, darker toward the viewer, so
    // the hill reads as lit from above. A tall hill gets five, a low one three.
    const std::vector<Point>& s = hill.points;
    double mean = 0.0, low = s.front().h;
    for (const Point& q : s) {
        mean += q.h;
        low = std::min(low, q.h);
    }
    mean /= static_cast<double>(s.size());
    int n = mean >= 80.0 ? 5 : 3;
    hill.bands.clear();
    hill.band_rgb.clear();
    for (int j = 0; j < n; ++j) {
        // Boundary j follows the silhouette, flattened toward the hill's lowest
        // height and lowered toward the canvas bottom at depth f: deeper bands
        // have less and less curvature. The sun sets the spacing of the depths
        // and tilts the boundaries lower on its lit side. No boundary rises
        // above the silhouette: (1 - f)(1 + 0.15 f) <= 1.
        double r = static_cast<double>(j) / n;
        double f = r * (1.0 - sun.spacing) + r * r * sun.spacing;
        std::vector<Point> edge;
        for (const Point& q : s) {
            double tilt = 1.0 + sun.tilt * f * (2.0 * q.x / kCanvasWidth - 1.0);
            edge.push_back({q.x, celestial::surface(q.h, low, f) * tilt});
        }
        // The crest gets the finest teeth, under a logical unit tall: visible
        // when zoomed in, not at full view. They rise above the silhouette, on
        // the top band only, so the hill's own path is unchanged. Inner
        // boundaries get grass teeth that grow toward the viewer with the
        // vegetation's depth scale, capped at the silhouette.
        if (j == 0) {
            edge = teeth(edge, 2.0, 0.8, nullptr);
        } else {
            double d = 0.35 + 1.65 * f;
            edge = teeth(edge, 4.0 * d, 5.0 * d, &s);
        }
        hill.bands.push_back(edge);
        // 8% steps toward white above the middle band and toward black below.
        hill.band_rgb.push_back(shade(hill.rgb, 0.08 * ((n - 1) / 2 - j)));
    }
}

std::vector<Hill> generate(const Hash256& hash, Theme theme, Season season) {
    Values v = decode(hash);
    const Params& p = default_params();
    std::vector<Hill> built;
    for (size_t i = 0; i < v.hills.size(); ++i) {
        built.push_back(build(p, v.two, static_cast<int>(i), v.hills[i], built, theme, season));
    }
    return built;
}

}  // namespace bh::hills
