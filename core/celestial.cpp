#include "celestial.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "mountains.hpp"
#include "partition.hpp"

namespace bh::celestial {

namespace {

// Azimuth 15-165 degrees in steps of 10, from the left, behind the scene, as
// tangents of the half angle: an exact rational direction, no trigonometry.
const double kHalfTan[16] = {0.1317, 0.2217, 0.3153, 0.4142, 0.5206, 0.6371, 0.7673, 0.9163,
                             1.0913, 1.3032, 1.5697, 1.9210, 2.4142, 3.1716, 4.5107, 7.5958};
// Elevation 20-65 degrees in steps of 3, as shadow length per unit of height
// (the cotangent).
const double kShadow[16] = {2.7475, 2.3559, 2.0503, 1.8040, 1.6003, 1.4281, 1.2799, 1.1504,
                            1.0355, 0.9325, 0.8391, 0.7536, 0.6745, 0.6009, 0.5317, 0.4663};

// How deep the front hill's ground runs, in logical units, from its
// silhouette (f = 0) to the canvas bottom (f = 1).
constexpr double kGroundDepth = 1200.0;

}  // namespace

Values decode(const Hash256& hash) {
    BitReader in(hash, kCelestial.begin, kCelestial.width);
    Values v;
    v.azimuth = in.read(kAzimuthBits);
    v.elevation = in.read(kElevationBits);
    if (in.remaining() != 0) {
        std::fprintf(stderr, "celestial: layout leaves %d bits unread\n", in.remaining());
        std::abort();
    }
    return v;
}

void encode(const Values& values, Hash256& hash) {
    BitWriter out(hash, kCelestial.begin, kCelestial.width);
    out.write(kAzimuthBits, values.azimuth);
    out.write(kElevationBits, values.elevation);
}

Sun sun(const Values& v) {
    double q = kHalfTan[v.azimuth];
    Sun s;
    // Shadows fall away from the sun: rightward when it is on the left, and
    // always toward the viewer, since the sun is behind the scene.
    s.dx = (1.0 - q * q) / (1.0 + q * q);
    s.dy = 2.0 * q / (1.0 + q * q);
    s.length = kShadow[v.elevation];
    s.tilt = 0.15 * s.dx;
    s.spacing = 0.25 - static_cast<double>(v.elevation) / 30.0;
    s.elevation = static_cast<int>(v.elevation);
    return s;
}

Sun generate(const Hash256& hash) { return sun(decode(hash)); }

double surface(double s, double low, double f) {
    if (f > 1.0) return (1.0 - f) * low;  // past the canvas bottom: keeps sinking
    return (1.0 - f) * ((1.0 - f) * s + f * low);
}

void cast_shadows(std::vector<Tree>& trees, const Hill& front, const Sun& sun) {
    double low = front.points.front().h;
    for (const Point& q : front.points) low = std::min(low, q.h);
    for (Tree& t : trees) {
        t.shadow.clear();
        if (t.hill != 0) continue;
        // The base's depth on the surface. The surface falls steadily with f
        // on [0, 1], so bisection finds it with only + - * /.
        double s = mountains::height_at(front.points, t.base.x);
        double a = 0.0, b = 1.0;
        for (int k = 0; k < 50; ++k) {
            double m = 0.5 * (a + b);
            if (surface(s, low, m) > t.base.h) a = m;
            else b = m;
        }
        double fb = 0.5 * (a + b);
        // A point at height z above the base lands z * length along the
        // shadow direction: sideways on the canvas, and deeper on the ground,
        // where the surface flattens toward the bottom like the bands.
        auto cast = [&](Point p) {
            double z = p.h - t.base.h;
            double x = p.x + z * sun.length * sun.dx;
            double f = fb + z * sun.length * sun.dy / kGroundDepth;
            return Point{x, surface(mountains::height_at(front.points, x), low, f)};
        };
        for (const Limb& l : t.limbs) {
            Point pa = cast(l.a), pb = cast(l.b);
            double dx = pb.x - pa.x, dh = pb.h - pa.h;
            double m = std::max(std::fabs(dx), std::fabs(dh));
            if (m == 0.0) continue;
            // Perpendicular scaled by the larger component (no square root);
            // shadows are a little thicker than their branches.
            double nx = -dh / m, nh = dx / m;
            double wa = 0.65 * l.wa, wb = 0.65 * l.wb;
            t.shadow.push_back({{pa.x + nx * wa, pa.h + nh * wa},
                                {pb.x + nx * wb, pb.h + nh * wb},
                                {pb.x - nx * wb, pb.h - nh * wb},
                                {pa.x - nx * wa, pa.h - nh * wa}});
        }
    }
}

}  // namespace bh::celestial
