#pragma once

#include <vector>

#include "scene.hpp"
#include "theme.hpp"

// Canopy textures: small fixed templates stamped row by row over a surface,
// each a step brighter or darker than the color beneath it. The front ridge
// gets trees (subspec/mountains.md, Front-ridge canopy) and the front hill
// gets grass blades (subspec/hills.md, Grass blades). No bits.
namespace bh::canopy {

// Covers the front ridge with conifers (Alpine) or needle sprays (Temperate),
// above whatever the hills hide, and lights the sunlit faces of its sharp
// peaks. Other biomes get none.
void cover_ridge(Ridge& front, const std::vector<Hill>& hills, const Sun& sun, Theme theme, Season season);

// Covers the front hill with grass blades over its bands, in Alpine and
// Temperate, sparsely in winter. Needs the bands.
void cover_hill(Hill& hill, const Sun& sun, Theme theme, Season season);

}  // namespace bh::canopy
