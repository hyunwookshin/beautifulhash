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

// One path of small templates (trees or grass blades) of one color, one closed
// subpath each.
struct CanopyPath {
    uint32_t rgb;
    std::vector<std::vector<Point>> polys = {};
    bool facet = false;  // a front-ridge facet (class "facet") rather than a canopy template
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
    // Alpine snow faces (subspec/mountains.md, Snow faces), drawn clipped to the
    // snow caps and lit by the sun: the left (-1) and right (+1) halves of
    // each peak, and the realistic reflective triangles as glints.
    std::vector<std::vector<Point>> faces;
    std::vector<int> face_side;
    std::vector<std::vector<Point>> glints;
    std::vector<int> glint_side;
    double fade_top = 0.0, fade_bottom = 0.0;  // faces: full strength above fade_top, gone by fade_bottom
    // Clipped to the ridge: the front ridge's canopy of small template trees
    // in Alpine and Temperate (subspec/mountains.md, Front-ridge canopy), or
    // rock facets (subspec/mountains.md, Ridge facets).
    std::vector<CanopyPath> canopy;
    // Facets over the snow cap, in the snow's color, clipped to the cap.
    std::vector<CanopyPath> snow_facets;
    // Each faced peak's top and incoming slope (subspec/mountains.md, Ridge
    // faces), which the facets follow.
    std::vector<Point> peak_tops, peak_slopes;
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
    // Front hill only: shading bands, top to bottom. Each is the top edge of a
    // region closed down to the canvas bottom, drawn over the hill in order.
    std::vector<std::vector<Point>> bands;
    std::vector<uint32_t> band_rgb;
    // Back hill only: a line of small fixed-shape trees along its crest.
    // Each shape is one path: an outline, then any holes (even-odd fill).
    std::vector<std::vector<std::vector<Point>>> crest_trees;
    std::vector<uint32_t> crest_rgb;
    // Front hill only, Alpine and Temperate: grass blades over the bands
    // (subspec/hills.md, Grass blades).
    std::vector<CanopyPath> canopy;
};

// One drawn part of a tree: a polygon, or a circle when `poly` is empty.
struct TreeShape {
    std::vector<Point> poly;
    Point center{0, 0};
    double radius = 0;
    uint32_t rgb = 0;
    // Leaf (an eye shape) when ry > 0: radius is the half-length along `axis` (a
    // unit direction), ry the widest half-width; darker leaves render narrower.
    double ry = 0;
    Point axis{1, 0};
    // Many small polygons in one path when set (cactus spines), written as
    // relative steps; poly is then empty.
    std::vector<std::vector<Point>> polys = {};
};

// One trunk or branch segment, for casting shadows: its ends and widths.
struct Limb {
    Point a, b;
    double wa, wb;
};

struct Tree {
    std::vector<TreeShape> shapes;  // branches breadth-first, then foliage
    int hill = 0;                   // index into Scene::hills it stands on
    int group = 0;                  // trees::Group
    double x = 0.0;                 // base position, for left-to-right leaf order
    int leaves = 0;                 // leaf dots: the last `leaves` shapes, left to right
    Point base{0, 0};               // where the trunk meets the ground
    double scale = 1.0;             // overall size factor (composition and size class)
    std::vector<Limb> limbs;        // trunk and branches (not foliage)
    std::vector<std::vector<Point>> shadow;  // front hill: shadow polygons on the ground
    std::vector<std::vector<Point>> cover;   // ground cover at the base: grass, snow, or rocks
    std::vector<uint32_t> cover_rgb;
    bool cover_tinted = false;              // grass or snow: takes the front hill's darkest band color
    // Desert plants, decorated once the sun is known (trees::finish): the
    // cactus segments and the Joshua tree's trunk get a sunlit side, cacti
    // their spines, and Joshua trees spike tufts at their branch tips.
    struct Stem {
        Point a, b, dir;
        double length = 0.0, girth = 0.0;
    };
    std::vector<Stem> stems, tips;
    int desert = 0;  // 1 cactus, 2 Joshua tree
    bool waisted = false;  // limbs narrow toward their middles (Joshua tree)
    uint32_t trunk_rgb = 0, leaf_rgb[3] = {0, 0, 0};
};

// One foreground plant (subspec/vegetation.md): a fallen leaf or petal (an eye
// shape like a tree leaf), a grass tuft of five thin blades, a desert rosette
// of five broad pointed leaves, or a winter snow pock: a small cup, \_/, where
// needles or a footprint pressed into the snow.
enum class PlantKind { kLeaf, kTuft, kRosette, kPock };

struct Plant {
    PlantKind kind = PlantKind::kLeaf;
    Point base{0, 0};    // leaf: its center; tuft and rosette: the middle of the root
    Point axis{1, 0};    // leaf: unit direction of its long axis
    double size = 1.0;   // leaf: half-length; tuft and rosette: tallest blade
    double lean = 0.0;   // tuft and rosette: how far the outer blades fan out, as a fraction of the height
    double wide = 1.0, deep = 0.4;  // pock: rim width factor and depth as a fraction of the half-width
    double slim = 1.0;   // leaf: width factor; fallen leaves seen edge-on are far skinnier
    uint32_t rgb = 0;
};

// The sky (subspec/sky.md): its tint and one cloud, a curved shape inside a
// trapezoid, crisp on its leading side and dissolving elsewhere.
struct Sky {
    uint32_t tint = 0;     // index into the sky tints
    Point corner[4]{};     // the trapezoid, clockwise from top left
    Point point[5]{};      // points on its boundary, in order
    int edge[5]{};         // the edge (0 top, 1 right, 2 bottom, 3 left) each point lies on
    std::vector<Point> outline;  // the refined outline: 625 points, in order
    Point source[5]{};     // glow sources: the five boundary points, pulled inward
    bool strong[5]{};      // sources on the leading half glow twice as strong
    double glow_r = 0.0;   // glow radius
    int lead = 0;          // crisp side: 0 left, 1 right
    int peak = 85;         // peak opacity, hundredths
};

// One haze layer, repeated in every depth gap (subspec/fog.md). Opacities are in
// hundredths, at the top and bottom of the canvas.
struct Fog {
    uint32_t rgb = 0xdde5ec;
    int top = 32;
    int bottom = 32;
};

// The implicit sun (subspec/celestial.md): never drawn, but it lights the front
// hill's bands and casts the trees' shadows.
struct Sun {
    double dx = 1.0, dy = 0.0;  // shadow direction on the ground: right, toward the viewer
    double length = 1.0;        // shadow length per unit of height (cot of the elevation)
    double tilt = 0.0;          // band tilt toward the shaded side
    double spacing = 0.0;       // band spacing curve
    int elevation = 0;          // elevation step, 0 (20 degrees) to 15 (65 degrees)
};

struct Scene {
    int version = 0;
    std::vector<Ridge> ridges;  // front to back: ridges[0] is Ridge 1
    std::vector<Hill> hills;    // front to back: hills[0] is the front (or only) hill
    std::vector<Tree> trees;    // in drawing order within each hill
    std::vector<Plant> vegetation;  // on the front (or only) hill, back to front
    Fog fog;
    Sun sun;
    Sky sky;
};

}  // namespace bh
