#include "mountains.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <thread>
#include <tuple>

#include "palette.hpp"
#include "partition.hpp"

namespace bh::mountains {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// The interval rule: largest |t| keeping P strictly between A and B in x,
// with margin gamma, and within the r * |AB| deviation bound.
double interval(double dx, double dh, const Params& p) {
    if (dh == 0.0) return 1.0;
    double limit = (p.margin * dx) / (2.0 * p.roughness * std::fabs(dh));
    return limit < 1.0 ? limit : 1.0;
}

Point displace(Point a, Point b, double t, const Params& p) {
    double dx = b.x - a.x;
    double dh = b.h - a.h;
    double rt = p.roughness * t;
    double mx = (a.x + b.x) / 2.0;
    double mh = (a.h + b.h) / 2.0;
    return {mx - rt * dh, mh + rt * dx};
}

// Subdivides every segment of pts with detail points, `levels` times.
void add_detail(std::vector<Point>& pts, int levels, const Params& p) {
    for (int k = 0; k < levels; ++k) {
        std::vector<Point> next;
        next.reserve(2 * pts.size() - 1);
        for (size_t j = 0; j + 1 < pts.size(); ++j) {
            next.push_back(pts[j]);
            next.push_back(detail_point(pts[j], pts[j + 1], p));
        }
        next.push_back(pts.back());
        pts.swap(next);
    }
}

// Integer mix to [0, 1). Deterministic on every platform.
double unit(uint32_t key, uint32_t salt) {
    uint32_t x = key * 0x9E3779B1u ^ salt * 0x85EBCA77u;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return static_cast<double>(x) / 4294967296.0;
}

// Identifies the subtree under one last-level bit point: its index i, its
// decoded value v, and which child segment (side 0 or 1). Different hashes
// give different keys, so jag patterns vary with the bits.
uint32_t jag_key(uint32_t i, uint32_t v, int bits, uint32_t side) {
    return ((i << bits) | v) * 2u + side;
}

// Bit-reusing jagged detail for one segment {a, b}: `levels` more constrained
// midpoint displacements. Each midpoint's t is a fixed mix of the subtree key
// and the midpoint's position, with amplitude decaying by jag_decay per level
// and at least jag_floor of the allowed interval, so the line stays jagged.
void jag(std::vector<Point>& seg, int levels, uint32_t key, const Params& p) {
    double amp = 1.0;
    for (int m = 0; m < levels; ++m) {
        std::vector<Point> next;
        next.reserve(2 * seg.size() - 1);
        for (size_t q = 0; q + 1 < seg.size(); ++q) {
            Point a = seg[q], b = seg[q + 1];
            uint32_t node = key * 64u + ((1u << m) + static_cast<uint32_t>(q));
            double mag = p.jag_floor + (1.0 - p.jag_floor) * unit(node, 11);
            double sign = unit(node, 12) < 0.5 ? -1.0 : 1.0;
            double t = interval(b.x - a.x, b.h - a.h, p) * amp * mag * sign;
            next.push_back(a);
            next.push_back(displace(a, b, t, p));
        }
        next.push_back(seg.back());
        seg.swap(next);
        amp *= p.jag_decay;
    }
}

}  // namespace

double steps(int bits) { return static_cast<double>((1u << bits) - 1u); }

double tilt_value(uint32_t v, int bits, const Params& p) {
    return p.tilt_range * ((2.0 * v) / steps(bits) - 1.0);
}

Point subdivide(Point a, Point b, uint32_t v, int bits, const Params& p) {
    double t_max = interval(b.x - a.x, b.h - a.h, p);
    double t = t_max * ((2.0 * v) / steps(bits) - 1.0);
    return displace(a, b, t, p);
}

SummitFields split_summit(const RidgeLayout& layout, uint32_t tilt, uint32_t first) {
    SummitFields f;
    f.x_bits = 3;
    f.drop_bits = 3;
    f.rise_bits = layout.tilt + layout.levels[0] - f.x_bits - f.drop_bits;
    uint32_t c = tilt << layout.levels[0] | first;
    f.x = c >> (f.rise_bits + f.drop_bits);
    f.rise = (c >> f.drop_bits) & ((1u << f.rise_bits) - 1u);
    f.drop = c & ((1u << f.drop_bits) - 1u);
    return f;
}

void join_summit(const RidgeLayout& layout, const SummitFields& f, uint32_t& tilt, uint32_t& first) {
    uint32_t c = f.x << (f.rise_bits + f.drop_bits) | f.rise << f.drop_bits | f.drop;
    tilt = c >> layout.levels[0];
    first = c & ((1u << layout.levels[0]) - 1u);
}

std::vector<Point> skeleton(const RidgeLayout& layout, uint32_t tilt, uint32_t first, const Params& p) {
    if (p.generator == Generator::kSummit) {
        SummitFields f = split_summit(layout, tilt, first);
        double x = kCanvasWidth * (p.summit_x_min + (p.summit_x_max - p.summit_x_min) * (f.x / steps(f.x_bits)));
        double rise = p.rise_min + (p.rise_max - p.rise_min) * (f.rise / steps(f.rise_bits));
        double drop = p.rise_min + (p.rise_max - p.rise_min) * (f.drop / steps(f.drop_bits));
        return {{0.0, 0.0}, {x, rise}, {kCanvasWidth, rise - drop}};
    }
    Point right{kCanvasWidth, tilt_value(tilt, layout.tilt, p)};
    return {{0.0, 0.0}, subdivide({0.0, 0.0}, right, first, layout.levels[0], p), right};
}

Point detail_point(Point a, Point b, const Params& p) {
    double dx = b.x - a.x;
    double dh = b.h - a.h;
    // u = k * |slope|; t = -u / (1 + u) lies in (-1, 0]. Negative t moves the
    // midpoint below the chord on both rising and falling segments, so steep
    // faces become slightly concave. Flat segments are left unchanged.
    double u = p.detail * std::fabs(dh / dx);
    double t = interval(dx, dh, p) * (-u / (1.0 + u));
    return displace(a, b, t, p);
}

namespace {

// Exhaustive search for the tallest possible shape. All bit levels but the
// last are enumerated. Each last-level point, together with the detail points
// beneath it, depends only on its two parents, so the best choice per point
// is found directly.
class HeightSearch {
public:
    HeightSearch(const RidgeLayout& layout, const Params& p, int depth)
        : layout_(layout), p_(p), detail_(depth - static_cast<int>(layout.levels.size())) {}

    // Enumerates every first-level value for one tilt value, then the rest.
    double run(uint32_t tilt) {
        best_ = 0.0;
        for (uint32_t first = 0; first < (1u << layout_.levels[0]); ++first) {
            level(skeleton(layout_, tilt, first, p_), 1);
        }
        return best_;
    }

private:
    void level(const std::vector<Point>& pts, size_t k) {
        int bits = layout_.levels[k];
        size_t segs = pts.size() - 1;
        if (k + 1 == layout_.levels.size()) {
            last(pts, bits);
            return;
        }
        uint32_t n = 1u << bits;
        std::vector<uint32_t> idx(segs, 0);
        std::vector<Point> next(2 * segs + 1);
        for (;;) {
            for (size_t j = 0; j < segs; ++j) {
                next[2 * j] = pts[j];
                next[2 * j + 1] = subdivide(pts[j], pts[j + 1], idx[j], bits, p_);
            }
            next[2 * segs] = pts[segs];
            level(next, k + 1);
            size_t j = 0;
            while (j < segs && ++idx[j] == n) idx[j++] = 0;
            if (j == segs) break;
        }
    }

    void last(const std::vector<Point>& pts, int bits) {
        double fmax = -kInf, fmin = kInf;
        for (const Point& q : pts) {
            fmax = std::max(fmax, q.h);
            fmin = std::min(fmin, q.h);
        }
        size_t segs = pts.size() - 1;
        uint32_t n = 1u << bits;
        // For each segment and option: highest and lowest point it creates.
        std::vector<double> hi(segs, -kInf), lo(segs, kInf);
        double best = fmax - fmin;
        for (size_t j = 0; j < segs; ++j) {
            for (uint32_t v = 0; v < n; ++v) {
                Point mid = subdivide(pts[j], pts[j + 1], v, bits, p_);
                std::vector<Point> local;
                if (p_.jagged) {
                    uint32_t i = static_cast<uint32_t>(j);
                    std::vector<Point> left = {pts[j], mid}, right = {mid, pts[j + 1]};
                    jag(left, detail_, jag_key(i, v, bits, 0), p_);
                    jag(right, detail_, jag_key(i, v, bits, 1), p_);
                    local = left;
                    local.insert(local.end(), right.begin() + 1, right.end());
                } else {
                    local = {pts[j], mid, pts[j + 1]};
                    add_detail(local, detail_, p_);
                }
                double mx = -kInf, mn = kInf;
                for (const Point& q : local) {
                    mx = std::max(mx, q.h);
                    mn = std::min(mn, q.h);
                }
                hi[j] = std::max(hi[j], mx);
                lo[j] = std::min(lo[j], mn);
                // One segment's single option supplying both extremes.
                best = std::max(best, std::max(fmax, mx) - std::min(fmin, mn));
            }
        }
        for (size_t a = 0; a < segs; ++a) {
            for (size_t b = 0; b < segs; ++b) {
                if (a != b) best = std::max(best, std::max(fmax, hi[a]) - std::min(fmin, lo[b]));
            }
        }
        best_ = std::max(best_, best);
    }

    const RidgeLayout& layout_;
    const Params& p_;
    int detail_;
    double best_ = 0.0;
};

// The exhaustive search depends only on the layout, depth, generator, and
// shape parameters, not on scale, visibility, or share.
}  // namespace

double max_height(const RidgeLayout& layout, const Params& p, int depth) {
    using Key = std::tuple<const RidgeLayout*, int, int, double, double, double, double, double, double, double,
                           double, bool, double, double>;
    static std::map<Key, double> cache;
    Key key{&layout, depth, static_cast<int>(p.generator), p.roughness, p.margin, p.tilt_range, p.detail,
            p.summit_x_min, p.summit_x_max, p.rise_min, p.rise_max, p.jagged, p.jag_decay, p.jag_floor};
    auto it = cache.find(key);
    if (it == cache.end()) it = cache.emplace(key, max_shape_height(layout, p, depth)).first;
    return it->second;
}

namespace {

Budget make_budget(bool three_ridge, const Params& p) {
    const auto& ridges = layout(three_ridge);
    Budget b;
    for (size_t i = 0; i < ridges.size(); ++i) {
        Params rp = ridge_params(p, three_ridge, static_cast<int>(i));
        double h = rp.scale * max_height(ridges[i], rp, rp.resolution);
        // A tiny allowance covers rounding between s * max(Q) and max(s * Q).
        h += 1e-6;
        b.max_height.push_back(h);
        b.reserve.push_back(std::max(p.visibility * h, p.min_exposure));
    }
    b.slack = p.ceiling - p.ground - b.max_height[0];
    for (size_t i = 1; i < ridges.size(); ++i) b.slack -= b.reserve[i];
    return b;
}

double envelope(const std::vector<Ridge>& front, double x) {
    double f = -kInf;
    for (const Ridge& r : front) f = std::max(f, height_at(r.points, x));
    return f;
}

}  // namespace

const std::vector<RidgeLayout>& layout(bool three_ridge) {
    // {elevation, tilt, levels, color, palette}
    static const std::vector<RidgeLayout> two = {
        {3, 3, {7, 3, 2, 1}, 8, kPaletteFront},   // 43 bits
        {4, 4, {6, 3, 2, 1}, 8, kPaletteMiddle},  // 44 bits
    };
    static const std::vector<RidgeLayout> three = {
        {3, 3, {6, 2, 2}, 8, kPaletteFront},   // 32 bits
        {3, 3, {6, 2, 2}, 8, kPaletteMiddle},  // 32 bits
        {3, 3, {7, 3}, 4, kPaletteFar},        // 23 bits
    };
    return three_ridge ? three : two;
}

Config make_config(const Params& params) {
    for (bool three : {false, true}) {
        const auto& ridges = layout(three);
        for (size_t i = 0; i < ridges.size(); ++i) {
            if (ridges[i].levels.size() < 2) {
                std::fprintf(stderr, "mountains: ridge %zu needs at least two bit levels\n", i + 1);
                std::abort();
            }
            if (ridge_params(params, three, static_cast<int>(i)).resolution < static_cast<int>(ridges[i].levels.size())) {
                std::fprintf(stderr, "mountains: resolution of ridge %zu is below its bit levels\n", i + 1);
                std::abort();
            }
        }
    }
    Config c;
    c.params = params;
    c.two = make_budget(false, params);
    c.three = make_budget(true, params);
    return c;
}

const Config& default_config() {
    static const Config c = make_config(Params{});
    return c;
}

int role(bool three_ridge, int index) {
    int last = three_ridge ? 2 : 1;
    if (index == 0) return 0;
    return index == last ? 2 : 1;
}

Params ridge_params(const Params& p, bool three_ridge, int index) {
    Params out = p;
    const Params::Style& s = p.style[role(three_ridge, index)];
    out.generator = s.generator;
    out.roughness = s.roughness;
    out.scale = s.scale;
    out.resolution = s.resolution;
    out.jagged = s.jagged;
    return out;
}

Values decode(const Hash256& hash) {
    BitReader in(hash, kMountains.begin, kMountains.width);
    Values v;
    v.three_ridge = in.read(1) == 1;
    for (const RidgeLayout& l : layout(v.three_ridge)) {
        RidgeValues r;
        r.elevation = in.read(l.elevation);
        r.tilt = in.read(l.tilt);
        for (size_t k = 0; k < l.levels.size(); ++k) {
            for (size_t j = 0; j < (size_t{1} << k); ++j) r.points.push_back(in.read(l.levels[k]));
        }
        r.color = in.read(l.color);
        v.ridges.push_back(r);
    }
    if (in.remaining() != 0) {
        std::fprintf(stderr, "mountains: layout leaves %d bits unread\n", in.remaining());
        std::abort();
    }
    return v;
}

void encode(const Values& values, Hash256& hash) {
    BitWriter out(hash, kMountains.begin, kMountains.width);
    out.write(1, values.three_ridge ? 1 : 0);
    const auto& ridges = layout(values.three_ridge);
    for (size_t i = 0; i < ridges.size(); ++i) {
        const RidgeLayout& l = ridges[i];
        const RidgeValues& r = values.ridges[i];
        out.write(l.elevation, r.elevation);
        out.write(l.tilt, r.tilt);
        size_t n = 0;
        for (size_t k = 0; k < l.levels.size(); ++k) {
            for (size_t j = 0; j < (size_t{1} << k); ++j) out.write(l.levels[k], r.points[n++]);
        }
        out.write(l.color, r.color);
    }
}

std::vector<Point> shape(const RidgeLayout& layout, uint32_t tilt,
                         const std::vector<uint32_t>& points, const Params& p, double s, int depth) {
    // The first bit level (Level 1 midpoint or summit) is the skeleton.
    std::vector<Point> pts = skeleton(layout, tilt, points[0], p);
    size_t n = 1;
    for (size_t k = 1; k < layout.levels.size(); ++k) {
        int bits = layout.levels[k];
        std::vector<Point> next;
        next.reserve(2 * pts.size() - 1);
        for (size_t j = 0; j + 1 < pts.size(); ++j) {
            next.push_back(pts[j]);
            next.push_back(subdivide(pts[j], pts[j + 1], points[n++], bits, p));
        }
        next.push_back(pts.back());
        pts.swap(next);
    }
    int extra = depth - static_cast<int>(layout.levels.size());
    if (p.jagged) {
        // Every segment descends from one last-level bit point; its jagged
        // detail reuses that point's decoded value.
        size_t last = layout.levels.size() - 1;
        int bits = layout.levels[last];
        size_t first = (size_t{1} << last) - 1;
        std::vector<Point> out;
        for (size_t j = 0; j + 1 < pts.size(); ++j) {
            uint32_t i = static_cast<uint32_t>(j / 2);
            std::vector<Point> seg = {pts[j], pts[j + 1]};
            jag(seg, extra, jag_key(i, points[first + i], bits, static_cast<uint32_t>(j % 2)), p);
            out.insert(out.end(), seg.begin(), seg.end() - 1);
        }
        out.push_back(pts.back());
        pts.swap(out);
    } else {
        add_detail(pts, extra, p);
    }
    for (Point& q : pts) q.h = s * q.h;
    return pts;
}

double max_shape_height(const RidgeLayout& layout, const Params& p, int depth) {
    uint32_t tilts = 1u << layout.tilt;
    std::vector<double> best(tilts, 0.0);
    std::vector<std::thread> workers;
    for (uint32_t t = 0; t < tilts; ++t) {
        workers.emplace_back([&, t] { best[t] = HeightSearch(layout, p, depth).run(t); });
    }
    for (auto& w : workers) w.join();
    return *std::max_element(best.begin(), best.end());
}

double height_at(const std::vector<Point>& poly, double x) {
    if (x <= poly.front().x) return poly.front().h;
    if (x >= poly.back().x) return poly.back().h;
    auto it = std::upper_bound(poly.begin(), poly.end(), x,
                               [](double v, const Point& q) { return v < q.x; });
    const Point& b = *it;
    const Point& a = *(it - 1);
    return a.h + (b.h - a.h) * ((x - a.x) / (b.x - a.x));
}

Placement place(const Config& config, bool three_ridge, int index, const RidgeValues& values,
                const std::vector<Ridge>& front) {
    const RidgeLayout& l = layout(three_ridge)[index];
    const Params& p = config.params;
    const Budget& b = config.budget(three_ridge);

    Placement out;
    Params rp = ridge_params(p, three_ridge, index);
    out.shape = shape(l, values.tilt, values.points, rp, rp.scale, rp.resolution);

    double qmin = kInf, qmax = -kInf;
    for (const Point& q : out.shape) {
        qmin = std::min(qmin, q.h);
        qmax = std::max(qmax, q.h);
    }
    double height = qmax - qmin;

    double behind = 0.0;
    for (size_t k = index + 1; k < b.reserve.size(); ++k) behind += b.reserve[k];

    if (index == 0) {
        out.base = p.ground - qmin;
        out.room = p.ceiling - (p.ground + height) - behind;
    } else {
        // The largest exposure above the front envelope is at a vertex of
        // this ridge or of a front ridge.
        std::vector<double> xs;
        for (const Point& q : out.shape) xs.push_back(q.x);
        double top_front = -kInf;
        for (const Ridge& r : front) {
            for (const Point& q : r.points) {
                xs.push_back(q.x);
                top_front = std::max(top_front, q.h);
            }
        }
        std::sort(xs.begin(), xs.end());
        double exposure = -kInf;
        for (double x : xs) exposure = std::max(exposure, height_at(out.shape, x) - envelope(front, x));

        double required = std::max(p.visibility * height, p.min_exposure);
        out.base = required - exposure;
        out.room = p.ceiling - top_front - required - behind;
    }

    if (!(out.room > 0.0)) {
        std::fprintf(stderr, "mountains: ridge %d has no headroom (%.3f)\n", index + 1, out.room);
        std::abort();
    }
    double e = (p.share[index] * out.room) * (static_cast<double>(values.elevation) / steps(l.elevation));
    out.offset = out.base + e;
    return out;
}

std::vector<Ridge> generate(const Config& config, const Hash256& hash) {
    Values v = decode(hash);
    const auto& ridges = layout(v.three_ridge);
    std::vector<Ridge> placed;
    for (size_t i = 0; i < ridges.size(); ++i) {
        Placement pl = place(config, v.three_ridge, static_cast<int>(i), v.ridges[i], placed);
        Ridge r;
        r.points = pl.shape;
        for (Point& q : r.points) q.h = q.h + pl.offset;
        r.rgb = palette(config.params.theme, ridges[i].palette)[v.ridges[i].color];
        placed.push_back(r);
    }

    // Biome styling runs only after every ridge is placed, so it can never
    // change placement or the hash-derived geometry.
    const Params& p = config.params;
    for (size_t i = 0; i < placed.size(); ++i) {
        int index = static_cast<int>(i);
        if (p.theme == Theme::kAlpine && role(v.three_ridge, index) == 2) {
            placed[i].snow = snow_caps(placed[i].points, p, v.ridges[i]);
            placed[i].snow_rgb = p.snow_rgb;
        }
        placed[i].points = style_points(placed[i].points, p, v.three_ridge, index, v.ridges[i]);
    }
    return placed;
}



namespace {

// Straight subdivision with no displacement, used before texture levels.
std::vector<Point> split(std::vector<Point> pts, int levels) {
    for (int k = 0; k < levels; ++k) {
        std::vector<Point> next;
        next.reserve(2 * pts.size() - 1);
        for (size_t j = 0; j + 1 < pts.size(); ++j) {
            next.push_back(pts[j]);
            next.push_back({(pts[j].x + pts[j + 1].x) / 2.0, (pts[j].h + pts[j + 1].h) / 2.0});
        }
        next.push_back(pts.back());
        pts.swap(next);
    }
    return pts;
}

// Fixed variation in [0, 1) for texture element `index`. It depends only on the
// element's position along the ridge, never on hash bits, so every hash gets
// the same pattern. Integer-only, so identical on every platform.
double jitter(uint32_t index, uint32_t salt) {
    uint32_t x = index * 0x9E3779B1u ^ salt * 0x85EBCA77u;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return static_cast<double>(x) / 4294967296.0;
}

Point lerp_at(Point a, Point b, double f, double lift) {
    return {a.x + (b.x - a.x) * f, a.h + (b.h - a.h) * f + lift};
}

}  // namespace

std::vector<Point> style_points(const std::vector<Point>& pts, const Params& p, bool three_ridge, int index,
                                const RidgeValues& values) {
    if (role(three_ridge, index) != 0) return pts;
    if (p.theme != Theme::kAlpine && p.theme != Theme::kTemperate) return pts;
    int depth = ridge_params(p, three_ridge, index).resolution;
    std::vector<Point> density = tree_density(pts, layout(three_ridge)[index], values, depth);
    if (p.theme == Theme::kAlpine) return add_conifers(pts, p, density);
    return add_canopy(pts, p, density);
}

std::vector<Point> tree_density(const std::vector<Point>& pts, const RidgeLayout& layout, const RidgeValues& values,
                                int depth) {
    // Zones are the deepest bit level's points. A zone's score averages its
    // point's value with its parent point's value, both normalized to [0, 1].
    size_t levels = layout.levels.size();
    size_t deepest = levels - 1;
    size_t first = (size_t{1} << deepest) - 1;           // index of the deepest level's first value
    size_t parent_first = (size_t{1} << (deepest - 1)) - 1;
    std::vector<Point> knots;
    for (size_t j = 0; j < (size_t{1} << deepest); ++j) {
        size_t vertex = (2 * j + 1) << (depth - static_cast<int>(levels));
        double own = values.points[first + j] / steps(layout.levels[deepest]);
        double parent = values.points[parent_first + j / 2] / steps(layout.levels[deepest - 1]);
        knots.push_back({pts[vertex].x, (own + parent) / 2.0});
    }
    return knots;
}

namespace {

// Tree size factor in [0, 1] at x: 0 (a flat clearing) below the threshold,
// rising linearly to 1 at the densest score.
double tree_factor(const std::vector<Point>& density, double x, const Params& p) {
    double s = height_at(density, x);
    return s <= p.tree_threshold ? 0.0 : (s - p.tree_threshold) / (1.0 - p.tree_threshold);
}

}  // namespace

std::vector<Point> add_conifers(const std::vector<Point>& pts, const Params& p, const std::vector<Point>& density) {
    // Straight subdivision, then the new (non-original) points become valleys
    // cut conifer_depth below the silhouette, and one tooth per segment rises
    // to 35-65% of the segment. Original points, including every bit-driven
    // point, never move. Sizes scale with the local tree factor; where it is 0
    // the texture is flat.
    std::vector<Point> base = split(pts, p.conifer_subdivisions);
    size_t stride = size_t{1} << p.conifer_subdivisions;
    for (size_t j = 0; j < base.size(); ++j) {
        if (j % stride != 0) base[j].h = base[j].h - p.conifer_depth * tree_factor(density, base[j].x, p);
    }
    // Each segment holds a continuous zigzag of two teeth: tip, V-valley cut
    // conifer_depth below the silhouette, tip. All edges are straight, so both
    // peaks and valleys are sharp. Three points per segment add two levels of
    // structure. Positions lean and heights vary by a fixed pattern.
    std::vector<Point> out;
    out.reserve(4 * base.size() - 3);
    for (size_t j = 0; j + 1 < base.size(); ++j) {
        uint32_t k = static_cast<uint32_t>(j);
        double f1 = 0.20 + 0.10 * jitter(k, 1);
        double fv = 0.45 + 0.10 * jitter(k, 5);
        double f2 = 0.70 + 0.10 * jitter(k, 6);
        double g = tree_factor(density, lerp_at(base[j], base[j + 1], fv, 0.0).x, p);
        double h1 = p.conifer_height * (0.6 + 0.6 * jitter(k, 2)) * g;
        double h2 = p.conifer_height * (0.6 + 0.6 * jitter(k, 7)) * g;
        out.push_back(base[j]);
        out.push_back(lerp_at(base[j], base[j + 1], f1, h1));
        out.push_back(lerp_at(base[j], base[j + 1], fv, -p.conifer_depth * g));
        out.push_back(lerp_at(base[j], base[j + 1], f2, h2));
    }
    out.push_back(base.back());
    return out;
}

std::vector<Point> add_canopy(const std::vector<Point>& pts, const Params& p, const std::vector<Point>& density) {
    // One rounded bump per segment: a semicircle-like arc of seven points
    // sqrt(1 - (2f - 1)^2) at f = k/8, from a constant table (no square root at
    // run time). The arc covers 65-100% of the segment and its height varies
    // from 0.55x to 1.3x of canopy_height, scaled by the local tree factor.
    // Seven points per segment add three levels of structure, so bit-driven
    // points keep known indices.
    static const double kArc[7] = {0.6614378277661477, 0.8660254037844386, 0.9682458365518543, 1.0,
                                   0.9682458365518543, 0.8660254037844386, 0.6614378277661477};
    std::vector<Point> base = split(pts, p.canopy_subdivisions);
    std::vector<Point> out;
    out.reserve(8 * base.size() - 7);
    for (size_t j = 0; j + 1 < base.size(); ++j) {
        uint32_t k = static_cast<uint32_t>(j);
        double w = 0.65 + 0.35 * jitter(k, 3);
        double g = tree_factor(density, lerp_at(base[j], base[j + 1], w / 2.0, 0.0).x, p);
        double h = p.canopy_height * (0.55 + 0.75 * jitter(k, 4)) * g;
        out.push_back(base[j]);
        for (int i = 0; i < 7; ++i) out.push_back(lerp_at(base[j], base[j + 1], w * (i + 1) / 8.0, h * kArc[i]));
    }
    out.push_back(base.back());
    return out;
}


std::vector<std::vector<Point>> snow_caps(const std::vector<Point>& pts, const Params& p, const RidgeValues& values) {
    double top = -kInf, base = kInf;
    for (const Point& q : pts) {
        top = std::max(top, q.h);
        base = std::min(base, q.h);
    }
    double center = top - (top - base) * p.snow_fraction;

    // Seed from the ridge's decoded values, so each hash gets its own snowline.
    uint32_t seed = values.tilt * 0x9E3779B1u + values.elevation;
    for (size_t i = 0; i < values.points.size(); ++i) seed = seed * 31u + values.points[i] + static_cast<uint32_t>(i);

    // Smooth wavy snowline: 1D midpoint displacement in h on a uniform x grid,
    // amplitude decaying by snow_decay at each level.
    double amp = p.snow_wave;
    std::vector<Point> line = {{0.0, center + amp * (2.0 * unit(seed, 30) - 1.0)},
                               {kCanvasWidth, center + amp * (2.0 * unit(seed, 31) - 1.0)}};
    for (int m = 0; m < p.snow_levels; ++m) {
        amp *= p.snow_decay;
        std::vector<Point> next;
        for (size_t q = 0; q + 1 < line.size(); ++q) {
            uint32_t node = (1u << m) + static_cast<uint32_t>(q);
            next.push_back(line[q]);
            next.push_back({(line[q].x + line[q + 1].x) / 2.0,
                            (line[q].h + line[q + 1].h) / 2.0 + amp * (2.0 * unit(seed + node, 32) - 1.0)});
        }
        next.push_back(line.back());
        line.swap(next);
    }

    // Both polylines are linear between the merged x positions, so their
    // difference is too, and crossings are exact.
    std::vector<double> xs;
    for (const Point& q : pts) xs.push_back(q.x);
    for (const Point& q : line) xs.push_back(q.x);
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());

    std::vector<std::vector<Point>> caps;
    std::vector<Point> topside, bottom;
    auto close = [&]() {
        std::vector<Point> poly = topside;
        poly.insert(poly.end(), bottom.rbegin(), bottom.rend());
        caps.push_back(poly);
        topside.clear();
        bottom.clear();
    };
    double prev_x = 0.0, prev_d = 0.0;
    for (size_t k = 0; k < xs.size(); ++k) {
        double x = xs[k];
        double s = height_at(pts, x), w = height_at(line, x);
        double d = s - w;
        if (k > 0 && (prev_d > 0.0) != (d > 0.0)) {
            double xc = prev_x + (x - prev_x) * (prev_d / (prev_d - d));
            double hc = height_at(pts, xc);
            topside.push_back({xc, hc});
            if (prev_d > 0.0) close();
        }
        if (d > 0.0) {
            topside.push_back({x, s});
            bottom.push_back({x, w});
        }
        prev_x = x;
        prev_d = d;
    }
    if (!topside.empty()) close();
    return caps;
}

int styled_depth(const Params& p, bool three_ridge, int index) {
    int depth = ridge_params(p, three_ridge, index).resolution;
    if (role(three_ridge, index) == 0) {
        if (p.theme == Theme::kAlpine) depth += p.conifer_subdivisions + 2;
        if (p.theme == Theme::kTemperate) depth += p.canopy_subdivisions + 3;
    }
    return depth;
}

double texture_min_fraction(const Params& p, bool three_ridge, int index) {
    if (role(three_ridge, index) != 0) return 1.0;
    // Narrowest conifer piece: tip to valley, at least 0.45 - 0.30 = 0.15.
    if (p.theme == Theme::kAlpine) return 0.15 / std::pow(2.0, p.conifer_subdivisions);
    if (p.theme == Theme::kTemperate) return (0.65 / 8.0) / std::pow(2.0, p.canopy_subdivisions);
    return 1.0;
}

}  // namespace bh::mountains
