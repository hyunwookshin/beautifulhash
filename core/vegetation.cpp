#include "vegetation.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "format.hpp"
#include "mountains.hpp"
#include "palette.hpp"
#include "partition.hpp"

namespace bh::vegetation {

namespace {

const int kPerCluster[4] = {40, 56, 72, 96};
const double kSpread[4] = {0.8, 1.0, 1.25, 1.5};

// The Vegetation PRNG: a fixed integer mix (the splitmix64 finalizer) of the
// seed bits and the plant's place, so every value is reproducible without
// any running state.
double uniform(uint32_t seed, size_t anchor, size_t item, int stream) {
    uint64_t z = static_cast<uint64_t>(seed) << 32 | static_cast<uint64_t>(anchor) << 20 |
                 static_cast<uint64_t>(item) << 4 | static_cast<uint64_t>(stream);
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);
}

void append_hex(std::string& out, uint32_t rgb) {
    static const char* digits = "0123456789abcdef";
    out.push_back('#');
    for (int shift = 20; shift >= 0; shift -= 4) out.push_back(digits[(rgb >> shift) & 15]);
}

void append_point(std::string& out, double x, double h, double scale) {
    append_fixed2(out, x * scale);
    out += ",";
    append_fixed2(out, (kCanvasHeight - h) * scale);
}

// The plant for a biome and season: winter is snow pocks everywhere; the
// desert grows rosettes; elsewhere spring has petals and summer grass. Only
// Temperate (deciduous) trees drop leaves in fall; the evergreen biomes have
// dry grass instead.
PlantKind kind_of(Theme theme, Season season) {
    if (season == Season::kWinter) return PlantKind::kPock;
    if (theme == Theme::kDesert) return PlantKind::kRosette;
    if (season == Season::kSpring) return PlantKind::kLeaf;
    if (season == Season::kFall && theme == Theme::kTemperate) return PlantKind::kLeaf;
    return PlantKind::kTuft;
}

}  // namespace

Values decode(const Hash256& hash) {
    BitReader in(hash, kVegetation.begin, kVegetation.width);
    Values v;
    v.palette = in.read(kPaletteBits);
    v.density = in.read(kDensityBits);
    v.spread = in.read(kSpreadBits);
    v.seed = in.read(kSeedBits);
    if (in.remaining() != 0) {
        std::fprintf(stderr, "vegetation: layout leaves %d bits unread\n", in.remaining());
        std::abort();
    }
    return v;
}

void encode(const Values& values, Hash256& hash) {
    BitWriter out(hash, kVegetation.begin, kVegetation.width);
    out.write(kPaletteBits, values.palette);
    out.write(kDensityBits, values.density);
    out.write(kSpreadBits, values.spread);
    out.write(kSeedBits, values.seed);
}

int per_cluster(uint32_t density) { return kPerCluster[density]; }

std::vector<Anchor> anchors(const std::vector<Tree>& trees, const std::vector<Hill>& hills) {
    std::vector<Anchor> out;
    for (const Tree& t : trees) {
        if (t.hill == 0) out.push_back({t.base, t.scale});
    }
    std::stable_sort(out.begin(), out.end(), [](const Anchor& a, const Anchor& b) { return a.base.x < b.base.x; });
    if (out.empty()) {
        // A bare front hill: one stand-in below its crest, partway down the slope.
        const HillProfile& p = hills[0].profile;
        out.push_back({{p.crest_x, 0.55 * mountains::height_at(hills[0].points, p.crest_x)}, 1.0});
    }
    return out;
}

std::vector<Plant> scatter(const Values& v, const std::vector<Anchor>& anchors, const Hill& front, Theme theme,
                           Season season, size_t limit) {
    std::vector<Plant> out;
    int n = kPerCluster[v.density];
    // Fallen leaves (Temperate fall) and frosted grass (winter) gather at the
    // trees; everything else fills the open ground away from them: the gaps
    // between the anchors and the canvas edges.
    bool fallen = theme == Theme::kTemperate && season == Season::kFall;
    bool near = fallen || season == Season::kWinter;
    PlantKind kind = kind_of(theme, season);
    std::vector<double> edges = {0.0};
    for (const Anchor& an : anchors) edges.push_back(an.base.x);
    edges.push_back(kCanvasWidth);
    for (size_t a = 0; a < anchors.size(); ++a) {
        const Anchor& an = anchors[a];
        double r = kSpread[v.spread] * 90.0 * an.scale;
        for (int i = 0; i < n; ++i) {
            if (out.size() >= limit) return out;
            auto u = [&](int stream) { return uniform(v.seed, a, static_cast<size_t>(i), stream); };
            double x, h, rim, sigma;
            // Two fallen leaves in five have blown loose: they land anywhere on
            // the hill, on open ground as well as under the trees.
            bool stray = fallen && (i % 5 == 1 || i % 5 == 4);
            if (near && !stray) {
                // Across: a triangular spread peaked at the tree, mapped into
                // the part of the canvas within 1.5 r of it (2.3 r for fallen
                // leaves, which scatter more loosely than snow pocks).
                double reach = fallen ? 2.3 * r : 1.5 * r;
                double x_lo = std::max(0.0, an.base.x - reach), x_hi = std::min(kCanvasWidth, an.base.x + reach);
                x = x_lo + (x_hi - x_lo) * (0.5 + 0.5 * (u(0) - u(1)));
                // Down the slope, never above the silhouette: snow pocks gather
                // just in front of the tree (the product of two uniforms is
                // mostly near 0); fallen leaves spread evenly over a deeper band.
                rim = mountains::height_at(front.points, x);
                double h_hi = std::min(an.base.h + (fallen ? 0.3 : 0.15) * r, 0.97 * rim);
                double h_lo = std::min(std::max(0.0, an.base.h - (fallen ? 1.0 : 0.6) * r), h_hi);
                h = h_hi - (h_hi - h_lo) * (fallen ? u(2) : u(2) * u(3));
                sigma = an.scale;
            } else {
                if (stray) {
                    x = kCanvasWidth * u(0);
                } else {
                    // The negative of the clusters: a gap is picked in proportion
                    // to its width, and the plant lands on a triangular spread
                    // peaked at the gap's middle, thinning to nothing at the trees.
                    double s = kCanvasWidth * u(0);
                    size_t g = 0;
                    while (g + 2 < edges.size() && s >= edges[g + 1]) ++g;
                    x = edges[g] + (edges[g + 1] - edges[g]) * (0.5 + 0.5 * (u(1) - u(2)));
                }
                // Anywhere down the slope, denser toward the rim, as receding
                // ground packs more plants into less of the canvas, and sparse
                // in front so the ground shows: the depth below the rim goes
                // with the fourth power of a uniform.
                rim = mountains::height_at(front.points, x);
                double c = u(3) * u(3);
                h = 0.97 * rim * (1.0 - c * c);
                // No tree to share a size with: the spread scales the plants, so
                // its bits still have an effect.
                sigma = kSpread[v.spread];
            }
            // Perspective: tiny at the rim, growing toward the viewer (a little
            // less on open ground, so the front stays light).
            double t = 1.0 - h / rim;
            double depth = sigma * (0.35 + (near && !stray ? 1.65 : 1.25) * t);
            double jitter = 0.85 + 0.3 * u(4);
            Plant p;
            p.kind = kind;
            p.base = {x, h};
            if (kind == PlantKind::kLeaf) {
                // A rational rotation, so no trigonometry: q in [-1, 1] covers
                // -90 to 90 degrees, all an eye shape needs.
                double q = 2.0 * u(5) - 1.0;
                p.axis = {(1.0 - q * q) / (1.0 + q * q), 2.0 * q / (1.0 + q * q)};
                p.size = 2.2 * depth * jitter;
                // About 30% of fallen leaves lie edge-on to the viewer (face down
                // at a steep angle): they render far skinnier.
                if (fallen && u(6) < 0.3) p.slim = 0.25 + 0.1 * u(7);
            } else if (kind == PlantKind::kPock) {
                p.size = 3.2 * depth * jitter;  // half-width of the rim
            } else if (kind == PlantKind::kTuft) {
                p.size = 6.0 * depth * jitter;
                p.lean = 0.25 + 0.3 * u(5);
            } else {
                p.size = 5.0 * depth * jitter;
                p.lean = 0.9 + 0.4 * u(5);
            }
            out.push_back(p);
        }
    }
    return out;
}

std::vector<Plant> build(const Values& v, const std::vector<Tree>& trees, const std::vector<Hill>& hills, Theme theme,
                         Season season) {
    std::vector<Plant> out = scatter(v, anchors(trees, hills), hills[0], theme, season);
    const std::vector<uint32_t>& colors = vegetation_palette(theme, season, static_cast<int>(v.palette));
    for (size_t i = 0; i < out.size(); ++i) out[i].rgb = colors[i % colors.size()];
    // Pocks take their color from the hill (a gradient), so the palette bits
    // shape them instead: rim width and depth, two bits each.
    for (Plant& p : out) {
        if (p.kind != PlantKind::kPock) continue;
        p.wide = 1.0 + 0.1 * static_cast<double>(v.palette >> 2);
        p.deep = 0.32 + 0.06 * static_cast<double>(v.palette & 3u);
    }
    // Back to front, so nearer plants overlap farther ones.
    std::stable_sort(out.begin(), out.end(), [](const Plant& a, const Plant& b) { return a.base.h > b.base.h; });
    return out;
}

std::vector<Plant> generate(const Hash256& hash, const std::vector<Tree>& trees, const std::vector<Hill>& hills,
                            Theme theme, Season season) {
    return build(decode(hash), trees, hills, theme, season);
}

std::string plant_svg(const Plant& p, double scale) {
    if (p.kind == PlantKind::kPock) {
        // A small cup pressed into the snow, seen from the side: a flat rim,
        // sides tapering in, and a rounded bottom. Filled with the shared
        // pock gradient (darker rim, lighter bottom), placed by translation.
        double a = p.size * p.wide * scale, d = p.size * p.deep * scale;
        std::string out = "<path fill=\"url(#pock)\" d=\"M";
        append_fixed2(out, -a);
        out += ",0L";
        append_fixed2(out, a);
        out += ",0L";
        append_fixed2(out, 0.55 * a);
        out += ",";
        append_fixed2(out, d);
        out += "Q0,";
        append_fixed2(out, 1.3 * d);
        out += " ";
        append_fixed2(out, -0.55 * a);
        out += ",";
        append_fixed2(out, d);
        out += "Z\" transform=\"translate(";
        append_fixed2(out, p.base.x * scale);
        out += " ";
        append_fixed2(out, (kCanvasHeight - p.base.h) * scale);
        out += ")\"/>";
        return out;
    }
    std::string out = "<path fill=\"";
    append_hex(out, p.rgb);
    out += "\" d=\"";
    if (p.kind != PlantKind::kLeaf) {
        // Five blades fanning out from one root, the middle tallest: thin for
        // grass (\|||/), broad and lower for a desert rosette.
        static const double kGrass[5] = {0.6, 0.85, 1.0, 0.85, 0.6};
        static const double kRosette[5] = {0.5, 0.8, 1.0, 0.8, 0.5};
        bool grass = p.kind == PlantKind::kTuft;
        const double* height = grass ? kGrass : kRosette;
        double gap = (grass ? 0.12 : 0.1) * p.size, half = (grass ? 0.07 : 0.16) * p.size;
        for (int k = 0; k < 5; ++k) {
            double root = p.base.x + (k - 2) * gap;
            double tip = p.base.x + (k - 2) * 0.5 * p.lean * p.size;
            out += k == 0 ? "M" : " M";
            append_point(out, root - half, p.base.h, scale);
            out += " L";
            append_point(out, tip, p.base.h + height[k] * p.size, scale);
            out += " L";
            append_point(out, root + half, p.base.h, scale);
            out += " Z";
        }
        out += "\"/>";
        return out;
    }
    // An eye shape, as for tree leaves (subspec/trees.md, Leaves): darker
    // ones are narrower.
    uint32_t c = p.rgb;
    double bright = (299.0 * (c >> 16) + 587.0 * ((c >> 8) & 255u) + 114.0 * (c & 255u)) / 255000.0;
    double a = p.size * scale, w = 2.0 * 0.7 * p.size * (0.35 + 0.8 * bright) * p.slim * scale;
    out += "M";
    append_fixed2(out, -a);
    out += ",0Q0,";
    append_fixed2(out, w);
    out += " ";
    append_fixed2(out, a);
    out += ",0Q0,";
    append_fixed2(out, -w);
    out += " ";
    append_fixed2(out, -a);
    out += ",0Z\" transform=\"matrix(";
    const double m[6] = {p.axis.x, -p.axis.h, p.axis.h, p.axis.x, p.base.x * scale, (kCanvasHeight - p.base.h) * scale};
    for (int q = 0; q < 6; ++q) {
        if (q > 0) out += " ";
        append_fixed2(out, m[q]);
    }
    out += ")\"/>";
    return out;
}

}  // namespace bh::vegetation
