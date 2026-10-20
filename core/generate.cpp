#include "generate.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "celestial.hpp"
#include "fog.hpp"
#include "format.hpp"
#include "palette.hpp"
#include "hills.hpp"
#include "mountains.hpp"
#include "trees.hpp"
#include "vegetation.hpp"

namespace bh {

namespace {


void append_hex(std::string& out, uint32_t rgb) {
    static const char* digits = "0123456789abcdef";
    out.push_back('#');
    for (int shift = 20; shift >= 0; shift -= 4) out.push_back(digits[(rgb >> shift) & 15]);
}

// Each channel moved a fraction k of the way from a to b.
uint32_t mix(uint32_t a, uint32_t b, double k) {
    uint32_t out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        double ca = static_cast<double>((a >> shift) & 255u), cb = static_cast<double>((b >> shift) & 255u);
        out |= static_cast<uint32_t>(std::lround(ca + (cb - ca) * k)) << shift;
    }
    return out;
}

void append_uint(std::string& out, uint32_t v) {
    char digits[12];
    int len = 0;
    do {
        digits[len++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v > 0);
    while (len > 0) out.push_back(digits[--len]);
}

void append_full_rect(std::string& out, uint32_t width, uint32_t height) {
    out += "width=\"";
    append_uint(out, width);
    out += "\" height=\"";
    append_uint(out, height);
    out += "\"";
}

// An opacity in hundredths, as "0.NN".
void append_opacity(std::string& out, int hundredths) {
    out += "0.";
    out.push_back(static_cast<char>('0' + hundredths / 10));
    out.push_back(static_cast<char>('0' + hundredths % 10));
}

// One haze layer over the whole canvas (subspec/fog.md, Rendering).
void append_fog(std::string& out, uint32_t width, uint32_t height) {
    out += "<rect class=\"fog\" ";
    append_full_rect(out, width, height);
    out += " fill=\"url(#fog)\"/>\n";
}

}  // namespace

Scene generate_scene(const Hash256& hash, int version) {
    if (version != 0) {
        std::fprintf(stderr, "unsupported algorithm version %d\n", version);
        std::abort();
    }
    Scene scene;
    scene.version = version;
    const mountains::Config& config = mountains::default_config();
    scene.ridges = mountains::generate(config, hash);
    // Hills are generated after Mountains and before Trees and Vegetation,
    // which will plant on them using the advertised hill profiles.
    scene.hills = hills::generate(hash, config.params.theme, config.params.season);
    scene.trees = trees::generate(hash, scene.hills, config.params.theme, config.params.season);
    trees::add_crest_trees(scene.hills, hash, config.params.theme, config.params.season);
    // The implicit sun lights the front hill and casts the trees' shadows.
    scene.sun = celestial::generate(hash);
    hills::add_bands(scene.hills[0], scene.sun);
    celestial::cast_shadows(scene.trees, scene.hills[0], scene.sun);
    trees::tint_cover(scene.trees, scene.hills[0]);
    scene.vegetation = vegetation::generate(hash, scene.trees, scene.hills, config.params.theme, config.params.season);
    // Fog comes last: it hazes every depth gap the other layers create.
    scene.fog = fog::generate(hash, config.params.theme, config.params.season);
    return scene;
}

std::string render_svg(const Scene& scene, uint32_t width, uint32_t height) {
    double scale = static_cast<double>(width) / kCanvasWidth;
    std::string out;
    out.reserve(4096);

    out += "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
    append_full_rect(out, width, height);
    out += " viewBox=\"0 0 ";
    append_uint(out, width);
    out += " ";
    append_uint(out, height);
    out += "\">\n";
    // Placeholder sky until its namespace exists (no bits): a vertical gradient
    // whose hue the low sun warms toward sunset colors (subspec/celestial.md,
    // Sky warmth).
    const std::vector<uint32_t>& sky = sky_colors(scene.sun.elevation);
    static const char* kSkyOffset[4] = {"0", "0.45", "0.75", "1"};
    out += "<defs><linearGradient id=\"sky\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">";
    for (int s = 0; s < 4; ++s) {
        out += "<stop offset=\"";
        out += kSkyOffset[s];
        out += "\" stop-color=\"";
        append_hex(out, sky[static_cast<size_t>(s)]);
        out += "\"/>";
    }
    out += "</linearGradient>";
    int glow = scene.sun.elevation <= 6 ? static_cast<int>(std::lround(70.0 * (7 - scene.sun.elevation) / 7.0)) : 0;
    if (glow > 0) {
        // Warmest toward the sun: a radial glow low in the sky on its side.
        // The sun sits left of center when shadows fall right (dx > 0).
        out += "<radialGradient id=\"sun-glow\" gradientUnits=\"userSpaceOnUse\" cx=\"";
        append_fixed2(out, 0.5 * (1.0 - scene.sun.dx) * width);
        out += "\" cy=\"";
        append_fixed2(out, 0.8 * height);
        out += "\" r=\"";
        append_fixed2(out, 0.75 * width);
        out += "\"><stop offset=\"0\" stop-color=\"";
        append_hex(out, sky[4]);
        out += "\" stop-opacity=\"";
        append_opacity(out, glow);
        out += "\"/><stop offset=\"1\" stop-color=\"";
        append_hex(out, sky[4]);
        out += "\" stop-opacity=\"0.00\"/></radialGradient>";
    }
    // The haze shared by every fog layer: tint and opacity from the Fog
    // namespace, denser toward the ground when misty.
    out += "<linearGradient id=\"fog\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0\" stop-color=\"";
    append_hex(out, scene.fog.rgb);
    out += "\" stop-opacity=\"";
    append_opacity(out, scene.fog.top);
    out += "\"/><stop offset=\"1\" stop-color=\"";
    append_hex(out, scene.fog.rgb);
    out += "\" stop-opacity=\"";
    append_opacity(out, scene.fog.bottom);
    out += "\"/></linearGradient>";
    // Winter snow pocks share one gradient: the front hill's darkest band at the
    // rim to its second darkest at the bottom (subspec/vegetation.md).
    bool pocks = false;
    for (const Plant& p : scene.vegetation) pocks = pocks || p.kind == PlantKind::kPock;
    if (pocks && !scene.hills.empty()) {
        const Hill& front = scene.hills[0];
        size_t nb = front.band_rgb.size();
        uint32_t rim = nb > 0 ? front.band_rgb[nb - 1] : front.rgb, floor = nb > 1 ? front.band_rgb[nb - 2] : front.rgb;
        out += "<linearGradient id=\"pock\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0\" stop-color=\"";
        append_hex(out, rim);
        out += "\"/><stop offset=\"1\" stop-color=\"";
        append_hex(out, floor);
        out += "\"/></linearGradient>";
    }
    out += "</defs>\n";
    out += "<rect class=\"sky\" ";
    append_full_rect(out, width, height);
    out += " fill=\"url(#sky)\"/>\n";
    if (glow > 0) {
        out += "<rect class=\"sun-glow\" ";
        append_full_rect(out, width, height);
        out += " fill=\"url(#sun-glow)\"/>\n";
    }

    // Back to front. Fog is a separate element between ridges, so it never
    // rewrites a ridge's fill; farther ridges accumulate more haze.
    for (size_t n = scene.ridges.size(); n-- > 0;) {
        const Ridge& r = scene.ridges[n];
        if (r.gradient) {
            // Vertical gradient over the ridge's bounding box: rgb at its top
            // (the peak) to rgb_bottom at the canvas bottom.
            out += "<linearGradient id=\"ridge-";
            append_uint(out, static_cast<uint32_t>(n + 1));
            out += "-fill\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0\" stop-color=\"";
            append_hex(out, r.rgb);
            out += "\"/><stop offset=\"1\" stop-color=\"";
            append_hex(out, r.rgb_bottom);
            out += "\"/></linearGradient>\n";
        }
        out += "<path class=\"ridge\" id=\"ridge-";
        append_uint(out, static_cast<uint32_t>(n + 1));
        out += "\" fill=\"";
        if (r.gradient) {
            out += "url(#ridge-";
            append_uint(out, static_cast<uint32_t>(n + 1));
            out += "-fill)";
        } else {
            append_hex(out, r.rgb);
        }
        out += "\" d=\"";
        for (size_t i = 0; i < r.points.size(); ++i) {
            out += i == 0 ? "M" : " L";
            append_fixed2(out, r.points[i].x * scale);
            out += ",";
            append_fixed2(out, (kCanvasHeight - r.points[i].h) * scale);
        }
        out += " L";
        append_uint(out, width);
        out += ",";
        append_uint(out, height);
        out += " L0,";
        append_uint(out, height);
        out += " Z\"/>\n";

        // Snow caps: a separate element over the ridge, so the ridge path itself
        // is unchanged. Drawn before the haze in front of this ridge.
        std::string snow_d;
        if (!r.snow.empty()) {
            for (const auto& poly : r.snow) {
                for (size_t i = 0; i < poly.size(); ++i) {
                    snow_d += i == 0 ? (&poly == &r.snow.front() ? "M" : " M") : " L";
                    append_fixed2(snow_d, poly[i].x * scale);
                    snow_d += ",";
                    append_fixed2(snow_d, (kCanvasHeight - poly[i].h) * scale);
                }
                snow_d += " Z";
            }
            out += "<path class=\"snow\" fill=\"";
            append_hex(out, r.snow_rgb);
            out += "\" d=\"" + snow_d + "\"/>\n";

        }

        // Faces (subspec/mountains.md, Ridge faces): the half of each peak
        // turned away from the sun is overlaid with the mountain's color
        // darkened, at an opacity that grows as the sun drops. It holds full
        // strength down to the snow's lowest point (or from the top of a bare
        // ridge) and fades out by the ridge's lowest point, so the snow cap
        // reads crisp and the rock below only hints at it. At sunset the
        // sunlit halves glow orange-pink the same way. On snow, realistic
        // reflective triangles on the sunlit side catch the light as glints.
        if (!r.faces.empty()) {
            std::string num = std::to_string(n + 1);
            int shaded = scene.sun.dx >= 0.0 ? 1 : -1;  // sun on the left shades right halves
            bool sunset = scene.sun.elevation <= 2;
            auto layer = [&](const std::vector<std::vector<Point>>& polys, const std::vector<int>& sides, int side,
                             const std::string& fill, const std::string& extra) {
                std::string d;
                for (size_t f = 0; f < polys.size(); ++f) {
                    if (sides[f] != side) continue;
                    for (size_t i = 0; i < polys[f].size(); ++i) {
                        d += i == 0 ? (d.empty() ? "M" : " M") : " L";
                        append_fixed2(d, polys[f][i].x * scale);
                        d += ",";
                        append_fixed2(d, (kCanvasHeight - polys[f][i].h) * scale);
                    }
                    d += " Z";
                }
                if (d.empty()) return;
                out += "<path class=\"ridge-face\" fill=\"" + fill + "\"" + extra + " d=\"" + d + "\"/>\n";
            };
            auto fade = [&](const std::string& id, uint32_t rgb, int hundredths) {
                out += "<linearGradient id=\"" + id + "\" gradientUnits=\"userSpaceOnUse\" x1=\"0\" y1=\"";
                append_fixed2(out, (kCanvasHeight - r.fade_top) * scale);
                out += "\" x2=\"0\" y2=\"";
                append_fixed2(out, (kCanvasHeight - r.fade_bottom) * scale);
                for (int s = 0; s < 2; ++s) {
                    out += s == 0 ? "\"><stop offset=\"0\" stop-color=\"" : "\"/><stop offset=\"1\" stop-color=\"";
                    append_hex(out, rgb);
                    out += "\" stop-opacity=\"";
                    append_opacity(out, s == 0 ? hundredths : 0);
                }
                out += "\"/></linearGradient>\n";
            };
            fade("shade-" + num, mix(r.rgb, 0x000000, 0.4), 25 + 2 * (15 - scene.sun.elevation));
            layer(r.faces, r.face_side, shaded, "url(#shade-" + num + ")", "");
            if (sunset) {
                fade("glow-" + num, 0xF6A98C, 35 + 10 * (2 - scene.sun.elevation));
                layer(r.faces, r.face_side, -shaded, "url(#glow-" + num + ")", "");
            }
            if (!r.glints.empty() && !r.snow.empty()) {
                std::string id = "snow-clip-" + num;
                out += "<clipPath id=\"" + id + "\"><path d=\"" + snow_d + "\"/></clipPath>\n";
                std::string glint = "#ffffff";
                if (sunset) {
                    glint.clear();
                    append_hex(glint, mix(r.snow_rgb, 0xFFD9C2, 0.8));
                }
                layer(r.glints, r.glint_side, -shaded, glint, " clip-path=\"url(#" + id + ")\"");
            }
        }

        if (n > 0) append_fog(out, width, height);
    }
    // Hills: in front of every ridge. Every
    // depth gap gets the same haze layer: between ridges, between the front
    // ridge and the hills, and between the back hill (with its trees) and the
    // front hill.
    if (!scene.hills.empty()) append_fog(out, width, height);
    for (size_t n = scene.hills.size(); n-- > 0;) {
        const Hill& h = scene.hills[n];
        out += "<path class=\"hill\" id=\"hill-";
        append_uint(out, static_cast<uint32_t>(n + 1));
        out += "\" fill=\"";
        append_hex(out, h.rgb);
        out += "\" d=\"";
        for (size_t i = 0; i < h.points.size(); ++i) {
            out += i == 0 ? "M" : " L";
            append_fixed2(out, h.points[i].x * scale);
            out += ",";
            append_fixed2(out, (kCanvasHeight - h.points[i].h) * scale);
        }
        out += " L";
        append_uint(out, width);
        out += ",";
        append_uint(out, height);
        out += " L0,";
        append_uint(out, height);
        out += " Z\"/>\n";
        // The back hill's crest trees, a distant tree line on its silhouette
        // (subspec/trees.md, Crest trees), behind the trees standing on it.
        for (size_t k = 0; k < h.crest_trees.size(); ++k) {
            out += "<path class=\"crest-tree\" fill=\"";
            append_hex(out, h.crest_rgb[k]);
            out += h.crest_trees[k].size() > 1 ? "\" fill-rule=\"evenodd\" d=\"" : "\" d=\"";
            for (size_t p = 0; p < h.crest_trees[k].size(); ++p) {
                const std::vector<Point>& poly = h.crest_trees[k][p];
                for (size_t i = 0; i < poly.size(); ++i) {
                    out += i == 0 ? (p == 0 ? "M" : " M") : " L";
                    append_fixed2(out, poly[i].x * scale);
                    out += ",";
                    append_fixed2(out, (kCanvasHeight - poly[i].h) * scale);
                }
                out += " Z";
            }
            out += "\"/>\n";
        }
        // The front hill's shading bands, top to bottom, each closed down to the
        // canvas bottom over the one before (subspec/hills.md, Shading bands).
        for (size_t b = 0; b < h.bands.size(); ++b) {
            out += "<path class=\"hill-band\" fill=\"";
            append_hex(out, h.band_rgb[b]);
            out += "\" d=\"";
            for (size_t i = 0; i < h.bands[b].size(); ++i) {
                out += i == 0 ? "M" : " L";
                append_fixed2(out, h.bands[b][i].x * scale);
                out += ",";
                append_fixed2(out, (kCanvasHeight - h.bands[b][i].h) * scale);
            }
            out += " L";
            append_uint(out, width);
            out += ",";
            append_uint(out, height);
            out += " L0,";
            append_uint(out, height);
            out += " Z\"/>\n";
        }
        // Tree shadows on the front hill, in its darkest band color, under the
        // ground cover (subspec/celestial.md, Shadows).
        for (size_t t = 0; t < scene.trees.size(); ++t) {
            const Tree& tr = scene.trees[t];
            if (tr.hill != static_cast<int>(n) || tr.shadow.empty() || h.band_rgb.empty()) continue;
            out += "<path class=\"shadow\" fill=\"";
            append_hex(out, h.band_rgb.back());
            out += "\" d=\"";
            for (size_t k = 0; k < tr.shadow.size(); ++k) {
                for (size_t i = 0; i < tr.shadow[k].size(); ++i) {
                    out += i == 0 ? (k == 0 ? "M" : " M") : " L";
                    append_fixed2(out, tr.shadow[k][i].x * scale);
                    out += ",";
                    append_fixed2(out, (kCanvasHeight - tr.shadow[k][i].h) * scale);
                }
                out += " Z";
            }
            out += "\"/>\n";
        }
        // Vegetation covers the front (or only) hill's ground, under its trees
        // (subspec/vegetation.md, Rendering).
        if (n == 0 && !scene.vegetation.empty()) {
            out += "<g class=\"vegetation\">\n";
            for (const Plant& p : scene.vegetation) {
                out += vegetation::plant_svg(p, scale);
                out += "\n";
            }
            out += "</g>\n";
        }
        // Trees standing on this hill, drawn right after it.
        for (size_t t = 0; t < scene.trees.size(); ++t) {
            const Tree& tr = scene.trees[t];
            if (tr.hill != static_cast<int>(n)) continue;
            out += trees::tree_svg(tr, static_cast<int>(t), scale);
            // Ground cover over the trunk's foot (subspec/trees.md, Ground cover).
            for (size_t k = 0; k < tr.cover.size(); ++k) {
                out += "<path class=\"ground\" fill=\"";
                append_hex(out, tr.cover_rgb[k]);
                out += "\" d=\"";
                for (size_t i = 0; i < tr.cover[k].size(); ++i) {
                    out += i == 0 ? "M" : " L";
                    append_fixed2(out, tr.cover[k][i].x * scale);
                    out += ",";
                    append_fixed2(out, (kCanvasHeight - tr.cover[k][i].h) * scale);
                }
                out += " Z\"/>\n";
            }
        }
        if (n > 0) append_fog(out, width, height);
    }
    out += "</svg>\n";
    return out;
}

}  // namespace bh
