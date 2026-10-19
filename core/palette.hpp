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

}  // namespace bh
