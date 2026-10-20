## Celestial Namespace

| Bit range | Width |
|---|---:|
| 40–47 | 8 |

### Overview

The Celestial namespace sets the sun. The sun is never drawn: it is implicit,
and the scene shows it only through its light. It shades the faces of Alpine, Tundra, and Desert ridges
(subspec/mountains.md, Ridge faces), and it does two things on the front (or
only) hill:

* It lights the hill's shading bands, tilting and spacing them
  (subspec/hills.md, Shading bands).
* It casts the shadows of the trees standing there onto the ground.

The sun is always behind the scene, so shadows fall forward, down the slope
toward the viewer, where they are long and easy to see. The back hill gets no
shadows.

### Bit layout

Fields are read most significant bit first, in this order:

| Field | Bits | Meaning |
|---|---:|---|
| azimuth $a$ | 4 | Direction of the sun around the ground: $15° + 10°a$, from 15° to 165° |
| elevation $e$ | 4 | Height of the sun above the ground: $20° + 3°e$, from 20° to 65° |

The azimuth is measured in the ground plane from the left $x$ axis, turning
behind the scene: 15° is a sun low on the left, 90° directly behind, and 165°
on the right. The elevation is measured up from that direction.

**Every bit always has an effect.** The bands are always drawn, and both
angles change them, so the sun is visible even on a front hill with no trees.

### Angles without trigonometry

The angles are fixed tables of decimal constants, so no trigonometry is
computed. For azimuth $a$, $q_a = \tan(\alpha_a / 2)$ is given to four
decimals, and the shadow direction on the ground is the exact rational unit
vector

$$
(d_x, d_y) = \left(\frac{1 - q_a^2}{1 + q_a^2},\; \frac{2q_a}{1 + q_a^2}\right)
$$

pointing away from the sun: $d_x$ to the right, $d_y$ toward the viewer.
For elevation $e$, the shadow length per unit of height is
$\ell_e = \cot \phi_e$, also to four decimals.

| $a$ or $e$ | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| $q_a$ | 0.1317 | 0.2217 | 0.3153 | 0.4142 | 0.5206 | 0.6371 | 0.7673 | 0.9163 |
| $\ell_e$ | 2.7475 | 2.3559 | 2.0503 | 1.8040 | 1.6003 | 1.4281 | 1.2799 | 1.1504 |

| $a$ or $e$ | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| $q_a$ | 1.0913 | 1.3032 | 1.5697 | 1.9210 | 2.4142 | 3.1716 | 4.5107 | 7.5958 |
| $\ell_e$ | 1.0355 | 0.9325 | 0.8391 | 0.7536 | 0.6745 | 0.6009 | 0.5317 | 0.4663 |

From these, the sun also sets two band parameters (subspec/hills.md, Shading
bands):

* **Tilt** $\tau = 0.15\,d_x$: the bands sit lower on the side the sun lights.
* **Spacing** $\beta = 0.25 - e/30$: how the band depths are spread.

### The ground surface

The front hill's ground is the same surface that shapes its bands. With
silhouette $S(x)$, the hill's lowest height $S_{\min}$, and depth fraction
$f$ (0 on the silhouette, 1 at the canvas bottom), the height is

$$
H(x, f) = (1 - f)\left((1 - f)\,S(x) + f\,S_{\min}\right) \quad (f \le 1),
\qquad
H(x, f) = (1 - f)\,S_{\min} \quad (f > 1)
$$

The surface follows the crest's contour near the top and flattens toward the
bottom. The ground runs $G = 1200$ logical units deep from silhouette to canvas
bottom.

### Shadows

Each tree on the front hill casts its trunk and branches, not its foliage or
leaves.

1. **The base's depth.** $H(x_b, f)$ falls steadily with $f$ on $[0, 1]$, so the
   base depth $f_b$ with $H(x_b, f_b) = h_b$ is found by 50 steps of bisection.
2. **Casting a point.** A point of the tree at $(x, h)$, a height
   $z = h - h_b$ above the base, lands on the ground at
   $x' = x + z\,\ell\,d_x$ and depth $f' = f_b + z\,\ell\,d_y / G$, at height
   $H(x', f')$. Shadows therefore follow the hill's contour and flatten toward
   the bottom like the bands. Shadows that run past the bottom leave the
   canvas.
3. **A limb's shadow.** Each trunk or branch segment, with end widths $w_a$ and
   $w_b$, becomes a quadrilateral between its two cast ends. It has half-widths
   $0.65\,w_a$ and $0.65\,w_b$, slightly thicker than the branch itself. The
   offset is the cast segment $(\Delta x, \Delta h)$ turned a quarter and
   divided by $\max(|\Delta x|, |\Delta h|)$, so no square root is needed.

**Consistency across trees.** Every tree uses the same direction
$(d_x, d_y)$ and the same $\ell$, so on the ground all shadows are parallel and
their lengths are proportional to height. On the canvas they are not parallel:

* The sideways offset $z\,\ell\,d_x$ is the same for every tree.
* The drop down the canvas is that ground offset taken through $H$, whose
  slope $\partial H / \partial f = -2(1 - f)S(x) + (1 - 2f)S_{\min}$ is steep
  at the rim and nearly flat at the bottom.
* So rim trees cast steeper shadows, and trees lower on the slope cast flatter,
  more sideways ones, as shadows do on a curved slope.

Heights are the drawn heights, which already include each tree's perspective
scale, while the ground depth $G$ is uniform. Near trees therefore cast
proportionally longer shadows than a strict perspective model would give.

### Sky warmth

A low sun paints the sky in sunset colors, warmest on the sun's side, even
though the sun itself is never drawn. The hue moves and the color deepens,
while the lightness stays. The cloud is lit too. No bits: it all follows from
the sun.

**Warmth** $w = (7 - e)/7$ for elevation steps $e \le 6$ (a sun at 38° or lower),
and 0 above. That is full at 20° and gone by 41°.

**Gradient.** The sky gradient (subspec/sky.md, Tint) has four stops at offsets
0, 0.45, 0.75, and 1. Stop $s$ starts from the untinted gradient's color at its
offset, from the sky's tint at the top to `#E9EEF2` at the horizon, interpolated
in sRGB. Converted to OKLCH, with $k = w\,\omega_s$:

* the lightness stays, except the top stop, which deepens by $0.04\,k$;
* the chroma grows by $0.15\,k$;
* the hue turns the shortest way toward a target by the fraction $k$.

| Stop | 0 (top) | 0.45 | 0.75 | 1 (horizon) |
|---|---|---|---|---|
| Target hue | 285° violet | 335° magenta-pink | 20° coral | 48° amber |
| Weight $\omega_s$ | 0.55 | 0.75 | 0.9 | 1 |

So as the sun drops, the sky runs from deepening violet at the top, through
pink and coral, to amber at the horizon.

**Sun glow.** When $w > 0$, a full-canvas `<rect class="sun-glow">` over the sky
is filled with a radial gradient. It is centered low in the sky on the sun's
side: $x = \tfrac12(1 - d_x)\,W$, $y = 0.8\,H$, radius $0.75\,W$. Its color is
the horizon's lightness less 0.03, chroma $0.05 + 0.10\,w$, hue 45°, at
opacity $\operatorname{round}(70w)/100$ fading to 0 at the rim.

**Cloud.** The cloud is white when $w = 0$, and otherwise OKLCH
$(0.97 - 0.07w,\; 0.12w,\; 25° + 30°w)$: pink and gold under a low sun.

The colors are constant tables, computed once in the reference implementation:

* per tint and elevation step, four stops and a glow;
* per elevation step, a cloud color.

### Rendering

Each tree's shadow is one `<path class="shadow">` with one closed subpath per
limb, in its darkest band color. Shadows are drawn after the front hill's bands
and before its vegetation and trees, so the ground cover and the trees stand
on top of them.

### Verification

`make verify` decodes the sun from the SVG text alone. For sample scenes in
every biome and season, all 256 suns are tried on the same hills and trees, and
exactly one must reproduce the bands and shadows. Scenes with bare front hills
are included.

### Dependencies

* Consumes: the front hill's silhouette from Hills, and the tree bases and
  limbs from Trees.
* Provides: the sun's light on the front hill (bands and shadows), on the ridge
  faces, and on the sky (Sky warmth).

### Open decisions

* Whether Sky should later follow the sun, for example a warmer sky for a low
  sun.
* Ranges: the azimuth and elevation spans, the ground depth $G$, and the shadow
  width.
* Whether to cast from true heights (the drawn height divided by the tree's
  perspective scale) so lengths are consistent across depths.
