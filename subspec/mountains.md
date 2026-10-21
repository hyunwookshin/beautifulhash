## Mountains Namespace

| Bit range | Width |
|---|---:|
| 48–135 | 88 |

### Overview

The Mountains namespace draws the mountain ridges that form the distant
skyline: two or three hash-derived silhouettes, each with its own color, with
taller and more distant ridges set further back. Ridge 1 is the nearest. The
foreground is calm and detailed, and ridges become gently peaked with distance.
Together the ridges should read as one coherent mountain range rather than
unrelated shapes.

It owns the largest share of the hash because ridge silhouettes and colors carry
much of the landscape's character. BeautifulHash is an art generator, not just a
geometry generator, so color receives a meaningful share of the budget.

Mountains is self-contained: it decides its own ridge count from its own bits.
Global supplies the theme and broad composition constraints, but not the
number of ridges.

### Configurations

A 1-bit header at namespace bit 0 (absolute bit 48) selects one of two
independent decoding layouts for the remaining 87 payload bits:

```text
Mountains namespace (88 bits)
│
├── Header (1 bit)
│   └── 0 = two-ridge mode, 1 = three-ridge mode
│
└── Payload (87 bits), decoded by the selected layout
    ├── Two-ridge mode:   Ridge 1 (43) + Ridge 2 (44)
    └── Three-ridge mode: Ridge 1 (32) + Ridge 2 (32) + Ridge 3 (23)
```

| Mode | Header | Ridge 1 | Ridge 2 | Ridge 3 | Total |
|---|---:|---:|---:|---:|---:|
| Two-ridge | 1 | 43 | 44 | — | 88 |
| Three-ridge | 1 | 32 | 32 | 23 | 88 |

Each layout is designed on its own terms. The two-ridge layout is not the
three-ridge layout with leftover bits redistributed; it uses its larger budget
for a deeper subdivision.

Rules that apply to both modes:

* **All 88 bits are consumed.** Every bit is assigned to exactly one slot: the
  header, one point, or one color.
* **No rehashing.** Payload bits are decoded directly, as unsigned integers read
  most significant bit first. They are never hashed, fed through a PRNG, or
  otherwise mixed before decoding.
* **No hidden ridges.** In two-ridge mode, Ridge 3 does not exist in the scene
  or the SVG. There is no empty or placeholder ridge.
* **Strict ownership.** Mountains reads only bits 48–135.

These layouts are provisional until sample landscapes have been rendered and reviewed.

### Ridge generation: constrained midpoint displacement

Every ridge is built the same way: a three-point **skeleton** from its first bit
level, then recursive constrained midpoint displacement for the remaining bit
levels, then bit-free detail levels. What differs per ridge is its **style**,
chosen by its depth role (see Depth roles and ridge styles): which skeleton
generator it uses, its roughness, its vertical scale, and its total depth.

The number of peaks is not a parameter; peaks emerge from the skeleton and the
displacements. Coarse levels set the major shape, and each deeper level adds
finer detail.

#### Depth roles and ridge styles

Each ridge has a depth role: **front** (Ridge 1), **back** (the last ridge:
Ridge 2 in two-ridge mode, Ridge 3 in three-ridge mode), or **middle** (Ridge 2
in three-ridge mode). The role selects the ridge's style:

| Role | Skeleton generator | Roughness $r$ | Vertical scale $s$ | Total depth | Points |
|---|---|---:|---:|---:|---:|
| Front | midpoint | 0.100 | 1.372 | 5 | 33 |
| Middle | summit | 0.125 | 1.070 | 6 | 65 |
| Back | summit | 0.150 | 0.930 | 6 | 65 |

The intent is a calm, detailed foreground and a gently peaked distance:

* **Front:** the midpoint skeleton with low roughness gives rolling, detailed
  terrain. Horn-like peaks in the foreground read as volcanic, so they are
  avoided here.
* **Middle and back:** the summit skeleton gives each ridge a recognizable
  summit with shoulders. Roughness rises gradually with distance, so peaks get
  slightly sharper further back without becoming obvious.
* **Vertical scales** bring every role's exact worst-case silhouette height to
  about 600–680 logical units. Worst-case height grows roughly linearly with $r$, so
  equalizing it keeps rougher ridges from overflowing the canvas budget.
  Typical heights are about half the worst case for every style.

The roughness values were chosen by comparing rendered galleries on identical
hashes. Above about $r = 0.2$, perpendicular displacement produces terraced,
mesa-like silhouettes rather than peaks, because a large perpendicular move on
a sloped segment shifts the midpoint sideways almost as much as upward.

#### Coordinate spaces

Each ridge's shape $Q$ is generated in a normalized, anchor-relative profile space:

* $x$ runs from $0$ to $W = 2048$ (the logical canvas width).
* $h$ is elevation relative to the ridge's left anchor, which sits at $h = 0$.

Generation of the shape happens entirely in profile space. The theme's vertical
scale and the ridge's placement in the scene are applied only afterward (see
Roughness and vertical scale, and Ridge placement).

#### Anchors and skeleton

The **left anchor** slot never affects the shape. Its bits choose the ridge's
extra elevation: how far the whole ridge is raised above its minimum visible
placement, within the headroom available on the canvas (see Ridge placement).

The **right anchor** (tilt) and **Level 1** slots are adjacent in bit order.
Together they build the skeleton: the left edge at $(0, 0)$, one interior point,
and the right edge at $x = W$. How they are read depends on the generator.

**Midpoint skeleton (front role).** The right anchor's $b$ bits are read as
$v \in [0, 2^b - 1]$ and mapped to a tilt $d \in [-D, D]$ (proposed $D = 256$):

$$
d = D \left( \frac{2v}{2^b - 1} - 1 \right)
$$

The skeleton is $(0, 0)$, the Level 1 midpoint of the chord to $(W, d)$
(placed by the subdivision step below), and $(W, d)$.

**Summit skeleton (middle and back roles).** The tilt and Level 1 bits are read
together, most significant bit first, as three fields:

```
[ x (3 bits) | rise (remaining bits) | drop (3 bits) ]
```

* **x** places the summit at
  $x_s = W \left( x_{\min} + (x_{\max} - x_{\min}) \frac{v_x}{2^3 - 1} \right)$,
  with $x_{\min} = 0.2$ and $x_{\max} = 0.8$. Summits are never centered by default,
  which gives asymmetry.
* **rise** sets the summit's height above the left edge,
  $h_s = P_{\min} + (P_{\max} - P_{\min}) \frac{v_{\text{rise}}}{2^b - 1}$.
* **drop** sets how far the right edge lies below the summit, over the same
  range $[P_{\min}, P_{\max}] = [160, 440]$.

The skeleton is $(0, 0)$, $(x_s, h_s)$, and $(W, h_s - \text{drop})$. The summit
is always higher than both edges, so every middle and back ridge has a dominant
peak before any detail is added. Rise has 3 bits for the middle ridge in
three-ridge mode (9 bits in total) and 4 bits otherwise (10 bits in total).

This is a different reading of the same bits, not new bits: the bit layouts,
decoding, and bit coverage are unchanged. Every field is a bijection of its own
bits, so no information is lost.

#### Subdivision constraints

For parent points $A$ and $B$, every new subanchor $P$ must satisfy two constraints:

1. **Bounded deviation:** $P$ stays close to the segment $\overline{AB}$:

$$
\operatorname{distance}(P, \overline{AB}) \le r \, \lVert B - A \rVert
$$

2. **Left-to-right monotonicity:** $P$ lies strictly between its parents in $x$:

$$
A_x < P_x < B_x
$$

$r$ is the roughness, default $0.1$ (the 10% rule).

Every child segment ($AP$ and $PB$) inherits both constraints at the next level,
so by induction the whole silhouette stays strictly left-to-right monotonic at
any depth. No overhangs or protruding ledges are possible.

Monotonicity is a geometric correctness constraint. Aesthetic constraints, such
as a cap on slope steepness, are deliberately left out until rendered samples
show whether they are needed.

#### Subdivision step

Points are never clamped after the fact. Clamping would map different bit
patterns to the same coordinates and destroy injectivity. Instead, the allowed
displacement interval is computed first, and the bits are mapped injectively
into it.

With $\Delta = B - A = (\Delta_x, \Delta_h)$, where $\Delta_x > 0$ by induction:

1. Compute the midpoint $M = \frac{A + B}{2}$.
2. Displacement is perpendicular to $AB$, parameterized as

$$
P(t) = M + r\,t\,\left(-\Delta_h,\; \Delta_x\right)
$$

   This moves $M$ a distance $r|t|\lVert B - A \rVert$ from the segment, so
   constraint 1 is exactly $|t| \le 1$. The segment length cancels out, so no
   square root is needed.
3. Constraint 2 requires $|P_x - M_x| = r|t||\Delta_h| < \frac{\Delta_x}{2}$.
   Compute the allowed interval $t \in [-t_{\max}, t_{\max}]$ with

$$
t_{\max} =
\begin{cases}
1 & \text{if } \Delta_h = 0 \\[4pt]
\min\!\left(1,\; \dfrac{\gamma\,\Delta_x}{2r\,|\Delta_h|}\right) & \text{otherwise}
\end{cases}
$$

   where $\gamma \in (0, 1)$ is a fixed monotonicity margin. Proposed: $\gamma = 0.5$.
   The margin turns the strict inequality into a guaranteed gap, which the
   separation bounds below rely on.
4. Read the $b$ bits allocated to this subanchor as $v \in [0, 2^b - 1]$ and map
   them linearly onto the interval:

$$
t = t_{\max} \left( \frac{2v}{2^b - 1} - 1 \right)
$$

   Since $t_{\max} > 0$, this is strictly increasing in $v$. A 1-bit subanchor
   always moves to one of the two extremes, $\pm t_{\max}$.
5. Replace $AB$ with $AP$ and $PB$, and continue with the next level.

Level $k$ displaces $2^{k-1}$ midpoints, ordered left to right. Point $Lk.j$ is
the $j$-th midpoint of Level $k$, counting from the left.

With the defaults, $t_{\max} < 1$ only when $|\Delta_h| / \Delta_x > \gamma / (2r) = 2.5$,
that is, on segments steeper than about $68^\circ$. Everywhere else the plain
10% rule applies unchanged.

#### Minimum segment width

With the margin, $|P_x - M_x| \le \gamma \frac{\Delta_x}{2}$ in both cases of $t_{\max}$:

* If $t_{\max} = 1$, then $r|\Delta_h| \le \gamma\Delta_x / 2$ by the definition of the minimum, so $r|t||\Delta_h| \le \gamma\Delta_x/2$.
* Otherwise $r\,t_{\max}|\Delta_h| = \gamma\Delta_x/2$ exactly.

So each child's $x$-extent satisfies

$$
\Delta x_{\text{child}} \ge \frac{\Delta_x}{2} - \frac{\gamma\Delta_x}{2} = \frac{1 - \gamma}{2} \, \Delta x_{\text{parent}}
$$

With $\gamma = 0.5$ the factor is $\frac14$. Starting from the anchor chord
($\Delta x = 2048$), the guaranteed minimum width of the segments subdivided at each level is:

| Segments subdivided at | Minimum $\Delta x$ |
|---|---:|
| Level 1 | 2048 |
| Level 2 | 2048 × ¼ = 512 |
| Level 3 | 512 × ¼ = 128 |
| Level 4 | 128 × ¼ = 32 |
| Final (after Level 4) | 32 × ¼ = 8 |

For the summit skeleton, the summit sits at least -e.2W either edge, so
the segments subdivided at Level 2 are at least .6, then .4 .6
and .4 the following levels.

This holds for every hash and does not depend on slope, so deeper layouts never
risk folding.

#### Roughness and vertical scale

Two independent theme parameters control the profile:

* **Roughness $r$:** how far each subanchor may move relative to its segment.
  It controls how jagged the ridge is at every scale. One value applies to every
  level of a ridge; it varies only by depth role.
* **Vertical scale $s$:** applied after generation, mapping each profile point
  $(x, h)$ to $(x, s \cdot h)$. It controls how tall the ridge is without
  changing its fractal character.

Vertical scaling leaves every $x$ unchanged, so it cannot break monotonicity.

#### Detail levels (bit-free resolution)

Nearer ridges should look more detailed than farther ones, regardless of how
many bits each ridge has, and distant ridges should look rugged. So resolution
is set deterministically by depth role, not by the bit budget. After its
bit-driven levels, each ridge is subdivided further by **detail levels** that
consume no bits, until it reaches its total depth:

| Role | Total depth | Points | Detail kind |
|---|---:|---:|---|
| Front | 5 | 33 | smooth (concave) |
| Middle | 6 | 65 | jagged (bit-reusing) |
| Back | 6 | 65 | jagged (bit-reusing) |

For example, two-ridge Ridge 1 has 4 bit levels plus 1 detail level,
three-ridge Ridge 1 has 3 bit levels plus 2, and three-ridge Ridge 3 has 2 bit
levels plus 4. The front ridge gets further vertices from its biome tree
texture (see Biome styling).

A detail point uses the same perpendicular displacement and interval rule as a
bit-driven point. Only its $t$ differs, and there are two kinds.

**Smooth detail (front role).** $t$ is a fixed function of the segment's slope:

$$
u = k \left| \frac{\Delta_h}{\Delta_x} \right|,
\qquad
t = -\,t_{\max} \cdot \frac{u}{1 + u}
$$

where $k$ is the detail strength (proposed $2$). $t$ always lies in
$(-t_{\max}, 0]$, so the midpoint moves just below the chord on both rising and
falling segments. Steep faces become slightly concave, like eroded slopes, and
flat segments are left unchanged.

**Jagged detail (middle and back roles).** Every segment after the last bit
level descends from one last-level bit point $P_i$: it is the left or right
child of the segment that $P_i$ subdivided. Its detail reuses that point's
decoded value $v_i$. At detail level $m$ ($m = 0, 1, \ldots$), the $q$-th midpoint
of the subtree gets

$$
t = t_{\max} \cdot \lambda^m \cdot \big(f + (1 - f)\, r_1\big) \cdot \operatorname{sign}(r_2)
$$

where $\lambda = 0.75$ is the decay, $f = 0.35$ is the floor, and $r_1, r_2 \in [0, 1)$
are a fixed integer mix of the key $(i, v_i, \text{side})$ and the midpoint's
position $(m, q)$; $\operatorname{sign}(r_2)$ is $-1$ below $0.5$ and $+1$ otherwise. Every
level displaces by at least a fraction $f$ of its allowed interval, so the
silhouette stays rugged, and the amplitude shrinks with depth like the
bit-driven levels. Because the key contains $v_i$, different hashes give
different jag patterns without consuming any extra bits. The bits themselves
are still decoded directly; this only expands values after decoding.

Both kinds use only `+`, `-`, `*`, `/`, `abs`, comparisons, and integer
arithmetic.

Properties:

* **No information, no new entropy:** detail points are a fixed function of the
  bit-driven points and their decoded values, so they cannot affect
  injectivity. Decoding reads only the bit-driven points, which sit at known
  indices: the $j$-th point of bit level $k$ is vertex $(2j + 1) \cdot 2^{D - k}$
  of a ridge with total depth $D$.
* **Monotonic:** detail points obey the same interval rule, so every segment
  keeps the guaranteed minimum width.
* **Exact canvas budget:** each last-level point's detail depends only on its
  own value and its two parents, so the exhaustive height search still solves
  the last level directly, detail included. Smooth detail never changes the
  exact maximum heights. Jagged detail raises them slightly (see Canvas bounds).

### Ridge placement (height hierarchy)

Ridge heights are relative, not independent. Each ridge keeps its own
hash-generated shape, but is positioned vertically relative to the ridges in
front of it:

* **Ridge 1 (foreground):** the lowest overall silhouette.
* **Ridge 2 (middle):** placed relative to Ridge 1.
* **Ridge 3 (background, when present):** placed relative to Ridges 1 and 2.

The rule is a **minimum visibility constraint**, not a strict stacking order.
A background ridge must rise above the ridges in front of it *somewhere*, but
not everywhere. This avoids three neatly stacked bands, and guarantees that no
ridge is entirely hidden.

#### Placement is a rigid vertical translation

A ridge is placed by adding one vertical offset to every one of its points.
This preserves its geometry exactly: no control point is clamped, moved
individually, or reshaped. For ridge $i$ with scaled shape $s \cdot Q_i$:

$$
\text{output}_i(x) = s \cdot Q_i(x) + \underbrace{T_i + e_i}_{\text{offset}_i}
$$

* $T_i$ is the minimum placement offset, defined below.
* $e_i \ge 0$ is the ridge's extra elevation in output units, decoded from its
  left anchor (see Canvas bounds).

All elevations are in logical units, measured upward from the bottom of the
canvas. Ridges are placed front to back: Ridge 1, then Ridge 2, then Ridge 3.

#### Minimum placement offset

**Ridge 1** has nothing in front of it. It is placed so that its lowest point
sits at the ground level $G$ (proposed $260$), supplied by the theme and Global's
composition settings:

$$
T_1 = G - \min_x s Q_1(x)
$$

Every visible mountain silhouette therefore lies at or above $G$: Ridge 1's
lowest point is at least $G$, and any part of a farther ridge below Ridge 1 is
hidden behind it. Since $G$ is above the hill cap of 230.4 (subspec/hills.md),
hills never hide a mountain silhouette.

**Ridges 2 and 3.** Let $F(x)$ be the upper envelope of all ridges already
placed in front, at their final output positions:

$$
F(x) = \max_{j < i} \text{output}_j(x)
$$

Let $H_i = \max_x s Q_i(x) - \min_x s Q_i(x)$ be the ridge's silhouette height.
The required exposure is

$$
E_i = \max\!\left(\rho \, H_i,\; E_{\min}\right)
$$

where $\rho$ is the visibility ratio (proposed $0.35$, so at least 35% of the
ridge's own height is exposed) and $E_{\min}$ is an absolute floor (proposed
$48$), so a nearly flat ridge still shows clearly. The floor is at least twice the
tallest biome texture (about 22), so even behind tree tips at least half of it
stays visible (see Biome styling). 35% was chosen from rendered
samples: 20% leaves background ridges as thin slivers, and 50% starts to stack
three-ridge scenes into flat bands.

$T_i$ is the offset at which the ridge, with zero extra elevation, is exposed by
exactly $E_i$:

$$
T_i = E_i - \max_x \left( s Q_i(x) - F(x) \right)
$$

With extra elevation $e_i \ge 0$, the actual exposure is $E_i + e_i \ge E_i$.
So every ridge is always visible by at least the required amount, and the
anchor bits raise it further.

The maximum of $s Q_i(x) - F(x)$ is reached at a vertex of $Q_i$ or of one of the
front ridges, because all of them are piecewise linear. It is computed by
evaluating the difference at every such $x$, using linear interpolation between
vertices. This uses only `+`, `-`, `*`, `/`, and `max`.

#### Canvas bounds

Relative placement stacks ridges, so without a bound the background ridges
could be pushed off the top of the canvas. With a fixed extra-elevation range
of 512 per ridge, three ridges could add 1536 of extra elevation alone, the full
canvas height. Clamping would destroy injectivity, so instead the extra
elevation is mapped into an allowed interval computed first, the same way
displacements are.

Let $C$ be the ceiling (proposed $1472$, leaving a margin of 64 below the top
edge). No silhouette point may rise above it.

**Static reserves.** Let $\hat H_i$ be the exact maximum silhouette height ridge
$i$ can have over all of its shape bits, found by exhaustive enumeration (see
Verification). Each ridge behind reserves the most exposure it could ever need:

$$
\text{reserve}_i = \max\!\left(\rho \hat H_i,\; E_{\min}\right)
$$

**Headroom.** Each ridge's headroom $R_i$ is what remains below the ceiling after
its minimum placement and after the reserves of every ridge behind it:

$$
R_1 = C - (G + H_1) - \sum_{k > 1} \text{reserve}_k
\qquad
R_i = C - \max_{j < i} \text{top}_j - E_i - \sum_{k > i} \text{reserve}_k
$$

where $\text{top}_j$ is the highest point of ridge $j$ as placed.

**Extra elevation.** The left anchor's $b$ bits are read as $v \in [0, 2^b - 1]$
and mapped into a share $\kappa_i$ of the headroom. Each ridge has its own share
(proposed $\kappa = (0.25, 0.5, 0.5)$, front to back). The smaller front share keeps
the front ridge low, leaving more room for the ridges behind it.

$$
e_i = \kappa_i R_i \cdot \frac{v}{2^b - 1}
$$

**Guarantees.** Two facts follow from the definitions:

1. $\text{top}_i \le \max_{j<i}\text{top}_j + E_i + e_i$, because the ridge is
   placed to be exposed by exactly $E_i + e_i$ at its most exposed point. With
   $e_i \le \kappa_i R_i$, every ridge stays below the ceiling:
   $\text{top}_i \le C - \sum_{k>i}\text{reserve}_k - (1 - \kappa_i) R_i \le C$.
2. Because $\text{reserve}_i \ge E_i$, each ridge's headroom is at least
   $(1 - \kappa_j)$ times the headroom of each ridge $j$ in front of it, so

$$
R_i \ge S \prod_{j < i} (1 - \kappa_j),
\qquad
S = C - G - \hat H_1 - \sum_{k > 1} \text{reserve}_k
$$

The static condition is $S > 0$. It is checked for both modes, and then every
ridge has a strictly positive headroom for every hash.

| | Two-ridge mode | Three-ridge mode |
|---|---:|---:|
| Exact $\hat H$ per ridge, after vertical scale | 650.20, 657.10 | 602.71, 680.20, 591.82 |
| Reserves behind Ridge 1 | 229.99 | 238.07 + 207.14 |
| Slack $S$ | **331.81** | **164.08** |
| Smallest extra-elevation step | 4.15 px (Ridge 2) | 2.20 px (Ridge 3) |

Values are for the default styles, $G = 260$, $C = 1472$, and $\rho = 0.35$, with steps at
1024 × 768. For example, two-ridge mode: $S = 1472 - 260 - 650.20 - 229.99 = 331.81$.
Jagged detail raised the middle and back worst cases slightly (three-ridge
middle from 649.81 to 680.20, back from 563.36 to 591.82); the scales were
left unchanged.

The exact heights are far below the analytic bound from summing every level's
maximum displacement (about twice as large), because a single shape cannot reach
its maximum rise and maximum dip along the same chain of points.

#### The rule that preserves injectivity

$T_i$ and $R_i$ may depend on the ridges in front and on the ridge's own shape
$Q_i$, but **never on its own extra elevation $e_i$**.

This matters because the extra elevation shifts a ridge rigidly upward. If the
offset or headroom were computed from the ridge's absolute height, it could
cancel that shift, and two different hashes would produce the same SVG. Because
$Q_i$ is anchor-relative and the left anchor's bits enter only through $e_i$,
neither $T_i$ nor $R_i$ carries any information about them. The map
$v \mapsto e_i$ is strictly increasing because $R_i > 0$.

### Ridge color

Each ridge has a base color index: 8 bits (256 colors) for Ridges 1 and 2, and
4 bits (16 colors) for Ridge 3. Distant mountains are more uniform in color, so
Ridge 3 spends its budget on geometry instead.

#### Palette mapping

The index is not split into RGB channels. Instead, the theme defines one
coherent **color family** shared by every ridge, and each ridge's bits select a
distinct shade within it:

* The family is a theme-defined hue range in a perceptual color space (OKLCH),
  optionally different per depth band, with a chroma multiplier (see Biome
  styling for each biome's family).
* Each ridge position has its own lightness band within the family: darkest in
  front, lightest at the back. The bands do not overlap, so depth order is
  always preserved.
* A ridge's index selects a shade within its band. For 256 entries:
  8 lightness × 8 hue × 4 chroma steps. For Ridge 3's 16 entries:
  4 lightness × 4 hue steps at a fixed chroma.

| Palette | Lightness band | Chroma (before the biome multiplier) | Entries |
|---|---|---|---:|
| Front | 0.30–0.48 | 0.030–0.075 | 256 |
| Middle | 0.51–0.65 | 0.025–0.065 | 256 |
| Far (Ridge 3) | 0.68–0.77 | 0.040 | 16 |

The palette tables are constants of the theme. The reference implementation
currently builds them at startup; they must become precomputed tables before V0.

#### Color injectivity

A color code does not automatically give a distinct output color. Converting
to sRGB and rounding to `#RRGGBB` can merge nearby colors, and so can lighting.
So the following must hold for each theme and ridge:

* **Distinct palette entries:** all entries serialize to pairwise distinct
  `#RRGGBB` values.
* **Lighting must not merge colors:** either lighting is applied as separate SVG
  elements and never rewrites the ridge's fill, or, if it does transform the
  fill, the transformed entries must remain pairwise distinct for every
  lighting mode.

Both conditions can be verified exhaustively: there are at most 256 entries per
palette, multiplied by the number of themes, ridges, and lighting modes. An
automated test checks every combination.

Distinct `#RRGGBB` values guarantee SVG uniqueness, not that viewers can tell
the colors apart. Perceptual separation is an aesthetic goal for palette design,
not an injectivity requirement.

### Biome styling

The theme (biome) is context from the Global namespace, not Mountains bits. It
changes styling only:

* **Applied after placement:** every ridge is placed first, and styling is
  applied afterward. Geometry and placement are therefore identical in every
  biome; the same hash gives the same mountain shapes everywhere.
* **Deterministic:** every styling rule is a fixed function of the ridge's
  geometry and decoded values. Styling consumes no bits.
* **Injectivity preserved:** the injectivity claim already holds for a fixed
  theme. Textures only insert vertices with a fixed structure and never move a
  bit-driven point, so decoding is unchanged. Snow and the front-ridge canopy
  are separate SVG elements.
* **Provisional:** all styling parameters are tuned from rendered galleries.

#### Color families

| Biome | Front hues | Middle hues | Far hues | Chroma multiplier |
|---|---|---|---|---:|
| Alpine (cool slate blue) | 215–275° | 215–275° | 215–275° | 1.0 |
| Temperate (bluish teal, fading to soft blue) | 175–205° | 188–232° | 210–240° | 1.0 |
| Tundra (muted purple, cool lavender) | 285–325° | 285–325° | 285–325° | 0.9 |
| Desert (warm golden, ochre, sand) | 50–85° | 50–85° | 50–85° | 1.2 |

Temperate shifts toward blue with distance (aerial perspective). Every palette
of every biome has pairwise distinct `#RRGGBB` entries, which `make verify`
checks. Tuning found two collisions, both fixed: a Desert chroma multiplier of
1.35 pushed colors out of the sRGB gamut, and a 195–225° Temperate middle range
packed hue steps too closely.

#### Alpine: snow caps

The back ridge gets snow above a gently wavy snowline. The line is centered
one third of the way down from the ridge's highest point to its lowest:

$$
h_{\text{snow}} = h_{\max} - \tfrac13 \left( h_{\max} - h_{\min} \right)
$$

The wave is smooth 1D midpoint displacement in $h$ on a uniform grid across the
canvas width:

* The two endpoints are offset from $h_{\text{snow}}$ by up to $\pm A$, with $A = 36$
  logical units.
* Each of 5 levels inserts midpoints at the average of their neighbors, plus an
  offset of up to $\pm A \cdot 0.85^{m+1}$ at level $m$, giving 33 knots.
* Offsets come from a fixed integer mix of a seed folded from the back ridge's
  decoded values, so each hash gets its own snowline without any extra bits.

The amplitude is absolute rather than a fraction of the ridge's height, and it
decays slowly, so the undulation is visible at the scale of the cap itself
while staying much smoother than the jagged ridgeline. On a low ridge the wave
can split the snow into separate patches.

The silhouette and the snowline are both linear between their merged vertex
positions, so their difference is too, and the crossings are exact. Each
maximal run of the silhouette above the snowline becomes a closed polygon: the
entry crossing, the silhouette above, the exit crossing, then back along the
snowline. The polygons form one `<path class="snow">` element (color
`#EEF2F6`), drawn right after the ridge and before the haze in front of it, so
nearer ridges cover it and fog tints it. The ridge's own path is unchanged.

#### Ridge faces: Alpine, Tundra, Desert

A flat fill reads as a cutout, so bare and snowy ridges also show their facets:
the lit and shaded halves of their peaks, and on snow, reflective triangles
that catch the light. All of it is lit by the implicit sun
(subspec/celestial.md) and carries no bits.

* **Which ridges:** every Alpine ridge except the tree-covered front one, and
  every Tundra and Desert ridge.
* **Not Temperate:** its wooded ridges lack that clear-cut contrast.
* **Source:** each ridge's final silhouette $P_0, \dots, P_{n-1}$, after any
  styling.

**Sharp turns.** A vertex $b$ between $a$ and $c$ turns by more than 30° when
$u = b - a$ and $v = c - b$ satisfy $u \cdot v < 0$, or
$(u \cdot v)^2 < 0.75\,|u|^2 |v|^2$, with no square root.

* **Peak:** $b_h > a_h$, $b_h > c_h$ (a strict rise into a strict descent, so a
  plateau never counts) and a sharp turn.
* **Valley:** $b_h < a_h$, $b_h < c_h$ and a sharp turn.
* **Width:** a peak counts only if its key valleys are at least 10% of the
  canvas apart (204.8 logical units). Its key valley on each side is the lowest
  point before the ridge climbs higher than the peak, or the canvas edge.
* **Relaxed for Tundra and Desert:** their bare ridges otherwise look plain, so
  a peak there needs only a turn of more than 15° (cos² 15° = 0.9330127 in
  place of 0.75) and key valleys 5% of the canvas apart (102.4 units). Valleys
  keep the 30° rule.
* **Peak height** $H$: the peak's height minus the ridge's lowest point.

**Incoming slope.** From each qualifying peak, a ray runs down into the face,
toward the viewer like a tripod's third leg. It lies between the rising slope
extended down past the peak, $\hat d_L = a - b$, and the descending one,
$\hat d_R = c - b$. Each is scaled so its larger component is 1. With
$N = 37$:

$$
d = \hat d_L + f\,(\hat d_R - \hat d_L), \qquad f = 0.1 + 0.8\,\frac{b_h \bmod N}{N}
$$

A convex combination always stays between the two slopes, and the padding keeps
it off both.

**Cutoff slope and reflective surface.** A cutoff is a ridge seen edge-on,
crossing the face. Each valley $b$ has two candidates:

* its descending slope extended past it, a ray from $b$ along $b - a$, under
  the nearest qualifying peak to the right;
* its ascending slope extended back past it, a ray from $b$ along $b - c$,
  under the nearest qualifying peak to the left.

A straight extension meets the other slope at 180° minus the valley's angle. A
candidate is usable when its ray meets that peak's incoming slope, both rays
going forward. That closes a triangle: the valley, the peak, and the meeting
point. It is the reflective surface.

* **Realism:** a surface is realistic when its vertical extent, from its lowest
  corner up to the peak, is at most $H/2$. Larger flat faces are rare on real
  mountains and are dropped. (The rule began at $H/3$; that left too few
  faces.)
* **Choice:** if both candidates are realistic, the valley's
  $(b_h \bmod N)/N < 0.5$ picks the descending one, otherwise the ascending
  one. If only one is realistic, it is used; if neither, the valley has no
  surface.

The full triangle, including any part below the snowline, is what the rules
measure. The meeting point may lie above the snowline, in which case the whole
cutoff shows inside the cap.

Reflective surfaces are computed only on ridges with a snow cap, where they
become glints.

**Halves.** Each qualifying peak's incoming slope splits its stretch of the
ridge into two faces. The stretch runs from the lowest point between it and the
previous qualifying peak (or the canvas edge) to the lowest point before the
next one, so the stretches of neighboring peaks never overlap.

* The **left half** runs from the peak along the silhouette to that left low
  point, down to a floor 10 units below the ridge's lowest point, then back up
  the incoming slope.
* The **right half** mirrors it.

The top edge of each half is the silhouette itself, so nothing shows above the
ridge.

**High-poly lines.** The incoming and cutoff slopes are not drawn straight. As
a post-process, each straight line from $a$ to $b$ between two fixed corners
(the peak, the foot, the valley, the meeting point) becomes 12 segments, bent
to follow the ridge:

* **Ridge sample:** take the silhouette for 12 points from the line's anchor
  (its peak or valley), walking down the ridge away from the line's other end
  (to the right for a peak's incoming slope).
* **Deviation:** $w_k$ is how far point $k$ of that sample strays from the
  sample's own chord, normalized by the largest $|w_k|$.
* **Offset:** interior point $k$, at $t = k/12$, is pushed sideways by
  $0.1\,L \cdot 4t(1 - t) \cdot w_k$, where $L$ is the line's larger extent and the
  sideways direction is the line turned a quarter, scaled so its larger
  component is 1. The taper keeps both corners fixed.

Both halves of a peak share the same bent incoming slope, so they meet with no
gap. A glint's triangle runs along the actual silhouette from the valley to the
peak, then down its bent incoming slope to the meeting point and back along its
bent cutoff. Its corners are unchanged, so the realism rule still measures the
straight triangle.

**Light.** With the sun's shadow direction $d_x$ and elevation step $e$:

* **Shade:** the half turned away from the sun is overlaid with the ridge's
  color darkened to 60%, at opacity $0.25 + 0.02\,(15 - e)$. That is 25% under
  a high sun and 55% under the lowest. It is the right half when $d_x \ge 0$
  (sun on the left), the left half otherwise. Over snow, an overlay at opacity
  $k$ is the snow color mixed a fraction $k$ toward the darkened ridge color,
  staying in the mountain's color family.
* **Alpenglow:** at sunset ($e \le 2$, a sun at 26° or lower), the sunlit half
  is overlaid with `#F6A98C`, an orange-pink, at opacity $0.35 + 0.1\,(2 - e)$.
* **Fade:** both overlays use a vertical `userSpaceOnUse` gradient that holds
  full opacity down to the fade top and falls to 0 at the fade bottom.
  * The fade top is the snow's lowest point, or the ridge's highest point on a
    bare ridge.
  * The fade bottom is the ridge's lowest point.

  So the snow cap reads crisply, and the rock below only hints at the facets,
  fading out as it goes down.
* **Glints:** realistic reflective surfaces on the sunlit side, clipped to the
  snow cap, are pure white, or at sunset the snow moved 80% toward `#FFD9C2`.

**Rendering.** After the ridge's snow path (if any), each overlay is a
`<linearGradient id="shade-N">` or `id="glow-N"` followed by a
`<path class="ridge-face">` filled with it. Each such path has one closed
subpath per half. Glints follow, clipped by a `<clipPath id="snow-clip-N">`
holding a copy of the cap outline. The fog in front softens all of it like the
rest of the ridge. The ridge and snow paths are unchanged, so decoding is
unaffected.

**Frequency.**

* On 1,000 random Alpine hashes, 97% of two-ridge snow caps and 66% of
  three-ridge ones have a qualifying peak.
* 58% of two-ridge caps also carry a glint; in three-ridge mode about 4% do,
  because the far ridge is mostly a single summit with few sharp valleys.
* In the gallery, every Tundra and Desert scene and 10 of 12 Alpine scenes
  show faces. With the relaxed rule, 98% of Tundra ridges have a qualifying
  peak, about 3.3 per ridge (200 random hashes).

#### Front-ridge trees: Alpine and Temperate

The front ridge's silhouette carries a tree texture: conifers in Alpine, a
deciduous canopy in Temperate. The texture is part of the ridge's own path.

**Tree density from the ridge's own values.** The deepest bit level's points
divide the front ridge into zones: 8 in two-ridge mode and 4 in three-ridge
mode. A zone's score averages its point's value with its parent point's value,
each normalized to $[0, 1]$. Scores are interpolated linearly between the zone
points, and the tree factor is

$$
g(x) = \max\!\left(0,\; \frac{s(x) - \theta}{1 - \theta}\right), \qquad \theta = 0.35
$$

Where $g = 0$ the texture is exactly flat, a clearing. Forest cover therefore
varies from hash to hash without using any extra bits.

**Fixed variation.** Each tooth or bump also varies in size and position by a
fixed integer mixer of its index along the ridge. The variation is the same for
every hash; it could later be seeded from the hash.

**Alpine conifers.** Each front-ridge segment is split once (to about 32
units). The new split points are cut $60 \cdot (0.5\text{–}1.0) \cdot g$ below the
silhouette as deep valleys. Each resulting segment then holds a sharp two-tooth
zigzag: a tip at 20–30% of the segment, a V-valley at 45–55% cut
$60 \cdot (0.5\text{–}1.0) \cdot g$ below the silhouette, and a tip at 70–80%. Tip
heights are $18 \cdot (0.6\text{–}1.2) \cdot g$ above the silhouette. Tips and
V-valleys are measured from the unlowered silhouette, so tree tips follow the
ridge line while the valleys needle deep into it, and each tree reads as a
distinct spire. All edges are straight, so peaks and valleys are both sharp.
Every fourth valley falls on an original ridge vertex, which never moves, so
valley depths vary naturally. Cuts only remove ridge area, so the ceiling and
visibility guarantees are unaffected.

**Temperate canopy.** Each front-ridge segment (about 64 units) becomes one
rounded bump: fifteen points at $f = k/16$ on a semicircle-like arc
$\sqrt{1 - (2f - 1)^2}$, taken from a constant table, so no square root is
computed at run time. The arc covers 65–100% of the segment, and its height is
$h = 16 \cdot (0.55\text{–}1.3) \cdot g$. The points carve a fractal texture
into the crown, which depends on the season:

| Point $k$ | Spring, summer, fall (leafy) | Winter (bare) |
|---|---|---|
| $k \equiv 0 \pmod 4$ | on the arc | on the arc |
| $k \equiv 2 \pmod 4$ | 80% of the arc | cut $0.4\,d$ below the silhouette |
| $k = 1, 15$ (crown edges) | groove cut $(0.6\text{–}1.0)\,c$ below the silhouette | cut $(0.5\text{–}1.0)\,d$ below the silhouette (needles) |
| other odd $k$ | cavity cut $(0.3\text{–}1.0)\,c$ below the silhouette where the pattern value $r < 0.4$; otherwise 10–50% of the arc | cut $(0.5\text{–}1.0)\,d$ below the silhouette (needles) |

where $c = 45 \cdot g$ is the canopy depth and $d = 100 \cdot g$ the bare depth.
Leafy crowns stay rounded on top, but grooves separate neighboring crowns and
deep cavities break up each crown, so the ridge reads as a rugged deciduous
forest edge rather than a smooth band. Winter's needles cut deeper, like a
leafless forest against the sky. All cuts only remove ridge area, so the
ceiling and visibility guarantees are unaffected. Ranges marked with a dash vary by the fixed pattern.

**Properties:**

* **Decoding:** conifers add three levels of fixed structure (one straight
  split plus three points per segment), giving total depth 8 (257 points). The
  canopy adds four levels (fifteen points per segment), giving total depth 9
  (513 points), in every season. Original vertices never move, so the
  bit-driven points sit at known indices.
* **Monotonic:** every inserted point lies strictly between its neighbors in
  $x$. The narrowest serialized segment is 0.07 px (Alpine) and 0.04 px
  (Temperate) at 1024 × 768, above the 0.01 quantum.
* **Ceiling:** the tallest texture is about 22 (conifers $18 \times 1.2$, canopy
  $16 \times 1.3 = 20.8$). It is smaller than the reserves kept behind the front
  ridge, so tree tips never rise above the ceiling.
* **Visibility:** trees can cover at most about 22 of a back ridge's exposure.
  Required exposure is at least $E_{\min} = 48$, so at least 26 stays visible.

#### Front-ridge canopy

Below its tree-line silhouette, the front ridge's face is covered by a canopy of
small template trees, so it reads as forested slope rather than a flat fill:
conifers in Alpine, needle sprays in Temperate. Tundra and Desert get none. The
canopy carries no bits and never moves a ridge point: it is a separate group of
paths clipped to the ridge, a fixed function of the ridge, the hills, the sun
(subspec/celestial.md), and the biome and season.

**Treetop line.** Sample the ridge every 4 units, and take at each sample the
highest sample within 32 units on either side. This envelope $T(x)$ runs along
the tree tips rather than into the tree-line texture's cuts. Linear
interpolation reads it between samples.

**Rows.** Row $r$ hangs at depth $d$ below the treetop line, starting at
$d = 4.5$ and stepping by $0.35\sigma$, where the template size is
$\sigma = 18\,(0.85 + 0.15\min(1, d / h_{\max}))$ and $h_{\max}$ is the ridge's
highest point. Templates are spaced $0.44\sigma$ (conifers) or $0.35\sigma$
(needle sprays) along the row; odd rows start half a spacing in. Rows continue
until the deepest row is a template below everything the hills leave visible.

**One template.** At grid position $x_0$ in a row at depth $d$, the key is
$\kappa = \lfloor 7x_0 + 131d \rfloor$. The template stands at
$x = x_0 + 0.125\,(\kappa \bmod 5 - 2)\,s_p$ (with $s_p$ the spacing) and
$h = T(x) - d + 0.12\sigma\,(\lfloor\kappa/25\rfloor \bmod 3 - 1)$, with height
$z = \sigma\,(0.8 + 0.1\,(\lfloor\kappa/5\rfloor \bmod 5))$. A template whose top
$h + z$ is at or below the hills' highest silhouette at $x$ is hidden and
skipped.

* **Conifer** (7 points): a two-tier spire of width $0.46z$, its tiers
  meeting at $0.45z$.
* **Needle spray** (7 points): a base $0.14z$ wide, a center needle to the top,
  and a needle to each side tipped at height $0.78z$ and out $0.4z$ times
  $0.7 + 0.05\,(\kappa \bmod 3)$ on the left and
  $0.7 + 0.05\,(\lfloor\kappa/3\rfloor \bmod 3)$ on the right.

**Color.** A template starts from the ridge's own color at its base height:
the palette color, or in Temperate fall the ridge's gradient at
$t = 1 - h/h_{\max}$. It then moves by one of ten steps
$k = \lfloor x h \rfloor \bmod 10$, from the template's coordinates. If $k$
equals its left neighbor's step, it becomes $(k + 3) \bmod 10$, so neighbors
always differ.

* Every step shades the color by $0.025\,(k - 4.5)$, toward white above 0 and
  toward black below, as the hill bands do: the canopy keeps the ridge's color
  and gradient and varies almost only in brightness.
* Temperate fall adds foliage: steps 1, 3, 6, and 8 are mixed 38% toward
  `#B8432A` (red), `#D9802E` (orange), `#D6B03C` (yellow), and `#C25E2C`
  (rust).

**Sunlit faces.** Sample the treetop line every 64 units. A sample higher than
the one before and at least as high as the one after is a peak. Each side's
valley is the lowest sample before the line rises above the peak, or reaches
the canvas edge. A peak is **sharp** when both hold:

* it stands at least 40 units above both valleys;
* its two valleys meet at an angle of 140° or less at its top, tested on the
  cosine without trigonometry.

The sun lights the face on its side of a sharp peak (left when $d_x > 0$). The
face is the part of the mountain under the sunward slope, the line from the
peak to that side's valley:

* within 60° of that slope, turning down into the mountain;
* on the peak's side of the line equidistant from the peak and the valley.

So each face is a triangle hugging the sunlit slope. Inside it, a template is
lightened further by $0.05\,\ell$ with light level $\ell = 3, 2, 1$ for
$e < 0.5$, $e < 0.8$, and otherwise. Here $e$ is the larger of how far the base
is from the face's middle direction (0 at 30° from the slope, measured on
$\sin^2$; 1 at its sides) and its squared distance to the peak over that to the
valley. The patch is brightest in its middle and fades toward its edges, and
the neighbor steps still vary inside it. The front ridge is low and wide, so
peaks this sharp are rare: about one front ridge in five has one.

**Rendering.** Right after the front ridge's path, a
`<clipPath id="ridge-1-clip">` uses the ridge's path, and a group clipped to it
holds the canopy. Over blocks of four rows from the top down, the templates of
one color form one `<path class="canopy">`. Rows overlap only their neighbors,
so the order within a block barely shows.

Canopy templates are a few pixels tall, so their points are written to tenths
of a pixel, not hundredths. Each template is an absolute move to its first
rounded point, then relative steps between its rounded points, so the drawn
points are exactly the rounded ones. The hill's grass blades are written the
same way.

#### Front-ridge facets: Tundra and Desert

A bare front ridge is faceted like weathered rock: a fine triangulation over
its face, each triangle a step lighter or darker by how it faces the sun. The
facets sit on the ridge's fill, under the peaks' lit and shaded halves (Ridge
faces), and their steps are small, so the halves still dominate and the facets
only fill in between them. They show at a glance as texture; the individual
shapes take a closer look. Only the front ridge gets them, and no bits are
used.

**Points.** A grid of cells 32 units wide and 28 tall covers the canvas from
one row below the lowest visible point (the hills' highest silhouette) to one
row above the ridge's highest point. Each grid corner $(i, j)$ is moved by the
digits $D$ of $\pi$ (the first 100, cycling), keyed by
$k = 37i + 11j + \lfloor S(32i) \rfloor$, where $S$ is the ridge's silhouette:

* by $0.8 \cdot 32\,(D_k - 4.5)/9$ across and $0.8 \cdot 28\,(D_{k+3} - 4.5)/9$
  up, under half a cell, so the points look scattered but never cross;
* to a depth out of the screen $z = 0.6\,(S(x) - h) + 14\,(D_{k+7} - 4.5)/9$:
  the face leans back from the silhouette and follows its rise and fall, so
  facets on a peak's flank tilt the way the flank does.

**Triangles.** Each cell splits into two triangles along one diagonal, chosen
by the parity of $D_{53i + 29j}$. A triangle is kept when at least one corner
is on or below the silhouette and at least one is at or above the hills; the
group is clipped to the ridge.

**Light.** The light comes from the sun's side and above,
$L = (-d_x \ell,\; 1,\; 0)$, with the sun's shadow direction $d_x$ and shadow
length $\ell$ (subspec/celestial.md). A triangle's normal $n$, turned toward
the viewer, gives the signed squared cosine
$s = \operatorname{sign}(n \cdot L)\,(n \cdot L)^2 / (|n|^2 |L|^2)$, with no
square root. By $s$:

| $s$ | Share (typical) | Shade of the ridge color |
|---|---:|---:|
| above 0.52 | 6% | +9% (toward white) |
| 0.40 to 0.52 | 12% | +4% |
| 0.05 to 0.40 | 70% | not drawn: the ridge's own fill |
| 0.012 to 0.05 | 7% | −4% |
| below 0.012 | 5% | −9% (toward black) |

**Rendering.** Each drawn level is one `<path class="facet">`, one closed
subpath per triangle, written like the canopy (tenths, relative steps), in the
front ridge's clipped group. Leaving the middle tones undrawn also keeps
neighboring facets from showing seams.

#### Tundra and Desert

No textures in the ridge paths: the palette, the lit and shaded ridge faces,
and on the front ridge the facets above.

#### Seasons

The season (SPECIFICATIONS.md, Seasons) is context from Global, like the biome.
For mountains it changes only the Temperate palette:

Each entry is a thematic color family realized as a palette of coordinated
shades, not a single color.

| Season | Front hues | Middle hues | Far hues | Chroma (front, middle, far) | Look |
|---|---|---|---|---|---|
| Winter | 195–255° | 200–260° | 205–265° | 0.7, 0.7, 0.7 | Bluish gray |
| Spring | 175–205° | 188–232° | 210–240° | 1.0, 1.0, 1.0 | Unchanged (bluish teal) |
| Summer | 175–205° | 188–232° | 210–240° | 1.0, 1.0, 1.0 | Unchanged (bluish teal) |
| Fall | 30–60° | 188–232° | 210–240° | 1.35, 1.0, 1.0 | Orange front ridge; distant ridges keep the cool summer blues |

**Fall front-ridge gradient.** In fall, the front ridge fills with a vertical
gradient over its bounding box, from its palette color (orange) at the peak to a
paired golden shade at the canvas bottom. The golden shade uses the same index
grid as the front palette, with hue moved to 72–92° and lightness raised by 0.10.
The gradient is emitted as a `linearGradient` just before the ridge path. Its
top stop is the palette entry itself, and all 256 are distinct, so the ridge's
color bits are still recoverable from the SVG.

The front-ridge canopy texture also changes in winter (bare needles instead of
leafy cavities; see Front-ridge trees above).

Alpine, Tundra, and Desert mountains are the same in every season. Alpine snow
caps and tree textures are present in every season.

Every seasonal palette keeps the structure of the biome palettes (per-role
lightness bands, 256 + 256 + 16 entries), and every entry serializes to a
distinct `#RRGGBB`, checked for every (biome, season) pair by `make verify`.
Tuning found collisions in both new families, both fixed: winter at chroma 0.55
merged low-chroma grays, and fall with narrow hue ranges (35–70°) merged dark,
low-chroma oranges.

### Bit layouts (provisional)

Within each ridge, bits are consumed in this order: left anchor, right anchor,
Level 1, Level 2, Level 3, Level 4 (each level left to right), then color.
Bit positions below are namespace-relative; add 48 for the absolute bit index.

For summit-skeleton ridges (the middle and back roles), the right anchor and L1.1
slots below are read together as the summit fields [x | rise | drop] (see Anchors
and skeleton). Their bit positions are the same.

#### Two-ridge mode (header = 0)

Both ridges use four bit-driven subdivision levels. With detail levels, Ridge 1 has 33 points and Ridge 2 has 17.

| Ridge | Anchors | Level 1 | Level 2 | Level 3 | Level 4 | Geometry | Color | Total |
|---|---|---|---|---|---|---:|---:|---:|
| Ridge 1 | 2 × 3 | 1 × 7 | 2 × 3 | 4 × 2 | 8 × 1 | 35 | 8 | **43** |
| Ridge 2 | 2 × 4 | 1 × 6 | 2 × 3 | 4 × 2 | 8 × 1 | 36 | 8 | **44** |

1 (header) + 43 + 44 = 88.

Ridge 1, the nearest ridge, puts its extra precision into Level 1, its main
shape. Ridge 2 puts it into its anchors, giving the farther ridge more variety
in overall height and tilt.

**Ridge 1** (bits 1–43, absolute 49–91):

| Slot | Bits | Namespace bits | Absolute bits |
|---|---:|---|---|
| Left anchor (extra elevation) | 3 | 1–3 | 49–51 |
| Right anchor (tilt) | 3 | 4–6 | 52–54 |
| L1.1 | 7 | 7–13 | 55–61 |
| L2.1, L2.2 | 3 each | 14–16, 17–19 | 62–64, 65–67 |
| L3.1 – L3.4 | 2 each | 20–21, 22–23, 24–25, 26–27 | 68–69, 70–71, 72–73, 74–75 |
| L4.1 – L4.8 | 1 each | 28, 29, …, 35 | 76, 77, …, 83 |
| Color | 8 | 36–43 | 84–91 |

**Ridge 2** (bits 44–87, absolute 92–135):

| Slot | Bits | Namespace bits | Absolute bits |
|---|---:|---|---|
| Left anchor (extra elevation) | 4 | 44–47 | 92–95 |
| Right anchor (tilt) | 4 | 48–51 | 96–99 |
| L1.1 | 6 | 52–57 | 100–105 |
| L2.1, L2.2 | 3 each | 58–60, 61–63 | 106–108, 109–111 |
| L3.1 – L3.4 | 2 each | 64–65, 66–67, 68–69, 70–71 | 112–113, 114–115, 116–117, 118–119 |
| L4.1 – L4.8 | 1 each | 72, 73, …, 79 | 120, 121, …, 127 |
| Color | 8 | 80–87 | 128–135 |

#### Three-ridge mode (header = 1)

Ridges 1 and 2 use three bit-driven levels and Ridge 3 uses two, giving Ridge 3
more bits per point. With detail levels, the ridges have 33, 17, and 17 points.

| Ridge | Anchors | Level 1 | Level 2 | Level 3 | Geometry | Color | Total |
|---|---|---|---|---|---:|---:|---:|
| Ridge 1 | 2 × 3 | 1 × 6 | 2 × 2 | 4 × 2 | 24 | 8 | **32** |
| Ridge 2 | 2 × 3 | 1 × 6 | 2 × 2 | 4 × 2 | 24 | 8 | **32** |
| Ridge 3 | 2 × 3 | 1 × 7 | 2 × 3 | — | 19 | 4 | **23** |

1 (header) + 32 + 32 + 23 = 88.

No point in three-ridge mode has fewer than 2 bits, so no displacement is
forced to an extreme.

**Ridge 1** (bits 1–32, absolute 49–80):

| Slot | Bits | Namespace bits | Absolute bits |
|---|---:|---|---|
| Left anchor (extra elevation) | 3 | 1–3 | 49–51 |
| Right anchor (tilt) | 3 | 4–6 | 52–54 |
| L1.1 | 6 | 7–12 | 55–60 |
| L2.1, L2.2 | 2 each | 13–14, 15–16 | 61–62, 63–64 |
| L3.1 – L3.4 | 2 each | 17–18, 19–20, 21–22, 23–24 | 65–66, 67–68, 69–70, 71–72 |
| Color | 8 | 25–32 | 73–80 |

**Ridge 2** (bits 33–64, absolute 81–112):

| Slot | Bits | Namespace bits | Absolute bits |
|---|---:|---|---|
| Left anchor (extra elevation) | 3 | 33–35 | 81–83 |
| Right anchor (tilt) | 3 | 36–38 | 84–86 |
| L1.1 | 6 | 39–44 | 87–92 |
| L2.1, L2.2 | 2 each | 45–46, 47–48 | 93–94, 95–96 |
| L3.1 – L3.4 | 2 each | 49–50, 51–52, 53–54, 55–56 | 97–98, 99–100, 101–102, 103–104 |
| Color | 8 | 57–64 | 105–112 |

**Ridge 3** (bits 65–87, absolute 113–135):

| Slot | Bits | Namespace bits | Absolute bits |
|---|---:|---|---|
| Left anchor (extra elevation) | 3 | 65–67 | 113–115 |
| Right anchor (tilt) | 3 | 68–70 | 116–118 |
| L1.1 | 7 | 71–77 | 119–125 |
| L2.1, L2.2 | 3 each | 78–80, 81–83 | 126–128, 129–131 |
| Color | 4 | 84–87 | 132–135 |

#### Bit coverage

In both modes, every one of the 88 namespace bits is assigned to exactly one
slot. An automated test checks this for both layouts.

### Serialization guarantee

Distinct displacements must remain distinct in the SVG bytes.

#### Arithmetic

* All geometry is computed in IEEE 754 binary64 with a specified operation order.
* Only `+`, `-`, `*`, `/`, `abs`, `min`, and comparisons are used. These are
  exact or correctly rounded, and identical in native C++ and WebAssembly.
* Fused multiply-add contraction must be disabled (for example, `-ffp-contract=off`).
* No square roots or transcendental functions are used during generation.

#### Number formatting

* Output coordinates are logical coordinates multiplied by $\text{width} / 2048$.
* Each coordinate is written with exactly $p$ fixed decimal places, using
  round-half-even on the exact binary64 value. Proposed: $p = 2$, so the quantum
  is $q = 0.01$.
* Formatting uses BeautifulHash's own formatter, never `printf`, iostreams, or
  any locale-dependent routine.

#### Separation bound

Consider one subanchor with $b$ bits and two values of $v$ one step apart, with
the same parents $A$ and $B$.

**Step 1: displacement difference.** The values of $t$ differ by
$\Delta t = \frac{2\,t_{\max}}{2^b - 1}$, so the two subanchors differ by the vector
$r\,\Delta t\,(-\Delta_h, \Delta_x)$, whose length is

$$
\ell = r\,\Delta t\,\lVert B - A \rVert = \frac{2}{2^b - 1} \cdot r\,t_{\max}\lVert B - A \rVert
$$

**Step 2: lower-bound $r\,t_{\max}\lVert B - A \rVert$.** Using $\lVert B - A \rVert \ge \Delta_x$ and $\lVert B - A \rVert \ge |\Delta_h|$:

* If $t_{\max} = 1$: $r\lVert B - A \rVert \ge r\,\Delta_x$.
* Otherwise: $r \cdot \frac{\gamma\Delta_x}{2r|\Delta_h|} \cdot \lVert B - A \rVert \ge \frac{\gamma\Delta_x}{2}$.

So $r\,t_{\max}\lVert B - A \rVert \ge \Delta_x \min\!\left(r, \frac{\gamma}{2}\right)$, and

$$
\ell \ge \frac{2\,\Delta_x \min(r, \gamma/2)}{2^b - 1}
$$

**Step 3: largest coordinate difference.** A vector of length $\ell$ has at least
one coordinate of magnitude $\ge \ell/\sqrt2$. Vertical scaling multiplies only the
$h$ coordinate by $s$, so after scaling some coordinate still differs by at least
$\min(1, s)\,\ell/\sqrt2$.

**Step 4: output resolution.** Multiplying by $\text{width}/2048$ gives the
guaranteed difference in output units:

$$
\delta = \min(1, s) \cdot \frac{2\,\Delta x_{\min} \min(r, \gamma/2)}{\sqrt2\,(2^b - 1)} \cdot \frac{\text{width}}{2048}
$$

**Step 5: rounding.** Rounding moves each value by at most $q/2$. If the true
values differ by more than $q + 2\varepsilon$, where $\varepsilon$ bounds the
accumulated binary64 error, the rounded strings must differ.

The anchors differ only in elevation. A tilt step moves the right anchor by
exactly $s \cdot \frac{2D}{2^b - 1} \cdot \frac{\text{width}}{2048}$, and an
extra-elevation step moves the whole ridge by
$\frac{\kappa_i R_i}{2^b - 1} \cdot \frac{\text{width}}{2048} \ge
\frac{\kappa_i S \prod_{j<i}(1-\kappa_j)}{2^b - 1} \cdot \frac{\text{width}}{2048}$.

**Step 6: placement offset.** Steps 1–5 bound differences in the anchor-relative
shape $Q$. In the output, every point of a ridge also receives the same
offset, which itself may differ between two inputs. Differences in $x$ are
unaffected, because the offset is vertical. For elevation, compare each point
with the ridge's left anchor, which sits at $Q = 0$ and so is output exactly at
the offset. If a point's relative elevation differs by $\delta$, then either the
left anchor's output or the point's output must differ by at least
$(\delta - 2\varepsilon)/2$. The working condition is therefore

$$
\delta > 2q + 6\varepsilon
$$

for subanchors and the right anchor. For the left anchor, which sets the extra
elevation $e$, the full anchor step applies directly.

The condition must hold for every slot, at the smallest supported resolution
(1024 × 768, factor 0.5) and the smallest vertical scale any theme uses.

#### Worked numbers

For subanchors, at 1024 × 768 (resolution factor $0.5$), with $\gamma = 0.5$:

$$
\delta = \min(1, s) \cdot \frac{2 \times \Delta x_{\min} \times \min(r, 0.25)}{\sqrt2 \times (2^b - 1)} \times 0.5
$$

Vertical differences are then halved for the placement offset (Step 6).
Horizontal differences, such as the summit's $x$, are not.

For example, the front ridge's 7-bit Level 1 point ($r = 0.1$, $s = 1.372$, so
$\min(1, s) = 1$): $\ell \ge 2 \times 2048 \times 0.1 / 127 = 3.225$, then
$3.225 / \sqrt2 = 2.281$, then $\times\, 0.5 = 1.140$ px, then halved: **0.570 px**.

Guaranteed output difference per slot for the default styles, after halving
where it applies:

| Ridge (role) | Skeleton slots | Level 2 | Level 3 | Level 4 |
|---|---|---:|---:|---:|
| 2R Ridge 1 (front, midpoint) | tilt (3 bits) 25.09 px; L1 (7 bits) **0.570 px** | 2.586 px (3 bits) | 1.508 px (2 bits) | 1.131 px (1 bit) |
| 2R Ridge 2 (back, summit) | x 87.77 px; rise (4 bits) 4.34 px; drop (3 bits) 9.30 px | 2.886 px (3 bits) | 1.683 px (2 bits) | 1.263 px (1 bit) |
| 3R Ridge 1 (front, midpoint) | tilt (3 bits) 25.09 px; L1 (6 bits) 1.149 px | 6.034 px (2 bits) | 1.508 px (2 bits) | — |
| 3R Ridge 2 (middle, summit) | x 87.77 px; rise (3 bits) 10.70 px; drop (3 bits) 10.70 px | 6.034 px (2 bits) | 1.508 px (2 bits) | — |
| 3R Ridge 3 (back, summit) | x 87.77 px; rise (4 bits) 4.34 px; drop (3 bits) 9.30 px | 2.886 px (3 bits) | — | — |

For summit ridges, Level 2 segments are at least $409.6$ wide (the summit sits
at least $0.2W$ from each edge), and summit steps use the field ranges: $x$ steps
by $0.6W / 7 = 175.5$, rise and drop by $s \cdot 280 / (2^b - 1)$. The smallest
extra-elevation step is 2.47 px (three-ridge Ridge 3), and it is not halved.

The smallest guaranteed step is **0.570 px**, from the front ridge's 7-bit Level 1
point, about 57 times the 0.01 quantum. The narrowest final segment (2 logical,
1 px at 1024 × 768) likewise keeps rounded $x$ coordinates strictly increasing.

These are worst-case bounds. Typical segments are about twice as wide at each
level, rather than a quarter, so real steps are usually much larger.

### Injectivity of the 88-bit mapping

**Claim.** For a fixed algorithm version, theme, season, and target resolution, the map
from the 88 Mountains bits to the Mountains SVG fragment is injective.

**Output structure.** The fragment is a sequence of ridge elements with fixed
identifiers, back to front: two in two-ridge mode, three in three-ridge mode.
Each element is a fill color followed by a path with a fixed number of points
(determined by the layout) in a fixed format.

**Proof.** Take two distinct 88-bit inputs $X \ne Y$.

1. **Different headers:** the fragments have different numbers of ridge
   elements, so they differ.
2. **Same header:** both are decoded by the same layout. The layout assigns every
   payload bit to exactly one slot, and each slot's value is its bits read
   directly as an unsigned integer. So $X$ and $Y$ differ in at least one slot
   value. Take the **nearest** ridge (front to back) that contains a differing
   slot. Every ridge in front of it has identical slot values, so it is
   generated and placed identically, and the envelope $F$ is bit-for-bit the
   same for $X$ and $Y$. Within this ridge:
   * **Color slot:** the ridge's fill differs, by the color injectivity
     requirements above.
   * **Skeleton slot (summit ridges):** if the summit fields differ, the summit
     point or the right edge differs. A different $x$ moves the summit
     horizontally by at least one $x$ step. With the same $x$, a different rise
     moves the summit vertically, and with the same rise, a different drop moves
     the right edge vertically. Each step exceeds the separation bound, so some
     serialized coordinate differs.
   * **Shape slot (tilt or subanchor):** order the shape's points geometrically:
     skeleton, then Level 2, Level 3, and so on, left to right. For midpoint
     ridges the skeleton is the right anchor, then Level 1. Take the
     first point whose value differs. All of its ancestors have identical values,
     and are computed by identical binary64 operations, so they are bit-for-bit
     equal. Its parents $A$ and $B$, and hence its interval $[-t_{\max}, t_{\max}]$,
     are therefore identical for $X$ and $Y$. Within that interval the map from
     value to point is strictly increasing, so the two shapes differ at this
     point by at least $\delta$. By Step 6, some serialized coordinate differs.
   * **Only the extra elevation differs:** the shapes $Q$ are identical, and $F$
     is identical, so $T_i$ and $R_i$ are identical. The offsets then differ by
     at least one extra-elevation step, $\kappa_i R_i / (2^b - 1)$, so the left
     anchor's serialized elevation differs.

Either way, the fragments differ. ∎

**Scope and visibility.** This establishes SVG uniqueness for the Mountains
namespace. No ridge is ever emitted without being part of the selected layout,
so no bits live in hidden placeholder geometry, and the placement rule
guarantees that every present ridge is partly visible. Portions of a
background ridge can still be covered by the ridges in front. Those portions
remain in the SVG, so SVG uniqueness holds, but that detail is not visible.

### Verification

The reference implementation is `core/mountains.cpp`, run through the real
`generate_scene` and `render_svg` pipeline. `make verify` runs `tools/verify.cpp`,
which checks:

* **Bit coverage:** both layouts consume exactly 88 bits, and decode followed by
  encode returns the original bits.
* **Placement budget:** the exact maximum silhouette heights $\hat H_i$, by
  exhaustive enumeration of every shape (up to $2^{24}$ configurations per
  ridge, with the last level solved directly), and $S > 0$ for both modes.
* **Separation:** the worst-case step for every slot, at 1024 × 768.
* **Palettes:** every entry of every biome's palettes has a distinct `#RRGGBB` value.
* **Round trip:** random hashes are rendered to SVG text, the text is parsed
  back, and the 88 bits are decoded from it, at 1024 × 768 and 2048 × 1536.
  Every ridge is also checked to stay below the ceiling, including tree tips,
  and to meet its required exposure. Front ridges with tree textures are
  decoded at their textured depth.

It also checks that serialized $x$ coordinates are strictly increasing, which
covers detail points, and repeats the budget and round-trip checks for every
gallery variant in `tools/variants.hpp`: one per biome.

Latest run: 20,000 hashes per mode and size for the default configuration
(Alpine), plus 2,000 per mode and size for each biome. 0 mismatches, smallest
observed decode margin 2.19 px, smallest ceiling clearance 240.9 logical units.
`make gallery` renders all four biomes on the same 40 random hashes.

### Dependencies

* Consumes: the theme (biome), season, and broad composition constraints from
  Global (roughness, vertical scale, color palettes, foreground level) and the
  lighting mode from Sky.
* Internal: each ridge's placement depends on the already placed ridges in
  front of it. This uses their decoded geometry, never their raw bits.
* Provides: ridge silhouettes, colors, and depths, used by Fog and for occlusion.

### Open decisions

* Both layouts and their per-point bit widths.
* Whether the 1-bit Level 4 points in two-ridge mode look too regular. If so,
  the same 8 bits could encode the eight Level 4 points jointly in base 3 (8 bits
  give 256 values, fewer than $3^8 = 6561$, so the encoding stays injective),
  letting points also take a centered position. Only some combinations would be
  reachable.
* Curated palettes versus a perceptual-grid mapping, per theme.
* Whether lighting rewrites ridge fills or is applied as separate SVG elements.
* Tilt range $D$, ground level $G$ (260), ceiling $C$ (1472), and headroom shares
  $\kappa$ (0.25, 0.5, 0.5).
* Detail strength $k$ and the total depth per role (5, 6, 6).
* Summit field ranges: position 0.2–0.8 of the width, rise and drop 160–440.
* Back ridges sometimes form a broad plateau with a small peak at one end.
  Keeping each flank's shoulder below its chord could prevent this.
* A front ridge drawn high can hide most of the ridges behind it.
* The theme's color family: hue range, lightness bands, and chroma ranges.
* Biome styling parameters: snowline fraction (1/3), snowline wave amplitude and
  decay (36, 0.85), tree threshold (0.35), conifer height and valley cut (18, 60),
  canopy height (16), texture spacing.
* Seasonal Temperate palettes and the fall gradient's golden shades.
* Canopy texture depths: leafy canopy depth (45) and winter bare depth (100).
* Whether mountains should carry a subtle seasonal tint in every biome, so all
  four seasons render distinctly (see SPECIFICATIONS.md, Seasons).
* Jagged detail parameters: decay $\lambda$ (0.75), floor $f$ (0.35), and the depth of
  middle and back ridges (6).
* Whether tree density should stay tied to the terrain's own values or come from
  an independent source, such as position or a dedicated seed.
* Conifer teeth on steep slopes form stair-step serrations; they could be scaled
  down where the slope is steep.
* Visibility ratio $\rho$ (0.35) and the absolute floor $E_{\min}$ (48).
* Ridge 1's foreground level $T_1$.
* Whether a background ridge's peak should also be required to exceed the
  front ridges' peak, in addition to the visibility constraint.
* Per-theme ridge styles: generator, roughness $r$, and vertical scale $s$ per role.
* The monotonicity margin $\gamma$.
* Decimal places $p$ in SVG coordinate output.
* Whether ridges may extend beyond the top of the canvas, or must be bounded.

### Status

The ridge algorithm, depth-role styles, biome styling, both layouts, color
requirements, serialization rules, and injectivity argument are proposed and
implemented in the reference renderer. The default styles were chosen from rendered galleries;
layouts and artistic parameters remain provisional until V0 is frozen.
