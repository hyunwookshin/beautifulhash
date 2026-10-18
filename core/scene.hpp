#pragma once

#include <cstdint>
#include <vector>

namespace bh {

// Logical canvas (SPECIFICATIONS.md, Canvas).
inline constexpr double kCanvasWidth = 2048.0;
inline constexpr double kCanvasHeight = 1536.0;

// Logical coordinates: x from the left edge, h upward from the bottom edge.
struct Point {
    double x;
    double h;
};

struct Ridge {
    std::vector<Point> points;  // strictly increasing x, from x = 0 to x = 2048
    uint32_t rgb;               // 0xRRGGBB, resolved from the theme palette

    // Biome styling drawn over the ridge: closed polygons above the snowline.
    std::vector<std::vector<Point>> snow;
    uint32_t snow_rgb = 0;
};

struct Fog {
    bool enabled = false;
};

struct Scene {
    int version = 0;
    std::vector<Ridge> ridges;  // front to back: ridges[0] is Ridge 1
    Fog fog;
};

}  // namespace bh
