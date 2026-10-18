## Mountains Namespace

| Bit range | Width |
|---|---:|
| 48–135 | 88 |

### Overview

The Mountains namespace draws the mountain ridges that form the distant
skyline: two or three hash-derived silhouettes, each with its own color, with
taller and more distant ridges set further back. Ridge 1 is the nearest.
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

Every ridge uses the same recursive algorithm. Only its subdivision depth and
the number of bits per point differ, according to its layout.

The number of peaks is not a parameter; peaks emerge from the displacements.
Coarse levels set the major shape, and each deeper level adds finer detail.

#### Coordinate spaces

Each ridge's shape $Q$ is generated in a normalized, anchor-relative profile space:

* $x$ runs from $0$ to $W = 2048$ (the logical canvas width).
* $h$ is elevation relative to the ridge's left anchor, which sits at $h = 0$.

Generation of the shape happens entirely in profile space. The theme's vertical
scale and the ridge's placement in the scene are applied only afterward (see
Roughness and vertical scale, and Ridge placement).

#### Anchors

An anchor with $b$ bits reads them as $v \in [0, 2^b - 1]$ and maps them to

$$
h = h_{\min} + v \, \frac{h_{\max} - h_{\min}}{2^b - 1}
$$

The proposed anchor range is $h_{\max} - h_{\min} = 0.25W = 512$.

The two decoded anchor values $h_L$ and $h_R$ are split into two roles:

* **Tilt** $d = h_R - h_L$ shapes the ridge. The shape starts from
  $A_0 = (0, 0)$ and $A_1 = (W, d)$.
* **Extra elevation** $e = h_L - h_{\min} \in [0, 512]$ raises the whole ridge
  above its minimum visible placement (see Ridge placement).

$(h_L, h_R) \mapsto (d, e)$ is a bijection, so no anchor information is lost.

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

This holds for every hash and does not depend on slope, so deeper layouts never
risk folding.

#### Roughness and vertical scale

Two independent theme parameters control the profile:

* **Roughness $r$:** how far each subanchor may move relative to its segment.
  It controls how jagged the ridge is at every scale. Default: $0.1$.
* **Vertical scale $s$:** applied after generation, mapping each profile point
  $(x, h)$ to $(x, s \cdot h)$. It controls how tall the ridge is without
  changing its fractal character. Default: $1$.

Vertical scaling leaves every $x$ unchanged, so it cannot break monotonicity.

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
\text{output}_i(x) = s \cdot Q_i(x) + \underbrace{s \cdot e_i + T_i}_{\text{offset}_i}
$$

* $e_i$ is the ridge's extra elevation, decoded from its left anchor.
* $T_i$ is the minimum placement offset, defined below.

Ridges are placed front to back: Ridge 1, then Ridge 2, then Ridge 3.

#### Minimum placement offset

**Ridge 1** has nothing in front of it. Its $T_1$ is a fixed foreground level
supplied by the theme and Global's composition settings.

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

where $\rho$ is the visibility ratio (proposed $0.2$, so at least 20% of the
ridge's own height is exposed) and $E_{\min}$ is a small absolute floor, so a
nearly flat ridge still shows clearly. Both are artistic parameters to tune from
samples.

$T_i$ is the offset at which the ridge, with zero extra elevation, is exposed by
exactly $E_i$:

$$
T_i = E_i - \max_x \left( s Q_i(x) - F(x) \right)
$$

With extra elevation $e_i \ge 0$, the actual exposure is $E_i + s \cdot e_i \ge E_i$.
So every ridge is always visible by at least the required amount, and the
anchor bits raise it further.

The maximum of $s Q_i(x) - F(x)$ is reached at a vertex of $Q_i$ or of one of the
front ridges, because all of them are piecewise linear. It is computed by
evaluating the difference at every such $x$, using linear interpolation between
vertices. This uses only `+`, `-`, `*`, `/`, and `max`.

#### The rule that preserves injectivity

$T_i$ may depend on the ridges in front and on the ridge's own shape $Q_i$, but
**never on its own extra elevation $e_i$**.

This matters because raising both anchors by the same step shifts a ridge
rigidly upward. If $T_i$ were computed from the ridge's absolute height, it
could cancel that shift exactly, and two different hashes would produce the same
SVG. Because $Q_i$ is anchor-relative, it carries no information about $e_i$, so
the offset cannot undo it.

### Ridge color

Each ridge has a base color index: 8 bits (256 colors) for Ridges 1 and 2, and
4 bits (16 colors) for Ridge 3. Distant mountains are more uniform in color, so
Ridge 3 spends its budget on geometry instead.

#### Palette mapping

The index is not split into RGB channels. Instead, each theme maps the indices
to coordinated colors, either:

* **A curated palette:** hand-designed colors per theme, or
* **A perceptual grid:** a structured mapping into a perceptual color space such
  as OKLCH, within theme-defined ranges. For example, 4 lightness × 8 hue ×
  8 chroma steps = 256.

A theme may use a different palette for each ridge, for example lighter and
cooler colors for farther ridges. Ridge 3 uses a 16-entry palette.

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

### Bit layouts (provisional)

Within each ridge, bits are consumed in this order: left anchor, right anchor,
Level 1, Level 2, Level 3, Level 4 (each level left to right), then color.
Bit positions below are namespace-relative; add 48 for the absolute bit index.

#### Two-ridge mode (header = 0)

Both ridges use four subdivision levels, giving 17-point silhouettes.

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
| Left anchor | 3 | 1–3 | 49–51 |
| Right anchor | 3 | 4–6 | 52–54 |
| L1.1 | 7 | 7–13 | 55–61 |
| L2.1, L2.2 | 3 each | 14–16, 17–19 | 62–64, 65–67 |
| L3.1 – L3.4 | 2 each | 20–21, 22–23, 24–25, 26–27 | 68–69, 70–71, 72–73, 74–75 |
| L4.1 – L4.8 | 1 each | 28, 29, …, 35 | 76, 77, …, 83 |
| Color | 8 | 36–43 | 84–91 |

**Ridge 2** (bits 44–87, absolute 92–135):

| Slot | Bits | Namespace bits | Absolute bits |
|---|---:|---|---|
| Left anchor | 4 | 44–47 | 92–95 |
| Right anchor | 4 | 48–51 | 96–99 |
| L1.1 | 6 | 52–57 | 100–105 |
| L2.1, L2.2 | 3 each | 58–60, 61–63 | 106–108, 109–111 |
| L3.1 – L3.4 | 2 each | 64–65, 66–67, 68–69, 70–71 | 112–113, 114–115, 116–117, 118–119 |
| L4.1 – L4.8 | 1 each | 72, 73, …, 79 | 120, 121, …, 127 |
| Color | 8 | 80–87 | 128–135 |

#### Three-ridge mode (header = 1)

Ridges 1 and 2 use three subdivision levels (9 points). Ridge 3 uses two levels
(5 points), a simpler silhouette with more bits per point.

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
| Left anchor | 3 | 1–3 | 49–51 |
| Right anchor | 3 | 4–6 | 52–54 |
| L1.1 | 6 | 7–12 | 55–60 |
| L2.1, L2.2 | 2 each | 13–14, 15–16 | 61–62, 63–64 |
| L3.1 – L3.4 | 2 each | 17–18, 19–20, 21–22, 23–24 | 65–66, 67–68, 69–70, 71–72 |
| Color | 8 | 25–32 | 73–80 |

**Ridge 2** (bits 33–64, absolute 81–112):

| Slot | Bits | Namespace bits | Absolute bits |
|---|---:|---|---|
| Left anchor | 3 | 33–35 | 81–83 |
| Right anchor | 3 | 36–38 | 84–86 |
| L1.1 | 6 | 39–44 | 87–92 |
| L2.1, L2.2 | 2 each | 45–46, 47–48 | 93–94, 95–96 |
| L3.1 – L3.4 | 2 each | 49–50, 51–52, 53–54, 55–56 | 97–98, 99–100, 101–102, 103–104 |
| Color | 8 | 57–64 | 105–112 |

**Ridge 3** (bits 65–87, absolute 113–135):

| Slot | Bits | Namespace bits | Absolute bits |
|---|---:|---|---|
| Left anchor | 3 | 65–67 | 113–115 |
| Right anchor | 3 | 68–70 | 116–118 |
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

Anchor values differ only in elevation, so their step is exactly
$s \cdot \frac{h_{\max} - h_{\min}}{2^b - 1} \cdot \frac{\text{width}}{2048}$.

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

Default configuration: $r = 0.1$, $\gamma = 0.5$, $s = 1$, so
$\min(r, \gamma/2) = \min(0.1, 0.25) = 0.1$ and $\min(1, s) = 1$. At 1024 × 768 the
resolution factor is $0.5$. So for subanchors

$$
\delta = \frac{2 \times \Delta x_{\min} \times 0.1}{\sqrt2 \times (2^b - 1)} \times 0.5
$$

The numerator $2 \times \Delta x_{\min} \times 0.1$ is 409.6 at Level 1, 102.4 at Level 2,
25.6 at Level 3, and 6.4 at Level 4.

For example, a 7-bit Level 1 point: $\ell \ge 409.6 / 127 = 3.225$, then
$3.225 / \sqrt2 = 2.281$, then $\delta = 2.281 \times 0.5 = 1.140$ px.

Every distinct (level, bits) combination used by either layout:

| Slot | $b$ | Used by | $\Delta x_{\min}$ | $\ell_{\min}$ (logical) | $\ell_{\min}/\sqrt2$ | $\delta$ at 1024 × 768 |
|---|---:|---|---:|---:|---:|---:|
| Anchors | 3 | 2R: R1; 3R: R1, R2, R3 | — | 512 / 7 = 73.14 | (vertical) | 36.57 px |
| Anchors | 4 | 2R: R2 | — | 512 / 15 = 34.13 | (vertical) | 17.07 px |
| Level 1 | 6 | 2R: R2; 3R: R1, R2 | 2048 | 409.6 / 63 = 6.502 | 4.597 | 2.30 px |
| Level 1 | 7 | 2R: R1; 3R: R3 | 2048 | 409.6 / 127 = 3.225 | 2.281 | **1.14 px** |
| Level 2 | 2 | 3R: R1, R2 | 512 | 102.4 / 3 = 34.13 | 24.14 | 12.07 px |
| Level 2 | 3 | 2R: R1, R2; 3R: R3 | 512 | 102.4 / 7 = 14.63 | 10.34 | 5.17 px |
| Level 3 | 2 | 2R: R1, R2; 3R: R1, R2 | 128 | 25.6 / 3 = 8.533 | 6.034 | 3.02 px |
| Level 4 | 1 | 2R: R1, R2 | 32 | 6.4 / 1 = 6.400 | 4.525 | 2.26 px |

The smallest guaranteed step is **1.14 px**, from the 7-bit Level 1 points. After
halving for the placement offset (Step 6), the guaranteed output difference is
still 0.57 px, about 57 times the 0.01 quantum. The smallest final $x$-extent (8 logical, 4 px at
1024 × 768) likewise keeps rounded $x$ coordinates strictly increasing.

These are worst-case bounds. Typical segments are about twice as wide at each
level, rather than a quarter, so real steps are usually much larger.

### Injectivity of the 88-bit mapping

**Claim.** For a fixed algorithm version, theme, and target resolution, the map
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
   * **Shape slot (tilt or subanchor):** order the shape's points geometrically:
     right anchor, then Level 1, Level 2, and so on, left to right. Take the
     first point whose value differs. All of its ancestors have identical values,
     and are computed by identical binary64 operations, so they are bit-for-bit
     equal. Its parents $A$ and $B$, and hence its interval $[-t_{\max}, t_{\max}]$,
     are therefore identical for $X$ and $Y$. Within that interval the map from
     value to point is strictly increasing, so the two shapes differ at this
     point by at least $\delta$. By Step 6, some serialized coordinate differs.
   * **Only the extra elevation differs:** the shapes $Q$ are identical, and $F$
     is identical, so $T$ is identical. The offsets then differ by $s$ times a
     whole anchor step, so the left anchor's serialized elevation differs.

Either way, the fragments differ. ∎

**Scope and visibility.** This establishes SVG uniqueness for the Mountains
namespace. No ridge is ever emitted without being part of the selected layout,
so no bits live in hidden placeholder geometry, and the placement rule
guarantees that every present ridge is partly visible. Portions of a
background ridge can still be covered by the ridges in front. Those portions
remain in the SVG, so SVG uniqueness holds, but that detail is not visible.

### Dependencies

* Consumes: the theme and broad composition constraints from Global (roughness,
  vertical scale, color palettes, foreground level) and the lighting mode from Sky.
* Internal: each ridge's placement depends on the already placed ridges in
  front of it. This uses their decoded geometry, never their raw bits.
* Provides: ridge silhouettes, colors, and depths, used by Fog and for occlusion.

### Open decisions

* Both layouts and their per-point bit widths, pending rendered samples.
* Whether the 1-bit Level 4 points in two-ridge mode look too regular. If so,
  the same 8 bits could encode the eight Level 4 points jointly in base 3 (8 bits
  give 256 values, fewer than $3^8 = 6561$, so the encoding stays injective),
  letting points also take a centered position. Only some combinations would be
  reachable.
* Curated palettes versus a perceptual-grid mapping, per theme.
* Whether lighting rewrites ridge fills or is applied as separate SVG elements.
* Anchor elevation range $h_{\min}, h_{\max}$. It now sets both the tilt range
  and the extra-elevation range; these could be given separate ranges.
* Visibility ratio $\rho$ (proposed 0.2) and the absolute floor $E_{\min}$.
* Ridge 1's foreground level $T_1$.
* Whether a background ridge's peak should also be required to exceed the
  front ridges' peak, in addition to the visibility constraint.
* Per-theme roughness $r$ and vertical scale $s$.
* The monotonicity margin $\gamma$.
* Decimal places $p$ in SVG coordinate output.
* Whether ridges may extend beyond the top of the canvas, or must be bounded.

### Status

The ridge algorithm, both layouts, color requirements, serialization rules, and
injectivity argument are proposed. Layouts are provisional until sample
landscapes have been rendered.
