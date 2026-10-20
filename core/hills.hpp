#pragma once

#include <cstdint>
#include <vector>

#include "hash.hpp"
#include "scene.hpp"
#include "theme.hpp"

// Hills namespace (subspec/hills.md).
namespace bh::hills {

// Bits per field for one hill.
struct HillLayout {
    int x;      // crest position
    int crest;  // crest height
    int left;   // left edge height, as a fraction of the crest height
    int right;  // right edge height, as a fraction of the crest height
    int shape;  // profile exponent n - 1
    int color;
    int palette;  // HillPaletteId
};

// Hills front to back. two = header bit 1.
const std::vector<HillLayout>& layout(bool two);

// Provisional geometric parameters, in logical units.
struct Params {
    double cap = 0.15 * kCanvasHeight;  // the back (or only) hill's crest never exceeds this
    double single_min = 60.0;           // one-hill mode: lowest crest
    double front_min = 40.0;            // two-hill mode: front crest range
    double front_max = 150.0;
    double exposure = 30.0;             // back crest rises at least this far above the front hill
    double x_min = 0.15;                // crest position range, as a fraction of the width
    double x_max = 0.85;
    double edge_min = 0.15;             // edge height range, as a fraction of the crest height
    double edge_max = 0.80;
    int samples = 32;                   // segments per side of the crest
};

const Params& default_params();

struct HillValues {
    uint32_t x = 0, crest = 0, left = 0, right = 0, shape = 0, color = 0;
};

struct Values {
    bool two = false;
    std::vector<HillValues> hills;  // front to back
};

Values decode(const Hash256& hash);
void encode(const Values& values, Hash256& hash);

double steps(int bits);

// Profile P_n(u): 1 at the crest (u = 0), 0 at the edge (u = 1).
double profile(int exponent, double u);

// Crest height interval for hill `index`, given the hills in front of it and
// the crest position: {lowest, highest}. The bits map linearly into it.
struct Interval {
    double lo, hi;
};
Interval crest_interval(const Params& p, bool two, int index, double crest_x, const std::vector<Hill>& front);

// Builds hill `index` from its values, given the already built hills in front.
Hill build(const Params& p, bool two, int index, const HillValues& v, const std::vector<Hill>& front,
           Theme theme, Season season);

// Adds the front hill's shading bands, lit by the sun (subspec/hills.md,
// Shading bands).
void add_bands(Hill& hill, const Sun& sun);

// Decodes the namespace and builds every hill, front to back.
std::vector<Hill> generate(const Hash256& hash, Theme theme, Season season);

}  // namespace bh::hills
