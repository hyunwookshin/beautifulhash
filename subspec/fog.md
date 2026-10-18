## Fog Namespace

| Bit range | Width |
|---|---:|
| 248–255 | 8 |

### Overview

The Fog namespace adds atmospheric depth. It applies fog and haze to
individual layers so that distant ridges fade into the sky and the scene
reads clearly from foreground to background.

Because fog depends on where everything else sits, it is generated after
the other layers.

### Subnamespaces (conceptual)

* Atmospheric Depth

### Dependencies

* Consumes: the theme from Global and the layer depths produced by the other namespaces.
* Provides: per-layer fog settings to the renderer.

### Status

Subnamespace widths and interpretation rules are not yet defined.
