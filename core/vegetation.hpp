#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hash.hpp"
#include "scene.hpp"
#include "theme.hpp"

// Vegetation namespace (subspec/vegetation.md).
namespace bh::vegetation {

// Bits per field, in reading order.
inline constexpr int kPaletteBits = 4;
inline constexpr int kDensityBits = 2;
inline constexpr int kSpreadBits = 2;
inline constexpr int kSeedBits = 16;

struct Values {
    uint32_t palette = 0, density = 0, spread = 0, seed = 0;
};

Values decode(const Hash256& hash);
void encode(const Values& values, Hash256& hash);

// Plants per cluster for a density value.
int per_cluster(uint32_t density);

// Where plants gather: the base and size of every tree on the front (or only)
// hill, left to right, or one stand-in on a bare hill.
struct Anchor {
    Point base;
    double scale;
};
std::vector<Anchor> anchors(const std::vector<Tree>& trees, const std::vector<Hill>& hills);

// Plants in generation order (cluster by cluster), before depth sorting.
// `limit` stops early (verify uses 1).
std::vector<Plant> scatter(const Values& v, const std::vector<Anchor>& anchors, const Hill& front, Theme theme,
                           Season season, size_t limit = SIZE_MAX);

// The drawn plants: scattered, colored, and sorted back to front.
std::vector<Plant> build(const Values& v, const std::vector<Tree>& trees, const std::vector<Hill>& hills,
                         Theme theme, Season season);

std::vector<Plant> generate(const Hash256& hash, const std::vector<Tree>& trees, const std::vector<Hill>& hills,
                            Theme theme, Season season);

// One plant's SVG element (no newline), shared by render_svg and verify.
std::string plant_svg(const Plant& p, double scale);

}  // namespace bh::vegetation
