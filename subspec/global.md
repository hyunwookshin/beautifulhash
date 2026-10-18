## Global Namespace

| Bit range | Width |
|---|---:|
| 0–15 | 16 |

### Overview

The Global namespace makes the decisions that shape the whole landscape before
any individual layer is generated. It selects the environmental theme (Alpine,
Temperate, Tundra, or Desert) and sets the overall
composition, such as how the scene is balanced and framed.

Every other namespace builds on these choices, so Global is generated first.

### Subnamespaces (conceptual)

* Theme
* Composition

### Dependencies

* Consumes: none.
* Provides: the selected theme and composition settings to all other namespaces.

### Status

Subnamespace widths and interpretation rules are not yet defined.
