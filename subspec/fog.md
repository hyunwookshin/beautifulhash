## Fog Namespace

| Bit range | Width |
|---|---:|
| 248–255 | 8 |

### Overview

The Fog namespace adds atmospheric depth. One haze layer goes into every depth
gap in the scene, so each layer back reads paler than the one in front of it
and distant ridges fade into the sky.

Fog is always on and applied the same way in every scene. The hash varies its
character, not whether it appears or where: intensity stays in a narrow band,
and every gap gets the identical layer. Fog is generated last, after every
layer it sits between.

### Bit layout

Fields are read most significant bit first, in this order:

| Field | Bits | Meaning |
|---|---:|---|
| density $d$ | 2 | Mean opacity of the haze |
| tint $t$ | 3 | Haze color, from the biome and season's fog palette |
| mist $m$ | 3 | How much the haze thickens toward the ground |

Every field always has an effect, so no bit is ever inactive.

### Opacity

Opacities are whole hundredths, so they serialize exactly as `0.NN`:

$$
\bar{o} = 27 + 3d, \qquad o_{\text{top}} = \bar{o} - m, \qquad o_{\text{bottom}} = \bar{o} + m
$$

* The mean is 27%, 30%, 33%, or 36%: denser or lighter, never absent or opaque.
* The mist tilts the haze from the top of the canvas to the bottom without
  changing its mean. $m = 0$ is an even haze; $m = 7$ runs from $\bar{o} - 7$
  to $\bar{o} + 7$, like valley fog settling between the ridges.
* Across all 256 values, opacity stays within 20–43%.

The pair decodes uniquely: $\bar{o} = (o_{\text{top}} + o_{\text{bottom}}) / 2$
gives $d$, and $(o_{\text{bottom}} - o_{\text{top}}) / 2$ gives $m$.

### Tint

Each biome and season has 8 pale tints near the sky's color, at OKLCH
lightness 0.93. Index $t$ = hue (2 bits) | chroma (1 bit): four hues evenly
spaced across the range, each at chroma 0.012 and 0.028.

| Biome and season | Hue range | Character |
|---|---|---|
| Any biome, winter | 215–265 | Cold blue-white |
| Alpine | 220–260 | Cool blue-gray |
| Temperate, spring and summer | 190–240 | Blue-green mist |
| Temperate, fall | 60–95 | Golden haze |
| Tundra | 275–320 | Lavender |
| Desert | 55–90 | Dust |

The 8 tints of every palette serialize to distinct `#RRGGBB` values, checked by
`make verify`.

### Rendering

The haze is one vertical gradient in `<defs>`, used by every fog layer:

```svg
<linearGradient id="fog" x1="0" y1="0" x2="0" y2="1">
  <stop offset="0" stop-color="#RRGGBB" stop-opacity="0.NN"/>
  <stop offset="1" stop-color="#RRGGBB" stop-opacity="0.NN"/>
</linearGradient>
```

Both stops use the tint. Each fog layer is a full-canvas
`<rect class="fog" fill="url(#fog)"/>`, drawn in every depth gap, back to front:

* between adjacent mountain ridges (in front of Ridge 3 and Ridge 2),
* between the front ridge and the hills,
* in two-hill mode, between the back hill (with its trees) and the front hill.

The front hill and its trees are never fogged. A scene therefore has
(ridges − 1) + (hills) fog layers: 2 to 4. A layer lies over every layer
behind it, so farther layers collect more haze. Fog never rewrites another
layer's fill.

### Verification

`make verify` renders all 256 fog values in each of the 16 biome and season
pairs, decodes $d$, $t$, and $m$ from the SVG text alone, and checks that the
bits round-trip. It also counts the fog layers against the expected depth gaps.

### Dependencies

* Consumes: the theme and season from Global, and the number of ridges and hills
  (the depth gaps) from Mountains and Hills.
* Provides: the haze over every depth gap.

### Open decisions

* Once Sky exists, whether the tint should follow the sky palette rather than
  the biome.
* Ranges: the 27–36% mean and the ±7 mist tilt.
