#pragma once

#include <cstdint>

#include "hash.hpp"

// Global namespace (subspec/global.md). Only the daylight field is read so
// far; theme and season still come from the configuration.
namespace bh::global {

inline constexpr int kDaylightBits = 2;
inline constexpr uint32_t kNight = 3;  // daylight 0-2 are day, refining the sun's direction

struct Values {
    uint32_t daylight = 0;
};

Values decode(const Hash256& hash);
void encode(const Values& values, Hash256& hash);

}  // namespace bh::global
