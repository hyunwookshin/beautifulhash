#include "hills.hpp"

#include <cstdio>
#include <cstdlib>

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
    return hill;
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
