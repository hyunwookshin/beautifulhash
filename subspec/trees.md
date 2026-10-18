## Trees Namespace

| Bit range | Width |
|---|---:|
| 176–223 | 48 |

### Overview

The Trees namespace owns all tree generation in V0. It places and grows the branched trees independently of the other layers: where they stand,
how large they are, and how they branch. Tree species must suit the theme,
for example conifers in Alpine scenes and palms in Beachside or Rainforest
scenes.

A primary group of trees is always present, and an additional arrangement is
optional. When the additional trees are absent, their bits must be
redistributed within the namespace.

### Subnamespaces (conceptual)

* Primary Trees
* Additional Trees (optional)

### Dependencies

* Consumes: the theme from Global and the landscape's depth model. Trees does not depend on Hills.
* Provides: tree placements and depths, used by Fog and for occlusion.

### Status

Subnamespace widths, the presence rule, and interpretation rules are not yet defined.
