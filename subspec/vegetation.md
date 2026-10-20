## Vegetation Namespace

| Bit range | Width |
|---|---:|
| 224–247 | 24 |

### Overview

The Vegetation namespace covers the ground of the front (or only) hill with
many small plants: petals in spring, grass in summer, fallen leaves under the
Temperate trees in fall, frosted grass in winter, and rosettes in the desert.
Where they grow depends on the plant, and the two placements are each
other's negative:

* **Near the trees:** Temperate fall leaves (one in five blown loose across
  the hill) and winter's frosted grass gather at the foot of the trees.
* **Away from the trees:** everything else fills the open ground between the
  trees, thinning to nothing at each trunk and growing denser toward the rim,
  as receding ground does.

Plants follow the same perspective as the trees: tiny by the rim, growing
toward the viewer. The back hill gets no vegetation.

### Bit layout

Fields are read most significant bit first, in this order:

| Field | Bits | Meaning |
|---|---:|---|
| palette | 4 | One of 16 six-color palettes for the biome and season |
| density | 2 | Plants per anchor: 40, 56, 72, or 96 |
| spread | 2 | Cluster radius factor $k$: 0.8, 1.0, 1.25, or 1.5 |
| seed | 16 | The Vegetation PRNG seed: every plant's position, size, and turn |

The spread sets the cluster radius, which only plants near the trees use. On
open ground it would have no effect, which would leave two bits inactive, so
there it scales the plants instead (see Size). Every field therefore has an
effect in every biome and season.

### Plants by biome and season

| Season | Plant | Colors |
|---|---|---|
| Fall, Temperate | Fallen leaf: an eye shape, as on the trees | Orange, gold, and red-brown |
| Fall, Alpine and Tundra | Grass tuft, dry: these evergreen biomes drop no leaves | Straw to olive-brown |
| Spring | Petal: the same eye shape | Creamy to cool whites, with one or two yellows |
| Summer | Grass tuft, `\|||/` | Dark greens |
| Winter | Grass tuft, frosted | Snowy blue-white and lavender-white tints |
| Desert, except winter | Rosette: five broad pointed leaves fanning from the ground | Sage (spring), dusty olive (summer), ochre (fall) |

Winter overrides the biome, so the desert also has frosted grass in winter.

### Placement

**Anchors.** Every tree standing on the front (or only) hill (Trees, Group:
rim or interior) is an anchor, at its base point $(x_b, h_b)$ with its overall
size factor $\sigma$ (composition scale times size class, subspec/trees.md).
Anchors are ordered left to right by $x_b$. A front hill with no trees gets one
stand-in anchor at the crest's $x$, at 55% of the hill's height there, with
$\sigma = 1$.

Each anchor gets $n$ plants (the density), so the plant count always follows
the number of trees. Plant $i$ of anchor $a$ draws uniforms $u_0, \dots, u_5$
from the PRNG. $R(x)$ is the front hill's silhouette height at $x$.

**Near the trees (Temperate fall and winter).** With radius $r = 90\,k\,\sigma$:

* **Across:** $x = x_{lo} + (x_{hi} - x_{lo}) \cdot \tfrac12(1 + u_0 - u_1)$,
  where $[x_{lo}, x_{hi}] = [x_b - 1.5r,\, x_b + 1.5r] \cap [0, 2048]$. The
  difference of two uniforms peaks at the tree.
* **Down the slope:** $h_{hi} = \min(h_b + 0.15r,\; 0.97R(x))$ and
  $h_{lo} = \min(\max(0,\, h_b - 0.6r),\; h_{hi})$:
  $h = h_{hi} - (h_{hi} - h_{lo})\,u_2 u_3$. The product of two uniforms
  gathers plants just in front of the tree, and no plant rises above the
  silhouette.
* The plant takes its tree's size: $\sigma_p = \sigma$.

In Temperate fall, every fifth leaf ($i \bmod 5 = 4$) is a stray, blown loose:
it lands at $x = 2048\,u_0$ and is otherwise placed and sized like an
open-ground plant, so leaves also appear well away from the trees.
**Away from the trees (everything else).** The anchors' $x_b$ and the canvas
edges $0$ and $2048$ cut the hill into gaps $[e_g, e_{g+1}]$:

* **Gap:** the gap containing $2048\,u_0$, so each gap is picked in proportion
  to its width.
* **Across:** $x = e_g + (e_{g+1} - e_g) \cdot \tfrac12(1 + u_1 - u_2)$, peaked
  at the gap's middle and thinning to nothing at the trees on either side.
* **Down the slope:** $h = 0.97\,R(x)\,(1 - u_3^4)$, anywhere from just below
  the rim to the canvas bottom. The fourth power packs plants toward the rim,
  as receding ground holds more of them in less of the canvas, and leaves the
  front light enough for the ground to show.
* Open-ground plants have no tree to share a size with; the spread scales them
  instead: $\sigma_p = k$.

**Size.** With depth below the rim $t = 1 - h / R(x)$ (0 at the rim, 1 at the
canvas bottom) and jitter $j = 0.85 + 0.3u_4$,

$$
d = \sigma_p\,(0.35 + g\,t)
$$

with $g = 1.65$ near the trees and $g = 1.25$ on open ground (and for strays),
so the open front stays light.

Plants are tiny by the rim and grow toward the viewer; near the trees, those by
a rim tree are also far smaller than those in front of a slope tree.

* Leaf or petal: half-length $2.2\,d\,j$.
* Tuft: blade height $6\,d\,j$; the outer blades lean out by $0.25 + 0.3u_5$
  of that height.
* Rosette: leaf height $5\,d\,j$; the outer leaves lean out by $0.9 + 0.4u_5$.

**Turn (leaves and petals):** with $q = 2u_5 - 1$, the long axis is the
rational direction $\left(\frac{1 - q^2}{1 + q^2}, \frac{2q}{1 + q^2}\right)$,
covering −90° to 90°. That is every turn an eye shape has, with no
trigonometry.

**PRNG.** Each uniform is a fixed integer mix of the seed and the plant's
place, so no running state is involved. With
$z = \text{seed} \cdot 2^{32} + a \cdot 2^{20} + i \cdot 2^4 + s$ for stream $s$,
the splitmix64 finalizer is applied:

```
z += 0x9E3779B97F4A7C15
z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
z = (z ^ (z >> 27)) * 0x94D049BB133111EB
z ^= z >> 31
u = (z >> 11) / 2^53
```

### Color

Colors are assigned in generation order (anchor by anchor, plant by plant),
cycling through the palette's six colors. Palette $p$ has hue step
$a = \lfloor p / 4 \rfloor$ and lightness step $b = p \bmod 4$; the interleaving
tables $\eta, \lambda, \kappa$ are the leaf palettes' (subspec/trees.md,
Leaves). For every palette but the spring petals, color $j$ is

$$
\text{hue} = h_0 + (h_1 - h_0)\left(0.7\,\eta_j + 0.15\,\frac{a}{3}\right),
\qquad
L = L_0 + \Delta L \cdot \lambda_j + 0.06\,\frac{b}{3},
\qquad
C = C_0 \cdot \kappa_j
$$

| Season | Hue $h_0$–$h_1$ | $L_0$ | $\Delta L$ | $C_0$ |
|---|---|---:|---:|---:|
| Fall (Temperate) | 30–85 | 0.55 | 0.18 | 0.15 |
| Summer | 115–150 | 0.36 | 0.14 | 0.08 |
| Winter (every biome) | 215–280 | 0.84 | 0.08 | 0.03 |
| Desert spring (sage) | 120–155 | 0.60 | 0.14 | 0.06 |
| Desert summer (olive) | 95–125 | 0.52 | 0.14 | 0.06 |
| Desert fall (ochre) | 60–85 | 0.56 | 0.16 | 0.09 |
| Alpine and Tundra fall (dry grass) | 70–100 | 0.42 | 0.16 | 0.07 |

Spring petals (every biome but the desert) are whites with yellows. Color 1 is always yellow, and color 4
is a deeper gold when $b$ is odd:

* yellow: $L = 0.86 + 0.02\lambda_j$, $C = 0.14 + 0.01\lfloor b/2 \rfloor$,
  hue $95 + 4a$ (10° less for color 4),
* white: $L = 0.90 + 0.05\lambda_j$, $C = 0.010 + 0.006a$,
  hue $80 + 40\eta_j + 10b$.

Leaves and petals are narrower when darker, as on the trees: half-width
$0.7 \cdot \text{half-length} \cdot (0.35 + 0.8\beta)$, with $\beta$ the fill's
brightness (subspec/trees.md, Leaves).

Every palette's six colors are distinct, and the 16 palettes of a biome and
season differ, checked by `make verify`.

### Rendering

Plants are sorted back to front (by $h$, highest first; stable), so nearer
plants overlap farther ones. They form one `<g class="vegetation">` group, one
`<path>` per line, drawn right after the front (or only) hill and before the
trees standing on it. Plants are ground cover, so the trunks rise in front of
them.

* **Leaf or petal:** the eye shape centered at the origin,
  `M-a,0Q0,2w a,0Q0,-2w -a,0Z`, placed by an exact `matrix(...)` built from
  the axis direction.
* **Tuft:** five thin blades, one subpath each, from roots $0.12$ of the height
  apart (half-width $0.07$ of the height at the root). Blade heights are 0.6,
  0.85, 1, 0.85, 0.6 of the tuft height, and the tips fan out to
  $(k - 2) \cdot 0.5 \cdot \text{lean} \cdot \text{height}$ for blade
  $k = 0, \dots, 4$.
* **Rosette:** the same five-blade form, broader and lower: roots $0.1$ of the
  height apart, half-width $0.16$ of the height at the root, leaf heights 0.5,
  0.8, 1, 0.8, 0.5, and the same fan-out with its wider lean.

### Verification

`make verify` decodes the namespace from the SVG text:

* The plant count over the anchor count gives the density.
* For every spread and seed, the first plant generated is looked up among the
  drawn plants, ignoring anything its color affects.
* Each candidate is rendered with all 16 palettes and compared with the whole
  group.

Exactly one candidate must match, and its bits must round-trip.

### Dependencies

* Consumes: the theme and season from Global, the front hill's silhouette from Hills, and
  the tree bases and sizes from Trees. Hills and Trees are generated first.
* Provides: the ground cover of the front hill.

### Open decisions

* Plants for Alpine and Tundra: they currently share the Temperate plants.
* Ranges: the plant counts, the radius, the plant sizes, and the depth factor.
