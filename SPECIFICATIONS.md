## Core Principles

BeautifulHash converts a 256-bit hash into a stylized, layered landscape.

The following principles govern the design:

* Determinism: The same input and algorithm version must produce the same landscape.
* Immutability: Once an algorithm version is finalized, its generation rules must never change,
  and it remains available for regenerating existing artwork.
* Beauty first: Generated landscapes should be aesthetically coherent rather than visually random.
* Max variation: The generator should support an enormous number of distinct compositions.
  A 1:1 mapping from hash to unique image is a design objective, not a guarantee.
* 256-bit entropy preservation: V0 aims to preserve all 256 bits of the input hash in its
  canonical SVG representation (see Entropy preservation objective).
* Portability: The generation algorithm should run consistently in native C++ and WebAssembly.
* Resolution independence: Changing export resolution must not change the generated composition.

No generative AI: All artwork is produced by a deterministic procedural algorithm.

## Input and Hashing

### Input

The BeautifulHash core accepts exactly 256 bits (32 bytes). External wrappers
convert arbitrary digital inputs into bytes and compute their SHA-256 digest
before invoking the generator. A caller may also supply a 32-byte digest directly.

Example wrapper inputs:

* The string `2026-10-18`
* The string `BeautifulHash`
* A personalized message
* A PDF, image, or other file

For the string wrapper, Unicode normalization, whitespace handling, and case sensitivity must be explicitly defined before V0 is frozen.

### Hashing

Wrappers transform their input into a 256-bit hash using SHA-256:
```
                  INPUT WRAPPERS
                          │
       ┌──────────────────┼──────────────────┐
       │                  │                  │
     String             File             Raw bytes
       │                  │                  │
   UTF-8 encode      Read bytes          As-is
       │                  │                  │
       └──────────────────┼──────────────────┘
                          │
                       SHA-256
                          │
                    256-bit hash
                          │
                 BeautifulHash V0
                          │
                       Scene
                          │
                       Renderer
```

The number of possible hash values is:

$$
2^{256} \approx 1.16 \times 10^{77}
$$

Distinct input strings are not guaranteed to have distinct SHA-256 hashes, although collisions are extraordinarily unlikely for ordinary use.

## Landscape Layers

Layers are listed from foreground to background.

| Order | Layer | Description | Optional |
|---|---|---|---|
| 1 | Vegetation | Ground cover on the front hill: fallen leaves, petals, grass | No |
| 2 | Trees | Branched trees | No |
| 3 | Additional trees | Extra tree arrangements | Yes |
| 4 | Primary hill | Terrain geometry, silhouette, and surface colors | No |
| 5 | Additional hill | Secondary terrain | Yes |
| 6 | Mountain ridge | Hash-derived silhouette | No |
| 7 | Taller mountain ridge | More distant/taller silhouette | No |
| 8 | Additional taller ridge | Third mountain ridge | Yes |
| 9 | Sky and clouds | Background atmosphere | No |
| 10 | Sun or moon | Celestial object linked to sky | Yes |

### Layer rules

* Maximum of three mountain ridges.
* No additional ground plane beyond the foreground vegetation.
* Optional layers are determined by the hash.
* Fog may be applied to individual layers to create depth.
* Layers must obey a consistent depth and occlusion model.
* Mountains and hills should form coherent silhouettes.
* Tree types and vegetation must be appropriate to the selected environment.
* Sun/moon placement must be consistent with the sky configuration.

## Environmental Themes

BeautifulHash focuses on mountainous landscapes. V0 supports four themes, also called biomes:

| Theme | Defining characteristics |
|---|---|
| Alpine | Slate-blue ranges, snow-capped back ridge, conifer tree line in front |
| Temperate | Bluish-teal ranges fading to soft blue, rounded deciduous canopy in front |
| Tundra | Muted purple and cool lavender ranges, bare silhouettes |
| Desert | Warm golden, ochre, and sandy ranges, bare silhouettes |

Beachside and Rainforest / volcanic were dropped when BeautifulHash committed to mountain scenes.

Theme selection is hash-derived, by the Global namespace.

A theme changes styling only: color palettes, textures, and overlays such as snow. It never changes the hash-derived geometry or placement, so the same hash produces the same mountain shapes in every theme.

Themes share the same underlying generation framework while providing distinct visual characteristics. Per-namespace effects are specified in each subspec; see [subspec/mountains.md](subspec/mountains.md), Biome styling.

## Seasons

Two bits of the Global namespace select one of four seasons: winter, spring,
summer, or fall. Like the theme, the season is context: Global decides it, and
other namespaces consume it without reading Global's bits. A season changes
styling only, never hash-derived geometry or placement.

Color names below describe thematic color families, not literal hex values.
"Green" means a palette of coordinated natural greens (sage, moss, deep
forest), "white" means snowy whites and pale blue-grays, and "yellowish orange"
means autumnal golds, ambers, and rusts. Each family is realized as a palette of
many distinct shades, chosen by hash bits and constrained by the biome, in the
same way as the mountain palettes (subspec/mountains.md, Ridge color).

The season cascades through every layer it affects:

| Season | Mountains | Hills | Trees | Foreground vegetation |
|---|---|---|---|---|
| Winter | Temperate: shades of bluish gray instead of the usual palette. Other biomes unchanged. | Snowy tints in every biome (pale blue-whites to lavender-whites), still varied by the color bits | Temperate broadleaf trees are bare, with occasional white (snow) spots | Frosted grass tufts in snowy tints clustered at the trees, in every biome |
| Spring | Unchanged | Green | Unchanged | White petals with occasional yellow, on the open ground away from the trees; desert rosettes in sage |
| Summer | Unchanged (the baseline palettes) | Green | Unchanged | Dark green grass tufts on the open ground away from the trees; desert rosettes in dusty olive |
| Fall | Temperate: shades of yellowish orange instead of the usual palette. Other biomes unchanged. | Green | Unchanged | Temperate: fallen orange and gold leaves at the trees, a few blown loose. Alpine and Tundra (evergreen): dry grass. Desert: rosettes in ochre |

Rules:

* **Winter overrides the biome** for hills and vegetation: they use snowy tints
  (pale blue-whites to lavender-whites, not plain white) whatever the biome's
  own palette is, still varied by their color bits so hashes stay distinct.
* **Summer is the baseline:** it uses each biome's standard palettes and
  textures as specified in the subspecs.
* **Biome styling still applies** unless a season rule above replaces it. For
  example, Alpine snow caps and conifer textures are present in every season.
* **Distinct output per season:** under the 256-bit entropy preservation
  objective, the two season bits must change the SVG in every biome, so all
  four seasons must render differently. The rules above do not yet guarantee
  this. Spring, summer, and fall render identically in Alpine and Desert, and
  summer and fall render identically in Tundra. These gaps must be closed
  before V0 is frozen, for example with a subtle seasonal tint per biome.

Per-namespace details are in each subspec.

## Deterministic Scene Generation

### Architecture

```
Input (via wrapper)
  ↓
SHA-256
  ↓
256-bit hash  ← core boundary
  ↓
Versioned deterministic PRNG
  ↓
Theme selection
  ↓
Layer and object generation
  ↓
Canonical scene description
  ↓
SVG renderer (canonical artwork)
  ├── Web preview
  └── High-resolution export
```

### Pseudorandom number generator

The generator must use an explicitly specified PRNG.

Requirements:

* No `Math.random()`.
* No implementation-dependent standard-library random distributions.
* Explicit integer widths and overflow behavior.
* Explicit seed derivation and byte ordering.
* Identical outputs in native C++ and WebAssembly.

The PRNG algorithm is TBD.

### Independent random streams

Each major layer should have its own deterministic random stream. Streams are not
derived from the entire hash; the hash is first split by the namespace partitioner,
and each namespace seeds its own stream or substreams from its own bits.

```
SHA-256 (256 bits)
    │
    └── Namespace partitioner
          ├── Global (16 bits) → Global PRNG
          ├── Sky (24 bits) → Sky PRNG
          ├── Celestial (8 bits) → Celestial PRNG
          ├── Mountains (88 bits) → Mountain substreams
          ├── Hills (40 bits) → Hill substreams
          ├── Trees (48 bits) → Tree substreams
          ├── Vegetation (24 bits) → Vegetation PRNG
          └── Fog (8 bits) → Fog PRNG
```

This prevents adding or changing one layer from unintentionally changing every other layer during development.

Each stream is seeded only from the bits of the namespace that owns it (see Hash Namespace Partitioning).

Before V0 is frozen, the stream derivation method and generation order must be specified.

### Scene representation

Scene generation and drawing must be separate operations.

The scene description contains geometry, colors, layer order, object attributes, and atmospheric settings.

Example conceptual structure:

```json
{
  "version": 0,
  "theme": "alpine",
  "sky": {
    "palette": 3,
    "clouds": []
  },
  "mountains": [],
  "hills": [],
  "trees": [],
  "vegetation": [],
  "fog": []
}
```

This is an illustrative structure, not the frozen V0 schema.

## Hash Namespace Partitioning

### Overview

BeautifulHash partitions the 256-bit SHA-256 digest into a hierarchy of **namespaces** and **subnamespaces**.

Each namespace owns a fixed number of bits and is responsible for a particular aspect of the landscape. A namespace may subdivide its allocated bits into smaller subnamespaces, allowing individual generation algorithms to define their own internal structure.

The purpose of partitioning is to establish explicit ownership of all 256 bits, isolate generation subsystems, and provide a stable foundation for deterministic scene generation.

### Top-Level Namespace Allocation

The proposed V0 allocation is:

| Namespace | Bit range | Width | Responsibility |
|---|---|---:|---|
| Global | 0–15 | 16 | Theme, season, and overall composition |
| Sky | 16–39 | 24 | Sky colors, clouds, atmospheric conditions |
| Celestial | 40–47 | 8 | Sun, moon, position, and lighting |
| Mountains | 48–135 | 88 | Mountain ridges and their geometry |
| Hills | 136–175 | 40 | Hill terrain geometry, silhouettes, and surface colors (no trees) |
| Trees | 176–223 | 48 | Tree arrangements, sizes, and branching |
| Vegetation | 224–247 | 24 | Foreground plants and foliage |
| Fog | 248–255 | 8 | Atmospheric depth and layer-specific fog |
| **Total** | **0–255** | **256** | **Entire SHA-256 digest** |

Bit 0 is the most significant bit of the first byte of the SHA-256 digest. Bit 255 is the least significant bit of the final byte.

All bit ranges are inclusive.

### Namespace Hierarchy

```text
SHA-256 (256 bits)
│
├── Global (16 bits)
│   ├── Theme
│   ├── Season
│   └── Composition
│
├── Sky (24 bits)
│   ├── Palette
│   ├── Clouds
│   └── Atmosphere
│
├── Celestial (8 bits)
│   ├── Sun / Moon (optional)
│   └── Position / Intensity
│
├── Mountains (88 bits)
│   ├── Header (Ridge 3 present?)
│   ├── Ridge 1
│   ├── Ridge 2
│   └── Ridge 3 (optional)
│
├── Hills (40 bits)
│   ├── Primary Hill
│   └── Secondary Hill (optional)
│
├── Trees (48 bits)
│   ├── Primary Trees
│   └── Additional Trees (optional)
│
├── Vegetation (24 bits)
│   └── Foreground Vegetation
│
└── Fog (8 bits)
    └── Atmospheric Depth
```

The subnamespaces shown above are conceptual. Their exact widths and boundaries have not yet been finalized.

Each namespace is specified in its own document under [`subspec/`](subspec/):
[Global](subspec/global.md), [Sky](subspec/sky.md), [Celestial](subspec/celestial.md),
[Mountains](subspec/mountains.md), [Hills](subspec/hills.md), [Trees](subspec/trees.md),
[Vegetation](subspec/vegetation.md), and [Fog](subspec/fog.md).

### Partitioning Rules

1. **Complete allocation:** Every bit of the 256-bit hash must belong to exactly one top-level namespace.

2. **Contiguous ranges:** Each namespace receives a contiguous bit range. All ranges are defined using the same bit-numbering convention.

3. **Hierarchical subdivision:** A namespace may subdivide its allocated bits into two or more subnamespaces. Subnamespaces may be recursively subdivided.

4. **Flexible allocation:** Subnamespaces do not need equal widths. Their allocation is determined by the requirements of the parent namespace.

5. **No overlap:** Sibling namespaces and subnamespaces must not overlap or consume bits owned by another namespace.

6. **Complete subdivision:** The widths of all immediate subnamespaces must sum to the width of their parent namespace. Any unassigned capacity must be explicitly declared as reserved.

7. **Entropy ownership:** A namespace may only use its allocated bits as its source of pseudorandom entropy. Deterministic expansion through a PRNG or another specified algorithm is permitted, but does not increase the amount of independent entropy.

8. **Context sharing:** Namespaces may consume deterministic context produced by other namespaces, such as the selected theme or lighting conditions, without directly consuming their bit allocations.

9. **Versioning:** Namespace widths, boundaries, and interpretation rules are part of the versioned algorithm. Once V0 is frozen, they must not change.

10. **Portability:** Bit extraction and interpretation must be independent of native integer endianness, compiler behavior, and execution platform.

### Subnamespace Allocation

Each namespace defines its own internal partitioning.

For example, the 88-bit Mountains namespace could use:

**Option A — Unequal ridge allocation**

| Subnamespace | Width |
|---|---:|
| Ridge 1 | 32 |
| Ridge 2 | 32 |
| Ridge 3 | 24 |
| **Total** | **88** |

**Option B — Approximately equal ridge allocation**

| Subnamespace | Width |
|---|---:|
| Ridge 1 | 30 |
| Ridge 2 | 29 |
| Ridge 3 | 29 |
| **Total** | **88** |

**Option C — Property-based allocation**

| Subnamespace | Width |
|---|---:|
| Geometry | 44 |
| Color | 28 |
| Texture | 16 |
| **Total** | **88** |

These are illustrative alternatives, not finalized V0 decisions. Mountains has provisionally adopted a variant: a 1-bit header selects between two independent layouts, two ridges (43/44) or three ridges (32/32/23); see [subspec/mountains.md](subspec/mountains.md).

The parent namespace is responsible for defining how its subnamespaces map to visual parameters, including their ranges, distributions, dependencies, and constraints.

### Optional Subnamespaces

The optional features in V0 are the layers marked optional in the Landscape Layers table:

* Third mountain ridge
* Secondary hill
* Additional trees
* Sun or moon

An optional feature may be omitted from the rendered landscape based on a deterministic decision.

However, its allocated bits remain owned by the parent namespace.

The parent namespace must explicitly define how those bits are interpreted when the optional feature is absent. Under the entropy preservation objective, it must redistribute them to other variations within the same namespace; it may not discard them. The behavior must be deterministic and documented.

Redistributing otherwise inactive bits does not by itself guarantee unique output for every hash.

### Independence and Dependencies

Namespace partitioning separates entropy ownership from visual dependencies.

For example:

* The Mountains namespace may use the Global namespace's selected theme to choose mountain colors and textures.
* The Trees and Vegetation namespaces plant on hills using the hill profiles advertised by Hills (crest, edge heights, and shape), so Hills is generated before them.
* The Fog namespace may use the scene's layer depths to determine atmospheric effects.
* The Celestial namespace may use the Sky namespace's lighting mode to select a compatible sun or moon.

These dependencies must not involve reading another namespace's raw hash bits.

The generation order and permitted dependencies must be specified before V0 is frozen.

### Uniqueness Considerations

A namespace containing $n$ bits has at most $2^n$ independent input states.

A deterministic PRNG may expand these bits into an arbitrary sequence of values, but cannot create additional independent information.

Partitioning all 256 bits ensures that every input bit has an assigned owner. It does not guarantee that every bit affects the rendered scene or that every distinct hash produces a distinct SVG.

Strict hash-to-SVG uniqueness is the V0 entropy preservation objective (see Uniqueness and Combination Space). Partitioning is necessary for it but not sufficient.

### Verification Requirements

Before V0 is frozen, automated tests must verify that:

* Top-level namespace widths sum to exactly 256 bits.
* Every bit belongs to exactly one namespace.
* Subnamespace widths sum to their parent namespace's width.
* No bit ranges overlap or extend beyond their parent.
* Bit extraction produces identical results in native C++ and WebAssembly.
* Optional-feature handling follows the specified allocation rules.
* All namespace and subnamespace definitions are versioned.
* Golden test vectors produce identical canonical scenes and SVG bytes across supported platforms.

### V0 Freeze Requirements

The following must be finalized before V0 is frozen:

* Top-level namespace boundaries.
* All subnamespace allocations.
* Bit interpretation and expansion algorithms.
* Optional-feature presence rules.
* Cross-namespace dependencies.
* Generation order.
* Parameter ranges and distributions.
* Tests validating partition integrity and deterministic output.
* Per-namespace accounting of every allocated bit, including when optional features are absent.
* A formal construction or proof that the hash-to-SVG mapping is injective.

**Status:** The top-level allocation is proposed. Subnamespace layouts and interpretation rules remain open design decisions.

## Visual Composition

### Canvas

* Proposed logical dimensions: 2048 × 1536
* Aspect ratio: 4:3

All geometry is expressed in logical scene coordinates.

Export resolution must not influence the scene-generation algorithm.

### Rendering targets

SVG is the canonical artwork format. Raster exports are produced by rendering the SVG at the target resolution.
Canvas may be used as an optional preview mechanism, but it is not the source of truth.

The target resolution is an explicit rendering parameter (width × height in pixels, e.g. 2048 × 1536)
passed to the SVG renderer. It must never be derived from the browser's display size.

| Target | Resolution |
|---|---|
| Website preview | 1024 × 768 |
| Standard composition | 2048 × 1536 |
| 16 × 12-inch print at 300 DPI | 4800 × 3600 |
| 24 × 18-inch print at 300 DPI | 7200 × 5400 |

SVG suits the artwork because it is primarily composed of stylized 2D shapes.

Pixel-identical rendering across different SVG engines is not guaranteed. The guarantee below covers
the SVG bytes, not how a given engine rasterizes them.

### V0 compatibility guarantee

For a given 256-bit hash, algorithm version, and target resolution, BeautifulHash must produce an
identical canonical scene and byte-for-byte identical SVG output, regardless of whether the generator
runs natively or in WebAssembly.

Changing the target resolution may change SVG coordinates or serialization, but must not change the
underlying landscape composition.

The core API separates these two steps:

```cpp
Scene generate_scene(Hash256 hash, Version version);

std::string render_svg(
    const Scene& scene,
    uint32_t width,
    uint32_t height
);
```

* `generate_scene` is independent of resolution.
* `render_svg` guarantees identical SVG bytes for identical arguments across supported platforms.

Golden tests verify this by comparing canonical scene digests and exact SVG hashes between native and WebAssembly builds.

### Visual constraints

* Maintain coherent foreground, midground, and background separation.
* Use atmospheric perspective to distinguish distant layers.
* Avoid excessive overlap that obscures major features.
* Preserve recognizable tree and mountain silhouettes.
* Maintain visual balance without forcing symmetry.
* Avoid combinations of colors or objects that conflict with the selected theme.

## Uniqueness and Combination Space

### Definitions

BeautifulHash distinguishes three types of uniqueness.

* Parameter uniqueness: Different hashes produce different scene parameter configurations.
* SVG uniqueness: Different hashes produce different canonical SVG output.
* Pixel uniqueness: Different hashes produce different final raster images.
* Perceptual uniqueness: Different hashes produce images that humans can visually distinguish.

These are separate properties and must not be treated as equivalent.

### Theoretical capacity

SHA-256 provides 256 bits of input hash space.

The actual number of possible landscapes depends on:

* Theme combinations
* Sky and cloud arrangements
* Sun/moon variations
* Mountain geometry
* Hill geometry
* Tree positions, sizes, and branching patterns
* Foreground vegetation
* Color variation
* Fog and atmospheric effects

The namespace partition allocates all 256 bits of input entropy. It does not establish
256 bits of distinct rendered output: how many distinct landscapes the generator can
actually produce depends on how each namespace interprets its bits.

### Entropy preservation objective

256-bit entropy preservation: BeautifulHash V0 aims to preserve all 256 bits of the input
hash in its canonical SVG representation. Every namespace must account for its complete
allocated bit width, including when optional visual features are absent. A namespace may
redistribute its bits among its own subnamespaces, but may not discard them.

Full entropy preservation requires an injective hash-to-SVG mapping. Merely consuming all
input bits through a PRNG does not satisfy this requirement.

The mapping must be verified through a formal construction or proof rather than
statistical collision testing alone.

### Uniqueness objective

Proposed V0 policy:

* Prioritize aesthetic quality and reproducibility.
* Pursue SVG uniqueness through the entropy preservation objective.
* Maximize independent variation across layers.
* Do not require pixel or perceptual uniqueness for every hash.
* Do not claim a collision-free hash-to-SVG mapping until the injectivity construction or proof is complete.
