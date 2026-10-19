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
    int trees = 0;  // gallery only: force this tree count (0 = use the hash)
};

struct Group {
    std::string name;
    std::string description;
};

inline std::vector<Group> variant_groups() {
    std::vector<Group> out;
    for (Theme t : kThemes) {
        out.push_back({theme_name(t), std::string(theme_name(t)) +
                                          ": all four seasons on identical hashes. Everything, including the "
                                          "tree count, comes from the hash."});
    }
    return out;
}

inline std::vector<Variant> variants() {
    std::vector<Variant> out;
    for (Theme t : kThemes) {
        for (Season s : kSeasons) {
            mountains::Params p;
            p.theme = t;
            p.season = s;
            out.push_back({theme_name(t), season_name(s), p, 0});
        }
    }
    return out;
}

}  // namespace bh
