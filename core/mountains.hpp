#pragma once

#include <cstdint>
#include <vector>

#include "hash.hpp"
#include "scene.hpp"

// Mountains namespace (subspec/mountains.md).
namespace bh::mountains {

// Bits per slot for one ridge. levels[k] is the bit width of every point at
// Level k + 1.
struct RidgeLayout {
    int elevation;  // left anchor: extra elevation
    int tilt;       // right anchor: tilt
    std::vector<int> levels;
    int color;
    int palette;
};

// Ridges front to back. two_ridge = header bit 0.
const std::vector<RidgeLayout>& layout(bool three_ridge);

// Which shape generator builds a ridge's first, coarse structure.
enum class Generator {
    kMidpoint,  // baseline: two edge anchors, then midpoint displacement
    kSummit,    // experimental: dominant summit, shoulders, then detail
};

// Artistic and geometric parameters. Provisional; supplied by the theme.
struct Params {
    Generator generator = Generator::kMidpoint;
    double roughness = 0.1;       // r
    double margin = 0.5;          // gamma
    double tilt_range = 256.0;    // D: tilt in [-D, D]
    double visibility = 0.35;     // rho
    double min_exposure = 24.0;   // E_min
    double ground = 260.0;        // G: Ridge 1's lowest point at zero extra elevation
    double ceiling = 1472.0;      // C: no silhouette point above this
    double share[3] = {0.25, 0.5, 0.5};  // kappa per ridge: fraction of headroom it may use
    // Shape style per depth role: front, middle, back. The last ridge of either
    // mode is the back ridge, so two-ridge mode has no middle ridge.
    struct Style {
        Generator generator;
        double roughness;  // r
        double scale;      // vertical scale s
        int resolution;    // total subdivision depth (bit levels + detail levels)
    };
    // Calm, detailed foreground; gently peaked distance. Each scale brings the
    // role's exact worst-case silhouette height to about 650.
    Style style[3] = {
        {Generator::kMidpoint, 0.100, 1.372, 5},
        {Generator::kSummit, 0.125, 1.070, 4},
        {Generator::kSummit, 0.150, 0.930, 4},
    };

    // Effective values for one ridge, filled in by ridge_params() from its
    // style. generator and roughness above are effective values as well.
    double scale = 1.0;
    int resolution = 5;
    double detail = 2.0;                // k: strength of bit-free detail levels

    // Summit generator only. The summit sits at x in [x_min, x_max] * W; the
    // rise from the left edge and the drop to the right edge lie in
    // [rise_min, rise_max]. All later levels use the same roughness r.
    double summit_x_min = 0.2;
    double summit_x_max = 0.8;
    double rise_min = 160.0;
    double rise_max = 440.0;
};

// Static placement budget for one mode.
struct Budget {
    std::vector<double> max_height;  // per ridge, after vertical scale
    std::vector<double> reserve;     // per ridge: max(rho * max_height, E_min)
    double slack;                    // S; must be positive
};

// Parameters plus the budgets derived from them.
struct Config {
    Params params;
    Budget two;
    Budget three;

    const Budget& budget(bool three_ridge) const { return three_ridge ? three : two; }
};

Config make_config(const Params& params);
const Config& default_config();

// Depth role of ridge `index`: 0 front, 1 middle, 2 back.
int role(bool three_ridge, int index);

// A copy of p with generator, roughness, scale, and resolution set from the
// style of ridge `index`.
Params ridge_params(const Params& p, bool three_ridge, int index);

// Decoded slot values, read directly from the payload bits.
struct RidgeValues {
    uint32_t elevation = 0;
    uint32_t tilt = 0;
    std::vector<uint32_t> points;  // Level 1, Level 2 (left to right), ...
    uint32_t color = 0;
};

struct Values {
    bool three_ridge = false;
    std::vector<RidgeValues> ridges;  // front to back
};

Values decode(const Hash256& hash);
void encode(const Values& values, Hash256& hash);

// 2^bits - 1, the number of steps between the lowest and highest value.
double steps(int bits);

// Right anchor: tilt d in [-D, D].
double tilt_value(uint32_t v, int bits, const Params& p);

// One constrained midpoint-displacement step: computes the allowed interval
// [-t_max, t_max] first, then maps v into it.
Point subdivide(Point a, Point b, uint32_t v, int bits, const Params& p);

// The summit generator reads the tilt and Level 1 slots, which are adjacent
// in bit order, as one field: [x | rise | drop], each read MSB first.
struct SummitFields {
    int x_bits, rise_bits, drop_bits;
    uint32_t x, rise, drop;
};
SummitFields split_summit(const RidgeLayout& layout, uint32_t tilt, uint32_t first);
void join_summit(const RidgeLayout& layout, const SummitFields& f, uint32_t& tilt, uint32_t& first);

// The three points after the first bit level: left edge, first interior
// point (Level 1 midpoint or summit), right edge. Anchor-relative, s = 1.
std::vector<Point> skeleton(const RidgeLayout& layout, uint32_t tilt, uint32_t first, const Params& p);

// One bit-free detail step: the displacement is a fixed function of the
// segment's slope, so it adds vertices but no information.
Point detail_point(Point a, Point b, const Params& p);

// Anchor-relative shape s * Q: starts at (0, 0), ends at (2048, s * d).
// Bit-driven levels come first, then detail levels up to `depth`.
std::vector<Point> shape(const RidgeLayout& layout, uint32_t tilt,
                         const std::vector<uint32_t>& points, const Params& p, double s, int depth);

// Exact maximum of max(Q) - min(Q) over every tilt and point value, with s = 1,
// including detail levels down to `depth`.
double max_shape_height(const RidgeLayout& layout, const Params& p, int depth);

// Same as max_shape_height, but cached per layout, depth, and shape parameters.
double max_height(const RidgeLayout& layout, const Params& p, int depth);

// Places ridge `index` given the already placed ridges in front of it.
struct Placement {
    std::vector<Point> shape;  // s * Q
    double base;               // T
    double room;               // R
    double offset;             // T + e
};

Placement place(const Config& config, bool three_ridge, int index, const RidgeValues& values,
                const std::vector<Ridge>& front);

// Decodes the namespace and builds every ridge, front to back.
std::vector<Ridge> generate(const Config& config, const Hash256& hash);

// Linear interpolation of a polyline with strictly increasing x.
double height_at(const std::vector<Point>& poly, double x);

}  // namespace bh::mountains
