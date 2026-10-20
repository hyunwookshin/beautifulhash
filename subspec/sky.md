## Sky Namespace

| Bit range | Width |
|---|---:|
| 16–39 | 24 |

### Overview

The Sky namespace paints the backdrop: the sky's tint and one cloud. It is the
farthest layer back. A low sun warms both, and the fog sets how clearly the
cloud shows.

### Bit layout

Fields are read most significant bit first, in this order:

| Field | Bits | Meaning |
|---|---:|---|
| top $a$ | 3 | Height of the cloud's top edge |
| height $b$ | 2 | Cloud height |
| width $c$ | 3 | Width of the top edge |
| center $d$ | 3 | Horizontal position |
| bottom $e$ | 2 | Width of the bottom edge, as a fraction of the top |
| skew $f$ | 2 | Shift of the bottom edge |
| lead $\ell$ | 1 | Leading (crisp) side: 0 left, 1 right |
| tint $t$ | 8 | Sky tint |

The first 16 bits shape the cloud, and the last 8 color the sky. Every field
always has an effect, so no bit is ever inactive.

### Cloud

**1. The trapezoid.** The trapezoid has two horizontal edges, and the top one
is always longer. In logical units:

* top edge at $h_T = 1536\,(0.88 - 0.035a)$, centered at
  $x_c = 2048\,(0.18 + 0.64\,d/7)$, width $W = 700 + 110c$;
* height $H = 70 + 30b$, so the cloud is flat, much wider than tall, and the
  bottom edge is at $h_T - H$;
* bottom edge width $W(0.35 + 0.1e)$, at most 65% of the top, centered at
  $x_c + \frac{f - 1.5}{1.5}\,0.2W$.

The bits set position, height, width, and skew; the shape stays a trapezoid
with a longer top. Corners run clockwise from the top left: edges 0 (top),
1 (right), 2 (bottom), 3 (left).

**2. Five points on its boundary.** With perimeter $P$ (each edge measured as
$|\Delta x| + |\Delta h|$), point $i$ lies at perimeter fraction
$(i + 0.2 + 0.6u_i)/5$ from the top-left corner, going clockwise. Here $u_i$ is
the splitmix64 finalizer of $8B + i$, scaled to $[0, 1)$ as for Vegetation, and
$B$ is the 16 cloud bits as one number. So the points stay in order around the
trapezoid, one in each fifth of it.

**3. Refining the outline.** Three rounds turn the 5 points into 625. In each
round, between every pair of neighbors $A$ and $B$, four new points $k = 0..3$
are placed inside a narrow rhombus whose long diagonal is $AB$. The rhombus is
15% of $|AB|$ wide at its middle, tapering to nothing at $A$ and $B$.

* Point $k$ lies at $t = (k + 0.25 + 0.5u)/4$ along $AB$.
* It is offset sideways by $0.15\,(1 - |2t - 1|)(2u' - 1)$ times $AB$ turned a
  quarter.
* $u, u'$ come from the splitmix64 finalizer of a key mixing $B$, the round,
  the pair, and $k$.
* A point that would leave the trapezoid takes the opposite offset, or none if
  that leaves too.

The cloud therefore stays inside the trapezoid, with fine irregular billows at
three scales.

**Smooth outline.** The 625 points are joined by quadratic curves through the
midpoints between neighbors, with each point as the control point.

**4. The gradient starts from the five points.** Each of the five boundary
points, pulled a quarter of the way toward the trapezoid's center, is a glow
source: a radial gradient of radius $0.36W$.

* Sources on the leading half (side $\ell$ of the top edge's middle) start at
  $0.7p$, falling to $0.35p$ at 40% of the radius and to 0 at the rim.
* The other sources start at half that.

So the cloud leads with side $\ell$ and its light comes from several points
rather than one.

**Depth and softness.**

* A very slight shade darkens the cloud toward its bottom: `#3C4A5A`, from
  nothing at 40% of the way down the trapezoid to $0.12p$ at its bottom edge.
* The whole cloud is blurred by a Gaussian of deviation $0.012W$, so its edges
  blend into the sky.

**5. Fog sets visibility.** The peak $p$ follows the fog density $\delta$
(subspec/fog.md):

| $\delta$ | 0 | 1 | 2 | 3 |
|---|---:|---:|---:|---:|
| Peak $p$ | 50% | 28% | 5% | 2% |

Clear air shows a wispy cloud. The two foggiest levels leave it effectively
gone. Its outline, sources, and shade remain in the SVG, so the cloud bits
still decode from the text.

**Color.** The cloud is white under a high sun. A low sun lights it pink and
gold (subspec/celestial.md, Sky warmth).

### Tint

The sky gradient runs from a tint at the top to `#E9EEF2` at the horizon, with
stops at 0, 0.45, 0.75, and 1 interpolated in sRGB. The 256 tints are light
blues in OKLCH, indexed by $t$ = lightness (2 bits) | hue (3 bits) |
chroma (3 bits):

$$
L = 0.70 + 0.035\,l, \qquad C = 0.03 + 0.007\,c, \qquad h = 212° + 6°\,k
$$

A low sun then warms the gradient's hue (subspec/celestial.md, Sky warmth).

### Rendering

`<defs>` holds the sky gradient (`id="sky"`), the five glow gradients
(`id="cloud-0"` to `cloud-4`), the shade (`id="cloud-shade"`), the blur
(`<filter id="cloud-soft">`), and a `<clipPath id="cloud-clip">` with the
outline. The sky rectangle and any sun glow come first. Then comes one
`<g class="cloud">` with the blur, holding a group clipped to the outline with
the five glows as `<circle>`s and the shade as a `<rect>`, behind every ridge and the fog.

### Verification

`make verify` checks two things:

* Every tint gives a distinct gradient under every sun step.
* For random hashes, it finds the 16 cloud bits by trying all 65,536 against
  the cloud's outline, glows, and gradients, and the tint by trying all 256 against the
  sky gradient. The fog and sun come from their own namespaces. Exactly one
  candidate must match each, and the bits must round-trip.

### Dependencies

* Consumes: the fog density from Fog, and the sun from Celestial (for warmth).
* Provides: the backdrop.

### Open decisions

* More than one cloud, or cloud types by biome.
* Ranges: the trapezoid's sizes, the opacity stops, and the tint grid.
