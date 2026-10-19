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
    int ratio_bits = 0, length_bits = 0;
    uint32_t ratio = 0, length = 0;
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
    std::vector<int> bits;
};

Plan plan(int count, uint32_t arrangement, int remaining, bool two_hills);
TreeValues tree_fields(int bits);  // field widths for a tree with this many bits

// Species by biome: conifers in Alpine and Tundra, deciduous in Temperate, and
// cactus or Joshua tree alternating in Desert (by index and arrangement).
Species species_of(Theme theme, int index, uint32_t arrangement);

// Prefix code for the count: 2-7 use 3 bits, 1 and 8-10 use 4 bits.
int count_code_bits(int count);

Values decode(const Hash256& hash, bool two_hills);
void encode(const Values& values, bool two_hills, Hash256& hash);

// Builds tree `index` (needs the hills it stands on).
Tree build(const Values& v, const Plan& plan, int index, const std::vector<Hill>& hills, Theme theme, Season season);

std::vector<Tree> generate(const Hash256& hash, const std::vector<Hill>& hills, Theme theme, Season season);

// One tree's SVG fragment, one element per line (shared by render_svg and verify).
std::string tree_svg(const Tree& tree, int index, double scale);

}  // namespace bh::trees
