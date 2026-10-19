## Trees Namespace

| Bit range | Width |
|---|---:|
| 176–223 | 48 |

### Overview

The Trees namespace owns all tree generation in V0. It places and grows the branched trees independently of the other layers: where they stand,
how large they are, and how they branch. Tree species must suit the theme,
for example conifers in Alpine scenes and broadleaf trees in Temperate
scenes.

A primary group of trees is always present, and an additional arrangement is
optional. When the additional trees are absent, their bits must be
redistributed within the namespace.

### Seasons

Colors are thematic families of coordinated shades, not single hex values
(SPECIFICATIONS.md, Seasons).

* **Winter:** Temperate broadleaf trees are bare, with no leaves, and
  occasionally carry white snow spots. Conifers keep their foliage.
* **Spring, summer, and fall:** trees keep their foliage.

### Subnamespaces (conceptual)

* Primary Trees
* Additional Trees (optional)

### Dependencies

* Consumes: the theme and season from Global, the landscape's depth model, and
  the hill profiles advertised by Hills (subspec/hills.md), so trees can be
  planted on hills. Hills is generated first.
* Provides: tree placements and depths, used by Fog and for occlusion.

### Status

Subnamespace widths, the presence rule, and interpretation rules are not yet defined.
