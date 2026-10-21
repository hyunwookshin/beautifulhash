#pragma once

#include <cstdint>

#include "hash.hpp"
#include "theme.hpp"

// Global namespace (subspec/global.md): daylight (bits 0-1), theme (2-3), and
// season (4-5). Bits 6-15 shape the second cloud (sky.hpp).
namespace bh::global {

inline constexpr int kDaylightBits = 2;
inline constexpr uint32_t kNight = 3;  // daylight 0-2 are day, refining the sun's direction

struct Values {
    uint32_t daylight = 0;
    uint32_t theme = 0;   // Alpine, Temperate, Tundra, Desert
    uint32_t season = 0;  // winter, spring, summer, fall
};

// The biome and season the bits select.
Theme theme(const Values& v);
Season season(const Values& v);

Values decode(const Hash256& hash);
void encode(const Values& values, Hash256& hash);

}  // namespace bh::global
