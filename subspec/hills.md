## Hills Namespace

| Bit range | Width |
|---|---:|
| 136–175 | 40 |

### Overview

The Hills namespace shapes the midground terrain between the mountains and the
foreground. It generates a primary hill and an optional secondary hill: their
terrain geometry, silhouettes, and surface colors. The hills lead the eye from
the foreground into the distance.

Hills contain no trees in V0. All tree generation belongs to the Trees
namespace. Hillside trees may be introduced in a later version.

When the secondary hill is absent, its bits must be redistributed within the
namespace.

### Subnamespaces (conceptual)

* Primary Hill
* Secondary Hill (optional)

### Dependencies

* Consumes: the theme from Global.
* Provides: hill silhouettes and depths, used by Fog and for occlusion.

### Status

Subnamespace widths, the presence rule, and interpretation rules are not yet defined.
