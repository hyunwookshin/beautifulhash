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
                                          " (summer): 1, 3, 6, and 10 trees on identical hashes. Only the tree "
                                          "count is forced; everything else comes from the hash."});
    }
    return out;
}

inline std::vector<Variant> variants() {
    std::vector<Variant> out;
    for (Theme t : kThemes) {
        for (int n : {1, 3, 6, 10}) {
            mountains::Params p;
            p.theme = t;
            p.season = Season::kSummer;
            out.push_back({theme_name(t), std::to_string(n) + (n == 1 ? " tree" : " trees"), p, n});
        }
    }
    return out;
}

}  // namespace bh
