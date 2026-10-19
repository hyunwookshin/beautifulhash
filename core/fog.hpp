#pragma once

#include <cstdint>

#include "hash.hpp"
#include "scene.hpp"
#include "theme.hpp"

// Fog namespace (subspec/fog.md).
namespace bh::fog {

// Bits per field, in reading order.
inline constexpr int kDensityBits = 2;
inline constexpr int kTintBits = 3;
inline constexpr int kMistBits = 3;

struct Values {
    uint32_t density = 0, tint = 0, mist = 0;
};

Values decode(const Hash256& hash);
void encode(const Values& values, Hash256& hash);

// The haze layer every depth gap gets: a tint and its top and bottom opacity.
Fog build(const Values& values, Theme theme, Season season);

Fog generate(const Hash256& hash, Theme theme, Season season);

}  // namespace bh::fog
