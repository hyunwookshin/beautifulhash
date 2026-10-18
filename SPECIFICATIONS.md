## Core Principles

BeautifulHash converts a 256-bit hash into a stylized, layered landscape.

The following principles govern the design:

* Determinism: The same input and algorithm version must produce the same landscape.
* Immutability: Once an algorithm version is finalized, its generation rules must never change,
  and it remains available for regenerating existing artwork.
* Beauty first: Generated landscapes should be aesthetically coherent rather than visually random.
* Max variation: The generator should support an enormous number of distinct compositions.
  A 1:1 mapping from hash to unique image is a design objective, not a guarantee.
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
| 1 | Vegetation | Foreground plants, grasses, foliage | No |
| 2 | Trees | Branched trees | No |
| 3 | Additional trees | Extra tree arrangements | Yes |
| 4 | Hill with trees | Terrain and vegetation | No |
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

The generator supports five initial themes.

| Theme | Defining characteristics |
|---|---|
| Tundra | Cold terrain, sparse vegetation, possible aurora |
| Alpine | Mountainous terrain, coniferous trees |
| Temperate | Deciduous or mixed vegetation |
| Rainforest / volcanic | Dense tropical vegetation, palms, volcanic forms |
| Beachside | Coastal scenery, palms, softer terrain |

Theme selection is hash-derived.

Each theme defines constraints on allowable terrain shapes, vegetation, atmospheric effects, and color palettes.

Themes should share the same underlying generation framework while providing distinct visual characteristics.

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

Each major layer should have its own deterministic random stream.

For example:

```
SHA-256 hash
  ├── Theme stream
  ├── Sky stream
  ├── Cloud stream
  ├── Celestial stream
  ├── Mountain 1 stream
  ├── Mountain 2 stream
  ├── Mountain 3 stream
  ├── Hill stream
  ├── Tree stream
  ├── Vegetation stream
  └── Fog stream
```

This prevents adding or changing one layer from unintentionally changing every other layer during development.

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

A previous illustrative allocation estimated approximately 219 bits of scene-parameter variation.

That estimate was hypothetical, not a verified capacity of the generator.

### Uniqueness objective

Proposed V0 policy:

* Prioritize aesthetic quality and reproducibility.
* Maximize independent variation across layers.
* Do not require perceptual uniqueness for every hash.
* Do not claim a collision-free hash-to-image mapping without a formal guarantee.

If strict image uniqueness becomes a requirement, a separate injective encoding mechanism must be designed and verified.
