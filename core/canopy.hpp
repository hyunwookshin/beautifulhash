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

// Tundra and Desert: faceted rock on the front ridge. A pi-jittered grid of
// points from the silhouette down to the hills, split into triangles, each
// lit by how its tilt faces the sun (subspec/mountains.md, Front-ridge facets).
void facet_ridge(Ridge& front, const std::vector<Hill>& hills, const Sun& sun, Theme theme);

// Covers the front hill with grass blades over its bands: in Alpine, Temperate,
// and Tundra, and a winter sprinkle in every biome (Desert only then). Needs
// the bands.
void cover_hill(Hill& hill, const Sun& sun, Theme theme, Season season);

}  // namespace bh::canopy
