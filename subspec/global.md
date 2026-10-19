## Global Namespace

| Bit range | Width |
|---|---:|
| 0–15 | 16 |

### Overview

The Global namespace makes the decisions that shape the whole landscape before
any individual layer is generated. It selects the environmental theme (Alpine,
Temperate, Tundra, or Desert) and the season (winter, spring, summer, or fall),
and sets the overall composition, such as how the scene is balanced and framed.

Every other namespace builds on these choices, so Global is generated first.

### Subnamespaces (provisional)

| Subnamespace | Namespace bits | Absolute bits | Width | Values |
|---|---|---|---:|---|
| Theme | 0–1 | 0–1 | 2 | Alpine, Temperate, Tundra, Desert |
| Season | 2–3 | 2–3 | 2 | Winter, spring, summer, fall |
| Composition | 4–15 | 4–15 | 12 | Not yet defined |

Theme and season are each read directly as a 2-bit value, so all four biomes
and all four seasons are equally likely. The season's effects cascade through
Mountains, Hills, Trees, and Vegetation (see SPECIFICATIONS.md, Seasons).

### Dependencies

* Consumes: none.
* Provides: the selected theme, season, and composition settings to all other
  namespaces. They receive these values as context and never read Global's bits.

### Status

Theme and season widths are provisional. The value order within each field and
the composition layout are not yet defined.
