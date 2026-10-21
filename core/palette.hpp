#pragma once

#include <cstdint>
#include <vector>

#include "theme.hpp"

namespace bh {

// Ridge palettes by depth role (subspec/mountains.md, Ridge color).
enum PaletteId {
    kPaletteFront = 0,  // 256 entries, nearest ridge
    kPaletteMiddle = 1, // 256 entries
    kPaletteFar = 2,    // 16 entries, Ridge 3
};

// Entries are 0xRRGGBB. Each theme and season defines one color family (hue
// ranges and a chroma multiplier) shared by every ridge. In the spec, palettes are constant tables; this
// prototype builds them once at startup from an OKLCH grid.
const std::vector<uint32_t>& palette(Theme theme, Season season, int id);

// Whether the front ridge fills with a vertical gradient (Temperate fall).
bool front_gradient(Theme theme, Season season);

// Bottom gradient colors paired with the front palette by index; empty when
// the front ridge has no gradient. The top color is the palette entry itself.
const std::vector<uint32_t>& gradient_bottom(Theme theme, Season season);

// Hill palettes (subspec/hills.md, Color). id: 0 = single hill (256 entries),
// 1 = front hill (4 entries), 2 = back hill (16 entries).
enum HillPaletteId {
    kHillSingle = 0,
    kHillFront = 1,
    kHillBack = 2,
};
const std::vector<uint32_t>& hill_palette(Theme theme, Season season, int id);

// Deciduous leaf palettes (subspec/trees.md, Leaves): kLeafPalettes sets of six
// colors per season, greens in spring and summer and oranges to yellows in
// fall. Winter trees are bare and read no leaf bits.
constexpr int kLeafPalettes = 16;
const std::vector<uint32_t>& leaf_palette(Season season, int id);

// Vegetation palettes (subspec/vegetation.md, Color): kVegetationPalettes sets of
// six colors per biome and season: white and yellow petals in spring, dark
// grass in summer, orange and gold leaves in fall, frosted grass in winter;
// desert rosettes in sage, olive, and ochre.
constexpr int kVegetationPalettes = 16;
const std::vector<uint32_t>& vegetation_palette(Theme theme, Season season, int id);

// Sky tints (subspec/sky.md, Tint): 256 light blues for the top of the sky.
uint32_t sky_tint(uint32_t index);

// The sky gradient's four stops (offsets 0, 0.45, 0.75, 1) and the sun glow color,
// by sky tint and sun elevation step (subspec/celestial.md, Sky warmth).
const std::vector<uint32_t>& sky_colors(int elevation, int tint_index, bool night);

// Cloud color by sun elevation step: white, or pink and gold under a low sun;
// moonlit gray-blue at night.
uint32_t cloud_color(int elevation, bool night);

// A color at night (subspec/global.md, Night): darker, grayer, and bluer,
// less so for far layers (far from 0, nearest, to 1, the sky's edge).
uint32_t night_rgb(uint32_t rgb, double far);

// A palette entry at night: night_rgb, except that entries which would collide
// move to the nearest unused color, so a palette's entries stay distinct
// (subspec/global.md, Night). Colors not in the palette fall back to night_rgb.
uint32_t night_entry(const std::vector<uint32_t>& pal, double far, uint32_t rgb);

// Fog tints (subspec/fog.md, Tint): 8 pale colors per biome and season.
const std::vector<uint32_t>& fog_palette(Theme theme, Season season);

}  // namespace bh
