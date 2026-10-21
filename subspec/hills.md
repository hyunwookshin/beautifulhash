## Hills Namespace

| Bit range | Width |
|---|---:|
| 136–175 | 40 |

### Overview

The Hills namespace shapes the low midground between the mountains and the
foreground: one hill, or a front hill and a back hill. Each hill is a smooth wave
defined by three hash-derived points. Hills give the scene a calm base and are the
ground that Trees and Vegetation will later plant on.

Hills have no edge texture: no tree silhouettes or fractal detail. Trees on hills
belong to the Trees namespace.

### Hill shape

A hill is defined by three points:

* the **left edge** $(0, y_L)$, where the hill meets the left side of the canvas,
* the **crest** $(x_c, y_c)$, its single peak,
* the **right edge** $(W, y_R)$, where it meets the right side.

Each side of the crest follows a profile $P_n(u)$, where $u$ runs from 0 at the
crest to 1 at the edge:

$$
y(x) = y_{\text{edge}} + (y_c - y_{\text{edge}}) \cdot P_n(u)
$$

| $n$ | $P_n(u)$ | Shape |
|---:|---|---|
| 1 | $1 - u$ | Tent: straight flanks |
| 2 | $1 - u^2$ | Dome: rounded, circular feel |
| 3 | $1 - u^2(3 - 2u)$ | Sigmoid: flat crest and flat foot |
| 4 | $1 - u^2(3 - 2u^2)$ | W: dips 12.5% of the hill's height below the edge level before rising to it |

Every profile is 1 at the crest and 0 at the edge, and no profile exceeds 1, so
the crest is always the hill's highest point. The exponent stops at 4. Two bits
choose among the four shapes, so every bit value gives a distinct shape.

Each side is sampled at 32 uniform steps in $x$, giving 65 points; vertex 32 is
the crest. All arithmetic is `+`, `-`, `*`, and `/`.

### Heights and placement

All heights are in logical units above the canvas bottom.

* **The back hill never exceeds 15% of the canvas height:** the back (or only)
  hill's crest is at most $0.15 \times 1536 = 230.4$.
* **The front hill stays lower:** its crest lies in $[40, 150]$.
* **The back hill is never hidden:** its crest is mapped into an interval that
  starts 30 above the front hill's height at the same $x$:

$$
y_c^{\text{back}} \in \left[\,F(x_c^{\text{back}}) + 30,\; 230.4\,\right]
$$

  where $F$ is the front hill's curve. This is the same no-clamp rule as
  Mountains: the interval is computed first, then the bits are mapped linearly
  into it. Because the front hill never exceeds 150, the interval is at least
  $230.4 - 180 = 50.4$ wide.
* **One-hill mode:** the crest lies in $[60, 230.4]$.
* **Edges are always below the crest:** each edge height is a fraction of the
  crest height, $y_{\text{edge}} = y_c \cdot f$ with $f \in [0.15, 0.80]$. With
  $f \ge 0.15$, even a W dip stays above the canvas bottom.
* **Crest position:** $x_c \in [0.15W, 0.85W]$, so crests are never at an edge.

**Hills never hide mountain silhouettes.** Every visible mountain silhouette
point lies at or above the ground level $G = 260$ (subspec/mountains.md, Ridge
placement): Ridge 1 never dips below it, and farther ridges below Ridge 1 are
hidden behind it. $G$ is above the 230.4 hill cap.

### Bit layouts (provisional)

A 1-bit header (namespace bit 0, absolute bit 136) selects one or two hills. Each
hill's fields are read in this order: crest position, crest height, left edge,
right edge, shape, color. In two-hill mode the front hill comes first, because
the back hill's crest interval depends on it.

**One-hill mode (header = 0):**

| Field | Bits | Namespace bits | Absolute bits |
|---|---:|---|---|
| Crest position | 8 | 1–8 | 137–144 |
| Crest height | 7 | 9–15 | 145–151 |
| Left edge | 7 | 16–22 | 152–158 |
| Right edge | 7 | 23–29 | 159–165 |
| Shape | 2 | 30–31 | 166–167 |
| Color | 8 | 32–39 | 168–175 |
| **Total** | **39** | | |

**Two-hill mode (header = 1):**

| Field | Front bits | Front namespace / absolute | Back bits | Back namespace / absolute |
|---|---:|---|---:|---|
| Crest position | 4 | 1–4 / 137–140 | 4 | 20–23 / 156–159 |
| Crest height | 4 | 5–8 / 141–144 | 4 | 24–27 / 160–163 |
| Left edge | 4 | 9–12 / 145–148 | 3 | 28–30 / 164–166 |
| Right edge | 3 | 13–15 / 149–151 | 3 | 31–33 / 167–169 |
| Shape | 2 | 16–17 / 152–153 | 2 | 34–35 / 170–171 |
| Color | 2 | 18–19 / 154–155 | 4 | 36–39 / 172–175 |
| **Total** | **19** | | **20** | |

1 + 39 = 1 + 19 + 20 = 40. Every bit is read directly as an unsigned integer.

### Color

Hill colors are thematic families, realized as palettes of distinct shades and
selected by the color bits. They follow the biome and season
(SPECIFICATIONS.md, Seasons):

| Season | Family |
|---|---|
| Spring, summer, fall | Green, tinted by biome: Alpine muted sage green (130–160°, chroma ×0.7), Temperate olive to moss green (95–150°, chroma ×0.75), Tundra muted olive-green (95–135°, chroma ×0.7), Desert olive (70–125°, chroma ×0.9) |
| Winter | Snowy tints in every biome: pale blue-whites through lavender-whites (220–290°, chroma ×0.5), very light and low in chroma. Hues below 220° read as pure cyan and are avoided, still varied by the color bits |

| Palette | Entries | Lightness (non-winter) | Lightness (winter) |
|---|---:|---|---|
| Single hill | 256 (8 lightness × 8 hue × 4 chroma) | 0.30–0.46 | 0.80–0.94 |
| Front hill | 4 (2 × 2) | 0.26–0.34 | 0.79–0.85 |
| Back hill | 16 (4 × 4) | 0.36–0.46 | 0.87–0.94 |

Every palette of every biome and season serializes to distinct `#RRGGBB` values,
checked by `make verify`. Tuning found collisions in the narrow Temperate and
Desert green ranges at low lightness; the ranges above are collision-free, and
the Temperate range was confirmed by a parameter search to have collision-free
neighbors.

### Shading bands

The front (or only) hill is shaded in discrete bands, lighter at the top and
darker toward the viewer, so it reads as a lit, rounded surface rather than a
flat fill. The bands carry no Hills bits: they follow from the hill's shape and
color, lit by the implicit sun (subspec/celestial.md). The back hill is not
banded.

* **Count:** 5 bands when the hill's mean height over its 65 points is at least
  80, otherwise 3.
* **Boundaries:** with $r = j / n$ and the sun's spacing $\beta$ and tilt
  $\tau$, boundary $j = 0, \dots, n - 1$ lies at depth
  $f_j = r(1 - \beta) + r^2\beta$ on the ground surface $H$ (subspec/celestial.md,
  The ground surface), tilted across the canvas:

  $$
  B_j(x) = H(x, f_j)\left(1 + \tau f_j \left(\frac{2x}{2048} - 1\right)\right)
  $$

  Boundary 0 is the silhouette. Deeper boundaries follow the crest's contour
  but flatten toward $S_{\min}$ and drop toward the canvas bottom, so they have
  less and less curvature toward the front, which gives the 3D effect. The
  tilt lowers them on the side the sun lights. Since $H(x, f) \le (1 - f)S(x)$
  and $|\tau| \le 0.15$, $B_j(x) \le (1 - f)(1 + 0.15f)\,S(x) \le S(x)$: no
  boundary rises above the silhouette. Since $|\beta| \le 0.25$, the depths
  increase with $j$.
* **Grass teeth:** no boundary is a smooth line; each becomes a row of teeth,
  `\/\/\/`. With tooth spacing $w$ and height $a$, the valleys lie on the
  boundary at $x = kw$, and the peaks halfway between rise by $a\,m_k$. The
  multiplier $m_k = 0.3 + 0.1\,p_{k \bmod 6}$ follows the repeating, irregular
  pattern $p = (2, 3, 0, 1, 4, 7)$, so the teeth are uneven but not random.
  Heights between points are read linearly off the boundary, and the row ends
  with a point at $x = 2048$.
  * **Crest** (boundary 0): the finest teeth, $w = 2$ and $a = 0.8$ logical
    units, under half a pixel at 1024 wide. They show when zoomed in and
    vanish at full view. They rise above the silhouette and belong to the top
    band only, so the hill's own path is unchanged.
  * **Inner boundaries** ($j \ge 1$): grass that grows toward the viewer with
    the vegetation's depth scale $d = 0.35 + 1.65 f_j$ (subspec/vegetation.md,
    Size): $w = 4d$ and $a = 5d$. Peaks are capped at the silhouette.
* **Colors:** band $j$ moves each channel $c$ of the hill's color by
  $k = 0.08\,(\lfloor (n - 1)/2 \rfloor - j)$: to
  $\operatorname{round}(c + (255 - c)k)$ toward white for $k > 0$, or
  $\operatorname{round}(c(1 + k))$ toward black otherwise. With 5 bands that is
  +16%, +8%, 0, −8%, −16%; with 3 bands, +8%, 0, −8%. The middle band is the
  palette color itself.
* **Rendering:** each band is a `<path class="hill-band">` from its boundary
  closed down to the canvas bottom, drawn top band first right after the hill's
  own path, so each deeper band covers the lower part of the one before. The
  hill's path keeps the palette color, so its bits still decode from it.

### Grass blades

In Alpine, Temperate, and Tundra, the front (or only) hill is covered by small
grass tufts over its bands, so it reads as a grassy slope rather than a flat
fill. A winter hill is clean, flat snow, so in every biome it keeps only a
sprinkle of one tuft in ten (see One tuft). Desert sand gets tufts only then,
as the winter sprinkle. The back hill gets none. The tufts carry no bits: they
follow from the hill, its bands, the sun, and the biome and season. They share their templates' coloring and rendering with the front ridge's
canopy (subspec/mountains.md, Front-ridge canopy).

**Rows.** Tufts stand in rows on the ground surface $H$ (subspec/celestial.md,
The ground surface), from the silhouette ($f = 0$) down to the canvas bottom.

* Row size $\sigma = 12\,(0.75 + 0.25f)$ logical units: smaller toward the
  crest, but only mildly, since the hill is steep and near.
* Spacing along the row $0.5\sigma$; odd rows start half a spacing in.
* The next row lies at depth $f + 0.5\sigma / \max(g, 0.25\bar S)$, where
  $g = 2(1 - f)\bar S - (1 - 2f)S_{\min}$ is the surface's slope at the hill's
  mean height $\bar S$. So each row is about half a tuft below the last, and
  rows overlap.

**One tuft.** At grid position $x_0$ in a row at depth $f$, the key is
$\kappa = \lfloor 7x_0 + 9973f \rfloor$. The tuft stands at
$x = x_0 + 0.125\,(\kappa \bmod 5 - 2)\,s_p$ (with $s_p$ the spacing), on the
row's tilted band surface $H(x, f)\,(1 + \tau f(2x/2048 - 1))$, raised or
lowered by $0.12\sigma\,(\lfloor\kappa/25\rfloor \bmod 3 - 1)$ so rows are
uneven. Its height is $z = \sigma\,(0.8 + 0.1\,(\lfloor\kappa/5\rfloor \bmod 5))$.
It is three blades in one 7-point outline, all leaning by
$0.1z\,(\lfloor\kappa/9\rfloor \bmod 3 - 1)$: a middle blade to the full
height, and side blades to $0.75z$ and $0.7z$. In winter, only the tufts with
$\lfloor\kappa/7\rfloor \bmod 10 = 0$ are drawn.

**Color.** A tuft takes the color of the band its base stands in (the deepest
band whose boundary is at or above the base) and moves it by its step, as for
the canopy (subspec/mountains.md, Front-ridge canopy, Color), Temperate fall
accents included. The hill has no sunlit faces.

**Rendering.** Row by row from the crest down, each row's tufts of one color
form one `<path class="canopy">`. The tufts are drawn after the bands and
before the shadows, vegetation, and trees.

### Rendering and order

* Hills are drawn after every mountain ridge and before Trees and Vegetation:
  back hill, then front hill. Each is a `<path class="hill">` closed down to the
  canvas bottom.
* The front hill's shading bands follow its path (see Shading bands).
* Its grass blades follow the bands (see Grass blades).
* A fog layer is drawn between the mountains and the hills, and in two-hill
  mode another between the back hill (with its trees) and the front hill
  ([Fog](fog.md)). The front hill is never fogged.

### Advertised profile

Each hill publishes its profile as context for the namespaces that plant on it:

| Field | Meaning |
|---|---|
| `crest_x`, `crest_h` | The crest point |
| `left_h`, `right_h` | Heights where the hill meets the left and right canvas edges |
| `exponent` | Profile $n \in \{1, 2, 3, 4\}$ |

Together these determine the hill's curve $y(x)$ exactly, so Trees and
Vegetation can place objects on a hill's surface without reading Hills bits.
Hills must therefore be generated before Trees and Vegetation.

### Injectivity

For a fixed algorithm version, theme, season, and target resolution, the map
from the 40 Hills bits to the hills' SVG output is injective:

* **Header:** one or two hill paths.
* **Per hill, front first:** the crest vertex gives the crest position and
  height; the first and last vertices give the edge heights; the samples give
  the shape; the fill gives the color index. Each field maps strictly
  monotonically, or by a distinct profile, onto its output. The back hill's
  crest interval depends only on the already decoded front hill.

Hill coordinates are absolute (no offset), so there is no Step 6 halving.
Worst-case steps at 1024 × 768:

| Field | One-hill mode | Two-hill mode (front / back) |
|---|---:|---|
| Crest position | 2.81 px | 47.8 px / 47.8 px |
| Crest height | 0.67 px | 3.67 px / 1.68 px |
| Edge height | 0.15 px | 0.87–1.86 px / 1.49 px |
| Shape | 0.56 px | larger |

The smallest is the one-hill edge (0.15 px), above the 0.01 quantum. The shape
bound uses the closest profile pair, tent versus sigmoid, which differ by about
0.094 at $u = 0.25$ and $u = 0.75$, both of which are sample points.

### Verification

`make verify` checks:

* both layouts consume exactly 40 bits, and decode followed by encode returns
  the original bits
* $150 + 30 < 230.4$ (the back hill's interval is never empty) and $G = 260 > 230.4$
* every hill palette is distinct, for every biome and season
* a round trip, for every biome and season, from hash to SVG text to bits

During the round trip it also checks:

* the back hill never exceeds 230.4
* the front hill never exceeds 150
* no point goes below the canvas bottom
* the back crest clears the front hill by at least 30

Latest run: 400 hashes for each of the 16 biome and season pairs. 0
mismatches, smallest decode margin 0.144 px, highest back hill exactly 230.4,
lowest point 1.77.

### Dependencies

* Consumes: the theme and season from Global.
* Provides: hill profiles, used by Trees and Vegetation to plant on hills, and
  hill silhouettes and depths, used by Fog and for occlusion.

### Open decisions

* Ranges: the 15% cap, the front hill range (40–150), the 30 exposure, edge
  fractions (0.15–0.80), and crest positions (0.15–0.85).
* Green and snowy family hue ranges and lightness bands. The darkest front-hill
  greens render nearly black.
* How Trees and Vegetation use the advertised profiles.

### Status

The hill shape, placement, bit layouts, colors, and advertised profile are
proposed and implemented in the reference renderer.
