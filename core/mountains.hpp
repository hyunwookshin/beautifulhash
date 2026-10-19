#pragma once

#include <cstdint>
#include <vector>

#include "hash.hpp"
#include "scene.hpp"
#include "theme.hpp"

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
    double min_exposure = 48.0;   // E_min: at least twice the tallest biome texture
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
        bool jagged;       // detail levels: jagged, reusing bit values (true) or smooth concave (false)
    };
    // Calm, detailed foreground; gently peaked, jagged distance. Each scale
    // brings the role's exact worst-case silhouette height to about 650.
    Style style[3] = {
        {Generator::kMidpoint, 0.100, 1.372, 5, false},
        {Generator::kSummit, 0.125, 1.070, 6, true},
        {Generator::kSummit, 0.150, 0.930, 6, true},
    };
    double jag_decay = 0.75;  // jagged detail: amplitude multiplier per level
    double jag_floor = 0.35;  // jagged detail: minimum fraction of the allowed interval

    // Effective values for one ridge, filled in by ridge_params() from its
    // style. generator and roughness above are effective values as well.
    double scale = 1.0;
    int resolution = 5;
    bool jagged = false;
    double detail = 2.0;                // k: strength of bit-free detail levels

    // Summit generator only. The summit sits at x in [x_min, x_max] * W; the
    // rise from the left edge and the drop to the right edge lie in
    // [rise_min, rise_max]. All later levels use the same roughness r.
    // Biome styling. Applied after placement, so it never changes placement or
    // the hash-derived geometry. Provisional; tuned from rendered samples.
    Theme theme = Theme::kAlpine;
    Season season = Season::kSummer;  // context from Global; changes only Temperate mountain colors
    int conifer_subdivisions = 1;     // alpine front ridge: straight subdivisions per segment
    double conifer_height = 18.0;     // alpine front ridge: nominal tooth height (varies 0.6-1.2x)
    double conifer_depth = 60.0;      // alpine front ridge: deepest valley cut below the silhouette (varies 0.5-1x)
    int canopy_subdivisions = 0;      // temperate front ridge: straight subdivisions per segment
    double canopy_height = 16.0;      // temperate front ridge: nominal bump height (varies 0.55-1.3x)
    double canopy_depth = 45.0;       // temperate front ridge: grooves and cavities below the silhouette
    double tree_threshold = 0.35;     // tree density score below which the front ridge is flat
    double bare_depth = 100.0;        // temperate winter: deepest needle cut below the silhouette
    double snow_fraction = 1.0 / 3.0; // alpine back ridge: snowline this far down from the peak
    double snow_wave = 36.0;          // alpine back ridge: snowline wave amplitude, logical units
    int snow_levels = 5;              // alpine back ridge: snowline subdivision levels (33 knots)
    double snow_decay = 0.85;         // alpine back ridge: snowline amplitude multiplier per level
    uint32_t snow_rgb = 0xEEF2F6;

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

// Alpine front ridge: subdivides every segment conifer_subdivisions times with
// no displacement, cuts the new points conifer_depth below the silhouette as
// valleys, and adds a sharp two-tooth zigzag per segment, giving a conifer tree
// line that is part of the silhouette. Sizes scale with the tree density; tooth position
// and height also vary by a fixed pattern that does not depend on the hash.
std::vector<Point> add_conifers(const std::vector<Point>& pts, const Params& p, const std::vector<Point>& density);

// Temperate front ridge: subdivides every segment canopy_subdivisions times
// with no displacement, then turns each segment into a rounded bump (a
// seven-point arc), giving a deciduous canopy that is part of the silhouette.
// Bump width and height vary by a fixed pattern that does not depend on the hash.
std::vector<Point> add_canopy(const std::vector<Point>& pts, const Params& p, const std::vector<Point>& density);

// Tree density along a front ridge, as knots (x, score in [0, 1]) at the
// deepest bit level's points. Each score averages that point's value with its
// parent point's value, so forest cover varies with the hash without using
// any extra bits. `pts` are the placed, unstyled points at total depth `depth`.
std::vector<Point> tree_density(const std::vector<Point>& pts, const RidgeLayout& layout, const RidgeValues& values,
                                int depth);

// The silhouette of ridge `index` as serialized: its placed points plus any
// biome texture (alpine conifers or temperate canopy on the front ridge), sized
// by the ridge's tree density and flat where the density is below threshold.
std::vector<Point> style_points(const std::vector<Point>& pts, const Params& p, bool three_ridge, int index,
                                const RidgeValues& values);

// Alpine back ridge: closed polygons of the ridge above a gently wavy snowline
// centered snow_fraction of the way down from its highest point to its lowest.
// The wave is smooth 1D midpoint displacement seeded from the ridge's decoded
// values, with amplitude decaying by snow_decay at each level.
std::vector<std::vector<Point>> snow_caps(const std::vector<Point>& pts, const Params& p, const RidgeValues& values);

// Total subdivision depth of ridge `index` as serialized, including biome
// styling levels.
int styled_depth(const Params& p, bool three_ridge, int index);

// Smallest fraction of a pre-texture segment that one serialized texture
// segment can span (1 when the ridge has no texture). Used for separation bounds.
double texture_min_fraction(const Params& p, bool three_ridge, int index);

// Decodes the namespace and builds every ridge, front to back.
std::vector<Ridge> generate(const Config& config, const Hash256& hash);

// Linear interpolation of a polyline with strictly increasing x.
double height_at(const std::vector<Point>& poly, double x);

}  // namespace bh::mountains
