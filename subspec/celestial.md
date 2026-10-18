## Celestial Namespace

| Bit range | Width |
|---|---:|
| 40–47 | 8 |

### Overview

The Celestial namespace places the sun or moon in the sky and decides its
position and intensity. It must agree with the sky: a moon belongs in a night
or dusk sky, and its placement should match the scene's lighting.

The sun or moon is optional. When it is absent, the namespace must still
account for all 8 bits by redistributing them within the namespace.

### Subnamespaces (conceptual)

* Sun / Moon (optional)
* Position / Intensity

### Dependencies

* Consumes: the theme from Global and the lighting mode from Sky.
* Provides: celestial position and lighting direction to later layers.

### Status

Subnamespace widths, the presence rule, and interpretation rules are not yet defined.
