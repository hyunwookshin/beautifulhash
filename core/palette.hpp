#pragma once

#include <cstdint>
#include <vector>

namespace bh {

// Provisional ridge palettes (subspec/mountains.md, Ridge color).
enum PaletteId {
    kPaletteFront = 0,  // 256 entries, nearest ridge
    kPaletteMiddle = 1, // 256 entries
    kPaletteFar = 2,    // 16 entries, Ridge 3
};

// Entries are 0xRRGGBB. In the spec, palettes are constant tables; this
// prototype builds them once at startup from an OKLCH grid.
const std::vector<uint32_t>& palette(int id);

}  // namespace bh
