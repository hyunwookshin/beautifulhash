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
