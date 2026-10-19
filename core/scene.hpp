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

    // When set, the ridge fills with a vertical gradient from rgb at its top to
    // rgb_bottom at the canvas bottom (Temperate fall front ridge).
    bool gradient = false;
    uint32_t rgb_bottom = 0;

    // Biome styling drawn over the ridge: closed polygons above the snowline.
    std::vector<std::vector<Point>> snow;
    uint32_t snow_rgb = 0;
};

// A hill's defining points and shape, advertised as context to the namespaces
// that plant on hills (Trees, Vegetation). Logical coordinates.
struct HillProfile {
    double crest_x, crest_h;  // the single peak
    double left_h, right_h;   // where the hill meets the left (x = 0) and right (x = 2048) edges
    int exponent;             // shape: 1 tent, 2 dome, 3 sigmoid, 4 W
};

struct Hill {
    std::vector<Point> points;  // sampled curve, strictly increasing x, from x = 0 to x = 2048
    uint32_t rgb;
    HillProfile profile;
};

// One drawn part of a tree: a polygon, or a circle when `poly` is empty.
struct TreeShape {
    std::vector<Point> poly;
    Point center{0, 0};
    double radius = 0;
    uint32_t rgb = 0;
    // Ellipse (leaves) when ry > 0: radius is the half-axis along `axis` (a unit
    // direction), ry the half-axis across it.
    double ry = 0;
    Point axis{1, 0};
};

struct Tree {
    std::vector<TreeShape> shapes;  // branches breadth-first, then foliage
    int hill = 0;                   // index into Scene::hills it stands on
    int group = 0;                  // trees::Group
    double x = 0.0;                 // base position, for left-to-right leaf order
    int leaves = 0;                 // leaf dots: the last `leaves` shapes, left to right
};

// One haze layer, repeated in every depth gap (subspec/fog.md). Opacities are in
// hundredths, at the top and bottom of the canvas.
struct Fog {
    uint32_t rgb = 0xdde5ec;
    int top = 32;
    int bottom = 32;
};

struct Scene {
    int version = 0;
    std::vector<Ridge> ridges;  // front to back: ridges[0] is Ridge 1
    std::vector<Hill> hills;    // front to back: hills[0] is the front (or only) hill
    std::vector<Tree> trees;    // in drawing order within each hill
    Fog fog;
};

}  // namespace bh
