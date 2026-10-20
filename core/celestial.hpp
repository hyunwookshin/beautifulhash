#pragma once

#include <cstdint>
#include <vector>

#include "hash.hpp"
#include "scene.hpp"

// Celestial namespace (subspec/celestial.md): the implicit sun. It is never
// drawn; it lights the front hill's bands and casts the trees' shadows.
namespace bh::celestial {

inline constexpr int kAzimuthBits = 4;
inline constexpr int kElevationBits = 4;

struct Values {
    uint32_t azimuth = 0, elevation = 0;
};

Values decode(const Hash256& hash);
void encode(const Values& values, Hash256& hash);

Sun sun(const Values& values);
Sun generate(const Hash256& hash);

// The front hill's ground surface: height at depth fraction f (0 on the
// silhouette, 1 at the canvas bottom) where the silhouette is s and the
// hill's lowest height is low. The same surface shapes the light bands.
double surface(double s, double low, double f);

// Shadow polygons of every tree on the front hill (hill 0), cast on its
// surface by the sun. Trees elsewhere get none.
void cast_shadows(std::vector<Tree>& trees, const Hill& front, const Sun& sun);

}  // namespace bh::celestial
