## Vegetation Namespace

| Bit range | Width |
|---|---:|
| 224–247 | 24 |

### Overview

The Vegetation namespace fills the very front of the scene with plants,
grasses, and foliage. It is the closest layer to the viewer and also acts as
the ground: there is no separate ground plane beyond it. Its plants must suit
the selected theme.

### Seasons

Colors are thematic families of coordinated shades, not single hex values
(SPECIFICATIONS.md, Seasons).

* **Winter:** vegetation is shades of white, in every biome. This overrides the
  biome's own vegetation palette.
* **Spring:** in Temperate and Tundra, foreground vegetation has varied flower
  colors: yellow, pink, and blue. Alpine does not. Desert is not yet specified.
* **Summer and fall:** foliage is green.

### Subnamespaces (conceptual)

* Foreground Vegetation

### Dependencies

* Consumes: the theme and season from Global.
* Provides: the foreground layer, the front edge of the depth model.

### Status

Subnamespace widths and interpretation rules are not yet defined.
