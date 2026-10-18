## BeautifulHash

The project is to turn any input (strings, numbers, pdfs) into a beautiful,
unique, and repeatable landscape imagery in SVG format.

### Rationale

Many people want to use landscape images on their site, their office or even
living room at home.

However, what kinds of image do people prefer. They don't want templates
from shutterstock or bigbox retailers, but also something that is not too artisan.

As of late 2026, the landscape images from image-generation models still feel
"synthetic", "generated" and childish in some ways (also known as "slop"). This
could change in the future, but at the moment, I personally wouldn't use the 
LLM-generated images to hang on the wall.

### How BeautifulHash is Different

We are taking algorithmic and deterministic approach to drawing beautiful landscapes.
The quality control is done by me, who loves painting and drawing for fun on the
side. I will also incorporate feedback from users to improve the generation
algorithm (please see versioning below).

### How will Versioning work?

Unlike other repository the algorithm will be treated and develop like a protocol.
The updates will only be committed if the new version consistently produces higher
fidelity images across large distribution of seeds, without significant regressions.

The cadence will also be significantly slower than other projects. Version 0.1
will minimum last 6 month, and subsequent versions will likely last significantly
longer than 6 months.

Each version is intended to be the "last" version. Meaning when the new algorithm
version is rolled out, it is intended to "last" forever. The reason is people
will prefer to regenerate the images from the seeds, and having multiple versions
to juggle through is not an optimal user experience.

### What is the Input to the Algorithm

The input is any digital medium, it can be plain text (i.e. `string`), numbers,
pdf files, zip file, even images. They are converted to sha256 hash (which
is essentially what is used to store 'passwords'). The sha256 hash is then
mapped to a unique image that depicts a beautiful landscape.

### Will each input have unique landscape

Each sha256 hash should map to a unique landscape image.
But it is not a guarantee that two inputs generate two unique hash,
although it would be an astronomically rare event.

#### Collision considerations

Even if the generator can represent at least $2^{256}$ different parameter combinations, that alone does not guarantee that all hashes map to different images.

The mapping must be injective to provide such a guarantee.

The number of available combinations and the uniqueness of the mapping are different mathematical questions.

### Implementation Architecture

#### Proposed technology

| Component | Technology |
|---|---|
| Core scene generator | C++ |
| Browser compilation | WebAssembly |
| Website interface | TypeScript |
| Preview rendering | SVG or Canvas |
| Native generation | C++ executable |
| High-resolution export | SVG or raster export |

#### Core boundaries

The C++ generator should not depend on:

* Browser APIs
* DOM or Canvas
* Platform-specific random number generators
* System time
* Network access
* Operating-system locale
* Display resolution

Its responsibility is to produce a canonical scene description from an input hash.

#### Suggested API

```cpp
Scene generate_scene(
    const Hash256& hash,
    AlgorithmVersion version
);
```

The renderer consumes the scene separately.

### Versioning and Immutability

Every generated artwork is associated with an algorithm version.

Example:

```
BeautifulHash V0
SHA-256: <64-character hexadecimal hash>
```

Once V0 is frozen:

* PRNG behavior cannot change.
* Hash-to-scene mapping cannot change.
* Theme selection rules cannot change.
* Layer-generation algorithms cannot change.
* Default palettes and geometry rules cannot change.
* Scene serialization rules must remain compatible.

Bug fixes that alter scene output require a new algorithm version.

Presentation-only changes may be made separately, but any change that alters the promised final visual output must be versioned.

### Verification

Before freezing V0, create golden test vectors.

Each test vector should include:

* Input string
* SHA-256 digest
* Algorithm version
* Canonical scene digest
* Reference rendering, if pixel reproducibility is required

Test categories should include:

* Empty string
* ASCII input
* Unicode input
* Korean characters
* Emoji
* Long strings
* Inputs differing by one character
* Optional-layer combinations
* Every environmental theme

Native and WebAssembly implementations must generate identical canonical scenes for the same test vectors.

### Open Design Decisions

The following remain unresolved and should not be treated as finalized V0 requirements:

* Unicode normalization policy
* PRNG algorithm and stream derivation
* Theme-selection distribution
* Exact mountain and hill geometry algorithms
* Tree branching and vegetation generation algorithms
* Palette definitions
* Fog model
* Canonical scene serialization format
* Whether pixel-level uniqueness is required
* Whether pixel-identical rendering across platforms is required
* Final logical canvas dimensions
* SVG versus Canvas rendering
* Whether the first release includes print ordering

### V0 Completion Criteria

V0 is ready to freeze when:

* All five themes can produce aesthetically acceptable landscapes.
* All required and optional layers function.
* Identical inputs produce identical scenes.
* Native C++ and WebAssembly scene outputs match.
* Scene composition is independent of export resolution.
* Golden tests cover the core generator.
* The visual output has been reviewed across a representative sample of hashes.
* All algorithm-affecting implementation decisions are documented.

Guiding principle: BeautifulHash should produce artwork worth keeping, not merely demonstrate that SHA-256 can generate a large number of pictures.
