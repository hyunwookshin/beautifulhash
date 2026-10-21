#include "sky.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "format.hpp"
#include "palette.hpp"
#include "partition.hpp"

namespace bh::sky {

namespace {

void append_hex(std::string& out, uint32_t rgb) {
    static const char* digits = "0123456789abcdef";
    out.push_back('#');
    for (int shift = 20; shift >= 0; shift -= 4) out.push_back(digits[(rgb >> shift) & 15]);
}

void append_opacity(std::string& out, int hundredths) {
    out += "0.";
    out.push_back(static_cast<char>('0' + hundredths / 10));
    out.push_back(static_cast<char>('0' + hundredths % 10));
}

void append_point(std::string& out, Point p, double scale) {
    append_fixed2(out, p.x * scale);
    out += ",";
    append_fixed2(out, (kCanvasHeight - p.h) * scale);
}

// A fixed integer mix (the splitmix64 finalizer) to [0, 1).
double unit(uint64_t z) {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);
}

Point lerp(Point a, Point b, double t) { return {a.x + (b.x - a.x) * t, a.h + (b.h - a.h) * t}; }

double manhattan(Point a, Point b) { return std::fabs(b.x - a.x) + std::fabs(b.h - a.h); }

}  // namespace

Values decode(const Hash256& hash) {
    BitReader in(hash, kSky.begin, kSky.width);
    Values v;
    v.top = in.read(kTopBits);
    v.height = in.read(kHeightBits);
    v.width = in.read(kWidthBits);
    v.center = in.read(kCenterBits);
    v.bottom = in.read(kBottomBits);
    v.skew = in.read(kSkewBits);
    v.lead = in.read(kLeadBits);
    v.tint = in.read(kTintBits);
    if (in.remaining() != 0) {
        std::fprintf(stderr, "sky: layout leaves %d bits unread\n", in.remaining());
        std::abort();
    }
    return v;
}

void encode(const Values& v, Hash256& hash) {
    BitWriter out(hash, kSky.begin, kSky.width);
    out.write(kTopBits, v.top);
    out.write(kHeightBits, v.height);
    out.write(kWidthBits, v.width);
    out.write(kCenterBits, v.center);
    out.write(kBottomBits, v.bottom);
    out.write(kSkewBits, v.skew);
    out.write(kLeadBits, v.lead);
    out.write(kTintBits, v.tint);
}

uint32_t cloud_bits(const Values& v) {
    uint32_t b = v.top;
    b = b << kHeightBits | v.height;
    b = b << kWidthBits | v.width;
    b = b << kCenterBits | v.center;
    b = b << kBottomBits | v.bottom;
    b = b << kSkewBits | v.skew;
    b = b << kLeadBits | v.lead;
    return b;
}

Values with_cloud_bits(Values v, uint32_t b) {
    v.lead = b & 1u, b >>= kLeadBits;
    v.skew = b & 3u, b >>= kSkewBits;
    v.bottom = b & 3u, b >>= kBottomBits;
    v.center = b & 7u, b >>= kCenterBits;
    v.width = b & 7u, b >>= kWidthBits;
    v.height = b & 3u, b >>= kHeightBits;
    v.top = b & 7u;
    return v;
}

uint32_t tint(uint32_t index) { return sky_tint(index); }

namespace {

// Steps 1-6 for one cloud, from its trapezoid (top height, height, top width,
// center, bottom width, bottom center), the seed for its points, and its
// leading side.
void shape(Sky& s, double top_h, double height, double top_w, double cx, double bottom_w, double bcx, uint64_t seed,
           uint32_t lead, const Fog& fog) {
    Point corner[4] = {{cx - 0.5 * top_w, top_h},
                       {cx + 0.5 * top_w, top_h},
                       {bcx + 0.5 * bottom_w, top_h - height},
                       {bcx - 0.5 * bottom_w, top_h - height}};  // clockwise from top left
    for (int k = 0; k < 4; ++k) s.corner[k] = corner[k];

    // 2. Five points on its boundary, in order around it, from a fixed mix
    // of the cloud bits: point i lies at perimeter fraction (i + 0.2 + 0.6 u) / 5.
    double len[4], total = 0.0;
    for (int k = 0; k < 4; ++k) total += len[k] = manhattan(corner[k], corner[(k + 1) % 4]);
    for (int i = 0; i < 5; ++i) {
        double f = (i + 0.2 + 0.6 * unit(seed * 8u + static_cast<uint64_t>(i))) / 5.0 * total;
        int e = 0;
        while (e < 3 && f > len[e]) f -= len[e++];
        s.point[i] = lerp(corner[e], corner[(e + 1) % 4], f / len[e]);
        s.edge[i] = e;
    }

    // 3. Refine the outline three times: between each pair of neighbors, four
    // new points inside a narrow rhombus whose long diagonal joins them
    // (half-width 15% of their distance at the middle, tapering to 0 at both
    // ends). Positions come from the cloud bits; a point that would leave the
    // trapezoid flips to the other side of the line, or falls onto it.
    auto inside = [&](Point p) {
        if (p.h > corner[0].h || p.h < corner[3].h) return false;
        double t = (corner[0].h - p.h) / (corner[0].h - corner[3].h);
        return p.x >= corner[0].x + (corner[3].x - corner[0].x) * t && p.x <= corner[1].x + (corner[2].x - corner[1].x) * t;
    };
    std::vector<Point> outline(s.point, s.point + 5);
    for (int order = 1; order <= 3; ++order) {
        std::vector<Point> next;
        for (size_t i = 0; i < outline.size(); ++i) {
            Point a = outline[i], b = outline[(i + 1) % outline.size()];
            next.push_back(a);
            double dx = b.x - a.x, dh = b.h - a.h;
            for (int k = 0; k < 4; ++k) {
                uint64_t key = (seed * 4u + static_cast<uint64_t>(order)) * 1000003u + i * 8u + static_cast<uint64_t>(k);
                double t = (k + 0.25 + 0.5 * unit(key * 2u)) / 4.0;
                double off = 0.15 * (1.0 - std::fabs(2.0 * t - 1.0)) * (2.0 * unit(key * 2u + 1u) - 1.0);
                Point p{a.x + t * dx - off * dh, a.h + t * dh + off * dx};
                if (!inside(p)) p = {a.x + t * dx + off * dh, a.h + t * dh - off * dx};
                if (!inside(p)) p = {a.x + t * dx, a.h + t * dh};
                next.push_back(p);
            }
        }
        outline.swap(next);
    }
    s.outline = outline;

    // 4. The gradient starts from the five boundary points: each is a glow
    // source, pulled a quarter of the way toward the center. Sources on the
    // leading half are twice as strong, so that side leads.
    s.lead = static_cast<int>(lead);
    double mid_x = 0.5 * (corner[0].x + corner[1].x);
    Point center{0.0, 0.0};
    for (const Point& c : corner) center = {center.x + 0.25 * c.x, center.h + 0.25 * c.h};
    for (int k = 0; k < 5; ++k) {
        s.source[k] = lerp(s.point[k], center, 0.25);
        s.strong[k] = s.lead == 0 ? s.point[k].x <= mid_x : s.point[k].x >= mid_x;
    }
    s.glow_r = 0.36 * (corner[1].x - corner[0].x);

    // 6. Fog sets visibility. Clear air shows a wispy cloud (50% peak); the
    // two foggiest levels leave it effectively gone (5% and 2%). Its outline
    // stays in the SVG, so the cloud bits still decode.
    static const int kPeak[4] = {50, 28, 5, 2};
    int density = ((fog.top + fog.bottom) / 2 - 27) / 3;
    s.peak = kPeak[std::max(0, std::min(3, density))];
}

}  // namespace

Sky build(const Values& v, const Fog& fog) {
    Sky s;
    s.tint = v.tint;
    // 1. The trapezoid: horizontal top and bottom edges, the top longer.
    double top_h = kCanvasHeight * (0.88 - 0.035 * v.top);
    double height = 70.0 + 30.0 * v.height;  // flat: much wider than tall
    double top_w = 700.0 + 110.0 * v.width;
    double cx = kCanvasWidth * (0.18 + 0.64 * v.center / 7.0);
    double bottom_w = top_w * (0.35 + 0.1 * v.bottom);
    double bcx = cx + (static_cast<double>(v.skew) - 1.5) / 1.5 * 0.2 * top_w;
    shape(s, top_h, height, top_w, cx, bottom_w, bcx, cloud_bits(v), v.lead, fog);
    return s;
}

Sky generate(const Hash256& hash, const Fog& fog) { return build(decode(hash), fog); }

std::string gradient_svg(const Sky& sky, const Sun& sun) {
    const std::vector<uint32_t>& c = sky_colors(sun.elevation, static_cast<int>(sky.tint), sun.night);
    static const char* kOffset[4] = {"0", "0.45", "0.75", "1"};
    std::string out = "<linearGradient id=\"sky\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">";
    for (int s = 0; s < 4; ++s) {
        out += "<stop offset=\"";
        out += kOffset[s];
        out += "\" stop-color=\"";
        append_hex(out, c[static_cast<size_t>(s)]);
        out += "\"/>";
    }
    out += "</linearGradient>";
    return out;
}

std::string cloud_path(const Sky& sky, double scale);

std::string cloud_defs_svg(const Sky& sky, const Sun& sun, double scale) {
    if (sky.outline.empty()) return "";  // no sky built (some verify scenes)
    // 5. Multi-point fade from the five sources: each is a radial gradient,
    // brightest at its source and fading to nothing at glow_r (sources on the
    // leading half at full strength, the others at half). A low sun lights
    // the cloud pink and gold. The outline clips them.
    std::string color;
    append_hex(color, cloud_color(sun.elevation, sun.night));
    std::string out;
    for (int k = 0; k < 5; ++k) {
        int full = sky.strong[k] ? 70 : 35;  // hundredths of the peak at the source
        out += "<radialGradient id=\"" + sky.id + "-" + std::to_string(k) + "\" gradientUnits=\"userSpaceOnUse\" cx=\"";
        append_fixed2(out, sky.source[k].x * scale);
        out += "\" cy=\"";
        append_fixed2(out, (kCanvasHeight - sky.source[k].h) * scale);
        out += "\" r=\"";
        append_fixed2(out, sky.glow_r * scale);
        out += "\"><stop offset=\"0\" stop-color=\"" + color + "\" stop-opacity=\"";
        append_opacity(out, sky.peak * full / 100);
        out += "\"/><stop offset=\"0.4\" stop-color=\"" + color + "\" stop-opacity=\"";
        append_opacity(out, sky.peak * full / 200);
        out += "\"/><stop offset=\"1\" stop-color=\"" + color + "\" stop-opacity=\"0.00\"/></radialGradient>";
    }
    // A very slight shade toward the cloud's bottom, for a little depth.
    out += "<linearGradient id=\"" + sky.id + "-shade\" gradientUnits=\"userSpaceOnUse\" x1=\"0\" y1=\"";
    append_fixed2(out, (kCanvasHeight - sky.corner[0].h) * scale);
    out += "\" x2=\"0\" y2=\"";
    append_fixed2(out, (kCanvasHeight - sky.corner[3].h) * scale);
    out += "\"><stop offset=\"0.4\" stop-color=\"#3c4a5a\" stop-opacity=\"0.00\"/><stop offset=\"1\" stop-color=\"#3c4a5a\" stop-opacity=\"";
    append_opacity(out, sky.peak * 12 / 100);
    out += "\"/></linearGradient>";
    // A soft blur, so the edges blend into the sky.
    out += "<filter id=\"" + sky.id + "-soft\" x=\"-10%\" y=\"-60%\" width=\"120%\" height=\"220%\"><feGaussianBlur stdDeviation=\"";
    append_fixed2(out, sky.blur * (sky.corner[1].x - sky.corner[0].x) * scale);
    out += "\"/></filter>";
    out += "<clipPath id=\"" + sky.id + "-clip\"><path d=\"" + cloud_path(sky, scale) + "\"/></clipPath>";
    return out;
}

std::string cloud_path(const Sky& sky, double scale) {
    // The refined outline joined smoothly: quadratic curves through the
    // midpoints between neighbors, with each point as a control point.
    const std::vector<Point>& o = sky.outline;
    size_t n = o.size();
    std::string out = "M";
    append_point(out, lerp(o[n - 1], o[0], 0.5), scale);
    for (size_t i = 0; i < n; ++i) {
        out += " Q";
        append_point(out, o[i], scale);
        out += " ";
        append_point(out, lerp(o[i], o[(i + 1) % n], 0.5), scale);
    }
    out += " Z";
    return out;
}

std::string cloud_svg(const Sky& sky, double scale) {
    if (sky.outline.empty()) return "";
    // The five glows (discs of radius glow_r) and the bottom shade, clipped to
    // the outline, then softly blurred.
    std::string out = "<g class=\"" + sky.id + "\" filter=\"url(#" + sky.id + "-soft)\"><g clip-path=\"url(#" + sky.id + "-clip)\">";
    for (int k = 0; k < 5; ++k) {
        out += "<circle cx=\"";
        append_fixed2(out, sky.source[k].x * scale);
        out += "\" cy=\"";
        append_fixed2(out, (kCanvasHeight - sky.source[k].h) * scale);
        out += "\" r=\"";
        append_fixed2(out, sky.glow_r * scale);
        out += "\" fill=\"url(#" + sky.id + "-" + std::to_string(k) + ")\"/>";
    }
    out += "<rect x=\"";
    append_fixed2(out, sky.corner[0].x * scale);
    out += "\" y=\"";
    append_fixed2(out, (kCanvasHeight - sky.corner[0].h) * scale);
    out += "\" width=\"";
    append_fixed2(out, (sky.corner[1].x - sky.corner[0].x) * scale);
    out += "\" height=\"";
    append_fixed2(out, (sky.corner[0].h - sky.corner[3].h) * scale);
    out += "\" fill=\"url(#" + sky.id + "-shade)\"/></g></g>";
    return out;
}

}  // namespace bh::sky

namespace bh::sky {

Second decode_second(const Hash256& hash) {
    BitReader in(hash, kGlobal.begin + 6, kSecondBits);
    Second w;
    w.top = in.read(3);
    w.width = in.read(3);
    w.center = in.read(3);
    w.lead = in.read(1);
    return w;
}

void encode_second(const Second& w, Hash256& hash) {
    BitWriter out(hash, kGlobal.begin + 6, kSecondBits);
    out.write(3, w.top);
    out.write(3, w.width);
    out.write(3, w.center);
    out.write(1, w.lead);
}

uint32_t second_bits(const Second& w) { return ((w.top << 3 | w.width) << 3 | w.center) << 1 | w.lead; }

Second with_second_bits(uint32_t b) {
    Second w;
    w.lead = b & 1u, b >>= 1;
    w.center = b & 7u, b >>= 3;
    w.width = b & 7u, b >>= 3;
    w.top = b & 7u;
    return w;
}

Sky build_second(const Second& w, const Values& v, const Fog& fog) {
    Sky s;
    s.tint = v.tint;
    s.id = "cloud2";
    s.blur = 0.008;  // a little crisper than the first cloud
    // Its own proportions, from its bits mixed with the first cloud's, so the
    // two never look copied: height and bottom steps move on by its width and
    // center, and its skew leans mostly the other way.
    uint32_t height = (v.height + w.width + 1) % 4;
    uint32_t bottom = (v.bottom + 2 + w.center) % 4;
    uint32_t skew = (3 - v.skew + w.top % 2) % 4;
    double top1 = kCanvasHeight * (0.88 - 0.035 * v.top);
    double top_h = kCanvasHeight * (0.88 - 0.035 * w.top);
    // Lower in the sky is farther away: a cloud whose top is below the first
    // cloud's shrinks by the ratio of their heights above the horizon.
    const double horizon = 0.45 * kCanvasHeight;
    double k = std::min(1.0, (top_h - horizon) / (top1 - horizon));
    double top_w = (700.0 + 110.0 * w.width) * k;
    double cx = kCanvasWidth * (0.18 + 0.64 * w.center / 7.0);
    double cx1 = kCanvasWidth * (0.18 + 0.64 * v.center / 7.0);
    // Clouds at a similar height stay apart: one that would sit within a
    // quarter of the canvas of the first moves away by a half-step multiple,
    // between the regular positions, so it never lands on another one.
    if (std::fabs(cx - cx1) < 0.25 * kCanvasWidth && std::fabs(top_h - top1) < 0.1 * kCanvasHeight) {
        const double step = 0.64 * kCanvasWidth / 7.0;
        double dir = cx >= cx1 ? 1.0 : -1.0;
        for (int pass = 0; pass < 2; ++pass, dir = -dir) {
            double moved = cx;
            for (int m = 0; m < 9; ++m) {
                moved = cx + dir * (m + 0.5) * step;
                if (std::fabs(moved - cx1) >= 0.25 * kCanvasWidth) break;
            }
            if (moved >= 0.08 * kCanvasWidth && moved <= 0.92 * kCanvasWidth) {
                cx = moved;
                break;
            }
        }
    }
    double bottom_w = top_w * (0.35 + 0.1 * bottom);
    double bcx = cx + (static_cast<double>(skew) - 1.5) / 1.5 * 0.2 * top_w;
    // Its own seed, from its bits and the first cloud's, outside the first
    // cloud's seed range, so its points and billows are its own.
    uint64_t seed = (1ull << 40) | static_cast<uint64_t>(second_bits(w)) << 16 | cloud_bits(v);
    shape(s, top_h, (70.0 + 30.0 * height) * k, top_w, cx, bottom_w, bcx, seed, w.lead, fog);
    return s;
}

Sky generate_second(const Hash256& hash, const Fog& fog) { return build_second(decode_second(hash), decode(hash), fog); }

bool aurora(Theme theme, const Sun& sun, const Sky& second) {
    return theme == Theme::kTundra && sun.night && !second.outline.empty() && second.corner[3].h >= 0.735 * kCanvasHeight;
}

namespace {

// The five points, lowest first, stretched upward about the lowest (up to four
// times, keeping the top at most 82% of the way up the canvas), so the ribbon
// winds up the sky like a snake and leaves room for the rays above. The
// stretch is affine, fixed by the points, so they still decode.
std::vector<Point> ribbon_points(const Sky& s) {
    std::vector<Point> p(s.point, s.point + 5);
    std::stable_sort(p.begin(), p.end(), [](const Point& a, const Point& b) { return a.h < b.h; });
    double span = p[4].h - p[0].h;
    double k = span > 0.0 ? std::max(1.0, std::min(4.0, (0.82 * kCanvasHeight - p[0].h) / span)) : 1.0;
    for (Point& q : p) q.h = p[0].h + k * (q.h - p[0].h);
    return p;
}

std::string ribbon_path(const std::vector<Point>& p, double scale) {
    std::string out = "M";
    append_point(out, p[0], scale);
    for (size_t i = 0; i + 1 < p.size(); ++i) {
        const Point& a = p[i == 0 ? 0 : i - 1];
        const Point& d = p[std::min(p.size() - 1, i + 2)];
        Point c1{p[i].x + (p[i + 1].x - a.x) / 6.0, p[i].h + (p[i + 1].h - a.h) / 6.0};
        Point c2{p[i + 1].x - (d.x - p[i].x) / 6.0, p[i + 1].h - (d.h - p[i].h) / 6.0};
        out += " C";
        append_point(out, c1, scale);
        out += " ";
        append_point(out, c2, scale);
        out += " ";
        append_point(out, p[i + 1], scale);
    }
    return out;
}

}  // namespace

std::string aurora_defs_svg(const Sky& s, double scale) {
    // The ribbon itself (stroked by each use), and soft blurs.
    std::string out = "<path id=\"aurora-ribbon\" d=\"" + ribbon_path(ribbon_points(s), scale) + "\"/>";
    out += "<filter id=\"aurora-soft\" x=\"-20%\" y=\"-50%\" width=\"140%\" height=\"200%\"><feGaussianBlur stdDeviation=\"";
    append_fixed2(out, 0.0035 * kCanvasWidth * scale);
    out += "\"/></filter><filter id=\"aurora-line\" x=\"-10%\" y=\"-20%\" width=\"120%\" height=\"140%\"><feGaussianBlur stdDeviation=\"";
    append_fixed2(out, 0.002 * kCanvasWidth * scale);
    out += "\"/></filter>";
    return out;
}

std::string aurora_svg(const Sky& s, double scale) {
    std::vector<Point> p = ribbon_points(s);
    double low = p[0].h;
    // The curtain: copies of the ribbon rising from it toward the top of the
    // canvas, a fixed gradient from green to pinkish purple, fading as they
    // rise, blurred only lightly so the color starts at the ribbon and stops
    // there. The ribbon itself is only a faint line, not a neon stroke.
    const int kCopies = 28;
    static const uint32_t kGreen = 0x59F0A6, kPurple = 0xB07CF2;
    double rise = (kCanvasHeight - low) / kCopies;  // the rays reach the top from the lowest point
    std::string out = "<g class=\"aurora\" fill=\"none\" stroke-linecap=\"round\"><g filter=\"url(#aurora-soft)\" stroke-width=\"";
    append_fixed2(out, 0.012 * kCanvasWidth * scale);
    out += "\">";
    for (int k = kCopies; k-- > 0;) {
        double t = static_cast<double>(k) / (kCopies - 1);
        uint32_t c = 0;
        for (int shift = 16; shift >= 0; shift -= 8) {
            double a = (kGreen >> shift) & 255u, b = (kPurple >> shift) & 255u;
            c |= static_cast<uint32_t>(std::lround(a + (b - a) * t)) << shift;
        }
        out += "<use href=\"#aurora-ribbon\" transform=\"translate(0,";
        append_fixed2(out, -k * rise * scale);
        out += ")\" stroke=\"";
        append_hex(out, c);
        out += "\" stroke-opacity=\"";
        append_opacity(out, static_cast<int>(std::lround(40.0 * (1.0 - 0.9 * t))));
        out += "\"/>";
    }
    out += "</g><use href=\"#aurora-ribbon\" filter=\"url(#aurora-line)\" stroke=\"#59f0a6\" stroke-opacity=\"0.22\" stroke-width=\"";
    append_fixed2(out, 0.006 * kCanvasWidth * scale);
    out += "\"/></g>";
    return out;
}

std::string aurora_tint_svg(const Sky& s, int ridges, uint32_t width, uint32_t height) {
    if (ridges <= 0) return "";
    double scale = static_cast<double>(width) / kCanvasWidth;
    double mx = 0.0;
    for (const Point& q : s.point) mx += q.x / 5.0;
    // A faint green cast on the ridges and their snow, strongest under the
    // aurora, clipped to the ridges' own paths.
    std::string out = "<defs><radialGradient id=\"aurora-tint\" gradientUnits=\"userSpaceOnUse\" cx=\"";
    append_fixed2(out, mx * scale);
    out += "\" cy=\"";
    append_fixed2(out, 0.45 * kCanvasHeight * scale);
    out += "\" r=\"";
    append_fixed2(out, 0.7 * kCanvasWidth * scale);
    out += "\"><stop offset=\"0\" stop-color=\"#6ef0b0\" stop-opacity=\"0.10\"/><stop offset=\"1\" stop-color=\"#6ef0b0\" "
           "stop-opacity=\"0.00\"/></radialGradient><clipPath id=\"aurora-tint-clip\">";
    for (int k = 1; k <= ridges; ++k) out += "<use href=\"#ridge-" + std::to_string(k) + "\"/>";
    out += "</clipPath></defs><rect class=\"aurora-tint\" clip-path=\"url(#aurora-tint-clip)\" width=\"" + std::to_string(width) +
           "\" height=\"" + std::to_string(height) + "\" fill=\"url(#aurora-tint)\"/>\n";
    return out;
}

}  // namespace bh::sky
