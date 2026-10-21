#include "trees.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "format.hpp"
#include "mountains.hpp"
#include "palette.hpp"
#include "partition.hpp"

namespace bh::trees {

namespace {

// Four diversity-biased group sequences. The first trees spread across
// depths; groups repeat once the count exceeds four.
constexpr Group FR = Group::kFrontRim, BR = Group::kBackRim, FI = Group::kFrontInterior, BI = Group::kBackInterior;
const Group kSequence[4][10] = {
    {FR, BR, FI, FR, BI, FI, BR, FR, FI, BR},
    {BR, FR, FI, BR, FR, BI, FI, FR, BR, FI},
    {FI, FR, BR, FI, BI, FR, BR, FI, FR, BI},
    {FR, FI, BR, BI, FR, FI, BR, FR, BI, FI},
};

bool is_back(Group g) { return g == Group::kBackRim || g == Group::kBackInterior; }
bool is_interior(Group g) { return g == Group::kFrontInterior || g == Group::kBackInterior; }

// Bit-allocation weights: nearer, larger trees get more branching detail.
int weight(Group g, bool two) {
    if (!two) return is_interior(g) ? 8 : 6;
    switch (g) {
        case Group::kFrontInterior: return 8;
        case Group::kFrontRim: return 6;
        case Group::kBackInterior: return 4;
        case Group::kBackRim: return 3;
    }
    return 1;
}

// Perspective scale: a considerable jump between the back and front hills. On
// the front (or only) hill, slope trees stand nearest the viewer: with the
// role prominences (0.8-1.15), the smallest slope tree is still at least 1.5
// times the largest rim tree (1.4 * 0.8 vs 0.6 * 1.15; one hill: 1.3 * 0.8 vs
// 0.55 * 1.15).
double perspective(Group g, bool two) {
    if (!two) return is_interior(g) ? 1.3 : 0.55;
    switch (g) {
        case Group::kFrontInterior: return 1.4;
        case Group::kFrontRim: return 0.6;
        case Group::kBackInterior: return 0.45;
        case Group::kBackRim: return 0.38;
    }
    return 1.0;
}

const double kSizeClass[4] = {0.8, 0.95, 1.1, 1.25};
const int kLongCounts[4] = {1, 8, 9, 10};  // counts with 4-bit codes
constexpr double kTan22_5 = 0.41421356237309515;  // tan(22.5 degrees): a 45-degree fork
constexpr size_t kMaxForks = 511;
constexpr size_t kMaxConiferForks = 127;

double unit_of(uint32_t v, int bits) { return bits == 0 ? 0.5 : v / static_cast<double>((1u << bits) - 1u); }
double frac(double x) { return x - std::floor(x); }

}  // namespace

Species species_of(Theme t, int index, uint32_t arrangement) {
    switch (t) {
        case Theme::kAlpine:
        case Theme::kTundra: return Species::kConifer;
        case Theme::kTemperate: return Species::kDeciduous;
        case Theme::kDesert: return (index + static_cast<int>(arrangement)) % 2 == 0 ? Species::kCactus : Species::kJoshua;
    }
    return Species::kConifer;
}

namespace {

struct Range {
    double lo, hi;
};

// Split-ratio range (the large branch's share) per species.
Range ratio_range(Species s) {
    switch (s) {
        case Species::kConifer: return {0.85, 0.97};    // strong, nearly straight trunk with side twigs
        case Species::kDeciduous: return {0.62, 0.90};  // no near-even splits: avoids a Y lattice
        case Species::kCactus: return {0.60, 0.90};
        case Species::kJoshua: return {0.60, 0.85};
    }
    return {0.5, 0.8};
}

// Rotates a unit direction by the angle whose half-angle tangent is t, using
// the exact rational form, so there is no trigonometry or square root.
Point rotate(Point d, double t, double sign) {
    double t2 = t * t;
    double c = (1.0 - t2) / (1.0 + t2);
    double s = sign * (2.0 * t) / (1.0 + t2);
    return {d.x * c - d.h * s, d.x * s + d.h * c};
}

// Rotates a unit direction by a fixed angle given as constant (cos, sin).
Point turn(Point d, double c, double s, double sign) {
    s *= sign;
    return {d.x * c - d.h * s, d.x * s + d.h * c};
}

// Bends a unit direction 10 degrees toward the ground, unless it already
// points at least about 35 degrees below horizontal (sin 35 = 0.574).
Point sag(Point d) {
    if (d.h <= -0.574) return d;
    return turn(d, 0.984807753012208, 0.17364817766693033, d.x < 0.0 ? 1.0 : -1.0);
}

Point add(Point a, Point b, double k) { return {a.x + b.x * k, a.h + b.h * k}; }

struct Branch {
    Point start, end, dir;
    double length, girth;
};

struct Colors {
    uint32_t trunk, foliage[3], accent;
};

// Darkens a color by num/100 per channel, in integers, so it is identical on
// every platform.
uint32_t darken(uint32_t rgb, uint32_t num) {
    uint32_t r = (rgb >> 16 & 255) * num / 100, g = (rgb >> 8 & 255) * num / 100, b = (rgb & 255) * num / 100;
    return r << 16 | g << 8 | b;
}

// Tree colors come from the hill a tree stands on, darkened well below it, so
// trees always read as significantly darker than their hill.
Colors hill_colors(uint32_t hill) {
    return {darken(hill, 40), {darken(hill, 52), darken(hill, 58), darken(hill, 48)}, 0};
}

TreeShape quad(Point s, Point e, Point d, double w0, double w1, uint32_t rgb) {
    Point n{-d.h, d.x};
    return {{add(s, n, w0 / 2), add(e, n, w1 / 2), add(e, n, -w1 / 2), add(s, n, -w0 / 2)}, {0, 0}, 0, rgb};
}

// The width factor along a waisted limb: 1 at both ends, 0.85 at the middle.
double waist_at(double t) { return 1.0 - 0.6 * t * (1.0 - t); }

// A Joshua tree limb: like quad, but narrowing toward its middle through five
// stations, so the plant reads as organic rather than built of blocks.
TreeShape waist(Point s, Point e, Point d, double w0, double w1, uint32_t rgb) {
    Point n{-d.h, d.x};
    std::vector<Point> left, right;
    for (int k = 0; k <= 4; ++k) {
        double t = k / 4.0, w = (w0 + (w1 - w0) * t) * waist_at(t);
        Point c{s.x + (e.x - s.x) * t, s.h + (e.h - s.h) * t};
        left.push_back(add(c, n, w / 2));
        right.push_back(add(c, n, -w / 2));
    }
    for (size_t k = right.size(); k-- > 0;) left.push_back(right[k]);
    return {left, {0, 0}, 0, rgb};
}

TreeShape circle(Point c, double r, uint32_t rgb) { return {{}, c, r, rgb}; }

// A leaf: an eye shape tapering to a point at both ends, its long axis along
// `axis`. `across` is the half-width of the brightest leaf; tree_svg narrows
// darker ones.
TreeShape leaf(Point c, Point axis, double along, double across) {
    TreeShape s{{}, c, along, 0};
    s.ry = across;
    s.axis = axis;
    return s;
}

void append_point(std::string& out, Point p, double scale) {
    append_fixed2(out, p.x * scale);
    out += ",";
    append_fixed2(out, (kCanvasHeight - p.h) * scale);
}

void append_hex(std::string& out, uint32_t rgb) {
    static const char* digits = "0123456789abcdef";
    out.push_back('#');
    for (int shift = 20; shift >= 0; shift -= 4) out.push_back(digits[(rgb >> shift) & 15]);
}

}  // namespace

int count_code_bits(int count) { return count >= 2 && count <= 7 ? 3 : 4; }

Plan plan(int count, uint32_t arrangement, int remaining, bool two_hills) {
    Plan p;
    for (int i = 0; i < count; ++i) {
        Group g = kSequence[arrangement][i];
        if (!two_hills) g = is_interior(g) ? Group::kFrontInterior : Group::kFrontRim;
        p.groups.push_back(g);
    }
    p.roles.assign(count, Role::kMember);
    p.x.assign(count, 0.0);
    p.prominence.assign(count, 1.0);

    // Composition per hill, from the count and arrangement only (no bits):
    // a grove of overlapping trees around a center, one isolated medium tree
    // well away from it, a straggler on the far side once there are five or
    // more trees, and open hillside elsewhere.
    for (int h = 0; h < (two_hills ? 2 : 1); ++h) {
        std::vector<int> members;
        for (int i = 0; i < count; ++i) {
            if ((two_hills && is_back(p.groups[i]) ? 1 : 0) == h) members.push_back(i);
        }
        if (members.empty()) continue;
        double c = kCanvasWidth * (0.2 + 0.6 * frac(0.37 + 0.29 * arrangement + 0.41 * h));
        double away = c < kCanvasWidth / 2 ? 1.0 : -1.0;
        size_t m = members.size();
        if (m == 1) {
            p.roles[members[0]] = Role::kIsolated;
            p.x[members[0]] = c;
        } else {
            int iso = members.back();
            members.pop_back();
            p.roles[iso] = Role::kIsolated;
            p.x[iso] = c + away * 0.38 * kCanvasWidth;
            if (m >= 5) {
                int straggler = members.back();
                members.pop_back();
                p.roles[straggler] = Role::kStraggler;
                p.x[straggler] = c - away * 0.2 * kCanvasWidth;
            }
            static const double kOffset[8] = {0.0, 0.9, -0.8, 1.7, -1.5, 0.45, -0.35, 2.4};
            double spacing = (h == 1 ? 0.025 : 0.035) * kCanvasWidth;
            for (size_t k = 0; k < members.size(); ++k) {
                p.roles[members[k]] = k == 0 ? Role::kLeader : Role::kMember;
                p.x[members[k]] = c + spacing * kOffset[k % 8];
            }
        }
    }
    int total = 0;
    std::vector<int> w(count);
    for (int i = 0; i < count; ++i) {
        p.x[i] = std::min(0.96 * kCanvasWidth, std::max(0.04 * kCanvasWidth, p.x[i]));
        int role_weight = 3;
        switch (p.roles[i]) {
            case Role::kLeader: p.prominence[i] = 1.15; role_weight = 6; break;
            case Role::kMember: p.prominence[i] = 0.8 + 0.15 * frac(0.6180339887498949 * (i + 1)); role_weight = 3; break;
            case Role::kIsolated: p.prominence[i] = 1.0; role_weight = 5; break;
            case Role::kStraggler: p.prominence[i] = 0.8; role_weight = 2; break;
        }
        double phi = frac(0.6180339887498949 * (i + 1) + 0.25 * arrangement);
        p.slope.push_back(is_interior(p.groups[i]) ? 0.30 + 0.35 * frac(phi * 7.0) : 0.0);
        // Front (or only) hill slope trees grow with their depth below the
        // rim: nearer the viewer, larger, from 1x at the shallowest to 1.6x at
        // the deepest. The factor is at least 1, so the 1.5x rule over rim
        // trees still holds.
        double depth_scale = p.groups[i] == Group::kFrontInterior ? 1.0 + 0.6 * (p.slope[i] - 0.30) / 0.35 : 1.0;
        p.scale.push_back(perspective(p.groups[i], two_hills) * p.prominence[i] * depth_scale);
        w[i] = weight(p.groups[i], two_hills) * role_weight;
        total += w[i];
    }
    // Proportional allocation by group and role weight; leftover bits go one
    // each to the heaviest trees (ties by index), so every bit is used.
    int used = 0;
    for (int i = 0; i < count; ++i) {
        int b = remaining * w[i] / total;
        p.bits.push_back(b);
        used += b;
    }
    std::vector<int> order(count);
    for (int i = 0; i < count; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return w[a] > w[b]; });
    for (int k = 0; used < remaining; ++k, ++used) p.bits[order[k % count]] += 1;
    return p;
}

TreeValues tree_fields(int bits, Species species) {
    // Deciduous forks also carry a side bit: which way the small branch turns.
    bool sided = species == Species::kDeciduous;
    int width = sided ? 4 : 3;
    TreeValues t;
    t.girth_bits = std::min(2, bits);
    t.height_bits = std::min(2, bits - t.girth_bits);
    int rest = bits - t.girth_bits - t.height_bits;
    int forks = rest / width, left = rest % width;
    for (int k = 0; k < forks; ++k) t.forks.push_back({2, sided ? 1 : 0, 1, 0, 0, 0});
    // Leftover bits refine the trunk height: one step is over a pixel even
    // on the smallest tree, while a finer ratio on a deep twig could round
    // to the same SVG.
    t.height_bits += left;
    return t;
}

bool leafy(Theme theme, Season season) { return theme == Theme::kTemperate && season != Season::kWinter; }

int tree_bit_budget(int count, Theme theme, Season season) {
    return kTrees.width - count_code_bits(count) - 4 - (leafy(theme, season) ? 4 : 0);
}

Values decode(const Hash256& hash, bool two_hills, Theme theme, Season season) {
    BitReader in(hash, kTrees.begin, kTrees.width);
    Values v;
    uint32_t c = in.read(3);
    v.count = c < 6 ? static_cast<int>(c) + 2 : kLongCounts[(c - 6) * 2 + in.read(1)];
    v.arrangement = in.read(2);
    v.size = in.read(2);
    v.leaf_bits = leafy(theme, season) ? 4 : 0;
    v.leaf = in.read(v.leaf_bits);
    Plan p = plan(v.count, v.arrangement, in.remaining(), two_hills);
    for (int i = 0; i < v.count; ++i) {
        TreeValues t = tree_fields(p.bits[i], species_of(theme, i, v.arrangement));
        t.girth = in.read(t.girth_bits);
        t.height = in.read(t.height_bits);
        for (Fork& f : t.forks) {
            f.ratio = in.read(f.ratio_bits);
            f.side = in.read(f.side_bits);
            f.length = in.read(f.length_bits);
        }
        v.trees.push_back(t);
    }
    if (in.remaining() != 0) {
        std::fprintf(stderr, "trees: plan leaves %d bits unread\n", in.remaining());
        std::abort();
    }
    return v;
}

void encode(const Values& v, Hash256& hash) {
    BitWriter out(hash, kTrees.begin, kTrees.width);
    if (v.count >= 2 && v.count <= 7) {
        out.write(3, static_cast<uint32_t>(v.count - 2));
    } else {
        int idx = static_cast<int>(std::find(kLongCounts, kLongCounts + 4, v.count) - kLongCounts);
        out.write(3, static_cast<uint32_t>(6 + idx / 2));
        out.write(1, static_cast<uint32_t>(idx % 2));
    }
    out.write(2, v.arrangement);
    out.write(2, v.size);
    out.write(v.leaf_bits, v.leaf);
    for (const TreeValues& t : v.trees) {
        out.write(t.girth_bits, t.girth);
        out.write(t.height_bits, t.height);
        for (const Fork& f : t.forks) {
            out.write(f.ratio_bits, f.ratio);
            out.write(f.side_bits, f.side);
            out.write(f.length_bits, f.length);
        }
    }
}

Tree build(const Values& v, const Plan& p, int index, const std::vector<Hill>& hills, Theme theme, Season season) {
    bool two = hills.size() == 2;
    Group g = p.groups[index];
    Tree tree;
    tree.group = static_cast<int>(g);
    tree.hill = two && is_back(g) ? 1 : 0;
    tree.x = p.x[index];
    const std::vector<Point>& surface = hills[tree.hill].points;

    // Position comes from the composition plan (no bits).
    double x = p.x[index];
    bool interior = is_interior(g);
    if (interior && two && tree.hill == 1) {
        // A back-hill interior tree needs the back hill to rise visibly above
        // the front hill where it stands; otherwise it stands on the back rim.
        interior = mountains::height_at(surface, x) - mountains::height_at(hills[0].points, x) >= 24.0;
    }
    double top = mountains::height_at(surface, x);
    double base_h = top;
    if (interior) {
        double floor = two && tree.hill == 1 ? mountains::height_at(hills[0].points, x) : 0.0;
        base_h = top - (top - floor) * p.slope[index];
    }

    Species sp = species_of(theme, index, v.arrangement);
    Colors col = hill_colors(hills[tree.hill].rgb);
    tree.desert = sp == Species::kCactus ? 1 : sp == Species::kJoshua ? 2 : 0;
    tree.waisted = sp == Species::kJoshua;
    tree.trunk_rgb = col.trunk;
    for (int c = 0; c < 3; ++c) tree.leaf_rgb[c] = col.foliage[c];
    double scale = kSizeClass[v.size] * p.scale[index];
    tree.base = {x, base_h};
    tree.scale = scale;
    const TreeValues& tv = v.trees[index];
    double gv = unit_of(tv.girth, tv.girth_bits), hv = unit_of(tv.height, tv.height_bits);
    // The tree's own phase for derived detail, from its index and decoded
    // girth and height (no extra bits): trees without fork bits still differ,
    // and the two arms of a Y do not always mirror each other.
    double phase = frac(0.4142135623730951 * static_cast<double>(index + 1) + 0.5 * gv + 0.25 * hv);
    double girth_base = sp == Species::kCactus ? 14.0 : sp == Species::kJoshua ? 13.0 : sp == Species::kDeciduous ? 11.0 : 9.0;
    double taper = sp == Species::kCactus || sp == Species::kJoshua ? 1.0 : sp == Species::kConifer ? 0.82 : 0.7;

    Branch trunk;
    trunk.start = {x, base_h};
    trunk.dir = {0.0, 1.0};
    trunk.length = scale * 130.0 * (0.6 + 0.8 * hv) * (sp == Species::kConifer ? 0.38 : 1.0);
    trunk.girth = scale * girth_base * (1.0 + 1.5 * gv);
    trunk.end = add(trunk.start, trunk.dir, trunk.length);
    tree.shapes.push_back(sp == Species::kJoshua ? waist(trunk.start, trunk.end, trunk.dir, trunk.girth, trunk.girth * taper, col.trunk)
                                                 : quad(trunk.start, trunk.end, trunk.dir, trunk.girth, trunk.girth * taper, col.trunk));
    if (tree.desert != 0) tree.stems.push_back({trunk.start, trunk.end, trunk.dir, trunk.length, trunk.girth});
    tree.limbs.push_back({trunk.start, trunk.end, trunk.girth, trunk.girth * taper});

    // Forks, breadth-first. The split ratio sets both shares and both angles:
    // the large branch tilts by about 90(1 - r) degrees and the small one by
    // about 90r, so 9:1 is a straight trunk with a side twig and 5:5 is a Y.
    //
    // The first forks are the tree's bit forks, applied to branches in
    // breadth-first order. After the bits run out, branching continues with
    // derived forks: each reuses the tree's decoded fork values in turn,
    // shifted by a fixed golden-ratio pattern, until branches are shorter than
    // 6% of the trunk or 63 forks are made. This adds detail but no entropy.
    Range rr = ratio_range(sp);
    std::vector<Branch> open = {trunk};
    // Desert plants: round joints where limbs meet at an angle, drawn after
    // every limb, so a bend reads as one continuous stem instead of two
    // blocks with a notch between them.
    std::vector<TreeShape> joints;
    std::vector<bool> forked = {false};
    std::vector<bool> stem = {true};  // on the main stem (trunk, then each large child)
    std::vector<int> depth = {0};     // forks between the trunk and this branch
    std::vector<std::vector<size_t>> kids = {{}};  // children of each branch
    std::vector<double> split = {0.0};               // ratio of the fork at each branch's end
    std::vector<double> reach = {0.0};  // conifers: path length from the stem to this branch's end
    // Conifers: no bough reaches more than a third of the tree's height (about
    // four trunk lengths) from the stem.
    double max_reach = 4.0 * trunk.length / 3.0;
    size_t bit_forks = tv.forks.size();
    double min_length = (sp == Species::kConifer ? 0.12 : 0.03) * trunk.length;
    // Conifers: drooping branches stay above the bottom 10% of the tree, whose
    // height is estimated as four trunk lengths.
    double floor_h = trunk.start.h + 0.1 * 4.0 * trunk.length;
    size_t k = 0;
    // Recursion caps by species; bit forks always run in full.
    size_t cap = sp == Species::kConifer ? kMaxConiferForks
                 : sp == Species::kJoshua ? 7
                 : sp == Species::kCactus ? 3
                                          : kMaxForks;
    size_t max_forks = std::max(cap, bit_forks);
    for (size_t idx = 0; idx < open.size() && k < max_forks; ++idx) {
        Branch parent = open[idx];
        double ru, lv;
        // Deciduous: which way the small branch turns, 1 = left. Bit forks read
        // it; derived forks flip the source fork's bit by a fixed irregular
        // (plastic-number) pattern.
        uint32_t side_bit = 0;
        double plastic = frac(0.7548776662466927 * static_cast<double>(k + 1) + phase);
        if (k < bit_forks) {
            ru = unit_of(tv.forks[k].ratio, tv.forks[k].ratio_bits);
            lv = unit_of(tv.forks[k].length, tv.forks[k].length_bits);
            side_bit = tv.forks[k].side;
        } else {
            if (parent.length < min_length) continue;
            double base_r = phase, base_l = frac(phase + 0.5);
            uint32_t base_s = 0;
            if (bit_forks > 0) {
                const Fork& src = tv.forks[(k - bit_forks) % bit_forks];
                base_r = unit_of(src.ratio, src.ratio_bits);
                base_l = unit_of(src.length, src.length_bits);
                base_s = src.side;
            }
            ru = frac(base_r + 0.6180339887498949 * static_cast<double>(k + 1));
            lv = frac(base_l + 0.3819660112501051 * static_cast<double>(k + 1));
            side_bit = base_s ^ (plastic < 0.5 ? 1u : 0u);
        }
        forked[idx] = true;
        double r = rr.lo + (rr.hi - rr.lo) * ru;
        split[idx] = r;
        double q = 2.0 * (1.0 - r);
        double t_big = kTan22_5 * q;
        double t_small = 1.0 - (1.0 - kTan22_5) * q;
        // Which side the large child turns to. Conifers alternate by depth, so
        // successive whorls on the stem switch sides.
        double side;
        if (sp == Species::kConifer) {
            side = depth[idx] % 2 == 0 ? 1.0 : -1.0;
        } else {
            // A fixed irregular pattern (plastic-number sequence), so forks do
            // not alternate strictly and the crown does not tile regularly.
            side = plastic < 0.5 ? 1.0 : -1.0;
        }
        // Derived forks on a conifer stem grow a full whorl: a side branch on
        // each side (a third child). Bit forks always have exactly two.
        bool whorl = sp == Species::kConifer && stem[idx] && k >= bit_forks;
        int children = whorl ? 3 : 2;
        for (int c = 0; c < children; ++c) {
            double share = c == 0 ? r : 1.0 - r;
            Branch b;
            b.start = parent.end;
            if (sp == Species::kConifer) {
                // Conifers use fixed angles: the stem continues straight; a side
                // branch off the main stem droops more the lower it is (see
                // below). Past the stem every fork sags: the continuation bends
                // 10 degrees toward the ground and twigs leave at 30 degrees from
                // it and hang a further 10, so boughs arc down instead of
                // piercing out straight.
                if (c == 0) {
                    b.dir = stem[idx] ? parent.dir : sag(parent.dir);
                } else if (stem[idx]) {
                    // Whorl angle from vertical: about 115 degrees near the top,
                    // drooping to about 156 near the base, with successive tiers
                    // wobbling around that trend. Set through the half-angle
                    // tangent t, so the rotation stays exact with no trigonometry.
                    double rel = std::min(1.0, std::max(0.0, (parent.end.h - trunk.start.h) / (4.0 * trunk.length)));
                    double t = 1.7320508075688772 + 2.6 * (1.0 - rel) + (depth[idx] % 2 == 1 ? 0.35 : -0.15);
                    b.dir = rotate(parent.dir, t, c == 1 ? -side : side);
                } else {
                    b.dir = sag(turn(sag(parent.dir), 0.8660254037844386, 0.5, -side));  // 30 degrees
                }
            } else {
                double s_big = side, s_small = -side;
                double tb = t_big;
                if (sp == Species::kDeciduous) {
                    // The small branch turns left or right by its side bit. The
                    // winner bends half as much and turns back toward vertical,
                    // so the main trunk keeps growing upward; on a vertical
                    // parent it leans away from the small branch.
                    s_small = side_bit == 1 ? 1.0 : -1.0;
                    tb = 0.5 * t_big;
                    s_big = parent.dir.x > 0.0 ? 1.0 : parent.dir.x < 0.0 ? -1.0 : -s_small;
                }
                b.dir = rotate(parent.dir, c == 0 ? tb : t_small, c == 0 ? s_big : s_small);
            }
            if (sp == Species::kConifer) {
                // The stem shortens quickly and side branches stay moderate, so
                // the silhouette tapers into a cone.
                double f;
                if (c == 0) {
                    // Short internodes: many tiers. Derived stem segments shrink
                    // more slowly still, adding tiers up the stem.
                    f = whorl ? 0.80 : 0.62 + 0.2 * share;
                } else if (stem[idx]) {
                    f = 0.7 + 0.6 * share;   // whorl branches shorter than the stem segment
                } else {
                    f = 0.45 + 0.35 * share; // short deeper twigs
                }
                b.length = parent.length * f * (0.85 + 0.3 * lv);
                if (b.dir.h < 0.0) {
                    // Keep drooping branches out of the bottom 10% of the tree. The
                    // scale depends only on where the branch starts and its fixed
                    // angle, not on its bits, so the bits still set its length
                    // monotonically. Bit forks keep at least 0.3 of their length so
                    // distinct values stay apart; derived forks carry no bits and
                    // may shrink to almost nothing.
                    double longest = parent.length * (c == 0 ? 0.82 : stem[idx] ? 0.79 : 0.80) * 1.15;
                    double room = (b.start.h - floor_h) / (-b.dir.h * 1.25);
                    double lowest = k < bit_forks ? 0.3 : 0.02;
                    b.length *= std::max(lowest, std::min(1.0, room / longest));
                }
                if (!(stem[idx] && c == 0)) {
                    // Bough reach: the same monotone scaling, against the reach left
                    // before this branch. Derived branches with no reach left are
                    // not grown at all.
                    double before = stem[idx] ? 0.0 : reach[idx];
                    double longest = parent.length * (c == 0 ? 0.82 : stem[idx] ? 0.79 : 0.80) * 1.15;
                    double fit = (max_reach - before) / longest;
                    if (k >= bit_forks && fit < 0.02) continue;
                    b.length *= std::max(k < bit_forks ? 0.3 : 0.0, std::min(1.0, fit));
                }
            } else {
                b.length = parent.length * (0.5 + 0.35 * share) * (0.85 + 0.3 * lv);
                if (sp == Species::kDeciduous) {
                    // Thinner branches fork more often: a segment's length follows its
                    // own girth relative to the trunk, not its parent's length. A main
                    // limb thins slowly, so it keeps its reach while its forks come
                    // closer together; thin side shoots stay short and bushy.
                    double thin = parent.girth * (0.35 + 0.65 * share) / trunk.girth;
                    b.length = trunk.length * (0.08 + 0.5 * thin) * (0.85 + 0.3 * lv);
                }
            }
            if (sp == Species::kJoshua) {
                b.girth = parent.girth;
            } else if (sp == Species::kCactus) {
                b.girth = parent.girth * (0.75 + 0.25 * share);
            } else {
                if (sp == Species::kConifer) {
                    // The stem and each branch continue at the parent's end width, so
                    // they thin continuously; side branches stay well under the stem.
                    b.girth = c == 0 ? parent.girth * taper : parent.girth * taper * (0.35 + 0.65 * share);
                } else {
                    b.girth = parent.girth * (0.35 + 0.65 * share);
                }
            }
            if (sp == Species::kCactus) {
                // Cactus arms leave at the fork angle, then turn straight up.
                Point elbow = add(b.start, b.dir, 0.3 * b.length);
                joints.push_back(circle(b.start, b.girth / 2.0, col.trunk));
                joints.push_back(circle(elbow, b.girth / 2.0, col.trunk));
                tree.shapes.push_back(quad(b.start, elbow, b.dir, b.girth, b.girth, col.trunk));
                tree.stems.push_back({b.start, elbow, b.dir, 0.3 * b.length, b.girth});
                tree.limbs.push_back({b.start, elbow, b.girth, b.girth});
                b.dir = {0.0, 1.0};
                b.end = add(elbow, b.dir, 0.7 * b.length);
                tree.shapes.push_back(quad(elbow, b.end, b.dir, b.girth, b.girth, col.trunk));
                tree.stems.push_back({elbow, b.end, b.dir, 0.7 * b.length, b.girth});
                tree.limbs.push_back({elbow, b.end, b.girth, b.girth});
            } else {
                b.end = add(b.start, b.dir, b.length);
                if (sp == Species::kJoshua) joints.push_back(circle(b.start, b.girth / 2.0, col.trunk));
                tree.shapes.push_back(sp == Species::kJoshua ? waist(b.start, b.end, b.dir, b.girth, b.girth * taper, col.trunk)
                                                             : quad(b.start, b.end, b.dir, b.girth, b.girth * taper, col.trunk));
                tree.limbs.push_back({b.start, b.end, b.girth, b.girth * taper});
            }
            kids[idx].push_back(open.size());
            kids.push_back({});
            split.push_back(0.0);
            reach.push_back(stem[idx] && c == 0 ? 0.0 : (stem[idx] ? 0.0 : reach[idx]) + b.length);
            open.push_back(b);
            forked.push_back(false);
            stem.push_back(stem[idx] && c == 0);
            depth.push_back(depth[idx] + 1);
        }
        ++k;
    }

    tree.shapes.insert(tree.shapes.end(), joints.begin(), joints.end());

    // Conifers: a straight feather of needles around every branch, widest
    // near its base and tapering past its tip, so the branches read as pine
    // boughs rather than bare sticks. Stem segments get narrower feathers, so
    // the main trunk is clothed in needles too.
    if (sp == Species::kConifer) {
        for (size_t k = 0; k < open.size(); ++k) {
            const Branch& b = open[k];
            Point n{-b.dir.h, b.dir.x};
            double kind_w = stem[k] ? 0.6 : 1.0;
            auto feather = [&](double scale_w) {
                double w = (0.18 * b.length + b.girth) * kind_w * scale_w;
                return std::vector<Point>{b.start, add(add(b.start, b.dir, 0.25 * b.length), n, w),
                                          add(add(b.start, b.dir, 0.6 * b.length), n, 0.75 * w),
                                          add(b.end, b.dir, 0.12 * b.length * scale_w),
                                          add(add(b.start, b.dir, 0.6 * b.length), n, -0.75 * w),
                                          add(add(b.start, b.dir, 0.25 * b.length), n, -w)};
            };
            // Feathers carry no bits, so they shrink toward their branch as far
            // as needed to stay out of the bottom 10% of the tree, or are dropped.
            double branch_low = std::min(b.start.h, b.end.h);
            if (branch_low <= floor_h) continue;
            std::vector<Point> poly = feather(1.0);
            double low = branch_low;
            for (const Point& q : poly) low = std::min(low, q.h);
            if (low < floor_h) poly = feather((branch_low - floor_h) / (branch_low - low));
            tree.shapes.push_back({poly, {0, 0}, 0, col.foliage[k % 3]});
        }
    }

    // Foliage on every terminal (unforked) branch, in breadth-first order.
    for (size_t k = 0; k < open.size(); ++k) {
        if (forked[k]) continue;
        const Branch& b = open[k];
        switch (sp) {
            case Species::kDeciduous:
                break;  // canopy deferred: deciduous trees are drawn as bare skeletons for now
            case Species::kConifer:
                // The stem ends in a needle-sharp tip.
                if (stem[k]) {
                    Point n{-b.dir.h, b.dir.x};
                    double w = b.girth * taper;
                    Point tip = add(b.end, b.dir, std::max(0.8 * b.length, 3.0 * w));
                    tree.shapes.push_back({{add(b.end, n, w / 2), tip, add(b.end, n, -w / 2)}, {0, 0}, 0, col.trunk});
                }
                break;
            case Species::kCactus:
                tree.shapes.push_back(circle(b.end, b.girth / 2.0, col.trunk));
                break;
            case Species::kJoshua:
                // Spike tufts are added once the sun is known (finish).
                tree.tips.push_back({b.start, b.end, b.dir, b.length, b.girth});
                break;
        }
    }

    // Deciduous leaves: 2 to 5 small dots at every branch tip and a few at
    // forks, with tips in the tree's left-to-right order: depth-first,
    // leftmost child first. paint_leaves colors them.
    if (sp == Species::kDeciduous && leafy(theme, season)) {
        double along = 4.0 * scale, across = 2.8 * scale, offset = 6.0 * scale;
        // Tip layouts by dot count, left to right, as (cos, sin) of the angle
        // from the branch direction; (0, 0) puts the dot on the tip itself.
        struct Dir {
            double c, s;
        };
        constexpr double k = 0.8660254037844386;  // sin 60 = sin 120
        static const std::vector<Dir> kTipLayout[4] = {
            {{0.5, k}, {0.5, -k}},                                // 2: 60, -60
            {{-0.5, k}, {1.0, 0.0}, {-0.5, -k}},                  // 3: 120, 0, -120
            {{-0.5, k}, {1.0, 0.0}, {0.0, 0.0}, {-0.5, -k}},      // 4: 120, 0, tip, -120
            {{-0.5, k}, {0.5, k}, {0.0, 0.0}, {0.5, -k}, {-0.5, -k}},  // 5: 120, 60, tip, -60, -120
        };
        size_t tip = 0;  // tips so far: the count cycles 2, 3, 4, 5
        std::vector<size_t> todo = {0};
        while (!todo.empty()) {
            size_t i = todo.back();
            todo.pop_back();
            if (kids[i].empty()) {
                const Branch& b = open[i];
                Point n{-b.dir.h, b.dir.x};  // to the branch's left
                const std::vector<Dir>& layout = kTipLayout[tip++ % 4];
                for (const Dir& a : layout) {
                    Point d{a.c * b.dir.x + a.s * n.x, a.c * b.dir.h + a.s * n.h};
                    bool on_tip = a.c == 0.0 && a.s == 0.0;
                    tree.shapes.push_back(leaf(add(b.end, d, offset), on_tip ? b.dir : d, along, across));
                }
                tree.leaves += static_cast<int>(layout.size());
                continue;
            }
            // A fork also holds leaves floating around it, since the crown is
            // 3D: more for even splits, none for the most lopsided. One dot
            // floats beyond the fork; two flank it up-left and up-right; three
            // do all of these, left to right.
            double r = split[i];
            int dots = r < 0.69 ? 3 : r < 0.76 ? 2 : r < 0.83 ? 1 : 0;
            if (dots > 0) {
                const Branch& b = open[i];
                Point n{-b.dir.h, b.dir.x};
                Point left{0.5 * b.dir.x + 0.8660254037844386 * n.x, 0.5 * b.dir.h + 0.8660254037844386 * n.h};
                Point right{0.5 * b.dir.x - 0.8660254037844386 * n.x, 0.5 * b.dir.h - 0.8660254037844386 * n.h};
                std::vector<Point> where = dots == 1 ? std::vector<Point>{b.dir}
                                           : dots == 2 ? std::vector<Point>{left, right}
                                                       : std::vector<Point>{left, b.dir, right};
                for (const Point& d : where) tree.shapes.push_back(leaf(add(b.end, d, 6.8 * scale), d, along, across));
                tree.leaves += dots;
            }
            std::vector<size_t> order = kids[i];
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t c) { return open[a].dir.x < open[c].dir.x; });
            for (size_t j = order.size(); j-- > 0;) todo.push_back(order[j]);
        }
    }
    return tree;
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

// Ground cover where a tree meets the ground (subspec/trees.md, Ground cover):
// snow in winter, rocks in the desert, grass blades elsewhere. Only a little
// wider than the trunk, and irregular: its shape reads digits of pi from an
// offset set by the tree's index. No bits.
void add_cover(Tree& t, int index, uint32_t hill_rgb, Theme theme, Season season) {
    static const char kPi[] = "3141592653589793238462643383279502884197169399375105820974944592";
    const int kDigits = 64;
    int start = (7 * index) % kDigits;
    auto digit = [&](int k) { return static_cast<double>(kPi[(start + k) % kDigits] - '0'); };
    t.cover.clear();
    t.cover_rgb.clear();
    t.cover_tinted = false;
    if (t.limbs.empty()) return;
    double g = t.limbs[0].wa;  // trunk girth at the base
    double x0 = t.base.x, h0 = t.base.h - 0.1 * g;  // a little below the base, over the trunk's foot
    if (season == Season::kWinter) {
        // A lumpy drift of snow: an outline across 1.6 trunk widths whose
        // nine heights come from the digits, highest near the trunk.
        static const double kHump[9] = {0.0, 0.5, 0.8, 1.0, 1.0, 1.0, 0.8, 0.5, 0.0};
        std::vector<Point> poly;
        for (int k = 0; k < 9; ++k) {
            double x = x0 + (k - 4) * 0.2 * g + (k > 0 && k < 8 ? (digit(k) - 4.5) * 0.015 * g : 0.0);
            poly.push_back({x, h0 + kHump[k] * g * (0.12 + 0.035 * digit(k + 9))});
        }
        t.cover.push_back(poly);
        t.cover_rgb.push_back(0xF8FAFC);
        t.cover_tinted = true;
    } else if (theme == Theme::kDesert) {
        // Two or three small rocks, each a six-point outline whose radii come
        // from the digits (fixed directions from 0 to 180 degrees).
        static const double kCos[6] = {1.0, 0.809017, 0.309017, -0.309017, -0.809017, -1.0};
        static const double kSin[6] = {0.0, 0.587785, 0.951057, 0.951057, 0.587785, 0.0};
        static const double kAt[3] = {-0.75, 0.7, 0.05};
        int rocks = digit(0) < 5.0 ? 2 : 3;
        for (int k = 0; k < rocks; ++k) {
            double r = g * (0.18 + 0.03 * digit(3 * k + 1)), x = x0 + kAt[k] * g;
            double base = k == 2 ? h0 - 0.15 * g : h0;  // the third sits in front of the trunk
            std::vector<Point> poly;
            for (int v = 0; v < 6; ++v) {
                double rv = r * (0.6 + 0.05 * digit(7 * k + v + 2));
                poly.push_back({x + kCos[v] * rv, base + 0.75 * kSin[v] * rv});
            }
            t.cover.push_back(poly);
            t.cover_rgb.push_back(k % 2 == 0 ? 0x9A8F80 : 0x7D7366);
        }
    } else {
        // Eleven grass blades across 1.5 trunk widths, irregular in spacing,
        // height, width, and lean, a little lighter than the hill.
        std::vector<Point> poly;
        for (int k = 0; k < 11; ++k) {
            double root = x0 + (k - 5) * 0.15 * g + (digit(k) - 4.5) * 0.012 * g;
            double half = g * (0.025 + 0.004 * digit(k + 11));
            double height = g * (0.2 + 0.048 * digit(k + 22));
            double lean = (digit(k + 33) - 4.5) * 0.04 * g + (k - 5) * 0.04 * g;
            poly.push_back({root - half, h0});
            poly.push_back({root + lean, h0 + height});
            poly.push_back({root + half, h0});
        }
        t.cover.push_back(poly);
        t.cover_rgb.push_back(shade(hill_rgb, 0.15));
        t.cover_tinted = true;
    }
}

}  // namespace

void paint_leaves(std::vector<Tree>& trees, const Values& v, Season season) {
    if (v.leaf_bits == 0) return;
    const std::vector<uint32_t>& colors = leaf_palette(season, static_cast<int>(v.leaf));
    std::vector<size_t> order(trees.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return trees[a].x < trees[b].x; });
    size_t next = 0;
    for (size_t i : order) {
        Tree& t = trees[i];
        size_t first = t.shapes.size() - static_cast<size_t>(t.leaves);
        // A slight gradient by height in the crown: the top leaves catch the
        // light, the lowest are shaded. It follows the geometry, not the bits.
        double low = 0.0, high = 0.0;
        for (size_t s = first; s < t.shapes.size(); ++s) {
            double h = t.shapes[s].center.h;
            if (s == first || h < low) low = h;
            if (s == first || h > high) high = h;
        }
        for (size_t s = first; s < t.shapes.size(); ++s) {
            double u = high > low ? (t.shapes[s].center.h - low) / (high - low) - 0.5 : 0.0;  // -0.5 to 0.5
            t.shapes[s].rgb = shade(colors[next++ % colors.size()], 0.3 * u);
        }
    }
}

std::vector<Tree> generate(const Hash256& hash, const std::vector<Hill>& hills, Theme theme, Season season) {
    bool two = hills.size() == 2;
    Values v = decode(hash, two, theme, season);
    Plan p = plan(v.count, v.arrangement, tree_bit_budget(v.count, theme, season), two);
    std::vector<Tree> out;
    for (int i = 0; i < v.count; ++i) out.push_back(build(v, p, i, hills, theme, season));
    paint_leaves(out, v, season);
    for (size_t i = 0; i < out.size(); ++i) add_cover(out[i], static_cast<int>(i), hills[out[i].hill].rgb, theme, season);
    return out;
}

std::string tree_svg(const Tree& tree, int index, double scale) {
    std::string out = "<g class=\"tree\" id=\"tree-";
    char digits[12];
    int len = 0;
    for (unsigned n = static_cast<unsigned>(index + 1); n > 0 || len == 0; n /= 10) digits[len++] = static_cast<char>('0' + n % 10);
    while (len > 0) out.push_back(digits[--len]);
    out += "\">\n";
    for (const TreeShape& s : tree.shapes) {
        if (s.poly.empty() && s.ry > 0.0) {
            // An eye shape: two quadratic arcs meeting in points at both ends.
            // A control point at twice the half-width gives that half-width at
            // the middle. Darker leaves are narrower, as if wilting: the width
            // follows the fill's brightness, so it carries no bits.
            uint32_t c = s.rgb;
            double bright = (299.0 * (c >> 16) + 587.0 * ((c >> 8) & 255u) + 114.0 * (c & 255u)) / 255000.0;
            double a = s.radius * scale, w = 2.0 * s.ry * (0.35 + 0.8 * bright) * scale;
            out += "<path fill=\"";
            append_hex(out, s.rgb);
            out += "\" d=\"M";
            append_fixed2(out, -a);
            out += ",0Q0,";
            append_fixed2(out, w);
            out += " ";
            append_fixed2(out, a);
            out += ",0Q0,";
            append_fixed2(out, -w);
            out += " ";
            append_fixed2(out, -a);
            out += ",0Z\"";
            // The rotation is an exact matrix from the axis direction (SVG y
            // points down), so no angle or trigonometry is needed.
            double ux = s.axis.x, uy = -s.axis.h;
            out += " transform=\"matrix(";
            const double m[6] = {ux, uy, -uy, ux, s.center.x * scale, (kCanvasHeight - s.center.h) * scale};
            for (int q = 0; q < 6; ++q) {
                if (q > 0) out += " ";
                append_fixed2(out, m[q]);
            }
            out += ")\"/>\n";
        } else if (!s.polys.empty()) {
            // Many hair-thin polygons in one path: an absolute move to each
            // one's first point, then relative steps.
            out += "<path fill=\"";
            append_hex(out, s.rgb);
            out += "\" d=\"";
            for (const std::vector<Point>& p : s.polys) {
                out += "M";
                append_point(out, p[0], scale);
                out += "l";
                for (size_t i = 1; i < p.size(); ++i) {
                    if (i > 1) out += " ";
                    append_fixed2(out, (p[i].x - p[i - 1].x) * scale);
                    out += ",";
                    append_fixed2(out, -(p[i].h - p[i - 1].h) * scale);
                }
                out += "z";
            }
            out += "\"/>\n";
        } else if (s.poly.empty()) {
            out += "<circle fill=\"";
            append_hex(out, s.rgb);
            out += "\" cx=\"";
            append_fixed2(out, s.center.x * scale);
            out += "\" cy=\"";
            append_fixed2(out, (kCanvasHeight - s.center.h) * scale);
            out += "\" r=\"";
            append_fixed2(out, s.radius * scale);
            out += "\"/>\n";
        } else {
            out += "<path fill=\"";
            append_hex(out, s.rgb);
            out += "\" d=\"";
            for (size_t i = 0; i < s.poly.size(); ++i) {
                out += i == 0 ? "M" : " L";
                append_point(out, s.poly[i], scale);
            }
            out += " Z\"/>\n";
        }
    }
    out += "</g>\n";
    return out;
}

}  // namespace bh::trees

namespace bh::trees {

void tint_cover(std::vector<Tree>& trees, const Hill& front) {
    if (front.band_rgb.empty()) return;
    for (Tree& t : trees) {
        if (t.hill != 0 || !t.cover_tinted) continue;
        for (uint32_t& c : t.cover_rgb) c = front.band_rgb.back();
    }
}

}  // namespace bh::trees

namespace bh::trees {

void add_crest_trees(std::vector<Hill>& hills, const Hash256& hash, Theme theme, Season season) {
    if (hills.size() < 2) return;
    Hill& back = hills[1];
    back.crest_trees.clear();
    back.crest_rgb.clear();
    bool conifer = theme == Theme::kAlpine;
    if (!conifer && theme != Theme::kTemperate) return;  // not Tundra or Desert

    // Candidate spots every 14 units along the crest, scored by height times
    // (x + canvas width), offset so the left edge is not always lowest,
    // modulo 150000. The product changes smoothly, so neighbors score alike and
    // the lowest scores fall in two or three groves rather than at even
    // intervals.
    struct Spot {
        double x, h, key;
        size_t index;
    };
    std::vector<Spot> spots;
    for (double x = 0.02 * kCanvasWidth; x <= 0.98 * kCanvasWidth; x += 14.0) {
        double h = mountains::height_at(back.points, x);
        spots.push_back({x, h, std::fmod(h * (x + kCanvasWidth), 150000.0), spots.size()});
    }
    int count = 8 + static_cast<int>(std::fmod(std::floor(back.profile.crest_h), 3.0));
    std::stable_sort(spots.begin(), spots.end(), [](const Spot& a, const Spot& b) { return a.key < b.key; });
    // Lowest scores first, each at least 34 units from those already taken,
    // so trees in a grove stand apart instead of merging into one blob.
    std::vector<Spot> taken;
    for (const Spot& s : spots) {
        if (static_cast<int>(taken.size()) >= count) break;
        bool clear = true;
        for (const Spot& t : taken) clear = clear && std::fabs(t.x - s.x) >= 34.0;
        if (clear) taken.push_back(s);
    }
    spots = taken;
    std::sort(spots.begin(), spots.end(), [](const Spot& a, const Spot& b) { return a.index < b.index; });

    // Colors: conifers, and deciduous trees in winter, are one dark grayish
    // green; deciduous trees in spring and summer nearly match the hill, like
    // silhouettes; in fall they take the scene's leaf colors in an irregular
    // order from the digits of pi, never repeating a color back to back.
    const uint32_t kDarkGreen = 0x3C4A40;
    std::vector<uint32_t> leaves;  // the scene's leaf colors, for deciduous trees outside winter
    if (!conifer && season != Season::kWinter) {
        Values v = decode(hash, true, theme, season);
        leaves = leaf_palette(season, static_cast<int>(v.leaf));
    }
    static const char kPi[] = "3141592653589793238462643383279502884197169399375105820974944592";
    for (size_t k = 0; k < spots.size(); ++k) {
        const Spot& s = spots[k];
        double size = 45.0 + 25.0 * std::fmod(s.h * s.x, 997.0) / 997.0;  // varied heights
        double x = s.x, h = s.h - 1.0;  // set a little into the hill
        std::vector<std::vector<Point>> shape;
        std::vector<Point> poly;
        if (conifer) {
            // A two-tier spire on a short trunk.
            double w = 0.42 * size;
            poly = {{x - 0.05 * w, h}, {x - 0.05 * w, h + 0.12 * size}, {x - 0.5 * w, h + 0.12 * size},
                    {x - 0.2 * w, h + 0.5 * size}, {x - 0.38 * w, h + 0.5 * size}, {x, h + size},
                    {x + 0.38 * w, h + 0.5 * size}, {x + 0.2 * w, h + 0.5 * size}, {x + 0.5 * w, h + 0.12 * size},
                    {x + 0.05 * w, h + 0.12 * size}, {x + 0.05 * w, h}};
        } else {
            // Directions every 15 degrees, as exact constants (no trigonometry).
            static const double kDir[24][2] = {
                {1.0, 0.0}, {0.9659258, 0.2588190}, {0.8660254, 0.5}, {0.7071068, 0.7071068}, {0.5, 0.8660254},
                {0.2588190, 0.9659258}, {0.0, 1.0}, {-0.2588190, 0.9659258}, {-0.5, 0.8660254},
                {-0.7071068, 0.7071068}, {-0.8660254, 0.5}, {-0.9659258, 0.2588190}, {-1.0, 0.0},
                {-0.9659258, -0.2588190}, {-0.8660254, -0.5}, {-0.7071068, -0.7071068}, {-0.5, -0.8660254},
                {-0.2588190, -0.9659258}, {0.0, -1.0}, {0.2588190, -0.9659258}, {0.5, -0.8660254},
                {0.7071068, -0.7071068}, {0.8660254, -0.5}, {0.9659258, -0.2588190}};
            auto dig = [&](int j) { return static_cast<double>(kPi[(5 * k + j) % 64] - '0'); };
            // A tapered needle from p along direction index d (0-23), length l,
            // half-width w at the base.
            auto needle = [&](Point p, int d, double l, double w) {
                double dx = kDir[d % 24][0], dh = kDir[d % 24][1];
                return std::vector<Point>{{p.x - dh * w, p.h + dx * w}, {p.x + dx * l, p.h + dh * l}, {p.x + dh * w, p.h - dx * w}};
            };
            // Every season: one main needle, one or two side needles, and at
            // most one sub-needle on each (three orders). In winter they are the
            // dark grayish green of the other trees; otherwise they blend into
            // the hill, and tiny leaf dots at the tips and forks carry the
            // season's color.
            uint32_t wood = season == Season::kWinter ? kDarkGreen : shade(back.rgb, -0.12);
            std::vector<Point> spots_at;  // forks and tips, where leaf dots gather
            Point base{x, h};
            back.crest_trees.push_back({needle(base, 6, size, 0.05 * size)});
            back.crest_rgb.push_back(wood);
            spots_at.push_back({x, h + size});
            int sides = 1 + static_cast<int>(dig(0)) % 2;
            for (int b = 0; b < sides; ++b) {
                int left = (static_cast<int>(dig(1)) + b) % 2;  // which side this branch leaves on
                double at = 0.3 + 0.04 * dig(2 + b);            // up the trunk
                int d = left ? 6 + 2 + static_cast<int>(dig(4 + b)) % 2 : 6 - 2 - static_cast<int>(dig(4 + b)) % 2;
                double l = size * (0.3 + 0.025 * dig(6 + b));
                Point p{x, h + at * size};
                back.crest_trees.push_back({needle(p, d, l, 0.022 * size)});
                back.crest_rgb.push_back(wood);
                spots_at.push_back(p);
                spots_at.push_back({p.x + kDir[d][0] * l, p.h + kDir[d][1] * l});
                if (static_cast<int>(dig(8 + b)) % 2 == 1) {
                    double m = 0.45 + 0.03 * dig(10 + b);
                    Point q{p.x + kDir[d][0] * l * m, p.h + kDir[d][1] * l * m};
                    int d2 = left ? d - 2 : d + 2;  // bends back toward vertical
                    back.crest_trees.push_back({needle(q, d2, 0.45 * l, 0.012 * size)});
                    back.crest_rgb.push_back(wood);
                    spots_at.push_back({q.x + kDir[d2][0] * 0.45 * l, q.h + kDir[d2][1] * 0.45 * l});
                }
            }
            if (!leaves.empty()) {
                // 1 to 4 tiny dots (2x2 or 2x4 logical units) around each fork
                // and tip, their count, offsets, and colors set by the crest
                // point's x and y.
                for (size_t j = 0; j < spots_at.size(); ++j) {
                    const Point& c = spots_at[j];
                    long key = static_cast<long>(std::floor(s.x * 7.0 + s.h * 13.0)) + static_cast<long>(j) * 31;
                    int dots = 1 + static_cast<int>(key % 4);
                    for (int dd = 0; dd < dots; ++dd) {
                        long u = key * 17 + dd * 101;
                        double ox = static_cast<double>(u % 9) - 4.0, oh = static_cast<double>((u / 9) % 7) - 3.0;
                        double w = 1.0, hh = (u % 3 == 0) ? 2.0 : 1.0;
                        Point o{c.x + ox, c.h + oh};
                        back.crest_trees.push_back({{{o.x - w, o.h - hh}, {o.x + w, o.h - hh}, {o.x + w, o.h + hh}, {o.x - w, o.h + hh}}});
                        back.crest_rgb.push_back(leaves[static_cast<size_t>((u / 63) % 6)]);
                    }
                }
            }
            continue;
        }
        // Conifers: the spire, in dark grayish green.
        shape.push_back(poly);
        back.crest_trees.push_back(shape);
        back.crest_rgb.push_back(kDarkGreen);
    }
}

}  // namespace bh::trees

namespace bh::trees {

namespace {

// Each channel moved a fraction t of the way from a to b.
uint32_t blend(uint32_t a, uint32_t b, double t) {
    uint32_t out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        double ca = static_cast<double>((a >> shift) & 255u), cb = static_cast<double>((b >> shift) & 255u);
        out |= static_cast<uint32_t>(std::lround(ca + (cb - ca) * t)) << shift;
    }
    return out;
}

// Collects many small polygons into one shape per color, in first-seen order.
struct Batch {
    std::vector<TreeShape> shapes;
    void add(uint32_t rgb, std::vector<Point> poly) {
        auto it = std::find_if(shapes.begin(), shapes.end(), [&](const TreeShape& s) { return s.rgb == rgb; });
        if (it == shapes.end()) {
            TreeShape s;
            s.rgb = rgb;
            shapes.push_back(s);
            it = shapes.end() - 1;
        }
        it->polys.push_back(std::move(poly));
    }
};

const char kPiDigits[] =
    "3141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067";
double pi_digit(long j) { return static_cast<double>(kPiDigits[((j % 100) + 100) % 100] - '0'); }

// cos and sin of 0, 10, ..., 80 degrees, so spike angles need no trigonometry.
const double kCos10[9] = {1.0, 0.98480775, 0.93969262, 0.8660254, 0.76604444, 0.64278761, 0.5, 0.34202014, 0.17364818};
const double kSin10[9] = {0.0, 0.17364818, 0.34202014, 0.5, 0.64278761, 0.76604444, 0.8660254, 0.93969262, 0.98480775};

}  // namespace

void finish(std::vector<Tree>& trees, const Sun& sun) {
    // The sun lights the side of each stem it stands on: the left when
    // shadows fall right (dx > 0). The screen-space light for spikes comes
    // from that side and above, as for the ridge facets.
    double lit_x = sun.dx > 0.0 ? -1.0 : 1.0;
    double lx = -sun.dx * sun.length, lh = 1.0, ll = lx * lx + lh * lh;
    for (Tree& tree : trees) {
        if (tree.desert == 0) continue;
        uint32_t trunk = tree.trunk_rgb;

        // Sunlit side: two bands along the lit edge of the Joshua tree's
        // trunk and of every upright cactus segment, the outer brighter, so
        // the stem reads round. They follow a waisted limb's narrowing and
        // taper into the edge over the first and last fifth of the segment, so
        // they never end in a block.
        for (const Tree::Stem& s : tree.stems) {
            bool upright = s.dir.x == 0.0;
            if (tree.desert == 1 && !upright) continue;
            Point n{-s.dir.h, s.dir.x};
            double side = n.x * lit_x > 0.0 ? 1.0 : -1.0;  // toward the sun along n
            static const double kBands[2][3] = {{0.5, 0.3, 0.12}, {0.3, 0.12, 0.05}};  // outer, inner offsets; lightening
            for (const auto& band : kBands) {
                std::vector<Point> out, in;
                for (int k = 0; k <= 10; ++k) {
                    double t = k / 10.0, w = s.girth * (tree.waisted ? waist_at(t) : 1.0);
                    double grow = std::min(1.0, std::min(t, 1.0 - t) / 0.2);  // 0 at the ends
                    Point c{s.a.x + (s.b.x - s.a.x) * t, s.a.h + (s.b.h - s.a.h) * t};
                    out.push_back(add(c, n, side * band[0] * w));
                    in.push_back(add(c, n, side * (band[0] - (band[0] - band[1]) * grow) * w));
                }
                for (size_t k = in.size(); k-- > 0;) out.push_back(in[k]);
                tree.shapes.push_back({out, {0, 0}, 0, blend(trunk, 0xFFFFFF, band[2])});
            }
        }

        // Cactus spines: hair-thin needles packed along both edges of every
        // segment and along two ribs on its face, their tilt, length, and
        // place from the digits of pi. Near the edges they are lit the other
        // way round, for depth: dark against the bright side, pale against the
        // shaded side. Ribs stay pale straw, and four in ten edge spines stick
        // far out in the trunk color, a spiky silhouette.
        if (tree.desert == 1) {
            uint32_t pale = blend(trunk, 0xE6DDB4, 0.4), light = blend(trunk, 0xF2EBC8, 0.65), dark = blend(trunk, 0x000000, 0.35);
            Batch spines;
            static const double kLines[4] = {-0.5, -0.2, 0.2, 0.5};
            int j = 0;
            for (const Tree::Stem& s : tree.stems) {
                Point u = s.dir, n{-s.dir.h, s.dir.x};
                double g = std::min(s.girth, 14.0 + 0.25 * s.girth);
                double step = 0.12 * g;
                for (double off : kLines) {
                    double side = off < 0.0 ? -1.0 : 1.0;
                    bool edge = std::fabs(off) == 0.5;
                    bool sunny = side * n.x * lit_x > 0.0;
                    uint32_t rgb = edge ? (sunny ? dark : light) : pale;
                    for (double t = 0.5 * step; t < s.length; t += step, ++j) {
                        double tilt = (pi_digit(j) - 4.5) / 10.0 + 0.15;
                        double len = g * (0.18 + 0.02 * pi_digit(7 * j + 3)) * (edge ? 1.0 : 0.6);
                        double at = t + 0.4 * step * (pi_digit(13 * j + 5) - 4.5) / 9.0;
                        Point p = add(add(s.a, u, at), n, off * s.girth);
                        Point d{side * n.x + tilt * u.x, side * n.h + tilt * u.h};
                        if (edge && pi_digit(3 * j + 1) >= 6.0) {
                            spines.add(trunk, {add(p, u, -0.25), add(p, d, 2.6 * len), add(p, u, 0.25)});
                        } else {
                            spines.add(rgb, {add(p, u, -0.15), add(p, d, len), add(p, u, 0.15)});
                        }
                    }
                }
            }
            for (TreeShape& s : spines.shapes) tree.shapes.push_back(std::move(s));
        }

        // Joshua tree spikes: a tuft at every branch tip, laid out like the
        // deciduous tips: 9, 11, then 13 spikes, cycling, fanned from -80 to
        // 80 degrees off the branch, each nudged a step by the digits of pi,
        // with its own length, thickness, and a slight curve. Thin spikes
        // turned toward the sun catch the light.
        if (tree.desert == 2) {
            Batch spikes;
            static const int kCounts[3] = {9, 11, 13};
            int j = 0;
            for (size_t k = 0; k < tree.tips.size(); ++k) {
                const Tree::Stem& b = tree.tips[k];
                int count = kCounts[k % 3];
                for (int m = 0; m < count; ++m, ++j) {
                    int a = static_cast<int>(std::lround(-8.0 + 16.0 * m / (count - 1)));
                    a += static_cast<int>(pi_digit(j)) % 3 - 1;
                    a = std::min(8, std::max(-8, a));
                    double c = kCos10[a < 0 ? -a : a], sn = a < 0 ? -kSin10[-a] : kSin10[a];
                    Point d{b.dir.x * c - b.dir.h * sn, b.dir.x * sn + b.dir.h * c};
                    Point side{-d.h, d.x};
                    double len = (0.45 * b.length + 2.0 * b.girth) * (0.75 + 0.05 * pi_digit(7 * j + 2));
                    double thick = pi_digit(11 * j + 4);
                    double w = b.girth * (0.08 + 0.025 * thick);
                    double bend = (pi_digit(13 * j + 6) - 4.5) / 4.5 * 0.12 * len;
                    Point mid = add(add(b.b, d, 0.5 * len), side, bend);
                    std::vector<Point> poly = {add(b.b, side, w), add(mid, side, 0.6 * w), add(add(b.b, d, len), side, 1.6 * bend),
                                               add(mid, side, -0.6 * w), add(b.b, side, -w)};
                    double dot = d.x * lx + d.h * lh, dd = d.x * d.x + d.h * d.h;
                    bool bright = thick <= 3.0 && dot > 0.0 && dot * dot >= 0.5 * dd * ll;
                    uint32_t leaf = tree.leaf_rgb[(k + static_cast<size_t>(m)) % 3];
                    spikes.add(bright ? blend(leaf, 0xDCE6A0, 0.45) : leaf, std::move(poly));
                }
            }
            for (TreeShape& s : spikes.shapes) tree.shapes.push_back(std::move(s));
        }
    }
}

}  // namespace bh::trees
