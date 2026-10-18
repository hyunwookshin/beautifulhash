## Mountains Namespace

| Bit range | Width |
|---|---:|
| 48–135 | 88 |

### Overview

The Mountains namespace draws the mountain ridges that form the distant
skyline. There are two required ridges and an optional third, each a
hash-derived silhouette, with taller and more distant ridges set further back.
Together they should read as one coherent mountain range rather than unrelated
shapes.

It owns the largest share of the hash because ridge silhouettes carry much of
the landscape's character. When the third ridge is absent, its bits must be
redistributed within the namespace.

### Subnamespaces (conceptual)

* Ridge 1
* Ridge 2
* Ridge 3 (optional)

See the specification for illustrative allocation options (by ridge or by property).

### Dependencies

* Consumes: the theme from Global.
* Provides: ridge silhouettes and depths, used by Fog and for occlusion.

### Status

Subnamespace allocation, the presence rule, and interpretation rules are not yet defined.
