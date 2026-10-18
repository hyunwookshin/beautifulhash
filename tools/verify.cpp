// Checks the Mountains namespace against subspec/mountains.md, for the
// default configuration and for every gallery variant:
//   1. bit coverage of both layouts
//   2. static placement budget (exact worst-case ridge heights)
//   3. analytic coordinate separation for every slot
//   4. palette distinctness
//   5. round trip: hash -> SVG text -> decoded bits, for random hashes
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "generate.hpp"
#include "mountains.hpp"
#include "palette.hpp"
#include "partition.hpp"
#include "variants.hpp"

using namespace bh;
namespace mt = bh::mountains;

namespace {

constexpr double kQuantum = 0.01;
constexpr double kInf = std::numeric_limits<double>::infinity();
int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

Hash256 random_hash(std::mt19937_64& rng, int mode) {
    Hash256 h;
    for (auto& b : h.bytes) b = static_cast<uint8_t>(rng());
    if (mode != 0) h.set_bit(kMountains.begin, mode == 3);
    return h;
}

bool same_mountain_bits(const Hash256& a, const Hash256& b) {
    for (int i = kMountains.begin; i < kMountains.begin + kMountains.width; ++i) {
        if (a.bit(i) != b.bit(i)) return false;
    }
    return true;
}

// ---- 1. Bit coverage -------------------------------------------------------

void check_coverage() {
    std::printf("== Bit coverage\n");
    for (bool three : {false, true}) {
        int total = 1;
        for (const auto& l : mt::layout(three)) {
            int bits = l.elevation + l.tilt + l.color;
            for (size_t k = 0; k < l.levels.size(); ++k) bits += l.levels[k] << k;
            total += bits;
        }
        std::printf("  %s-ridge total: %d bits\n", three ? "three" : "two", total);
        check(total == kMountains.width, "layout does not consume exactly 88 bits");
    }
    std::mt19937_64 rng(7);
    for (int i = 0; i < 10000; ++i) {
        Hash256 h = random_hash(rng, 0);
        Hash256 back;
        mt::encode(mt::decode(h), back);
        if (!same_mountain_bits(h, back)) {
            check(false, "decode/encode does not round-trip");
            break;
        }
    }
}

// ---- 2. Placement budget ---------------------------------------------------

void check_budget(const mt::Config& c, bool verbose) {
    const mt::Params& p = c.params;
    for (bool three : {false, true}) {
        const mt::Budget& b = c.budget(three);
        const auto& layout = mt::layout(three);
        if (verbose) {
            std::printf("  %s-ridge: max heights", three ? "three" : "two");
            for (double h : b.max_height) std::printf(" %.2f", h);
            std::printf("; slack S = %.2f\n", b.slack);
        } else {
            std::printf("    %s-ridge slack S = %.2f\n", three ? "three" : "two", b.slack);
        }
        check(b.slack > 0, "slack is not positive");
        // Headroom of ridge i is at least S times prod_{j<i} (1 - kappa_j).
        double room = b.slack;
        for (size_t i = 0; i < layout.size(); ++i) {
            double step = p.share[i] * room / mt::steps(layout[i].elevation);
            if (verbose) {
                std::printf("    ridge %zu: room >= %.2f, extra-elevation step >= %.3f px at 1024x768\n",
                            i + 1, room, step * 0.5);
            }
            check(step * 0.5 > kQuantum, "extra-elevation step below quantum");
            room *= 1.0 - p.share[i];
        }
    }
}

// ---- 3. Separation ---------------------------------------------------------

// Worst-case steps at 1024x768. Vertical (relative) steps are halved for the
// placement offset (Step 6); horizontal steps are not affected by it.
void check_separation(const mt::Config& c, bool verbose) {
    const mt::Params& p = c.params;
    double factor = (1.0 - p.margin) / 2.0;
    double smallest = kInf;
    double narrowest = kInf;
    for (bool three : {false, true}) {
        const auto& layout = mt::layout(three);
        for (size_t i = 0; i < layout.size(); ++i) {
            const mt::RidgeLayout& l = layout[i];
            const mt::Params p = mt::ridge_params(c.params, three, static_cast<int>(i));
            double s = p.scale;
            double dx;  // minimum width of the segments subdivided at Level 2
            if (p.generator == mt::Generator::kSummit) {
                mt::SummitFields f = mt::split_summit(l, 0, 0);
                double span = p.rise_max - p.rise_min;
                smallest = std::min(smallest, kCanvasWidth * (p.summit_x_max - p.summit_x_min) / mt::steps(f.x_bits) * 0.5);
                smallest = std::min(smallest, s * span / mt::steps(f.rise_bits) * 0.5 / 2.0);
                smallest = std::min(smallest, s * span / mt::steps(f.drop_bits) * 0.5 / 2.0);
                dx = kCanvasWidth * std::min(p.summit_x_min, 1.0 - p.summit_x_max);
            } else {
                smallest = std::min(smallest, s * 2.0 * p.tilt_range / mt::steps(l.tilt) * 0.5 / 2.0);
                double ell = 2.0 * kCanvasWidth * std::min(p.roughness, p.margin / 2.0) / mt::steps(l.levels[0]);
                smallest = std::min(smallest, std::min(1.0, s) * ell / std::sqrt(2.0) * 0.5 / 2.0);
                dx = kCanvasWidth * factor;
            }
            for (size_t k = 1; k < l.levels.size(); ++k) {
                double ell = 2.0 * dx * std::min(p.roughness, p.margin / 2.0) / mt::steps(l.levels[k]);
                smallest = std::min(smallest, std::min(1.0, s) * ell / std::sqrt(2.0) * 0.5 / 2.0);
                dx *= factor;
            }
            // dx is now the width of the segments subdivided at the next level.
            double final_dx = dx * std::pow(factor, p.resolution - static_cast<int>(l.levels.size())) * 0.5;
            narrowest = std::min(narrowest, final_dx);
            if (verbose) {
                std::printf("  %s R%zu: depth %d (%zu bit levels + %d detail), min final segment width %.2f px\n",
                            three ? "3R" : "2R", i + 1, p.resolution, l.levels.size(),
                            p.resolution - static_cast<int>(l.levels.size()), final_dx);
            }
        }
    }
    std::printf("    smallest guaranteed step %.3f px (after offset halving), narrowest final segment %.2f px\n",
                smallest, narrowest);
    check(smallest > kQuantum, "separation below quantum");
    check(narrowest > kQuantum, "final segment narrower than quantum");
}

// ---- 4. Palettes -----------------------------------------------------------

void check_palettes() {
    std::printf("== Palettes (shared color family)\n");
    for (int id : {kPaletteFront, kPaletteMiddle, kPaletteFar}) {
        const auto& pal = palette(id);
        std::set<uint32_t> distinct(pal.begin(), pal.end());
        std::printf("  palette %d: %zu entries, %zu distinct #RRGGBB values\n", id, pal.size(), distinct.size());
        check(distinct.size() == pal.size(), "palette has duplicate colors");
    }
}

// ---- 5. Round trip through SVG text ----------------------------------------

struct ParsedRidge {
    uint32_t fill;
    std::vector<double> xs, ys;
};

std::vector<ParsedRidge> parse_ridges(const std::string& svg) {
    std::vector<ParsedRidge> out;
    size_t pos = 0;
    while ((pos = svg.find("<path class=\"ridge\"", pos)) != std::string::npos) {
        ParsedRidge r;
        size_t f = svg.find("fill=\"#", pos) + 7;
        r.fill = static_cast<uint32_t>(std::strtoul(svg.substr(f, 6).c_str(), nullptr, 16));
        size_t d = svg.find(" d=\"", pos) + 4;
        size_t end = svg.find('"', d);
        const char* c = svg.c_str() + d;
        const char* stop = svg.c_str() + end;
        while (c < stop) {
            if (*c == 'M' || *c == 'L') {
                char* next;
                double x = std::strtod(c + 1, &next);
                double y = std::strtod(next + 1, &next);
                r.xs.push_back(x);
                r.ys.push_back(y);
                c = next;
            } else {
                ++c;
            }
        }
        r.xs.resize(r.xs.size() - 2);  // drop the two bottom corners
        r.ys.resize(r.ys.size() - 2);
        out.push_back(r);
        pos = end;
    }
    std::reverse(out.begin(), out.end());  // SVG is back to front
    return out;
}

struct Stats {
    double min_margin = kInf;          // second best minus best, px
    double min_clearance = kInf;       // C - top, logical
    double min_extra_exposure = kInf;  // exposure - required, logical
    bool monotonic = true;             // serialized x strictly increasing
};

template <typename F>
uint32_t pick(int bits, F distance, Stats& st) {
    double best = kInf, second = kInf;
    uint32_t arg = 0;
    for (uint32_t v = 0; v < (1u << bits); ++v) {
        double d = distance(v);
        if (d < best) {
            second = best;
            best = d;
            arg = v;
        } else if (d < second) {
            second = d;
        }
    }
    st.min_margin = std::min(st.min_margin, second - best);
    return arg;
}

double envelope(const std::vector<Ridge>& front, double x) {
    double f = -kInf;
    for (const Ridge& r : front) f = std::max(f, mt::height_at(r.points, x));
    return f;
}

bool round_trip(const mt::Config& c, const Hash256& hash, uint32_t width, uint32_t height, Stats& st) {
    double sc = static_cast<double>(width) / kCanvasWidth;
    Scene scene;
    scene.ridges = mt::generate(c, hash);
    std::vector<ParsedRidge> parsed = parse_ridges(render_svg(scene, width, height));

    mt::Values v;
    v.three_ridge = parsed.size() == 3;
    const auto& layout = mt::layout(v.three_ridge);
    std::vector<Ridge> front;

    for (size_t i = 0; i < layout.size(); ++i) {
        const mt::RidgeLayout& l = layout[i];
        const ParsedRidge& pr = parsed[i];
        const mt::Params p = mt::ridge_params(c.params, v.three_ridge, static_cast<int>(i));
        double s = p.scale;
        int depth = p.resolution;
        if (pr.xs.size() != (size_t{1} << depth) + 1) return false;
        for (size_t j = 1; j < pr.xs.size(); ++j) {
            if (!(pr.xs[j] > pr.xs[j - 1])) st.monotonic = false;
        }
        auto rel = [&](size_t j) { return pr.ys[0] - pr.ys[j]; };  // up is positive

        // Only bit-driven points are decoded; detail points are a fixed
        // function of them and carry no information.
        mt::RidgeValues rv;
        size_t last = pr.xs.size() - 1;
        size_t mid = size_t{1} << (depth - 1);
        // Distance between a candidate skeleton point and the parsed vertex.
        auto off = [&](const Point& q, size_t idx) {
            return std::max(std::fabs(pr.xs[idx] - q.x * sc), std::fabs(rel(idx) - s * q.h * sc));
        };
        uint32_t tilt = 0, first = 0;
        if (p.generator == mt::Generator::kSummit) {
            // Decode x from the summit's position, rise from its height, and
            // drop from the right edge's height.
            mt::SummitFields f = mt::split_summit(l, 0, 0);
            auto sk = [&]() {
                mt::join_summit(l, f, tilt, first);
                return mt::skeleton(l, tilt, first, p);
            };
            f.x = pick(f.x_bits, [&](uint32_t cand) { f.x = cand; return std::fabs(pr.xs[mid] - sk()[1].x * sc); }, st);
            f.rise = pick(f.rise_bits, [&](uint32_t cand) { f.rise = cand; return off(sk()[1], mid); }, st);
            f.drop = pick(f.drop_bits, [&](uint32_t cand) { f.drop = cand; return off(sk()[2], last); }, st);
            mt::join_summit(l, f, tilt, first);
        } else {
            tilt = pick(l.tilt, [&](uint32_t cand) {
                return std::fabs(rel(last) - s * mt::tilt_value(cand, l.tilt, p) * sc);
            }, st);
            first = pick(l.levels[0], [&](uint32_t cand) { return off(mt::skeleton(l, tilt, cand, p)[1], mid); }, st);
        }
        rv.tilt = tilt;
        rv.points.push_back(first);

        std::vector<Point> pts = mt::skeleton(l, tilt, first, p);
        for (size_t k = 1; k < l.levels.size(); ++k) {
            int b = l.levels[k];
            std::vector<Point> next;
            for (size_t j = 0; j + 1 < pts.size(); ++j) {
                size_t idx = (2 * j + 1) << (depth - 1 - static_cast<int>(k));
                uint32_t val = pick(b, [&](uint32_t cand) {
                    Point q = mt::subdivide(pts[j], pts[j + 1], cand, b, p);
                    return std::max(std::fabs(pr.xs[idx] - q.x * sc), std::fabs(rel(idx) - s * q.h * sc));
                }, st);
                rv.points.push_back(val);
                next.push_back(pts[j]);
                next.push_back(mt::subdivide(pts[j], pts[j + 1], val, b, p));
            }
            next.push_back(pts.back());
            pts.swap(next);
        }

        mt::Placement pl = mt::place(c, v.three_ridge, static_cast<int>(i), rv, front);
        rv.elevation = pick(l.elevation, [&](uint32_t cand) {
            double offset = pl.base + (p.share[i] * pl.room) * (static_cast<double>(cand) / mt::steps(l.elevation));
            return std::fabs(pr.ys[0] - (kCanvasHeight - offset) * sc);
        }, st);

        const auto& pal = palette(l.palette);
        auto it = std::find(pal.begin(), pal.end(), pr.fill);
        if (it == pal.end()) return false;
        rv.color = static_cast<uint32_t>(it - pal.begin());

        // Rebuild the ridge exactly as the generator does, for the next ridge's envelope.
        pl = mt::place(c, v.three_ridge, static_cast<int>(i), rv, front);
        Ridge r;
        r.points = pl.shape;
        for (Point& q : r.points) q.h = q.h + pl.offset;

        double top = -kInf;
        for (const Point& q : r.points) top = std::max(top, q.h);
        st.min_clearance = std::min(st.min_clearance, p.ceiling - top);
        if (i > 0) {
            double hmin = kInf, hmax = -kInf;
            for (const Point& q : pl.shape) {
                hmin = std::min(hmin, q.h);
                hmax = std::max(hmax, q.h);
            }
            double required = std::max(p.visibility * (hmax - hmin), p.min_exposure);
            double exposure = -kInf;
            for (const Point& q : r.points) exposure = std::max(exposure, q.h - envelope(front, q.x));
            for (const Ridge& fr : front) {
                for (const Point& q : fr.points) {
                    exposure = std::max(exposure, mt::height_at(r.points, q.x) - envelope(front, q.x));
                }
            }
            st.min_extra_exposure = std::min(st.min_extra_exposure, exposure - required);
        }
        front.push_back(r);
        v.ridges.push_back(rv);
    }

    Hash256 back;
    mt::encode(v, back);
    return same_mountain_bits(hash, back);
}

void check_round_trip(const mt::Config& c, int samples, bool verbose) {
    std::mt19937_64 rng(42);
    for (int mode : {2, 3}) {
        for (uint32_t w : {1024u, 2048u}) {
            Stats st;
            int bad = 0;
            for (int i = 0; i < samples; ++i) {
                if (!round_trip(c, random_hash(rng, mode), w, w * 3 / 4, st)) ++bad;
            }
            if (verbose || bad > 0) {
                std::printf("    %d ridges, %ux%u: %d mismatches; min decode margin %.3f px; "
                            "min ceiling clearance %.2f; min exposure beyond required %.4f\n",
                            mode, w, w * 3 / 4, bad, st.min_margin, st.min_clearance, st.min_extra_exposure);
            }
            check(bad == 0, "round trip mismatch");
            check(st.monotonic, "serialized x not strictly increasing");
            check(st.min_clearance >= 0.0, "ridge above ceiling");
            check(st.min_extra_exposure >= -1e-9, "ridge less exposed than required");
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    int samples = argc > 1 ? std::atoi(argv[1]) : 20000;
    const mt::Config& def = mt::default_config();

    check_coverage();
    std::printf("== Placement budget, default configuration\n");
    check_budget(def, true);
    std::printf("== Coordinate separation at 1024x768 (worst case)\n");
    check_separation(def, true);
    check_palettes();
    std::printf("== Round trip, default configuration (%d hashes per mode and size)\n", samples);
    check_round_trip(def, samples, true);

    int variant_samples = std::max(1, samples / 10);
    std::printf("== Gallery variants (budget + %d round trips per mode and size each)\n", variant_samples);
    for (const Variant& var : variants()) {
        std::printf("  [%s] %s\n", var.group.c_str(), var.label.c_str());
        mt::Config c = mt::make_config(var.params);
        check_budget(c, false);
        check_separation(c, false);
        check_round_trip(c, variant_samples, false);
    }

    std::printf(failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
