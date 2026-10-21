## Global Namespace

| Bit range | Width |
|---|---:|
| 0–15 | 16 |

### Overview

The Global namespace makes the decisions that shape the whole landscape before
any individual layer is generated. It selects the environmental theme (Alpine,
Temperate, Tundra, or Desert) and the season (winter, spring, summer, or fall),
and sets the overall composition, such as how the scene is balanced and framed.

Every other namespace builds on these choices, so Global is generated first.

### Subnamespaces (provisional)

| Subnamespace | Namespace bits | Absolute bits | Width | Values |
|---|---|---|---:|---|
| Daylight | 0–1 | 0–1 | 2 | Day, refining the sun's direction (0–2), or night (3) |
| Theme | 2–3 | 2–3 | 2 | Alpine, Temperate, Tundra, Desert |
| Season | 4–5 | 4–5 | 2 | Winter, spring, summer, fall |
| Composition | 6–15 | 6–15 | 10 | Not yet defined |

Theme and season are each read directly as a 2-bit value, so all four biomes
and all four seasons are equally likely. The season's effects cascade through
Mountains, Hills, Trees, and Vegetation (see SPECIFICATIONS.md, Seasons).

Only the daylight field is implemented. Theme and season still come from the
gallery configuration.

### Daylight

The daylight field $d$ (2 bits) makes one scene in four a night.

* **Day** ($d = 0, 1, 2$): the sun's azimuth (subspec/celestial.md) moves on by
  $d$ thirds of its 10° step, to $15° + 10°a + \tfrac{10}{3}°d$. The time of
  day is already the sun's position, so the field refines it rather than adding
  a separate mood. The front hill's bands always tilt with the sun, so each
  value always changes the picture.
* **Night** ($d = 3$): the light is the moon's. The Celestial bits place it on
  the plain 10° steps, and it lights and shades the scene as the sun does, with
  the night treatment below.

### Night

At night the scene is a moonlit dusk: a deep blue sky, darker and cooler
colors, and strong, sculpted light on the mountains.

**Sky.** The sky gradient (subspec/sky.md, Tint) takes night colors. With
the tint's top color in OKLCH $(L, C, h)$, the top is
$(0.24 + 0.8(L - 0.70),\; 0.02 + 0.4C,\; h + 22°)$: a navy that keeps each
tint's own steps, so all 256 stay distinct. The gradient runs to a rich dusk
blue at the horizon, $(0.50, 0.11, 252°)$, interpolated in OKLCH with the
stops at 0, 0.45, 0.75, and 1 placed at 0, 0.12, 0.4, and 1 of the way down, so
the blue glows near the bottom. The moon's height does not change the sky.
Instead of the sun glow, a cool halo, $(0.70, 0.04, 228°)$ at 30% opacity,
sits on the moon's side, and the cloud is a moonlit gray-blue,
$(0.70, 0.025, 240°)$.

**Every other color.** Each color in the scene is moved by a depth
$\varphi$ from 0 (nearest) to 1:

$$
L' = 0.03 + L\,(0.40 + 0.30\varphi) + 0.06\varphi, \qquad
C' = C\,(0.5 + 0.1\varphi)(0.55 + 0.45\nu), \qquad
h' = h + t\,\nu\,(0.3 + 0.25\varphi)
$$

where $t$ is the shortest turn from $h$ to 245° (blue) and
$\nu = \max(0, 1 - |t|/120°)$. Hues near blue turn toward it; warm hues far
from it only lose their color, as reds fade first at dusk, rather than
spinning through magenta or green.

| Layer | Depth $\varphi$ |
|---|---|
| Ridges, front to back | 0.45 to 0.85, evenly; Temperate 0 to 0.25, nearly silhouettes |
| Snow caps and their facets | the ridge's depth plus 0.1, at most 1 |
| Front hill, back hill | 0.12, 0.22 |
| Trees on the front hill, back hill | 0.08, 0.18 |
| Vegetation | 0.08 |
| Fog | 0 |

Temperate ridges are wooded and have no moonlit faces, so they go nearly to
silhouettes against the dusk sky, the far ones a little lighter.

**Less haze.** The fog spans the whole canvas, sky included, so it takes the
nearest, darkest shift: a navy haze that keeps the sky dark. It also thins by
14 hundredths at its top and bottom (subspec/fog.md), from 27–36% to 13–22%. A
constant step keeps every density and mist distinct, so the fog bits still
show and decode.

**Sculpted ridges.** With less glow in the air, the moon's light carves the
ridges (subspec/mountains.md, Ridge faces). Each peak's faces are lit evenly,
following the ridge's geometry rather than the screen height (no vertical
fade):

* the moon-facing half: a dim blue-gray, `#AFBFD6` at 22%, lighter than the
  sky but never glaring;
* the ridge's own night color between the halves: a muted slate;
* the half turned away: a deep navy, the ridge color mixed 70% toward
  `#0B1426` at 62%, almost merging into the night.

There is no alpenglow at night, and glints on snow are a dim `#C4D0E2`. The
moon's halo in the sky is 30%. The ridge facets carry most of the contrast:
more of them are drawn (signed squared cosine above 0.44 and 0.30 for the two
light steps, below 0.09 and 0.025 for the dark ones), and their steps grow to
+24%, +12%, −18%, and −36%, so the moonlit faces break into shards like a
sculpture lit from one side, with the contrast coming from shadow rather than
glare.

**Distinct at night.** Darkening squeezes colors together, so two neighboring
palette entries can round to the same `#RRGGBB`. The colors that carry bits,
the ridge, its fall gradient bottom, the hill, and the fog tint, therefore go
through a night table per palette and depth: entries in index order take the
shift above, and one that would repeat an earlier entry's color moves to the
nearest unused color, trying steps of one in each channel at growing distance
in a fixed order. Every entry stays distinct, and the nudge is at most a few
units. Leaf and vegetation palettes stay distinct under the plain shift.

The colors decode as by day: `make verify` checks that every palette keeps its
colors distinct after the night shift at every depth it is drawn at, that the
256 night skies are distinct, and it decodes the daylight field with the sun.

### Dependencies

* Consumes: none.
* Provides: daylight (to Celestial, which reads it with its own bits, and to
  every layer at night), and the selected theme, season, and composition
  settings to all other namespaces. They receive these values as context and
  never read Global's bits.

### Status

The daylight field is implemented. Theme and season widths are provisional; the
value order within each field and the composition layout are not yet defined.
