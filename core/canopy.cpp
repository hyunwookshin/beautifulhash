#include "canopy.hpp"

#include <algorithm>
#include <cmath>

#include "celestial.hpp"
#include "mountains.hpp"

namespace bh::canopy {

namespace {

enum class Shape { kConifer, kNeedle, kGrass };

// Moves each channel a fraction k toward white (k > 0) or black (k < 0).
uint32_t shade(uint32_t rgb, double k) {
    uint32_t out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        double c = static_cast<double>((rgb >> shift) & 255u);
        c = k > 0.0 ? c + (255.0 - c) * k : c * (1.0 + k);
        out |= static_cast<uint32_t>(std::lround(c)) << shift;
    }
    return out;
}

// Mixes a toward b by t, channel by channel.
uint32_t mix(uint32_t a, uint32_t b, double t) {
    uint32_t out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        double ca = static_cast<double>((a >> shift) & 255u), cb = static_cast<double>((b >> shift) & 255u);
        out |= static_cast<uint32_t>(std::lround(ca + (cb - ca) * t)) << shift;
    }
    return out;
}

double clamp_x(double x) { return std::min(std::max(x, 0.0), kCanvasWidth); }

// One of ten colors around `base`: brightness steps, and in fall four warm
// foliage accents; then lightened by the sunlit level (0 to 3).
uint32_t step_color(uint32_t base, int k, bool fall, int light) {
    static const uint32_t kWarm[4] = {0xB8432A, 0xD9802E, 0xD6B03C, 0xC25E2C};
    uint32_t c = shade(base, 0.025 * (k - 4.5));
    if (fall && k % 5 == 1) c = mix(c, kWarm[(k / 5) * 2], 0.38);
    if (fall && k % 5 == 3) c = mix(c, kWarm[(k / 5) * 2 + 1], 0.38);
    return shade(c, 0.05 * light);
}

// The template standing at (x, h), z tall, varied by its key.
std::vector<Point> stamp(Shape shape, double x, double h, double z, long key) {
    if (shape == Shape::kConifer) {
        // A two-tier spire.
        double w = 0.46 * z;
        return {{x - 0.5 * w, h}, {x - 0.17 * w, h + 0.45 * z}, {x - 0.34 * w, h + 0.45 * z}, {x, h + z},
                {x + 0.34 * w, h + 0.45 * z}, {x + 0.17 * w, h + 0.45 * z}, {x + 0.5 * w, h}};
    }
    double a = 0.7 + 0.05 * static_cast<double>(key % 3), b = 0.7 + 0.05 * static_cast<double>((key / 3) % 3);
    if (shape == Shape::kNeedle) {
        // A stem, a needle up the middle, and one up to each side.
        return {{x - 0.07 * z, h}, {x - 0.4 * a * z, h + 0.78 * z}, {x - 0.035 * z, h + 0.46 * z}, {x, h + z},
                {x + 0.035 * z, h + 0.46 * z}, {x + 0.4 * b * z, h + 0.78 * z}, {x + 0.07 * z, h}};
    }
    // A tuft of three blades, all leaning the same way.
    double lean = 0.1 * z * static_cast<double>((key / 9) % 3 - 1);
    return {{x - 0.12 * z, h}, {x - 0.3 * a * z + lean, h + 0.75 * z}, {x - 0.02 * z, h + 0.3 * z},
            {x + lean, h + z}, {x + 0.04 * z, h + 0.3 * z}, {x + 0.3 * b * z + lean, h + 0.7 * z},
            {x + 0.12 * z, h}};
}

// Collects one row's templates by color, in first-seen order.
struct Row {
    std::vector<CanopyPath> paths;
    int prev = -1;  // the left neighbor's step

    // The step for a template at (x, h): its coordinates mod 10, never the
    // same as its left neighbor's.
    int step(double x, double h) {
        int k = static_cast<int>(((static_cast<long>(std::floor(x * h)) % 10) + 10) % 10);
        if (k == prev) k = (k + 3) % 10;
        prev = k;
        return k;
    }

    void add(uint32_t rgb, std::vector<Point> poly) {
        auto it = std::find_if(paths.begin(), paths.end(), [&](const CanopyPath& p) { return p.rgb == rgb; });
        if (it == paths.end()) {
            paths.push_back({rgb, {}});
            it = paths.end() - 1;
        }
        it->polys.push_back(std::move(poly));
    }

    void flush(std::vector<CanopyPath>& out) {
        for (CanopyPath& p : paths) out.push_back(std::move(p));
        paths.clear();
    }
};

// A peak and the lowest points between it and its neighboring peaks.
struct Peak {
    Point top, left, right;
};

}  // namespace

void cover_ridge(Ridge& front, const std::vector<Hill>& hills, const Sun& sun, Theme theme, Season season) {
    front.canopy.clear();
    bool conifer = theme == Theme::kAlpine;
    if (!conifer && theme != Theme::kTemperate) return;
    Shape shape = conifer ? Shape::kConifer : Shape::kNeedle;
    bool fall = !conifer && season == Season::kFall;

    // The treetop line: the ridge's highest point within 32 units, sampled
    // every 4, so rows run under the tree-line texture rather than into it.
    const int kN = 512;
    std::vector<double> raw(kN + 1), top(kN + 1);
    for (int i = 0; i <= kN; ++i) raw[i] = mountains::height_at(front.points, 4.0 * i);
    for (int i = 0; i <= kN; ++i) {
        top[i] = raw[i];
        for (int j = std::max(0, i - 8); j <= std::min(kN, i + 8); ++j) top[i] = std::max(top[i], raw[j]);
    }
    auto top_at = [&](double x) {
        double u = clamp_x(x) / 4.0;
        int i = std::min(kN - 1, static_cast<int>(std::floor(u)));
        return top[i] + (top[i + 1] - top[i]) * (u - i);
    };
    // What the hills hide: below their highest silhouette.
    auto hidden_below = [&](double x) {
        double c = 0.0;
        for (const Hill& h : hills) c = std::max(c, mountains::height_at(h.points, clamp_x(x)));
        return c;
    };
    double peak_h = 0.0, reach = 0.0;
    for (const Point& p : front.points) peak_h = std::max(peak_h, p.h);
    for (int i = 0; i <= kN; ++i) reach = std::max(reach, top[i] - hidden_below(4.0 * i));

    // Peaks on the treetop line sampled every 64 units. Each side's valley is
    // the lowest sample before the line rises above the peak (or reaches the
    // canvas edge). A peak standing at least 40 units above both valleys, whose
    // valleys meet at 140 degrees or less at its top, is sharp; the angle is
    // tested on the cosine (cos 140 = -0.7660444).
    std::vector<Point> coarse;
    for (int i = 0; i <= kN; i += 16) coarse.push_back({4.0 * i, top[i]});
    std::vector<Peak> peaks;
    for (size_t i = 1; i + 1 < coarse.size(); ++i) {
        if (!(coarse[i].h > coarse[i - 1].h && coarse[i].h >= coarse[i + 1].h)) continue;
        Point l = coarse[i - 1], r = coarse[i + 1];
        for (size_t j = i - 1; j-- > 0 && coarse[j].h <= coarse[i].h;) {
            if (coarse[j].h < l.h) l = coarse[j];
        }
        for (size_t j = i + 2; j < coarse.size() && coarse[j].h <= coarse[i].h; ++j) {
            if (coarse[j].h < r.h) r = coarse[j];
        }
        if (coarse[i].h - std::max(l.h, r.h) < 40.0) continue;
        double ax = l.x - coarse[i].x, ah = l.h - coarse[i].h, bx = r.x - coarse[i].x, bh = r.h - coarse[i].h;
        double dot = ax * bx + ah * bh;
        if (dot >= 0.0 || dot * dot <= 0.7660444 * 0.7660444 * (ax * ax + ah * ah) * (bx * bx + bh * bh)) {
            peaks.push_back({coarse[i], l, r});
        }
    }
    double side = sun.dx > 0.0 ? -1.0 : 1.0;  // the sun is on the left when shadows fall right
    // Light level 0 to 3 at a template's base: on a sharp peak's sunlit face,
    // strongest in the middle of the face and fading toward its edges.
    auto lit = [&](double x, double h) {
        int best = 0;
        for (const Peak& p : peaks) {
            const Point& valley = side < 0.0 ? p.left : p.right;
            double ux = valley.x - p.top.x, uh = valley.h - p.top.h;  // down the sunward slope
            double vx = x - p.top.x, vh = h - p.top.h;
            // Under the slope (on the same side of it as straight down) and
            // within 60 degrees of it: cos^2 >= 1/4 with a positive cosine.
            if ((ux * vh - uh * vx) * -ux < 0.0) continue;
            double dot = ux * vx + uh * vh, uu = ux * ux + uh * uh, vv = vx * vx + vh * vh;
            if (dot <= 0.0 || dot * dot < 0.25 * uu * vv) continue;
            double dv = (x - valley.x) * (x - valley.x) + (h - valley.h) * (h - valley.h);
            if (vv > dv) continue;  // past the line equidistant from peak and valley
            // How far toward the face's edges: its sides at 0 and 60 degrees
            // (sin^2 = 0 and 3/4), and the equidistant line.
            double t = (1.0 - dot * dot / (uu * vv)) / 0.75;
            double edge = std::max(std::fabs(2.0 * t - 1.0), vv / dv);
            best = std::max(best, edge < 0.5 ? 3 : edge < 0.8 ? 2 : 1);
        }
        return best;
    };
    // The ridge's own color at height h: its vertical gradient in fall.
    auto base = [&](double h) {
        if (!front.gradient) return front.rgb;
        return mix(front.rgb, front.rgb_bottom, std::min(std::max(1.0 - h / peak_h, 0.0), 1.0));
    };

    // Rows hang below the treetop line, about a third of a template apart,
    // and templates grow a little toward the bottom. Colors are grouped over
    // blocks of four rows; rows overlap only their neighbors, so the order
    // within a block barely shows.
    const double kSize = 18.0;
    double d = 0.25 * kSize;
    Row out;
    for (int row = 0; d < reach + kSize; ++row) {
        double size = kSize * (0.85 + 0.15 * std::min(1.0, d / peak_h));
        double spacing = (conifer ? 0.44 : 0.35) * size;
        out.prev = -1;
        for (double x0 = (row % 2) * 0.5 * spacing; x0 < kCanvasWidth + spacing; x0 += spacing) {
            long key = static_cast<long>(std::floor(x0 * 7.0 + d * 131.0));
            double x = x0 + spacing * 0.125 * static_cast<double>(key % 5 - 2);
            double h = top_at(x) - d + size * 0.12 * static_cast<double>((key / 25) % 3 - 1);
            double z = size * (0.8 + 0.1 * static_cast<double>((key / 5) % 5));
            if (h + z <= hidden_below(x)) continue;  // behind the hills
            int k = out.step(x, h);
            out.add(step_color(base(h), k, fall, lit(x, h)), stamp(shape, x, h, z, key));
        }
        if (row % 4 == 3) out.flush(front.canopy);
        d += 0.35 * size;
    }
    out.flush(front.canopy);
}

namespace {

// Facets over one ridge, above what the silhouettes in `covers` (the hills and
// nearer ridges) hide. On a snow cap they also go over the snow, in its color.
// `front` marks the front ridge; farther ridges are faceted only in patches.
void facet(Ridge& ridge, const std::vector<const std::vector<Point>*>& covers, const Sun& sun, int index, bool front) {
    static const char kPi[] =
        "3141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067";
    auto digit = [](long k) { return static_cast<double>(kPi[((k % 100) + 100) % 100] - '0'); };
    auto ridge_at = [&](double x) { return mountains::height_at(ridge.points, clamp_x(x)); };
    auto hidden_below = [&](double x) {
        double c = 0.0;
        for (const std::vector<Point>* s : covers) c = std::max(c, mountains::height_at(*s, clamp_x(x)));
        return c;
    };
    const double kW = 48.0, kRow = 90.0, kLean = 0.6;
    double top = 0.0, floor_h = kCanvasHeight;
    for (int i = 0; i <= 512; ++i) {
        top = std::max(top, ridge_at(4.0 * i));
        floor_h = std::min(floor_h, hidden_below(4.0 * i));
    }
    if (top <= floor_h) return;

    // Columns run straight down from the silhouette, parallel to the nearby
    // peaks' incoming slopes (each weighted by 1 / (1 + (distance / 300)^2),
    // with straight down weighted 0.3), so the facets line up with the cuts
    // that split the faces. The slant (across per unit of drop) is held to
    // within 1, and neighboring columns differ by at most half a column width
    // over the ridge's full depth, so columns never cross.
    int cols = static_cast<int>(kCanvasWidth / kW) + 3;
    std::vector<double> slant(static_cast<size_t>(cols));
    for (int i = 0; i < cols; ++i) {
        double x = (i - 1) * kW, dx = 0.0, dh = -0.3;
        for (size_t k = 0; k < ridge.peak_tops.size(); ++k) {
            double u = (x - ridge.peak_tops[k].x) / 300.0, w = 1.0 / (1.0 + u * u);
            dx += w * ridge.peak_slopes[k].x;
            dh += w * ridge.peak_slopes[k].h;
        }
        slant[static_cast<size_t>(i)] = std::min(1.0, std::max(-1.0, dx / -dh));
    }
    double step = 0.5 * kW / (top - floor_h);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 1; i < cols; ++i) {
            size_t a = static_cast<size_t>(pass == 0 ? i - 1 : cols - i), b = static_cast<size_t>(pass == 0 ? i : cols - 1 - i);
            slant[b] = std::min(slant[a] + step, std::max(slant[a] - step, slant[b]));
        }
    }

    // Rows follow the silhouette exactly near the top and close in on the
    // base: row t (0 at the silhouette, 1 at the base) of column i drops
    // from the column's top point by t times its height above the base. Rows
    // are about 90 units apart under the highest point, so facets are tall.
    // Points move by the digits of pi, under a fifth of a cell, so the
    // structure stays precise. Each point gets a depth z out of the screen:
    // the face leans back from the silhouette, with a little jitter.
    int rows = std::max(2, static_cast<int>(std::ceil((top - floor_h) / kRow)));
    struct Vertex {
        double x, h, z;
    };
    auto vertex = [&](int i, int j) {
        double x0 = (i - 1) * kW;
        long k = static_cast<long>(i) * 37 + static_cast<long>(j) * 11 + static_cast<long>(std::floor(ridge_at(x0)));
        double xt = x0 + 0.4 * kW * (digit(k) - 4.5) / 9.0;
        double st = ridge_at(xt);
        double t = (j + 0.4 * (digit(k + 3) - 4.5) / 9.0) / rows;
        double drop = t * (st - floor_h);
        double z = kLean * drop + 0.25 * kRow * (digit(k + 7) - 4.5) / 9.0;
        return Vertex{xt + drop * slant[static_cast<size_t>(i)], st - drop, z};
    };

    // The light: from the sun's side and above, (-dx * l, 1, 0) with l the
    // shadow length per unit height. A facet's brightness is the cosine
    // between its normal (turned toward the viewer) and the light, compared
    // squared with its sign, so no square root is taken.
    double lx = -sun.dx * sun.length, lh = 1.0, ll = lx * lx + lh * lh;
    // At night more facets are drawn, so the moonlit faces break into shards.
    const double kBright = sun.night ? 0.44 : 0.52, kLight = sun.night ? 0.30 : 0.40;
    const double kShaded = sun.night ? 0.09 : 0.05, kDarkest = sun.night ? 0.025 : 0.012;
    // At night the steps grow: the facets read like a sculpture lit from one
    // side. The moon is dim, so the dark steps grow most, and the contrast
    // comes from shadow rather than glare.
    static const double kDay[4] = {0.13, 0.06, -0.06, -0.13}, kNightShade[4] = {0.24, 0.12, -0.18, -0.36};
    const double* kShade = sun.night ? kNightShade : kDay;
    // Groups by color, in first-seen order.
    std::vector<CanopyPath> rock, snow;
    auto add = [&](std::vector<CanopyPath>& to, uint32_t rgb, std::vector<Point> poly) {
        auto it = std::find_if(to.begin(), to.end(), [&](const CanopyPath& p) { return p.rgb == rgb; });
        if (it == to.end()) {
            to.push_back({rgb, {}});
            to.back().facet = true;
            it = to.end() - 1;
        }
        it->polys.push_back(std::move(poly));
    };
    for (int i = 0; i + 1 < cols; ++i) {
        for (int j = 0; j < rows; ++j) {
            Vertex a = vertex(i, j), b = vertex(i + 1, j), c = vertex(i + 1, j + 1), d = vertex(i, j + 1);
            long k = static_cast<long>(i) * 53 + static_cast<long>(j) * 29;
            Vertex tri[2][3] = {{a, b, c}, {a, c, d}};
            if (static_cast<int>(digit(k)) % 2 == 1) {
                tri[0][0] = a; tri[0][1] = b; tri[0][2] = d;
                tri[1][0] = b; tri[1][1] = c; tri[1][2] = d;
            }
            for (auto& t : tri) {
                double cx = (t[0].x + t[1].x + t[2].x) / 3.0, ch = (t[0].h + t[1].h + t[2].h) / 3.0;
                // Toward the ridge in front, facets brush off: full strength
                // from 120 units above what hides the ridge, fading in steps
                // to nothing.
                double clear = (ch - hidden_below(cx)) / 120.0;
                if (clear < 0.25) continue;
                double strength = (clear < 0.5 ? 0.4 : clear < 0.75 ? 0.7 : 1.0) * ridge.face_strength;
                // Farther ridges are faceted in patches: height zones a fifth
                // of the ridge's span, by chunks 320 units wide, and the
                // digits of pi drop two in five of them.
                if (!front) {
                    int zone = std::min(4, std::max(0, static_cast<int>(5.0 * (ch - floor_h) / (top - floor_h))));
                    long chunk = static_cast<long>(std::floor(cx / 320.0));
                    if (digit(zone * 13 + chunk * 7 + index * 29) < 4.0) continue;
                }
                double ux = t[1].x - t[0].x, uh = t[1].h - t[0].h, uz = t[1].z - t[0].z;
                double vx = t[2].x - t[0].x, vh = t[2].h - t[0].h, vz = t[2].z - t[0].z;
                double nx = uh * vz - uz * vh, nh = uz * vx - ux * vz, nz = ux * vh - uh * vx;
                if (nz < 0.0) {
                    nx = -nx;
                    nh = -nh;
                    nz = -nz;
                }
                double dot = nx * lx + nh * lh, nn = nx * nx + nh * nh + nz * nz;
                if (nn == 0.0) continue;
                double s = (dot < 0.0 ? -1.0 : 1.0) * dot * dot / (nn * ll);
                int lv = s > kBright ? 0 : s > kLight ? 1 : s < kDarkest ? 3 : s < kShaded ? 2 : -1;
                if (lv < 0) continue;  // the middle tones: the ridge's own fill shows
                std::vector<Point> poly = {{t[0].x, t[0].h}, {t[1].x, t[1].h}, {t[2].x, t[2].h}};
                if (!ridge.snow.empty()) add(snow, shade(ridge.snow_rgb, kShade[lv] * strength), poly);
                add(rock, shade(ridge.rgb, kShade[lv] * strength), std::move(poly));
            }
        }
    }
    for (CanopyPath& p : rock) ridge.canopy.push_back(std::move(p));
    for (CanopyPath& p : snow) ridge.snow_facets.push_back(std::move(p));
}

}  // namespace

void facet_ridges(std::vector<Ridge>& ridges, const std::vector<Hill>& hills, const Sun& sun, Theme theme, Season season) {
    // Tundra: every ridge. Desert: the front two. Alpine, and Temperate in fall
    // and winter: the back ridge, whose rock and snow are bare (the front ridge
    // is forested).
    bool bare = theme == Theme::kAlpine ||
                (theme == Theme::kTemperate && (season == Season::kFall || season == Season::kWinter));
    size_t first = 0, last = 0;
    if (theme == Theme::kTundra) {
        last = ridges.size();
    } else if (theme == Theme::kDesert) {
        last = std::min<size_t>(2, ridges.size());
    } else if (bare && ridges.size() >= 2) {
        first = ridges.size() - 1;
        last = ridges.size();
    }
    std::vector<const std::vector<Point>*> covers;
    for (const Hill& h : hills) covers.push_back(&h.points);
    for (size_t k = 0; k < last; ++k) {
        if (k >= first) facet(ridges[k], covers, sun, static_cast<int>(k), k == 0);
        covers.push_back(&ridges[k].points);
    }
}

void cover_hill(Hill& hill, const Sun& sun, Theme theme, Season season) {
    hill.canopy.clear();
    if (hill.bands.empty()) return;
    // Winter hills are clean, flat snow: only a sprinkle of tufts, one in ten,
    // in every biome. Otherwise Desert sand stays bare.
    bool winter = season == Season::kWinter;
    if (theme == Theme::kDesert && !winter) return;
    bool fall = theme == Theme::kTemperate && season == Season::kFall;
    const std::vector<Point>& s = hill.points;
    double mean = 0.0, low = s.front().h;
    for (const Point& q : s) {
        mean += q.h;
        low = std::min(low, q.h);
    }
    mean /= static_cast<double>(s.size());

    // Rows along the ground surface from the crest down, each half a tuft
    // lower, so rows overlap; tufts grow mildly toward the viewer.
    const double kSize = 12.0;
    double f = 0.0;
    for (int row = 0; f < 1.0; ++row) {
        double size = kSize * (0.75 + 0.25 * f);
        double spacing = 0.5 * size;
        Row out;
        for (double x0 = (row % 2) * 0.5 * spacing; x0 < kCanvasWidth + spacing; x0 += spacing) {
            long key = static_cast<long>(std::floor(x0 * 7.0 + f * 9973.0));
            if (winter && (key / 7) % 10 != 0) continue;
            double x = x0 + spacing * 0.125 * static_cast<double>(key % 5 - 2);
            double h = celestial::surface(mountains::height_at(s, clamp_x(x)), low, f) *
                       (1.0 + sun.tilt * f * (2.0 * x / kCanvasWidth - 1.0));
            h += size * 0.12 * static_cast<double>((key / 25) % 3 - 1);  // uneven rows
            double z = size * (0.8 + 0.1 * static_cast<double>((key / 5) % 5));
            // The band the base stands in: the deepest whose boundary is at or above it.
            size_t band = 0;
            for (size_t j = 0; j < hill.bands.size(); ++j) {
                if (mountains::height_at(hill.bands[j], clamp_x(x)) >= h) band = j;
            }
            int k = out.step(x, h);
            out.add(step_color(hill.band_rgb[band], k, fall, 0), stamp(Shape::kGrass, x, h, z, key));
        }
        out.flush(hill.canopy);
        double slope = 2.0 * (1.0 - f) * mean - (1.0 - 2.0 * f) * low;
        f += 0.5 * size / std::max(slope, 0.25 * mean);
    }
}

}  // namespace bh::canopy
