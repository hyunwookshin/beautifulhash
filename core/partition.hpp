#pragma once

#include <array>

namespace bh {

// Top-level namespace allocation (SPECIFICATIONS.md, Hash Namespace Partitioning).
struct Namespace {
    const char* name;
    int begin;  // first absolute bit
    int width;
};

inline constexpr Namespace kGlobal{"global", 0, 16};
inline constexpr Namespace kSky{"sky", 16, 24};
inline constexpr Namespace kCelestial{"celestial", 40, 8};
inline constexpr Namespace kMountains{"mountains", 48, 88};
inline constexpr Namespace kHills{"hills", 136, 40};
inline constexpr Namespace kTrees{"trees", 176, 48};
inline constexpr Namespace kVegetation{"vegetation", 224, 24};
inline constexpr Namespace kFog{"fog", 248, 8};

inline constexpr std::array<Namespace, 8> kNamespaces = {
    kGlobal, kSky, kCelestial, kMountains, kHills, kTrees, kVegetation, kFog,
};

}  // namespace bh
