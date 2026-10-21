#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hash.hpp"
#include "scene.hpp"
#include "theme.hpp"

// Trees namespace (subspec/trees.md).
namespace bh::trees {

enum class Species { kConifer, kDeciduous, kCactus, kJoshua };

// Where a tree stands: on a hill's rim (its silhouette) or in its interior
// (on the slope, below the rim and nearer the viewer).
enum class Group { kFrontRim, kBackRim, kFrontInterior, kBackInterior };

struct Fork {
    int ratio_bits = 0, side_bits = 0, length_bits = 0;
    uint32_t ratio = 0, side = 0, length = 0;  // side: deciduous only
};

struct TreeValues {
    int girth_bits = 0, height_bits = 0;
    uint32_t girth = 0, height = 0;
    std::vector<Fork> forks;  // breadth-first
};

struct Values {
    int count = 1;               // 1-10, prefix-coded
    uint32_t arrangement = 0;    // 2 bits: which group sequence
    uint32_t size = 0;           // 2 bits: global scale class
    int leaf_bits = 0;           // 4 when leafy(), else 0
    uint32_t leaf = 0;           // leaf palette
    std::vector<TreeValues> trees;
};

// The plan fixed by the header fields: each tree's group and bit allocation.
// Composition role of a tree on its hill: one grove (a leader and members),
// one isolated medium tree, and stragglers, leaving open hillside elsewhere.
enum class Role { kLeader, kMember, kIsolated, kStraggler };

struct Plan {
    std::vector<Group> groups;
    std::vector<Role> roles;
    std::vector<double> x;           // position along the hill, logical units
    std::vector<double> prominence;  // size multiplier from the composition
    std::vector<double> slope;       // interior trees: fraction of the way down from the rim (0 on the rim)
    std::vector<double> scale;       // perspective, prominence, and slope depth (before the size class)
    std::vector<int> bits;
};

Plan plan(int count, uint32_t arrangement, int remaining, bool two_hills);
TreeValues tree_fields(int bits, Species species);  // field widths for a tree with this many bits

// Species by biome: conifers in Alpine and Tundra, deciduous in Temperate, and
// cactus or Joshua tree alternating in Desert (by index and arrangement).
Species species_of(Theme theme, int index, uint32_t arrangement);

// Prefix code for the count: 2-7 use 3 bits, 1 and 8-10 use 4 bits.
int count_code_bits(int count);

// Whether deciduous leaves are drawn, and so whether the 4 leaf palette bits
// are read: Temperate, except in winter.
bool leafy(Theme theme, Season season);

// Bits left for the trees after the header.
int tree_bit_budget(int count, Theme theme, Season season);

Values decode(const Hash256& hash, bool two_hills, Theme theme, Season season);
void encode(const Values& values, Hash256& hash);

// Builds tree `index` (needs the hills it stands on).
Tree build(const Values& v, const Plan& plan, int index, const std::vector<Hill>& hills, Theme theme, Season season);

// Colors the leaf dots of every tree: trees from left to right, each tree's
// dots in its own left-to-right order, cycling through the six palette colors.
void paint_leaves(std::vector<Tree>& trees, const Values& v, Season season);

std::vector<Tree> generate(const Hash256& hash, const std::vector<Hill>& hills, Theme theme, Season season);

// On the front hill, grass and snow at the tree bases take the hill's darkest
// band color (subspec/trees.md, Ground cover). Call after the bands exist.
void tint_cover(std::vector<Tree>& trees, const Hill& front);

// Decorates desert plants once the sun is known (subspec/trees.md, Desert
// plants in the sun): a sunlit side on the Joshua tree's trunk and upright
// cactus segments, cactus spines, and Joshua tree spike tufts.
void finish(std::vector<Tree>& trees, const Sun& sun);

// Two-hill scenes: a line of small fixed-shape trees along the back hill's
// crest (subspec/trees.md, Crest trees). Alpine conifers and Temperate
// deciduous trees only. No bits.
void add_crest_trees(std::vector<Hill>& hills, const Hash256& hash, Theme theme, Season season);

// One tree's SVG fragment, one element per line (shared by render_svg and verify).
std::string tree_svg(const Tree& tree, int index, double scale);

}  // namespace bh::trees
