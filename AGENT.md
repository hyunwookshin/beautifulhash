# Agent instructions

Guidance for AI agents and contributors working on BeautifulHash.

## Project

BeautifulHash turns a 256-bit hash into a layered mountain landscape in SVG.
The algorithm is treated like a protocol: once a version is frozen, its output
must never change.

* `SPECIFICATIONS.md`: top-level spec (input contract, namespaces, guarantees).
* `subspec/*.md`: one spec per namespace. `subspec/mountains.md` and
  `subspec/hills.md` are fully specified and implemented.
* `core/`: C++ reference implementation (`generate_scene`, `render_svg`).
* `tools/verify.cpp`: correctness checks. `tools/gallery.cpp`: sample gallery.
  `tools/variants.hpp`: configurations both tools run.

## Commands

```sh
make            # build into build/
make verify     # all checks; must print ALL CHECKS PASSED
make gallery    # render gallery/index.html
```

`build/` and `gallery/` are generated and git-ignored. C++ only; do not add
Python to the project.

## Invariants every change must keep

* **Bit ownership:** a namespace reads only its own bit range, through
  `BitReader` on its slice. Every bit is consumed exactly once in every layout.
* **No rehashing:** payload bits are decoded directly as unsigned integers,
  most significant bit first. Never hash, mix, or feed them through a PRNG.
* **SVG injectivity:** distinct inputs must give distinct SVG bytes for a fixed
  version, theme, and resolution. `make verify` proves this empirically by
  rendering random hashes, parsing the SVG text back, and decoding the bits.
* **No clamping:** never clamp a hash-derived value. Compute the allowed
  interval first, then map bits into it injectively.
* **Monotonic silhouettes:** ridge x coordinates are strictly increasing.
* **Determinism:** geometry uses only `+ - * / abs min max` and comparisons in
  binary64. No `sqrt`, no transcendental functions, no FMA contraction
  (`-ffp-contract=off`). Use constant tables where a curve is needed.
* **Formatting:** coordinates go through `append_fixed2` (two decimals,
  round-half-even, locale-free). Never use `printf` or iostreams for SVG output.
* **Styling never changes geometry:** biome styling runs after all ridges are
  placed. Textures may only insert vertices with a fixed structure and must
  never move a bit-driven point; update `styled_depth` and
  `texture_min_fraction` when a texture changes.
* **Palettes:** every palette entry must serialize to a distinct `#RRGGBB`.
  Gamut clipping and narrow hue ranges cause collisions; verify catches them.
* **Canvas budget:** placement relies on exact worst-case heights from
  exhaustive search. If shape parameters change, re-run verify and confirm the
  slack S stays positive.

## Workflow

* Change the spec together with the code. If an algorithm or default changes,
  update `subspec/mountains.md` (and `SPECIFICATIONS.md` if needed) in the same
  change, including worked numbers, which should be computed, not estimated.
* Run `make verify` before every commit, and report failures plainly.
* Judge visual changes from rendered galleries on identical hashes. Add
  variants to `tools/variants.hpp` to compare one parameter at a time, and
  view the results at the gallery's display size, not only zoomed in.
* Keep experimental work uncommitted until the maintainer chooses a direction.
* Commit or push only when asked. Commit messages have no `Co-Authored-By`
  or other AI attribution lines.

## Spec writing conventions

* Markdown headings use `#` levels, never numbered headings.
* Use real tables, bullet lists, fenced code blocks, and `$…$` / `$$…$$` math.
* State guarantees precisely, separate guarantees from objectives, and mark
  provisional values as provisional.
