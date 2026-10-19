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
        {"all", "Temperate in winter, summer, and fall, then the other biomes (summer), on identical "
                "hashes. Geometry and placement are identical across columns. Spring is omitted: its "
                "mountains match summer. Alpine, Tundra, and Desert mountains are the same in every season."},
    };
}

inline std::vector<Variant> variants() {
    std::vector<Variant> out;
    for (Season s : {Season::kWinter, Season::kSummer, Season::kFall}) {
        mountains::Params p;
        p.theme = Theme::kTemperate;
        p.season = s;
        out.push_back({"all", std::string("temperate, ") + season_name(s), p});
    }
    for (Theme t : {Theme::kAlpine, Theme::kTundra, Theme::kDesert}) {
        mountains::Params p;
        p.theme = t;
        p.season = Season::kSummer;
        out.push_back({"all", theme_name(t), p});
    }
    return out;
}

}  // namespace bh
