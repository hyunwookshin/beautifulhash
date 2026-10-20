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
#include <unordered_set>
#include <vector>

#include "generate.hpp"
#include "hills.hpp"
#include "trees.hpp"
#include "mountains.hpp"
#include "celestial.hpp"
#include "fog.hpp"
#include "palette.hpp"
#include "partition.hpp"
#include "variants.hpp"
#include "vegetation.hpp"

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
            // Biome textures (conifers, canopy) split segments unevenly.
            final_dx *= mt::texture_min_fraction(c.params, three, static_cast<int>(i));
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
    std::printf("== Palettes (one color family per biome and season)\n");
    for (Theme t : kThemes) {
        for (Season s : kSeasons) {
            size_t entries = 0, distinct_total = 0;
            for (int id : {kPaletteFront, kPaletteMiddle, kPaletteFar}) {
                const auto& pal = palette(t, s, id);
                std::set<uint32_t> distinct(pal.begin(), pal.end());
                entries += pal.size();
                distinct_total += distinct.size();
                check(distinct.size() == pal.size(), "palette has duplicate colors");
            }
            std::printf("  %-9s %-6s: %zu entries, %zu distinct #RRGGBB values\n", theme_name(t), season_name(s),
                        entries, distinct_total);
        }
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
        // A gradient fill (url(#...)) carries the palette color as its top stop.
        size_t f = svg.find("fill=\"", pos) + 6;
        if (svg.compare(f, 4, "url(") == 0) {
            size_t g = svg.rfind("<linearGradient id=\"ridge-", pos);
            f = svg.find("stop-color=\"#", g) + 13;
        } else {
            f += 1;  // skip '#'
        }
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
        int depth = mt::styled_depth(c.params, v.three_ridge, static_cast<int>(i));
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

        const auto& pal = palette(c.params.theme, c.params.season, l.palette);
        auto it = std::find(pal.begin(), pal.end(), pr.fill);
        if (it == pal.end()) return false;
        rv.color = static_cast<uint32_t>(it - pal.begin());

        // Rebuild the ridge exactly as the generator does, for the next ridge's envelope.
        pl = mt::place(c, v.three_ridge, static_cast<int>(i), rv, front);
        Ridge r;
        r.points = pl.shape;
        for (Point& q : r.points) q.h = q.h + pl.offset;

        // Ceiling check includes biome styling (conifer and canopy tips); placement
        // and the exposure check use the unstyled ridge, as the generator does.
        std::vector<Point> styled = mt::style_points(r.points, c.params, v.three_ridge, static_cast<int>(i), rv);
        double top = -kInf;
        for (const Point& q : styled) top = std::max(top, q.h);
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

// ---- 6. Hills -------------------------------------------------------------

bool same_hill_bits(const Hash256& a, const Hash256& b) {
    for (int i = kHills.begin; i < kHills.begin + kHills.width; ++i) {
        if (a.bit(i) != b.bit(i)) return false;
    }
    return true;
}

std::vector<ParsedRidge> parse_hills(const std::string& svg) {
    std::vector<ParsedRidge> out;
    size_t pos = 0;
    while ((pos = svg.find("<path class=\"hill\"", pos)) != std::string::npos) {
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
        r.xs.resize(r.xs.size() - 2);
        r.ys.resize(r.ys.size() - 2);
        out.push_back(r);
        pos = end;
    }
    std::reverse(out.begin(), out.end());  // SVG is back to front
    return out;
}

void check_hills(int samples) {
    namespace hl = bh::hills;
    const hl::Params& p = hl::default_params();
    std::printf("== Hills\n");
    for (bool two : {false, true}) {
        int total = 1;
        for (const auto& l : hl::layout(two)) total += l.x + l.crest + l.left + l.right + l.shape + l.color;
        std::printf("  %s-hill total: %d bits\n", two ? "two" : "one", total);
        check(total == kHills.width, "hill layout does not consume exactly 40 bits");
    }
    std::mt19937_64 rng(11);
    for (int i = 0; i < 10000; ++i) {
        Hash256 h = random_hash(rng, 0);
        Hash256 back;
        hl::encode(hl::decode(h), back);
        if (!same_hill_bits(h, back)) {
            check(false, "hill decode/encode does not round-trip");
            break;
        }
    }

    // Static guarantees: the back hill's interval is never empty, and every
    // mountain silhouette stays above every hill.
    const mt::Params& mp = mt::default_config().params;
    std::printf("  cap %.1f; front max %.1f + exposure %.1f = %.1f; mountain ground %.1f\n", p.cap, p.front_max,
                p.exposure, p.front_max + p.exposure, mp.ground);
    check(p.front_max + p.exposure < p.cap, "back hill crest interval can be empty");
    check(mp.ground > p.cap, "hills can reach the mountains' lowest silhouette");

    for (Theme t : kThemes) {
        for (Season s : kSeasons) {
            for (int id : {kHillSingle, kHillFront, kHillBack}) {
                const auto& pal = hill_palette(t, s, id);
                std::set<uint32_t> distinct(pal.begin(), pal.end());
                if (distinct.size() != pal.size()) {
                    std::printf("  %s %s hill palette %d: %zu of %zu distinct\n", theme_name(t), season_name(s), id,
                                distinct.size(), pal.size());
                    check(false, "hill palette has duplicate colors");
                }
            }
        }
    }

    // Round trip through SVG text for every biome and season.
    double min_margin = kInf, min_exposure = kInf, max_back = -kInf, max_front = -kInf, min_h = kInf;
    int bad = 0;
    for (Theme t : kThemes) {
        for (Season s : kSeasons) {
            for (int i = 0; i < samples; ++i) {
                Hash256 hash = random_hash(rng, 0);
                Scene scene;
                scene.hills = hl::generate(hash, t, s);
                std::vector<ParsedRidge> parsed = parse_hills(render_svg(scene, 1024, 768));
                hl::Values v;
                v.two = parsed.size() == 2;
                const auto& layout = hl::layout(v.two);
                std::vector<Hill> front;
                double sc = 0.5;
                for (size_t k = 0; k < layout.size(); ++k) {
                    const hl::HillLayout& l = layout[k];
                    const ParsedRidge& pr = parsed[k];
                    size_t crest = static_cast<size_t>(p.samples), last = pr.xs.size() - 1;
                    auto y_at = [&](const Hill& h, size_t j) { return (kCanvasHeight - h.points[j].h) * sc; };
                    hl::HillValues hv;
                    Stats st;
                    auto field = [&](uint32_t& slot, int bits, auto dist) {
                        slot = pick(bits, [&](uint32_t c) {
                            slot = c;
                            return dist(hl::build(p, v.two, static_cast<int>(k), hv, front, t, s));
                        }, st);
                    };
                    field(hv.x, l.x, [&](const Hill& h) { return std::fabs(pr.xs[crest] - h.points[crest].x * sc); });
                    field(hv.crest, l.crest, [&](const Hill& h) { return std::fabs(pr.ys[crest] - y_at(h, crest)); });
                    field(hv.left, l.left, [&](const Hill& h) { return std::fabs(pr.ys[0] - y_at(h, 0)); });
                    field(hv.right, l.right, [&](const Hill& h) { return std::fabs(pr.ys[last] - y_at(h, last)); });
                    field(hv.shape, l.shape, [&](const Hill& h) {
                        double worst = 0.0;
                        for (size_t j = 0; j <= last; ++j) worst = std::max(worst, std::fabs(pr.ys[j] - y_at(h, j)));
                        return worst;
                    });
                    const auto& pal = hill_palette(t, s, l.palette);
                    auto it = std::find(pal.begin(), pal.end(), pr.fill);
                    if (it == pal.end()) {
                        ++bad;
                        break;
                    }
                    hv.color = static_cast<uint32_t>(it - pal.begin());
                    min_margin = std::min(min_margin, st.min_margin);

                    Hill h = hl::build(p, v.two, static_cast<int>(k), hv, front, t, s);
                    double top = -kInf;
                    for (const Point& q : h.points) {
                        top = std::max(top, q.h);
                        min_h = std::min(min_h, q.h);
                    }
                    bool back = !v.two || k == 1;
                    if (back) max_back = std::max(max_back, top);
                    else max_front = std::max(max_front, top);
                    if (v.two && k == 1) {
                        min_exposure = std::min(min_exposure, h.profile.crest_h -
                                                                   mt::height_at(front[0].points, h.profile.crest_x));
                    }
                    front.push_back(h);
                    v.hills.push_back(hv);
                }
                Hash256 back_hash;
                hl::encode(v, back_hash);
                if (!same_hill_bits(hash, back_hash)) ++bad;
            }
        }
    }
    std::printf("  round trip (%d hashes x 16 biome/season pairs): %d mismatches; min decode margin %.3f px\n",
                samples, bad, min_margin);
    std::printf("  highest back hill %.2f (cap %.1f); highest front hill %.2f; lowest point %.2f; "
                "min back crest exposure %.2f\n", max_back, p.cap, max_front, min_h, min_exposure);
    check(bad == 0, "hill round trip mismatch");
    check(max_back <= p.cap, "back hill above cap");
    check(max_front <= p.front_max, "front hill above its range");
    check(min_h >= 0.0, "hill below the canvas bottom");
    check(min_exposure >= p.exposure - 1e-9, "back hill less exposed than required");
}

// ---- 7. Trees -------------------------------------------------------------

bool same_tree_bits(const Hash256& a, const Hash256& b) {
    for (int i = kTrees.begin; i < kTrees.begin + kTrees.width; ++i) {
        if (a.bit(i) != b.bit(i)) return false;
    }
    return true;
}

std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0, nl;
    while ((nl = s.find('\n', start)) != std::string::npos) {
        out.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return out;
}

// Each tree's fragment from the SVG, by its index.
std::vector<std::vector<std::string>> parse_trees(const std::string& svg) {
    std::vector<std::pair<int, std::vector<std::string>>> found;
    size_t pos = 0;
    while ((pos = svg.find("<g class=\"tree\" id=\"tree-", pos)) != std::string::npos) {
        int id = std::atoi(svg.c_str() + pos + 25);
        size_t end = svg.find("</g>\n", pos) + 5;
        found.push_back({id - 1, split_lines(svg.substr(pos, end - pos))});
        pos = end;
    }
    std::vector<std::vector<std::string>> out(found.size());
    for (auto& f : found) out[static_cast<size_t>(f.first)] = f.second;
    return out;
}

bool prefix_equal(const std::vector<std::string>& a, const std::vector<std::string>& b, size_t n) {
    if (a.size() < n || b.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// Decodes every tree field by exact comparison of SVG lines. Returns the
// number of (arrangement, size) pairs that reproduce the trees exactly; the
// decoded values are left in `out` for the last match.
int decode_trees(const std::vector<std::vector<std::string>>& frags, const std::vector<Hill>& hills, Theme t,
                 Season s, double sc, bh::trees::Values& out) {
    namespace tr = bh::trees;
    bool two = hills.size() == 2;
    int n = static_cast<int>(frags.size());
    int matches = 0;
    for (uint32_t a = 0; a < 4; ++a) {
        for (uint32_t z = 0; z < 4; ++z) {
            tr::Values v;
            v.count = n;
            v.arrangement = a;
            v.size = z;
            v.leaf_bits = tr::leafy(t, s) ? 4 : 0;
            tr::Plan p = tr::plan(n, a, tr::tree_bit_budget(n, t, s), two);
            for (int i = 0; i < n; ++i) v.trees.push_back(tr::tree_fields(p.bits[i], tr::species_of(t, i, a)));
            bool ok = true;
            for (int i = 0; i < n && ok; ++i) {
                tr::TreeValues& tv = v.trees[static_cast<size_t>(i)];
                auto lines = [&]() { return split_lines(tr::tree_svg(tr::build(v, p, i, hills, t, s), i, sc)); };
                bool cactus = tr::species_of(t, i, a) == tr::Species::kCactus;
                size_t per_fork = cactus ? 4 : 2;
                // Girth and height together set the trunk (line 1).
                bool found = false;
                for (uint32_t g = 0; g < (1u << tv.girth_bits) && !found; ++g) {
                    for (uint32_t h = 0; h < (1u << tv.height_bits) && !found; ++h) {
                        tv.girth = g;
                        tv.height = h;
                        found = prefix_equal(lines(), frags[static_cast<size_t>(i)], 2);
                    }
                }
                ok = found;
                for (size_t k = 0; k < tv.forks.size() && ok; ++k) {
                    tr::Fork& f = tv.forks[k];
                    found = false;
                    for (uint32_t r = 0; r < (1u << f.ratio_bits) && !found; ++r) {
                        for (uint32_t d = 0; d < (1u << f.side_bits) && !found; ++d) {
                            for (uint32_t l = 0; l < (1u << f.length_bits) && !found; ++l) {
                                f.ratio = r;
                                f.side = d;
                                f.length = l;
                                found = prefix_equal(lines(), frags[static_cast<size_t>(i)], 2 + per_fork * (k + 1));
                            }
                        }
                    }
                    ok = found;
                }
            }
            // Every branch field is decoded; the leaf palette colors the dots
            // across all trees, so try each palette on the whole set.
            for (uint32_t leaf = 0; ok && leaf < (1u << v.leaf_bits); ++leaf) {
                v.leaf = leaf;
                std::vector<Tree> built;
                for (int i = 0; i < n; ++i) built.push_back(tr::build(v, p, i, hills, t, s));
                tr::paint_leaves(built, v, s);
                bool all = true;
                for (int i = 0; i < n && all; ++i) all = split_lines(tr::tree_svg(built[static_cast<size_t>(i)], i, sc)) == frags[static_cast<size_t>(i)];
                if (all) {
                    ++matches;
                    out = v;
                }
            }
        }
    }
    return matches;
}

void check_trees(int samples) {
    namespace tr = bh::trees;
    std::printf("== Trees\n");
    std::mt19937_64 rng(13);
    double worst_ratio = 1e9;
    for (bool two : {false, true}) {
        for (int i = 0; i < 5000; ++i) {
            Hash256 h = random_hash(rng, 0);
            Hash256 back;
            Theme t = kThemes[i % 4];
            Season s = kSeasons[(i / 4) % 4];
            tr::encode(tr::decode(h, two, t, s), back);
            if (!same_tree_bits(h, back)) {
                check(false, "tree decode/encode does not round-trip");
                break;
            }
        }
        for (int n = 1; n <= 10; ++n) {
            for (uint32_t a = 0; a < 4; ++a) {
                for (bool leafy : {false, true}) {
                    int remaining = tr::tree_bit_budget(n, leafy ? Theme::kTemperate : Theme::kAlpine, Season::kSummer);
                    tr::Plan p = tr::plan(n, a, remaining, two);
                    int sum = 0;
                    for (int b : p.bits) sum += b;
                    check(sum == remaining, "tree plan does not allocate every bit");
                    // Front (or only) hill: every slope tree at least 1.5x every rim tree.
                    double min_slope = 1e9, max_rim = 0.0;
                    for (int i = 0; i < n; ++i) {
                        tr::Group g = p.groups[static_cast<size_t>(i)];
                        if (g == tr::Group::kFrontInterior) min_slope = std::min(min_slope, p.scale[static_cast<size_t>(i)]);
                        if (g == tr::Group::kFrontRim) max_rim = std::max(max_rim, p.scale[static_cast<size_t>(i)]);
                    }
                    worst_ratio = std::min(worst_ratio, max_rim > 0.0 && min_slope < 1e9 ? min_slope / max_rim : 1e9);
                }
            }
        }
    }
    std::printf("  front hill: smallest slope tree / largest rim tree = %.3f (need >= 1.5)\n", worst_ratio);
    check(worst_ratio >= 1.5, "front slope trees not 1.5x the rim trees");
    for (Season s : kSeasons) {
        if (s == Season::kWinter) continue;
        std::set<uint32_t> firsts;
        for (int id = 0; id < kLeafPalettes; ++id) {
            const auto& pal = leaf_palette(s, id);
            firsts.insert(pal[0]);
            check(std::set<uint32_t>(pal.begin(), pal.end()).size() == 6, "leaf palette has duplicate colors");
        }
        check(firsts.size() == static_cast<size_t>(kLeafPalettes), "leaf palettes share a first color");
    }
    int bad = 0, ambiguous = 0, trees_seen = 0;
    for (Theme t : kThemes) {
        for (Season s : kSeasons) {
            for (int i = 0; i < samples; ++i) {
                Hash256 hash = random_hash(rng, 0);
                Scene scene;
                scene.hills = bh::hills::generate(hash, t, s);
                scene.trees = tr::generate(hash, scene.hills, t, s);
                trees_seen += static_cast<int>(scene.trees.size());
                std::string svg = render_svg(scene, 1024, 768);
                tr::Values v;
                int m = decode_trees(parse_trees(svg), scene.hills, t, s, 0.5, v);
                if (m != 1) {
                    if (m > 1) ++ambiguous;
                    if (bad < 5) {
                        std::printf("    no unique decode: %s %s %s, %zu trees\n", theme_name(t), season_name(s),
                                    hash.to_hex().c_str(), scene.trees.size());
                    }
                    ++bad;
                    continue;
                }
                Hash256 back;
                tr::encode(v, back);
                if (!same_tree_bits(hash, back)) ++bad;
            }
        }
    }
    std::printf("  round trip (%d hashes x 16 biome/season pairs, %d trees): %d mismatches (%d ambiguous)\n",
                samples, trees_seen, bad, ambiguous);
    check(bad == 0, "tree round trip mismatch");
}

// ---- 8. Fog ---------------------------------------------------------------

// The fog gradient's stops: tint and opacities in hundredths.
struct ParsedFog {
    uint32_t rgb[2];
    int opacity[2];
};

ParsedFog parse_fog(const std::string& svg) {
    ParsedFog f{};
    size_t pos = svg.find("<linearGradient id=\"fog\"");
    for (int k = 0; k < 2; ++k) {
        pos = svg.find("stop-color=\"#", pos) + 13;
        f.rgb[k] = static_cast<uint32_t>(std::strtoul(svg.substr(pos, 6).c_str(), nullptr, 16));
        pos = svg.find("stop-opacity=\"", pos) + 14;
        f.opacity[k] = static_cast<int>(std::lround(std::strtod(svg.c_str() + pos, nullptr) * 100.0));
    }
    return f;
}

int count_of(const std::string& s, const std::string& needle) {
    int n = 0;
    for (size_t pos = 0; (pos = s.find(needle, pos)) != std::string::npos; pos += needle.size()) ++n;
    return n;
}

void check_fog() {
    namespace fg = bh::fog;
    std::printf("== Fog\n");
    check(fg::kDensityBits + fg::kTintBits + fg::kMistBits == kFog.width, "fog layout does not consume exactly 8 bits");
    for (Theme t : kThemes) {
        for (Season s : kSeasons) {
            const auto& pal = fog_palette(t, s);
            check(std::set<uint32_t>(pal.begin(), pal.end()).size() == pal.size(), "fog palette has duplicate colors");
        }
    }

    // Every one of the 256 fog values, in every biome and season, decodes back
    // from the SVG text; the layers sit in every depth gap.
    std::mt19937_64 rng(13);
    int bad = 0, bad_layers = 0, lo = 100, hi = 0;
    for (Theme t : kThemes) {
        for (Season s : kSeasons) {
            for (uint32_t bits = 0; bits < 256; ++bits) {
                Hash256 hash = random_hash(rng, 0);
                for (int b = 0; b < kFog.width; ++b) hash.set_bit(kFog.begin + b, (bits >> (7 - b)) & 1);
                Scene scene;
                if (bits % 16 == 0) scene.ridges = mt::generate(mt::default_config(), hash);
                scene.hills = bh::hills::generate(hash, t, s);
                scene.fog = fg::generate(hash, t, s);
                std::string svg = render_svg(scene, 1024, 768);
                int ridges = static_cast<int>(scene.ridges.size()), hills = static_cast<int>(scene.hills.size());
                int want = (ridges > 0 ? ridges - 1 : 0) + hills;
                if (count_of(svg, "<rect class=\"fog\"") != want) ++bad_layers;

                ParsedFog f = parse_fog(svg);
                lo = std::min(lo, f.opacity[0]);
                hi = std::max(hi, f.opacity[1]);
                const auto& pal = fog_palette(t, s);
                auto it = std::find(pal.begin(), pal.end(), f.rgb[0]);
                int sum = f.opacity[0] + f.opacity[1], diff = f.opacity[1] - f.opacity[0];
                if (it == pal.end() || f.rgb[1] != f.rgb[0] || sum % 6 != 0 || diff % 2 != 0) {
                    ++bad;
                    continue;
                }
                fg::Values v;
                v.tint = static_cast<uint32_t>(it - pal.begin());
                v.density = static_cast<uint32_t>((sum / 2 - 27) / 3);
                v.mist = static_cast<uint32_t>(diff / 2);
                Hash256 back = hash;
                fg::encode(v, back);
                if (back.bytes != hash.bytes) ++bad;
            }
        }
    }
    std::printf("  round trip (256 values x 16 biome/season pairs): %d mismatches; %d misplaced layer sets; "
                "opacity %d-%d%%\n", bad, bad_layers, lo, hi);
    check(bad == 0, "fog round trip mismatch");
    check(bad_layers == 0, "fog layer missing from a depth gap");
}

// ---- 9. Vegetation --------------------------------------------------------

// A plant line without anything its color affects (the fill, and a leaf's
// width), so positions can be matched before the palette is known: a leaf's
// transform, or a tuft's blades.
std::string without_fill(const std::string& line) {
    size_t m = line.find(" transform=");
    return line.substr(m != std::string::npos ? m : line.find("\" d=\""));
}

void check_vegetation(int samples) {
    namespace vg = bh::vegetation;
    namespace tr = bh::trees;
    std::printf("== Vegetation\n");
    check(vg::kPaletteBits + vg::kDensityBits + vg::kSpreadBits + vg::kSeedBits == kVegetation.width,
          "vegetation layout does not consume exactly 24 bits");
    for (Theme t : kThemes)
    for (Season s : kSeasons) {
        std::set<std::vector<uint32_t>> sets;
        for (int id = 0; id < kVegetationPalettes; ++id) {
            const auto& pal = vegetation_palette(t, s, id);
            check(std::set<uint32_t>(pal.begin(), pal.end()).size() == pal.size(),
                  "vegetation palette has duplicate colors");
            sets.insert(pal);
        }
        check(sets.size() == static_cast<size_t>(kVegetationPalettes), "two vegetation palettes are identical");
    }

    // Round trip through SVG text. The first plant generated is found among
    // the drawn ones (they are depth sorted) for every spread and seed; the
    // count gives the density; the palette is tried on the full set.
    std::mt19937_64 rng(17);
    int bad = 0, plants = 0;
    for (Theme t : kThemes) {
        for (Season s : kSeasons) {
            for (int i = 0; i < samples; ++i) {
                Hash256 hash = random_hash(rng, 0);
                Scene scene;
                scene.hills = bh::hills::generate(hash, t, s);
                scene.trees = tr::generate(hash, scene.hills, t, s);
                scene.vegetation = vg::generate(hash, scene.trees, scene.hills, t, s);
                std::string svg = render_svg(scene, 1024, 768);
                size_t g = svg.find("<g class=\"vegetation\">\n");
                std::string block = svg.substr(g + 23, svg.find("</g>\n", g) - g - 23);
                std::vector<std::string> lines = split_lines(block);
                plants += static_cast<int>(lines.size());
                std::unordered_set<std::string> keys;
                for (const std::string& l : lines) keys.insert(without_fill(l));

                std::vector<vg::Anchor> anchors = vg::anchors(scene.trees, scene.hills);
                vg::Values v;
                int found = 0;
                for (uint32_t d = 0; d < 4; ++d) {
                    if (static_cast<size_t>(vg::per_cluster(d)) * anchors.size() == lines.size()) v.density = d;
                }
                for (uint32_t sp = 0; sp < 4; ++sp) {
                    for (uint32_t seed = 0; seed < (1u << vg::kSeedBits); ++seed) {
                        vg::Values c{0, v.density, sp, seed};
                        std::vector<Plant> first = vg::scatter(c, anchors, scene.hills[0], t, s, 1);
                        first[0].rgb = 0;
                        if (!keys.count(without_fill(vg::plant_svg(first[0], 0.5)))) continue;
                        for (uint32_t p = 0; p < static_cast<uint32_t>(kVegetationPalettes); ++p) {
                            c.palette = p;
                            std::string text;
                            for (const Plant& q : vg::build(c, scene.trees, scene.hills, t, s)) text += vg::plant_svg(q, 0.5) + "\n";
                            if (text == block) {
                                ++found;
                                v = c;
                            }
                        }
                    }
                }
                if (found != 1) {
                    if (bad < 5) std::printf("    no unique decode (%d): %s %s %s\n", found, theme_name(t), season_name(s), hash.to_hex().c_str());
                    ++bad;
                    continue;
                }
                Hash256 back = hash;
                vg::encode(v, back);
                if (back.bytes != hash.bytes) ++bad;
            }
        }
    }
    std::printf("  round trip (%d hashes x 16 biome/season pairs, %d plants): %d mismatches\n", samples, plants, bad);
    check(bad == 0, "vegetation round trip mismatch");
}

// ---- 10. Celestial ---------------------------------------------------------

// The SVG lines the sun shapes: the front hill's bands and the tree shadows.
std::string sunlit(const std::string& svg) {
    std::string out;
    for (const std::string& l : split_lines(svg)) {
        if (l.find("class=\"hill-band\"") != std::string::npos || l.find("class=\"shadow\"") != std::string::npos) out += l + "\n";
    }
    return out;
}

void check_celestial(int samples) {
    namespace cl = bh::celestial;
    std::printf("== Celestial\n");
    check(cl::kAzimuthBits + cl::kElevationBits == kCelestial.width, "celestial layout does not consume exactly 8 bits");

    // The sun is decoded from the bands and shadows alone: every one of the 256
    // suns is tried on the same hills and trees, and exactly one must match.
    // Tree foliage is dropped first; only the limbs cast shadows.
    std::mt19937_64 rng(19);
    int bad = 0, bare = 0, shadows = 0;
    // The second half drops the trees, so the bands alone must carry the sun.
    for (int i = 0; i < 2 * samples; ++i) {
        Hash256 hash = random_hash(rng, 0);
        Theme t = kThemes[i % 4];
        Season s = kSeasons[(i / 4) % 4];
        Scene base;
        base.hills = bh::hills::generate(hash, t, s);
        if (i < samples) base.trees = bh::trees::generate(hash, base.hills, t, s);
        for (Tree& tr : base.trees) {
            tr.shapes.clear();
            tr.leaves = 0;
        }
        auto lit = [&](const Sun& sun) {
            Scene c = base;
            c.sun = sun;
            bh::hills::add_bands(c.hills[0], sun);
            cl::cast_shadows(c.trees, c.hills[0], sun);
            return sunlit(render_svg(c, 1024, 768));
        };
        std::string actual = lit(cl::generate(hash));
        if (actual.find("class=\"shadow\"") == std::string::npos) ++bare;
        else ++shadows;
        int found = 0;
        cl::Values v;
        for (uint32_t a = 0; a < 16; ++a) {
            for (uint32_t e = 0; e < 16; ++e) {
                if (lit(cl::sun({a, e})) == actual) {
                    ++found;
                    v = {a, e};
                }
            }
        }
        Hash256 back = hash;
        if (found == 1) cl::encode(v, back);
        if (found != 1 || back.bytes != hash.bytes) {
            if (bad < 5) std::printf("    no unique decode (%d): %s\n", found, hash.to_hex().c_str());
            ++bad;
        }
    }
    std::printf("  round trip (%d scenes, %d with shadows, %d bare front hills): %d mismatches\n", 2 * samples, shadows,
                bare, bad);
    check(bad == 0, "celestial round trip mismatch");
}

}  // namespace

int main(int argc, char** argv) {
    // Default: a quick run (a tenth of the full sampling). `make verify-full`
    // passes 20000 for the full run before releases.
    int samples = argc > 1 ? std::atoi(argv[1]) : 2000;
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

    check_hills(std::max(1, samples / 20));
    check_trees(std::max(1, samples / 100));
    check_fog();
    check_vegetation(std::max(1, samples / 500));
    check_celestial(std::max(16, samples / 50));

    std::printf(failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
