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
        {"default", "Default configuration (subspec/mountains.md): front ridge midpoint r = 0.10, "
                    "middle ridge summit r = 0.125, back ridge summit r = 0.15"},
    };
}

inline std::vector<Variant> variants() {
    return {{"default", "default", mountains::Params{}}};
}

}  // namespace bh
