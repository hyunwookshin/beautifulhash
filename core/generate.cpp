#include "generate.hpp"

#include <cstdio>
#include <cstdlib>

#include "fog.hpp"
#include "format.hpp"
#include "hills.hpp"
#include "mountains.hpp"
#include "trees.hpp"
#include "vegetation.hpp"

namespace bh {

namespace {

// Placeholder sky until its namespace exists. Fixed; consumes no hash bits.
constexpr const char* kSkyTop = "#9db6d3";
constexpr const char* kSkyBottom = "#e9eef2";

void append_hex(std::string& out, uint32_t rgb) {
    static const char* digits = "0123456789abcdef";
    out.push_back('#');
    for (int shift = 20; shift >= 0; shift -= 4) out.push_back(digits[(rgb >> shift) & 15]);
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
    out += "<defs><linearGradient id=\"sky\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">";
    out += "<stop offset=\"0\" stop-color=\"";
    out += kSkyTop;
    out += "\"/><stop offset=\"1\" stop-color=\"";
    out += kSkyBottom;
    out += "\"/></linearGradient>";
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
    out += "\"/></linearGradient></defs>\n";
    out += "<rect class=\"sky\" ";
    append_full_rect(out, width, height);
    out += " fill=\"url(#sky)\"/>\n";

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
        if (!r.snow.empty()) {
            out += "<path class=\"snow\" fill=\"";
            append_hex(out, r.snow_rgb);
            out += "\" d=\"";
            for (const auto& poly : r.snow) {
                for (size_t i = 0; i < poly.size(); ++i) {
                    out += i == 0 ? (&poly == &r.snow.front() ? "M" : " M") : " L";
                    append_fixed2(out, poly[i].x * scale);
                    out += ",";
                    append_fixed2(out, (kCanvasHeight - poly[i].h) * scale);
                }
                out += " Z";
            }
            out += "\"/>\n";
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
            if (scene.trees[t].hill == static_cast<int>(n)) out += trees::tree_svg(scene.trees[t], static_cast<int>(t), scale);
        }
        if (n > 0) append_fog(out, width, height);
    }
    out += "</svg>\n";
    return out;
}

}  // namespace bh
