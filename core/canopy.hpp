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

// Faceted rock (subspec/mountains.md, Ridge facets): every Tundra ridge, the
// front two in Desert, and the Alpine back ridge with its snow. A pi-jittered
// grid of points split into triangles, each lit by how its tilt faces the sun.
void facet_ridges(std::vector<Ridge>& ridges, const std::vector<Hill>& hills, const Sun& sun, Theme theme);

// Covers the front hill with grass blades over its bands: in Alpine, Temperate,
// and Tundra, and a winter sprinkle in every biome (Desert only then). Needs
// the bands.
void cover_hill(Hill& hill, const Sun& sun, Theme theme, Season season);

}  // namespace bh::canopy
