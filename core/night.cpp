#include "night.hpp"

#include <algorithm>

#include "palette.hpp"

namespace bh::night {

namespace {

void shift(uint32_t& rgb, double far) { rgb = night_rgb(rgb, far); }

void shift(std::vector<CanopyPath>& paths, double far) {
    for (CanopyPath& p : paths) shift(p.rgb, far);
}

}  // namespace

void apply(Scene& scene, Theme theme, Season season) {
    // Depth from 0 (nearest) to 1 (the sky's edge): the front ridge 0.45, the
    // back ridge 0.85 (Temperate: 0 to 0.25), hills and plants nearer still. Snow keeps a little more
    // of its light. The fog spans the whole canvas, sky included, so it takes
    // the nearest, darkest shift: a navy haze that keeps the night sky dark.
    size_t n = scene.ridges.size();
    for (size_t k = 0; k < n; ++k) {
        Ridge& r = scene.ridges[k];
        double t = n > 1 ? static_cast<double>(k) / static_cast<double>(n - 1) : 0.0;
        // Temperate ridges are wooded, with no moonlit faces: they go nearly
        // to silhouettes against the dusk sky, the far ones a little lighter.
        double far = theme == Theme::kTemperate ? 0.25 * t : 0.45 + 0.4 * t;
        double snow = std::min(1.0, far + 0.1);
        // The bit-carrying colors go through their palette's night table, so
        // they stay distinct.
        r.rgb = night_entry(palette(theme, season, r.palette), far, r.rgb);
        if (r.gradient) r.rgb_bottom = night_entry(gradient_bottom(theme, season), far, r.rgb_bottom);
        shift(r.snow_rgb, snow);
        shift(r.canopy, far);
        shift(r.snow_facets, snow);
    }
    for (size_t k = 0; k < scene.hills.size(); ++k) {
        Hill& h = scene.hills[k];
        double far = k == 0 ? 0.12 : 0.22;
        h.rgb = night_entry(hill_palette(theme, season, h.palette), far, h.rgb);
        for (uint32_t& c : h.band_rgb) shift(c, far);
        for (uint32_t& c : h.crest_rgb) shift(c, far);
        shift(h.canopy, far);
    }
    for (Tree& t : scene.trees) {
        double far = t.hill == 0 ? 0.08 : 0.18;
        for (TreeShape& s : t.shapes) shift(s.rgb, far);
        for (uint32_t& c : t.cover_rgb) shift(c, far);
        shift(t.trunk_rgb, far);
        for (uint32_t& c : t.leaf_rgb) shift(c, far);
    }
    for (Plant& p : scene.vegetation) shift(p.rgb, 0.08);
    scene.fog.rgb = night_entry(fog_palette(theme, season), 0.0, scene.fog.rgb);
    // Less atmospheric glow at night: the haze thins by 14 hundredths at top
    // and bottom, from 27-36% to 13-22%, so the moonlit faces stay crisp. A
    // constant step keeps every density and mist distinct (the thinnest top,
    // 20, stays positive), so the fog bits still show and decode.
    scene.fog.top -= 14;
    scene.fog.bottom -= 14;
}

}  // namespace bh::night
