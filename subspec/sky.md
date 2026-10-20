## Sky Namespace

| Bit range | Width |
|---|---:|
| 16–39 | 24 |

### Overview

The Sky namespace paints the backdrop of the landscape: the sky's color
palette, the clouds, and the atmospheric conditions such as time of day or
weather mood. It is the furthest layer back and sets the lighting that the
rest of the scene should feel consistent with.

### Subnamespaces (conceptual)

* Palette
* Clouds
* Atmosphere

### Dependencies

* Consumes: the theme from Global.
* Provides: the sky palette and lighting mode, used by Celestial and by layer coloring.

### Status

Subnamespace widths and interpretation rules are not yet defined.

Until this namespace is built, the renderer draws a fixed placeholder sky: a
vertical gradient from `#9DB6D3` to `#E9EEF2`. The low sun shifts its hue toward
sunset colors (subspec/celestial.md, Sky warmth), and the namespace should keep
that behavior.
