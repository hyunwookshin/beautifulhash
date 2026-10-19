#pragma once

namespace bh {

// Biomes (SPECIFICATIONS.md, Environmental Themes). A biome changes ridge
// styling only, never the hash-derived geometry. Selected by the Global
// namespace; until that namespace exists, callers pass it in.
enum class Theme {
    kAlpine,
    kTemperate,
    kTundra,
    kDesert,
};

inline constexpr Theme kThemes[] = {Theme::kAlpine, Theme::kTemperate, Theme::kTundra, Theme::kDesert};

inline const char* theme_name(Theme t) {
    switch (t) {
        case Theme::kAlpine: return "alpine";
        case Theme::kTemperate: return "temperate";
        case Theme::kTundra: return "tundra";
        case Theme::kDesert: return "desert";
    }
    return "?";
}

}  // namespace bh

namespace bh {

// Seasons (SPECIFICATIONS.md, Seasons). Selected by the Global namespace;
// like the theme, a season changes styling only.
enum class Season {
    kWinter,
    kSpring,
    kSummer,
    kFall,
};

inline constexpr Season kSeasons[] = {Season::kWinter, Season::kSpring, Season::kSummer, Season::kFall};

inline const char* season_name(Season s) {
    switch (s) {
        case Season::kWinter: return "winter";
        case Season::kSpring: return "spring";
        case Season::kSummer: return "summer";
        case Season::kFall: return "fall";
    }
    return "?";
}

}  // namespace bh
