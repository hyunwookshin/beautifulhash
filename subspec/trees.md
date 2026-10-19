## Trees Namespace

| Bit range | Width |
|---|---:|
| 176–223 | 48 |

### Overview

The Trees namespace plants 1 to 10 fractal trees on the hills. Each tree grows
from a trunk by recursive forks. Its first forks are driven by its bits. After
the bits run out, branching continues with derived forks that reuse the
decoded values, which adds detail but no entropy. Fewer trees means more bits,
and richer branching, per tree. Species follow the biome:

| Biome | Species |
|---|---|
| Alpine, Tundra | Conifers |
| Temperate | Deciduous trees |
| Desert | Cactus and Joshua trees, alternating |

Trees consume the hill profiles and colors advertised by Hills
(subspec/hills.md), so Hills is generated first.

### Header fields

The namespace starts with three header fields, then the trees' own bits.

**Count (3 or 4 bits): a complete prefix code for 1–10 trees.** Every bit
string decodes to exactly one count, so no codes are wasted, and the bits left
over go to the trees:

| First 3 bits | Extra bit | Count |
|---|---|---|
| 0–5 | — | 2–7 |
| 6 | 0 / 1 | 1 / 8 |
| 7 | 0 / 1 | 9 / 10 |

At least one tree is always present, so every bit draws something.

**Arrangement (2 bits)** picks one of four group sequences. Each tree stands in
one of four groups: front rim, back rim, front interior, back interior. Rim
trees stand on a hill's silhouette. Interior trees stand on its slope, below
the rim and nearer the viewer. The sequences spread the first trees across
depths, and groups repeat once the count exceeds four:

| Arrangement | Sequence (trees 1–10) |
|---:|---|
| 0 | FR, BR, FI, FR, BI, FI, BR, FR, FI, BR |
| 1 | BR, FR, FI, BR, FR, BI, FI, FR, BR, FI |
| 2 | FI, FR, BR, FI, BI, FR, BR, FI, FR, BI |
| 3 | FR, FI, BR, BI, FR, FI, BR, FR, BI, FI |

In one-hill mode, rim groups become the hill's rim and interior groups its
slope.

**Size (2 bits)** picks a global scale class: 0.8, 0.95, 1.1, or 1.25.

**Leaf palette (4 bits, Temperate except winter only)** picks one of 16
six-color leaf palettes (see Leaves). In every other biome and in winter there
are no leaves, so the field is absent and its bits go to the trees.

### Composition

The count and arrangement fix a composition on each hill, using no further
bits. The trees on a hill, taken in index order, are given roles:

| Role | Who | Position | Prominence | Role weight |
|---|---|---|---:|---:|
| Isolated | The last tree on the hill (or the only one) | $c \pm 0.38W$, on the far side from the grove (at $c$ when alone) | 1.0 | 5 |
| Straggler | The second-to-last tree, when the hill has 5 or more | $c \mp 0.2W$, on the other side of the grove | 0.8 | 2 |
| Leader | The first remaining tree | $c$ | 1.15 | 6 |
| Member | The other remaining trees | $c + \text{spacing} \cdot o_k$ | $0.8 + 0.15\,\{\varphi(i+1)\}$ | 3 |

Here:

* The grove center is $c = W \cdot (0.2 + 0.6\,\{0.37 + 0.29a + 0.41h\})$, where
  $a$ is the arrangement, $h$ is 0 for the front (or only) hill and 1 for the
  back hill, and $\{\cdot\}$ is the fractional part.
* The isolated tree goes right of the grove when $c < W/2$, and left otherwise.
* Member offsets $o_k$ cycle through $0, 0.9, -0.8, 1.7, -1.5, 0.45, -0.35, 2.4$,
  with the leader at $k = 0$. Spacing is $0.025W$ on the back hill and $0.035W$
  otherwise.
* $\varphi = 0.6180339887498949$ and $i$ is the tree's index.
* Every position is then limited to $[0.04W, 0.96W]$.

The result is one grove of overlapping trees, one medium tree standing apart,
a straggler once there are enough trees, and open hillside elsewhere.

**Height on the hill.** Rim trees stand on the hill's curve. Interior trees
stand $0.30 + 0.35\,\{7\phi_i\}$ of the way down from the rim toward the floor,
where $\phi_i = \{\varphi(i + 1) + 0.25a\}$. The floor is the canvas bottom, or the
front hill for a back-hill tree. A back interior tree needs the back hill to
rise at least 24 above the front hill at its position. Otherwise it stands on
the back rim.

**Drawing order:** back hill, its trees, front hill, its trees. Within a hill,
trees are drawn in index order.

### Bit allocation

The remaining $R = 48 - c_b - 4 - \ell$ bits, where $c_b$ is the count code length (3
or 4) and $\ell$ is the leaf palette width (4 or 0), are split among the trees in proportion to group weight times role
weight. Nearer and more prominent trees get more branching detail:

| Group | Group weight | Perspective scale |
|---|---:|---:|
| Front interior | 8 | 1.4 |
| Front rim | 6 | 0.6 |
| Back interior | 4 | 0.45 |
| Back rim | 3 | 0.38 |

(One-hill mode: interior weight 8 and scale 1.3; rim weight 6 and scale 0.55.)

A tree's composition scale is its perspective scale times its prominence,
times a **depth factor** for slope trees on the front (or only) hill: the
further down the slope, the nearer the viewer and the larger the tree. With
$d$ the tree's fraction of the way down from the rim (0.30–0.65, see Height on
the hill), the factor is $1 + 0.6\,(d - 0.30) / 0.35$, from 1 to 1.6.

**Guarantee:** on the front (or only) hill, every slope tree's composition
scale is at least 1.5 times every rim tree's: $1.4 \cdot 0.8 = 1.12 \geq 1.5 \cdot 0.6
\cdot 1.15 = 1.035$, and in one-hill mode $1.3 \cdot 0.8 = 1.04 \geq 1.5 \cdot 0.55
\cdot 1.15 = 0.949$. The depth factor is at least 1, so it only widens the gap.
Slope trees stand nearest the viewer, so they read as
substantially larger than the rim trees, and far larger than the back hill's.
The per-tree height bits still vary the trunk on top of this scale. `make
verify` checks the ratio for every count and arrangement (worst case 1.656).

Each tree first gets $\lfloor R \cdot w_i / \sum w \rfloor$ bits. Leftover bits go
one at a time to the heaviest trees (ties by index), so every bit is used. The
header fully determines the allocation, so it is known before any tree bits are
read.

### Tree fields

A tree's bits are read in this order:

| Field | Bits | Meaning |
|---|---:|---|
| Girth | 2 | Trunk width |
| Height | 2 | Trunk length |
| Fork 1, 2, … | 3 each (ratio 2, length 1); deciduous 4 (ratio 2, side 1, length 1) | Breadth-first forks |

Leftover bits (fewer than one fork) extend the height field. One height step
moves the trunk by more than a pixel even on the smallest tree, while a finer
ratio on a deep twig could round to the same SVG coordinates. Fields with
fewer bits than listed use what is available, and a field with no bits takes
its midpoint. A value $v$ in a $b$-bit field becomes $u = v / (2^b - 1)$.

### Trunk

The tree's scale is $s = \text{size} \cdot \text{perspective} \cdot
\text{prominence}$. With girth value $g$ and height value $h$:

$$
L_0 = s \cdot 130 \cdot (0.6 + 0.8h) \cdot k_L,
\qquad
G_0 = s \cdot G_{\text{base}} \cdot (1 + 1.5g)
$$

| Species | $G_{\text{base}}$ | Taper per segment | $k_L$ |
|---|---:|---:|---:|
| Conifer | 9 | 0.82 | 0.38 |
| Deciduous | 11 | 0.7 | 1 |
| Cactus | 14 | 1.0 | 1 |
| Joshua tree | 13 | 1.0 | 1 |

A conifer's trunk is only its first stem segment, so it is short. The stem
keeps growing through its forks.

Every branch is drawn as a quadrilateral that narrows from its start girth to
girth × taper at its end.

### Forks

Forks are applied to branches in breadth-first order. A fork splits a branch
into a large child (the continuation) and a small child. The ratio value $u$
chooses the large child's share $r$ within a range per species:

| Species | Range of $r$ |
|---|---|
| Conifer | 0.85–0.97 (a strong stem with side branches) |
| Deciduous | 0.62–0.90 (no near-even splits, which would tile into a Y lattice) |
| Cactus | 0.60–0.90 |
| Joshua tree | 0.60–0.85 |

**Derived forks.** After the bit forks, branching continues. Derived fork $k$
reuses the bit fork $(k - n) \bmod n$, where $n$ is the number of bit forks, and
shifts its values by fixed irrational steps:

$$
u_r = \{u_{\text{ratio}} + 0.6180339887498949\,(k+1)\},
\qquad
u_\ell = \{u_{\text{length}} + 0.3819660112501051\,(k+1)\}
$$

A deciduous derived fork's side bit is the source fork's side bit, flipped when
$\{0.7548776662466927\,(k+1)\} < 0.5$. A tree with no bit forks uses 0.5 for both
base values and 0 for the side bit. Derived forks skip
branches shorter than a minimum: 12% of the trunk for conifers, 3% otherwise.
The total number of forks is capped per species, but bit forks always run in
full:

| Species | Fork cap |
|---|---:|
| Conifer | 127 |
| Deciduous | 511 |
| Joshua tree | 7 |
| Cactus | 3 |

### Branch angles

All rotations are exact, with no trigonometry or square root at run time.
Ratio-driven angles use the rational half-angle form

$$
(c, s) = \left(\frac{1 - t^2}{1 + t^2},\ \frac{2t}{1 + t^2}\right),
\qquad
t_{\text{large}} = \tan 22.5^\circ \cdot 2(1 - r),
\qquad
t_{\text{small}} = 1 - (1 - \tan 22.5^\circ) \cdot 2(1 - r)
$$

and fixed angles use constant $(\cos, \sin)$ pairs.

**Angles come from the ratio** for deciduous trees, cacti, and Joshua trees. The large
child tilts by about $90^\circ(1 - r)$ and the small one by about $90^\circ r$, to
opposite sides. Both are 45° at $r = 0.5$; at $r = 0.95$ they are about 5° and 87°.
For cacti and Joshua trees, the large child's side follows a fixed irregular
pattern, positive (counterclockwise) when $\{0.7548776662466927\,(k+1)\} < 0.5$.
The pattern does not alternate strictly, so the crown does not tile regularly.

**Deciduous forks.** The side bit sets which way the small child turns: 1 turns
it left (counterclockwise), 0 right, so limbs go both ways instead of all
hanging to one side. The large child (the winner) bends only half as much
($t = t_{\text{large}} / 2$) and turns back toward vertical: left if the parent leans
right, and the reverse. On a vertical parent it leans away from the small
child. The main trunk therefore keeps growing upward while the side limbs
spread out.

**Cactus arms** leave at the fork angle for 30% of their length, then turn
straight up, drawn as two segments.

**Conifers use fixed angles.**

* The main stem continues straight up.
* Side branches off the stem leave at an angle from vertical that droops lower
  down the tree. With $\rho$ = (height of the fork above the trunk base) / $4L_0$,
  limited to $[0, 1]$, the half-angle tangent is
  $t = \sqrt 3 + 2.6(1 - \rho) + w$, where the wobble $w$ is $+0.35$ at odd depths
  and $-0.15$ at even ones. That is about 115° from vertical near the top and
  about 156° near the base, so successive tiers vary around the trend instead of
  forming a regular fish bone.
* Successive stem forks put the large child's side on alternate sides by
  depth. Derived forks on the stem grow a full whorl: a side branch on each side
  (three children). Bit forks always have exactly two children.
* **Droop past the stem.** Every fork off the stem sags. The continuation bends
  10° toward the ground. Twigs leave the sagged continuation at 30° and hang a
  further 10°. A direction that already points more than about 35° below
  horizontal ($\sin 35^\circ = 0.574$) does not bend further. Boughs therefore arc
  downward instead of piercing straight out.

### Branch lengths and girths

With the length value $\ell$:

| Species | Child | Length factor | Girth factor |
|---|---|---|---|
| Deciduous | both | $L_0 \cdot (0.08 + 0.5\,\gamma)(0.85 + 0.3\ell)$, not relative to the parent | $0.35 + 0.65\,\text{share}$ |
| Cactus | both | $(0.5 + 0.35\,\text{share})(0.85 + 0.3\ell)$ | $0.75 + 0.25\,\text{share}$ |
| Joshua tree | both | $(0.5 + 0.35\,\text{share})(0.85 + 0.3\ell)$ | 1 (keeps the trunk's girth) |
| Conifer | stem, bit fork | $(0.62 + 0.2\,\text{share})(0.85 + 0.3\ell)$ | taper |
| Conifer | stem, derived whorl | $0.80\,(0.85 + 0.3\ell)$ | taper |
| Conifer | side branch off the stem | $(0.7 + 0.6\,\text{share})(0.85 + 0.3\ell)$ | taper $\cdot (0.35 + 0.65\,\text{share})$ |
| Conifer | deeper branch | $(0.45 + 0.35\,\text{share})(0.85 + 0.3\ell)$ | taper $\cdot (0.35 + 0.65\,\text{share})$ for twigs, taper for the continuation |

Factors multiply the parent's length and girth. The deciduous length is the exception:
it follows the child's thinness $\gamma$ = (its girth) / (the trunk's girth), so thinner
branches fork more often. A main limb thins by only about 10% per fork, so it
keeps its reach while its forks come closer together; thin side shoots stay
short and bushy, which fills the canopy. Conifer stems and branches
continue at the parent's end width, so they thin continuously. The short
internodes give many tiers, and the silhouette tapers into a cone.

**Conifer bough reach.** No bough reaches more than a third of the tree's
height (estimated as $4L_0$) from the stem, measured along its branches. Every
branch off the stem is scaled by $\max(\lambda, \min(1, (R - \text{before}) /
\text{longest}))$, where $R = 4L_0 / 3$ and before is the reach already used by
its parent. As for the floor, the scale does not depend on the branch's own
bits. Bit forks keep $\lambda = 0.3$; derived branches use $\lambda = 0$, and a derived
branch with less than 2% of its longest length left is not grown. Feathers
may extend a little past a bough's tip.

**Conifer bottom 10%.** The bottom 10% of a conifer stays clear of drooping
branches, so the trunk shows beneath the crown. The tree's height is estimated
as $4L_0$, giving a floor at $0.4L_0$ above the base. A downward branch is
scaled by

$$
\max\left(\lambda,\ \min\left(1,\ \frac{\text{room}}{\text{longest}}\right)\right),
\qquad
\text{room} = \frac{h_{\text{start}} - h_{\text{floor}}}{1.25 \cdot (-d_h)}
$$

where longest is the largest length that branch's factor can give, so the scale
depends only on where the branch starts and its direction, not on its own
bits. Bit forks keep $\lambda = 0.3$, so their length bits still change the
output. Derived forks carry no bits and use $\lambda = 0.02$.

### Foliage and color

**Colors use no bits.** They come from the color of the hill the tree stands on,
darkened per channel in integers ($c \cdot n / 100$), so trees always read as
darker than their hill and follow its biome and season:

| Part | Darkened to |
|---|---:|
| Trunk and branches | 40% |
| Foliage (by element index, cycling) | 52%, 58%, 48% |

| Species | Foliage |
|---|---|
| Conifer | A straight feather of needles around every branch: a hexagon widest near the branch base (half-width $0.18L + G$, times 0.6 on stem segments) and tapering past the tip. Stem feathers clothe the main trunk in needles; they use no bits. Feathers shrink toward their branch to stay above the bottom-10% floor, or are dropped. The stem ends in a needle-sharp tip, at least three times its end width. |
| Deciduous | Leaf dots (see Leaves) in spring, summer, and fall; bare in winter |
| Cactus | A round cap (radius = girth / 2) on each arm, in the trunk color |
| Joshua tree | A tuft of five spikes at each branch tip, at −60° to 60° from a constant table, of length $0.45L + 2G$ |

### Leaves

Deciduous trees in spring, summer, and fall carry leaves. The crown is meant
to read as 3D, so leaves float inside it as well as at its edge.

Each leaf is an eye shape that tapers to a point at both ends: two quadratic
arcs from $(-a, 0)$ to $(a, 0)$ with control points $(0, \pm 2w)$, so its
half-width at the middle is $w$. Its half-length is $a = 4.0s$, where $s$ is
the tree's scale. Its long axis points outward, along the leaf's direction from
its tip or fork (along the branch for a leaf on the tip itself).

**Darker leaves are narrower,** as if they had lost their strength. The width
follows the leaf's final color (after height shading, below), so it carries no
bits. With the fill's bytes $(R, G, B)$ and brightness
$\beta = (299R + 587G + 114B) / 255000$,

$$
w = 2.8s \cdot (0.35 + 0.8\,\beta)
$$

which runs from about half to full width across the leaf palettes.

* **Tips:** 2 to 5 leaves at every branch tip, centered $6.0s$ from the tip or
  on the tip itself.
  The count cycles 2, 3, 4, 5 along the tree's tips in left-to-right order, so
  clusters vary in density. Angles are from the branch direction, left to
  right:

  | Dots | Layout |
  |---:|---|
  | 2 | 60°, −60° |
  | 3 | 120°, 0°, −120° |
  | 4 | 120°, 0°, on the tip, −120° |
  | 5 | 120°, 60°, on the tip, −60°, −120° |
* **Forks:** 0–3 leaves floating around each fork point, centered $6.8s$ from it.
  The fork's ratio sets the count, with more dots for more even splits:
  3 for $r < 0.69$, 2 below 0.76, 1 below 0.83, and none above. One dot sits
  beyond the fork; two sit up-left and up-right (60° from the branch
  direction); three do all of these, left to right.

**Colors come from the leaf palette bits.** Each palette has six colors, built
from an OKLCH grid. Each palette spans most of the season's hue range with
moderate lightness and chroma contrast, interleaved so that neighboring leaves
differ: a crown reads as many leaves, not one color. Palette $p$ has hue step
$a = \lfloor p / 4 \rfloor$ and lightness step $b = p \bmod 4$. Color $j$ (0–5) has

$$
\text{hue} = h_0 + (h_1 - h_0)\left(0.7\,\eta_j + 0.15\,\frac{a}{3}\right),
\qquad
L = L_0 + \Delta L \cdot \lambda_j + 0.08\,\frac{b}{3},
\qquad
C = C_0 \cdot \kappa_j
$$

with the fixed interleaving tables

| $j$ | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---:|---:|---:|---:|---:|---:|
| $\eta_j$ (hue) | 0 | 0.6 | 0.2 | 1.0 | 0.4 | 0.8 |
| $\lambda_j$ (lightness) | 0.35 | 0.9 | 0 | 0.6 | 1.0 | 0.2 |
| $\kappa_j$ (chroma) | 0.92 | 1.1 | 1.0 | 0.9 | 1.08 | 0.97 |

| Season | Hue $h_0$–$h_1$ | $L_0$ | $\Delta L$ | $C_0$ | Look |
|---|---|---:|---:|---:|---|
| Spring | 90–145 | 0.585 | 0.18 | 0.15 | Yellow-green to fresh green |
| Summer | 110–165 | 0.40 | 0.19 | 0.13 | Olive to deep blue-green |
| Fall | 25–105 | 0.525 | 0.21 | 0.17 | Red, orange, gold, and olive |

All six colors in a palette are distinct, and the 16 palettes have distinct
first colors.

**Order.** Trees are taken from left to right by position (ties by index). Each
tree is traversed left to right: depth-first from the trunk, visiting the child
with the smaller direction $x$ first. A fork's dots come before its children's;
each tip's dots run from left to right, as in the table. Dots take the six colors in turn,
cycling continuously across all trees.

**Height shading.** Within each tree, leaves are slightly lighter toward the
top of the crown and darker toward the bottom, for a gentle gradient. With $u$
the leaf center's height between the tree's lowest ($-0.5$) and highest
($+0.5$) leaf centers, and $k = 0.3u$, each palette byte $c$ becomes
$\operatorname{round}(c + (255 - c)k)$ for $k > 0$ and
$\operatorname{round}(c(1 + k))$ otherwise: at most 15% toward white or black.
The shading follows the geometry, not the bits.

Winter trees are bare and the scene reads no leaf bits, so no bit is spent
without an effect.

### SVG

Each tree is a `<g class="tree" id="tree-N">` group, numbered from 1. One element
per line, in this order:

1. the trunk and every branch, breadth-first (a cactus arm is two paths),
2. conifer feathers, in branch order,
3. foliage on terminal branches, in branch order (paths, and circles for cacti),
4. deciduous leaves (forks and tips), in the tree's left-to-right order, as
   `<path>` elements (the eye shape, centered at the origin) placed by an exact
   `matrix(...)` built from the leaf's axis direction, so no angle or trigonometry is computed.

### Injectivity

For a fixed algorithm version, theme, season, target resolution, and hill
context, the map from the 48 Trees bits to the trees' SVG output is injective:

* The number of tree groups gives the count, which fixes the count code.
* For each arrangement and size pair, the allocation is known. Each field
  changes a known element: girth and height change the trunk, and bit fork $k$
  changes its two child branches (four elements for a cactus). Derived forks
  depend on the decoded values but add no bits of their own.
* The leaf palette sets the dot colors, and every scene with leaves has at
  least six dots, so all six colors appear.

`make verify` decodes every field by exact comparison of SVG lines, tries all 16
arrangement and size pairs, then tries every leaf palette on the whole set of
trees. It requires exactly one combination to reproduce the trees exactly,
including all derived detail and leaf colors.

Latest run: 200 hashes for each of the 16 biome and season pairs (16,463
trees). 0 mismatches, and no ambiguous pairs.

### Dependencies

* Consumes: the theme and season from Global, and the hill profiles, surfaces,
  and colors from Hills (generated first). The season sets the leaf colors and
  whether the leaf palette field exists; it also reaches trees through the hill
  color.
* Provides: tree placements and depths, used by Fog and for occlusion.

### Open decisions

* **Conifer droop is subtle at gallery size.** The first segment of each side
  branch is still straight, so boughs can read as straight spikes. Next step:
  draw each side branch as several segments that curve progressively downward.
* Winter deciduous trees are bare. The snow spots in the `SPECIFICATIONS.md`
  season table are not implemented.
* Groves of 10 trees can get dense in the center. The member spacing may need
  to widen.
* With few bit forks, the lowest conifer whorl can be one-sided.
* Positions are limited to $[0.04W, 0.96W]$. They depend only on the header, and
  verify confirms injectivity, but the limit is a clamp and should become an
  interval mapping.
* **Canvas top.** The larger slope trees can now reach the top of the canvas.
  There is no static bound yet.

### Status

Proposed and implemented in the reference renderer; visual tuning in progress.
