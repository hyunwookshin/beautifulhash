#pragma once

// Configurations rendered by the gallery and checked by verify. Add variants
// here to compare parameter changes on identical hashes.
#include <string>
#include <vector>

#include "mountains.hpp"

namespace bh {

struct Variant {
    std::string group;
    std::string label;
    mountains::Params params;
};

struct Group {
    std::string name;
    std::string description;
};

inline std::vector<Group> variant_groups() {
    return {
        {"biomes", "Biome styling on identical hashes (experimental). Geometry and placement are "
                   "identical across columns; only colors, alpine snow caps, and alpine conifer texture differ."},
    };
}

inline std::vector<Variant> variants() {
    std::vector<Variant> out;
    for (Theme t : kThemes) {
        mountains::Params p;
        p.theme = t;
        out.push_back({"biomes", theme_name(t), p});
    }
    return out;
}

}  // namespace bh
