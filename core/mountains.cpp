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
                std::vector<Point> local = {pts[j], subdivide(pts[j], pts[j + 1], v, bits, p_), pts[j + 1]};
                add_detail(local, detail_, p_);
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
                           double>;
    static std::map<Key, double> cache;
    Key key{&layout, depth, static_cast<int>(p.generator), p.roughness, p.margin, p.tilt_range, p.detail,
            p.summit_x_min, p.summit_x_max, p.rise_min, p.rise_max};
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
    add_detail(pts, depth - static_cast<int>(layout.levels.size()), p);
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
        r.rgb = palette(ridges[i].palette)[v.ridges[i].color];
        placed.push_back(r);
    }
    return placed;
}

}  // namespace bh::mountains
