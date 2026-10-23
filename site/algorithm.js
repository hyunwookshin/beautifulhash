// BeautifulHash algorithm page: draws each exhibit's figure from the formulas
// on its placard (subspec/*.md), with fixed values in place of hash bits.
// Line art only: strokes in the page's ink, guides dashed, one accent.
"use strict";

const SVGNS = "http://www.w3.org/2000/svg";
function el(parent, tag, attrs = {}, text) {
  const e = document.createElementNS(SVGNS, tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  if (text !== undefined) e.textContent = text;
  parent.appendChild(e);
  return e;
}
const f1 = (v) => (Math.round(v * 10) / 10).toString();
const pathOf = (pts, close) => "M" + pts.map((p) => `${f1(p[0])},${f1(p[1])}`).join("L") + (close ? "Z" : "");
const line = (g, a, b, cls) => el(g, "line", { x1: f1(a[0]), y1: f1(a[1]), x2: f1(b[0]), y2: f1(b[1]), class: cls });
const poly = (g, pts, cls, close) => el(g, "path", { d: pathOf(pts, close), class: cls });
const dot = (g, p, cls = "dot", r = 4) => el(g, "circle", { cx: f1(p[0]), cy: f1(p[1]), r, class: cls });
function label(g, p, text, cls = "", anchor = "start") {
  const t = el(g, "text", { x: f1(p[0]), y: f1(p[1]), class: cls, "text-anchor": anchor });
  for (const part of text.split(/(_\{[^}]*\}|_.)/)) {
    if (!part) continue;
    if (part[0] === "_") el(t, "tspan", { class: "sub", dy: "4" }, part[1] === "{" ? part.slice(2, -1) : part.slice(1)), el(t, "tspan", { dy: "-4" }, "\u200b");
    else el(t, "tspan", {}, part);
  }
  return t;
}
const note = (g, p, text, anchor = "start") => label(g, p, text, "note", anchor);
// An arc of points from angle a0 to a1 (radians, screen orientation) around c.
const arc = (c, r, a0, a1, n = 24) => Array.from({ length: n + 1 }, (_, i) => {
  const a = a0 + ((a1 - a0) * i) / n;
  return [c[0] + r * Math.cos(a), c[1] + r * Math.sin(a)];
});
function arrowHead(g, from, to, cls = "ink", size = 8) {
  const dx = to[0] - from[0], dy = to[1] - from[1], len = Math.hypot(dx, dy);
  const ux = dx / len, uy = dy / len;
  poly(g, [[to[0] - size * ux - size * 0.5 * uy, to[1] - size * uy + size * 0.5 * ux], to,
           [to[0] - size * ux + size * 0.5 * uy, to[1] - size * uy - size * 0.5 * ux]], cls);
}
function hatch(svg, id, gap, angle) {
  const defs = svg.querySelector("defs") || el(svg, "defs");
  const p = el(defs, "pattern", { id, width: gap, height: gap, patternUnits: "userSpaceOnUse", patternTransform: `rotate(${angle})` });
  el(p, "line", { x1: 0, y1: 0, x2: 0, y2: gap, class: "hatch" });
}
// A small deterministic generator standing in for hash bits.
function rng(seed) {
  let s = seed >>> 0;
  return () => {
    s = (s + 0x6d2b79f5) >>> 0;
    let t = s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
const frac = (v) => v - Math.floor(v);

// ---- I. The ridge: constrained midpoint displacement ------------------------
// Screen points are (x, y) with y down; heights h = -y.
function subdivide(A, B, r, t) {
  const dx = B[0] - A[0], dh = -(B[1] - A[1]);
  const M = [(A[0] + B[0]) / 2, (A[1] + B[1]) / 2];
  return [M[0] + r * t * -dh, M[1] - r * t * dx];
}
function tMax(A, B, r, gamma = 0.5) {
  const dx = B[0] - A[0], dh = Math.abs(B[1] - A[1]);
  return dh === 0 ? 1 : Math.min(1, (gamma * dx) / (2 * r * dh));
}
function figRidge(svg) {
  const g = el(svg, "g");
  // One step, drawn large (r exaggerated to 0.22 so the band is visible).
  const A = [36, 300], B = [372, 206], r = 0.22;
  const M = [(A[0] + B[0]) / 2, (A[1] + B[1]) / 2];
  const P1 = subdivide(A, B, r, 1), Pm = subdivide(A, B, r, -1), P = subdivide(A, B, r, 0.55);
  line(g, A, B, "guide");
  line(g, Pm, P1, "guide");
  for (const end of [P1, Pm]) {
    const ux = (B[0] - A[0]) / 2 / 168, uy = (B[1] - A[1]) / 2 / 168;
    line(g, [end[0] - 9 * ux, end[1] - 9 * uy], [end[0] + 9 * ux, end[1] + 9 * uy], "ink thin");
  }
  // right-angle mark at M
  const ab = [B[0] - A[0], B[1] - A[1]], abl = Math.hypot(...ab), pa = [P1[0] - M[0], P1[1] - M[1]], pal = Math.hypot(...pa);
  const u = [ab[0] / abl * 10, ab[1] / abl * 10], v = [pa[0] / pal * 10, pa[1] / pal * 10];
  poly(g, [[M[0] + u[0], M[1] + u[1]], [M[0] + u[0] + v[0], M[1] + u[1] + v[1]], [M[0] + v[0], M[1] + v[1]]], "ink thin");
  poly(g, [A, P, B], "acc");
  dot(g, A); dot(g, B); dot(g, M, "dot hollow", 3.5); dot(g, P, "dot accent", 4.5);
  label(g, [A[0] - 4, A[1] + 24], "A");
  label(g, [B[0] + 8, B[1] + 18], "B");
  label(g, [M[0] + 14, M[1] + 16], "M");
  label(g, [P[0] - 24, P[1] - 8], "P");
  note(g, [P1[0] + 12, P1[1] - 2], "t = +1");
  note(g, [Pm[0] + 12, Pm[1] + 6], "t = −1");
  note(g, [(M[0] + P1[0]) / 2 - 64, (M[1] + P1[1]) / 2 + 2], "r·|AB|");

  // The same rule, repeated: one ridge after 1, 3 and 7 levels (r = 0.1).
  const draw = (levels, y0) => {
    const rnd = rng(0x2b1d);
    const sx = 186 / 2048, sh = 0.12, base = [418, y0];
    const toS = (x, h) => [base[0] + x * sx, base[1] - h * sh];
    let pts = [toS(0, 0), toS(2048 * 0.36, 380), toS(2048, 160)];
    for (let k = 0; k < levels; k++) {
      const next = [pts[0]];
      for (let i = 1; i < pts.length; i++) {
        const a = pts[i - 1], b = pts[i];
        const v3 = Math.floor(rnd() * 8);
        next.push(subdivide(a, b, 0.1, tMax(a, b, 0.1) * ((2 * v3) / 7 - 1)), b);
      }
      pts = next;
    }
    poly(g, pts, "ink");
    note(g, [418, y0 + 22], `level ${levels} · ${pts.length - 1} segments`);
  };
  draw(1, 96); draw(3, 206); draw(7, 316);
  line(g, [400, 30], [400, 350], "rule");
}

// ---- II. The tree: recursive forks -------------------------------------------
function rotate([x, y], t, sign) {
  // (c, s) from the half-angle tangent t: exact, with no trigonometry.
  const c = (1 - t * t) / (1 + t * t), s = (2 * t) / (1 + t * t);
  return [x * c - sign * y * s, sign * x * s + y * c];
}
function figTree(svg) {
  const g = el(svg, "g");
  const T225 = Math.tan(Math.PI / 8), PHI = 0.6180339887498949;
  const u0 = 0.37, psi = 0.21;
  let k = 0;
  const branches = [];
  const grow = (start, dir, len, width, depth) => {
    const end = [start[0] + dir[0] * len, start[1] + dir[1] * len];
    branches.push({ start, end, width });
    if (depth === 0 || len < 7) return;
    const u = frac(u0 + PHI * (k + 1));
    const flip = frac(0.7548776662466927 * (k + 1) + psi) < 0.5;
    k++;
    const r = 0.62 + 0.28 * u;
    const tL = (T225 * 2 * (1 - r)) / 2, tS = 1 - (1 - T225) * 2 * (1 - r);
    // The winner leans back toward vertical; the small child turns the other way.
    const lean = dir[0] > 0.02 ? -1 : dir[0] < -0.02 ? 1 : flip ? 1 : -1;
    const big = rotate(dir, tL, lean), small = rotate(dir, tS, -lean);
    grow(end, big, len * (0.5 + 0.35 * r), width * 0.7 * (0.35 + 0.65 * r), depth - 1);
    grow(end, small, len * (0.5 + 0.35 * (1 - r)), width * 0.7 * (0.35 + 0.65 * (1 - r)), depth - 1);
  };
  const base = [250, 396];
  grow(base, [0, -1], 112, 12, 10);
  for (const b of branches) line(g, b.start, b.end, "ink").setAttribute("stroke-width", f1(Math.max(0.6, b.width * 0.55)));
  line(g, [110, 396], [390, 396], "ink thin");
  // The first fork, annotated.
  const fork = branches[0].end, kids = branches.filter((b) => b.start === fork);
  line(g, fork, [fork[0], fork[1] - 120], "guide");
  for (const [i, b] of kids.entries()) {
    const a1 = Math.atan2(b.end[1] - fork[1], b.end[0] - fork[0]);
    const pts = arc(fork, i === 0 ? 46 : 30, -Math.PI / 2, a1);
    poly(g, pts, "acc thin");
  }
  label(g, [fork[0] + 12, fork[1] - 52], "θ", "acc-text");
  label(g, [430, 74], "θ_{large} ≈ 90°(1 − r)");
  label(g, [430, 100], "θ_{small} ≈ 90°·r");
  note(g, [430, 132], "r: the larger child's share");
  note(g, [430, 152], `${branches.length} branches, ${k} forks`);
  note(g, [430, 172], "from 1 trunk");
}

// ---- III. The sun: an angle without trigonometry ------------------------------
function figSun(svg) {
  const g = el(svg, "g");
  const alpha = (55 * Math.PI) / 180, q = 0.5206; // azimuth step a = 4
  const dX = (1 - q * q) / (1 + q * q), dY = (2 * q) / (1 + q * q);
  // Left: the unit circle. A line from (−1, 0) with slope q meets it at (dx, dy).
  const C = [150, 210], R = 104, W = [C[0] - R, C[1]];
  const Q = [C[0] + R * dX, C[1] - R * dY];
  el(g, "circle", { cx: C[0], cy: C[1], r: R, class: "ink thin" });
  line(g, [W[0] - 16, C[1]], [C[0] + R + 18, C[1]], "guide");
  line(g, W, [W[0] + (Q[0] - W[0]) * 1.12, W[1] + (Q[1] - W[1]) * 1.12], "acc");
  line(g, C, Q, "ink thin");
  poly(g, arc(C, 24, 0, -alpha), "ink thin");
  poly(g, arc(W, 44, 0, -alpha / 2), "ink thin");
  dot(g, W); dot(g, C, "dot hollow", 3); dot(g, Q, "dot accent", 4.5);
  label(g, [C[0] + 28, C[1] - 8], "α");
  label(g, [W[0] + 48, W[1] - 6], "α/2");
  label(g, [W[0] - 20, W[1] + 22], "(−1, 0)");
  label(g, [Q[0] + 8, Q[1] - 10], "(d_x, d_y)");
  note(g, [60, 352], "slope q = tan(α/2)");

  line(g, [318, 30], [318, 350], "rule");

  // Right: the ground, a stick, its shadow, and the sun behind the scene.
  const G = [484, 280], Rg = 132;
  const P3 = ([u, v, z]) => [G[0] + u * Rg, G[1] + v * Rg * 0.3 - z * Rg * 0.95];
  const ell = 1.2799, phi = (38 * Math.PI) / 180, H = 0.56; // elevation step e = 6
  el(g, "ellipse", { cx: G[0], cy: G[1], rx: Rg, ry: Rg * 0.3, class: "ink thin" });
  line(g, P3([-1.08, 0, 0]), P3([1.08, 0, 0]), "guide");
  const top = P3([0, 0, H]), tip = P3([dX * ell * H, dY * ell * H, 0]);
  const sunDir = [-Math.cos(alpha) * Math.cos(phi), -Math.sin(alpha) * Math.cos(phi), Math.sin(phi)];
  const sun = P3([sunDir[0] * 1.05, sunDir[1] * 1.05, H + sunDir[2] * 1.05]);
  line(g, P3([-Math.cos(alpha), -Math.sin(alpha), 0]), G, "guide");
  line(g, sun, tip, "guide");
  line(g, G, top, "ink").setAttribute("stroke-width", "2.6");
  line(g, G, tip, "acc").setAttribute("stroke-width", "3");
  el(g, "circle", { cx: f1(sun[0]), cy: f1(sun[1]), r: 11, class: "ink" });
  for (let i = 0; i < 8; i++) {
    const a = (i * Math.PI) / 4;
    line(g, [sun[0] + 16 * Math.cos(a), sun[1] + 16 * Math.sin(a)], [sun[0] + 22 * Math.cos(a), sun[1] + 22 * Math.sin(a)], "ink thin");
  }
  // azimuth α on the ground, elevation φ in the light's own plane
  const az = Array.from({ length: 25 }, (_, i) => { const b = (alpha * i) / 24; return P3([-0.32 * Math.cos(b), -0.32 * Math.sin(b), 0]); });
  poly(g, az, "ink thin");
  const tipG = [dX * ell * H, dY * ell * H];
  const el3 = Array.from({ length: 25 }, (_, i) => {
    const b = (phi * i) / 24;
    return P3([tipG[0] - 0.3 * Math.cos(alpha) * Math.cos(b), tipG[1] - 0.3 * Math.sin(alpha) * Math.cos(b), 0.3 * Math.sin(b)]);
  });
  poly(g, el3, "ink thin");
  label(g, [P3([-0.42, -0.2, 0])[0] - 8, P3([-0.42, -0.2, 0])[1] + 4], "α");
  label(g, [el3[12][0] - 20, el3[12][1] - 4], "φ");
  label(g, [(G[0] + tip[0]) / 2 + 6, (G[1] + tip[1]) / 2 + 22], "ℓ·h");
  label(g, [G[0] - 22, (G[1] + top[1]) / 2 + 4], "h");
  note(g, [sun[0] + 28, sun[1] + 4], "sun or moon: never drawn");
}

// ---- IV. The shadow: casting onto a curved hill ------------------------------
function figShadow(svg) {
  const g = el(svg, "g");
  const S = (x) => 236 + 34 * Math.sin(x / 96 + 0.6) + 16 * Math.cos(x / 41);
  let Smin = Infinity;
  for (let x = 0; x <= 640; x += 2) Smin = Math.min(Smin, S(x));
  const Hf = (x, f) => (f <= 1 ? (1 - f) * ((1 - f) * S(x) + f * Smin) : (1 - f) * Smin);
  const Y = (h) => 372 - h;
  for (let i = 0; i <= 9; i++) {
    const f = i / 10, pts = [];
    for (let x = 10; x <= 630; x += 4) pts.push([x, Y(Hf(x, f))]);
    poly(g, pts, i === 0 ? "ink" : "contour");
  }
  note(g, [22, Y(Hf(22, 0)) - 12], "f = 0");
  note(g, [22, Y(Hf(22, 0.9)) - 6], "f = 0.9");
  // Sun as in the sun exhibit: azimuth 55°, elevation 38°.
  const dx = 0.5736, dy = 0.8192, ell = 1.2799, Gd = 300;
  const bisect = (x, h) => { // H(x, f) = h on f in [0, 1], 50 halvings
    let lo = 0, hi = 1;
    for (let i = 0; i < 50; i++) { const mid = (lo + hi) / 2; if (Hf(x, mid) > h) lo = mid; else hi = mid; }
    return (lo + hi) / 2;
  };
  const tree = (xb, fb, height) => {
    const hb = Hf(xb, fb), fFound = bisect(xb, hb);
    const base = [xb, Y(hb)];
    const parts = [[[0, 0], [0, height]], [[0, height * 0.62], [-height * 0.3, height * 0.95]], [[0, height * 0.55], [height * 0.28, height * 0.86]]];
    const cast = ([ox, z]) => { const x2 = xb + ox + z * ell * dx, f2 = fFound + (z * ell * dy) / Gd; return [x2, Y(Hf(x2, f2))]; };
    for (const [a, b] of parts) {
      const pts = Array.from({ length: 21 }, (_, i) => cast([a[0] + ((b[0] - a[0]) * i) / 20, a[1] + ((b[1] - a[1]) * i) / 20]));
      poly(g, pts, "shadow");
    }
    for (const [a, b] of parts) line(g, [base[0] + a[0], base[1] - a[1]], [base[0] + b[0], base[1] - b[1]], "ink trunk");
    dot(g, base, "dot hollow", 3);
  };
  tree(150, 0.1, 74); tree(330, 0.33, 92); tree(492, 0.58, 104);
  // the light comes from behind, on the left
  arrowHead(g, [36, 34], [80, 66], "ink");
  line(g, [36, 34], [80, 66], "ink thin");
  note(g, [90, 66], "light: from behind, left");
}

// ---- V. The cloud: five points, three refinements -----------------------------
function figCloud(svg) {
  const g = el(svg, "g");
  const rnd = rng(0x51c7);
  const top = 70, bot = 214, xL = 92, xR = 552, bw = (xR - xL) * 0.52, bc = 296;
  const trap = [[xL, top], [xR, top], [bc + bw / 2, bot], [bc - bw / 2, bot]];
  const inside = ([x, y]) => {
    if (y < top || y > bot) return false;
    const t = (y - top) / (bot - top);
    return x >= xL + (trap[3][0] - xL) * t && x <= xR + (trap[2][0] - xR) * t;
  };
  poly(g, trap, "guide", true);
  // Five points by perimeter fraction, each edge measured as |dx| + |dh|.
  const edges = trap.map((a, i) => [a, trap[(i + 1) % 4]]);
  const lens = edges.map(([a, b]) => Math.abs(b[0] - a[0]) + Math.abs(b[1] - a[1]));
  const per = lens.reduce((s, v) => s + v, 0);
  const at = (s) => {
    for (let i = 0; i < 4; i++) {
      if (s <= lens[i]) { const t = s / lens[i], [a, b] = edges[i]; return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t]; }
      s -= lens[i];
    }
    return trap[0];
  };
  const five = Array.from({ length: 5 }, (_, i) => at(((i + 0.2 + 0.6 * rnd()) / 5) * per));
  // Three rounds: four points per pair, inside a rhombus along each pair.
  const refine = (pts) => {
    const out = [];
    for (let i = 0; i < pts.length; i++) {
      const A = pts[i], B = pts[(i + 1) % pts.length];
      out.push(A);
      for (let k = 0; k < 4; k++) {
        const t = (k + 0.25 + 0.5 * rnd()) / 4, w = 0.15 * (1 - Math.abs(2 * t - 1)) * (2 * rnd() - 1);
        const base = [A[0] + (B[0] - A[0]) * t, A[1] + (B[1] - A[1]) * t];
        const off = [-(B[1] - A[1]) * w, (B[0] - A[0]) * w];
        let p = [base[0] + off[0], base[1] + off[1]];
        if (!inside(p)) p = [base[0] - off[0], base[1] - off[1]];
        if (!inside(p)) p = base;
        out.push(p);
      }
    }
    return out;
  };
  const r1 = refine(five), r3 = refine(refine(r1));
  poly(g, r1, "contour", true);
  // Smooth outline: quadratic curves through midpoints, each point a control.
  const mid = (a, b) => [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2];
  let d = `M${f1(mid(r3[r3.length - 1], r3[0])[0])},${f1(mid(r3[r3.length - 1], r3[0])[1])}`;
  for (let i = 0; i < r3.length; i++) {
    const p = r3[i], m = mid(p, r3[(i + 1) % r3.length]);
    d += `Q${f1(p[0])},${f1(p[1])} ${f1(m[0])},${f1(m[1])}`;
  }
  el(g, "path", { d: d + "Z", class: "ink" });
  // One rhombus, between points 1 and 2.
  const A = five[1], B = five[2], M = mid(A, B), n = [-(B[1] - A[1]) * 0.15, (B[0] - A[0]) * 0.15];
  poly(g, [A, [M[0] + n[0], M[1] + n[1]], B, [M[0] - n[0], M[1] - n[1]]], "acc thin", true);
  five.forEach((p, i) => {
    dot(g, p, "dot accent", 4.5);
    const out = [p[0] - 296, p[1] - 142];
    const len = Math.hypot(...out);
    label(g, [p[0] + (out[0] / len) * 18 - 4, p[1] + (out[1] / len) * 18 + 5], String(i));
  });
  note(g, [92, 260], "dashed: the trapezoid the bits set");
  note(g, [92, 280], "thin: 25 points after one round");
  note(g, [92, 300], "solid: 625 points after three, smoothed");
}

// ---- VI. The split: the incoming slope ----------------------------------------
function figSplit(svg) {
  const g = el(svg, "g");
  hatch(svg, "hatch-split", 7, 45);
  const sil = [[16, 312], [86, 252], [148, 232], [214, 172], [300, 84], [386, 160], [452, 192], [522, 170], [624, 246]];
  const a = sil[3], b = sil[4], c = sil[5];
  const cheb = ([x, y]) => { const m = Math.max(Math.abs(x), Math.abs(y)); return [x / m, y / m]; };
  const dL = cheb([a[0] - b[0], a[1] - b[1]]), dR = cheb([c[0] - b[0], c[1] - b[1]]);
  const f = 0.1 + 0.8 * (19 / 37);
  const d = [dL[0] + f * (dR[0] - dL[0]), dL[1] + f * (dR[1] - dL[1])];
  const foot = [b[0] + (d[0] * (370 - b[1])) / d[1], 370];
  // The half turned away from a sun on the left is shaded.
  el(g, "path", { d: pathOf([b, ...sil.slice(5), [624, 370], foot], true), fill: "url(#hatch-split)", class: "nostroke" });
  poly(g, [...sil, [624, 370], [16, 370]], "ink", true);
  line(g, b, foot, "acc").setAttribute("stroke-width", "2.6");
  // the turn: the rising slope continued past the peak, against the descent
  line(g, b, [b[0] + (b[0] - a[0]) * 0.75, b[1] + (b[1] - a[1]) * 0.75], "guide");
  const aExt = Math.atan2(b[1] - a[1], b[0] - a[0]), aDown = Math.atan2(c[1] - b[1], c[0] - b[0]);
  poly(g, arc(b, 34, aExt, aDown), "ink thin");
  note(g, [b[0] + 50, b[1] - 34], "turn > 25°: a peak");
  // the two slopes the cut lies between, and where it falls
  for (const [v, txt, dxl] of [[dL, "d̂_L", -40], [dR, "d̂_R", 14]]) {
    const end = [b[0] + v[0] * 62, b[1] + v[1] * 62];
    arrowHead(g, b, end, "ink");
    label(g, [end[0] + dxl, end[1] + 6], txt);
  }
  poly(g, arc(b, 92, Math.atan2(dL[1], dL[0]), Math.atan2(dR[1], dR[0]), 32), "guide");
  label(g, [foot[0] + 10, 318], "d", "acc-text");
  label(g, [a[0] - 20, a[1] - 10], "a"); label(g, [b[0] - 6, b[1] - 12], "b"); label(g, [c[0] + 4, c[1] - 12], "c");
  dot(g, a); dot(g, b, "dot accent", 4.5); dot(g, c);
  note(g, [130, 350], "lit half");
  note(g, [470, 300], "shaded half");
}

// ---- VII. The facets: triangulating the rock -----------------------------------
const PI100 = "3141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067";
function figFacets(svg) {
  const g = el(svg, "g");
  hatch(svg, "hatch-dark", 4, 45);
  hatch(svg, "hatch-mid", 8, 45);
  const D = (k) => Number(PI100[((k % 100) + 100) % 100]);
  const S = (x) => 108 + 218 * Math.exp(-(((x - 300) / 150) ** 2)) + 18 * Math.sin(x / 37);
  const F = 16, Y = (h) => 372 - h, step = 40;
  let T = 0;
  for (let x = 0; x <= 640; x += 2) T = Math.max(T, S(x));
  const R = Math.max(2, Math.ceil((T - F) / 80));
  const silPts = [];
  for (let x = 0; x <= 640; x += 4) silPts.push([x, Y(S(x))]);
  const clip = el(el(svg, "defs"), "clipPath", { id: "facet-clip" });
  el(clip, "path", { d: pathOf([...silPts, [640, Y(F)], [0, Y(F)]], true) });
  const mesh = el(g, "g", { "clip-path": "url(#facet-clip)" });
  // Columns lean with the peak's cut, weighted 1 / (1 + ((x − p)/300)²), plus straight down at 0.3.
  const px = 300, cutAcross = 0.36;
  const grid = [];
  for (let i = 0; i * step <= 680; i++) {
    const x0 = step * (i - 1) + 20, w = 1 / (1 + ((x0 - px) / 300) ** 2);
    const slant = Math.max(-1, Math.min(1, (w * cutAcross) / (w + 0.3)));
    const col = [];
    for (let j = 0; j <= R; j++) {
      const k = 37 * i + 11 * j + Math.floor(S(x0));
      const xt = x0 + (0.4 * step * (D(k) - 4.5)) / 9;
      const t = Math.min(1, Math.max(0, j / R + (j > 0 && j < R ? (0.4 * (D(k + 3) - 4.5)) / (9 * R) : 0)));
      const top = S(xt), drop = t * (top - F);
      col.push([xt + drop * slant, top - drop, 0.6 * drop + (22.5 * (D(k + 7) - 4.5)) / 9]);
    }
    grid.push(col);
  }
  // Light from the sun's side and above: L = (−dx·ℓ, 1, 0).
  const L = [-0.5736 * 1.2799, 1, 0];
  const counts = { dark: 0, mid: 0, lit: 0 };
  const sub = (p, q) => [p[0] - q[0], p[1] - q[1], p[2] - q[2]];
  for (let i = 0; i + 1 < grid.length; i++) {
    for (let j = 0; j < R; j++) {
      const p00 = grid[i][j], p10 = grid[i + 1][j], p01 = grid[i][j + 1], p11 = grid[i + 1][j + 1];
      const tris = D(53 * i + 29 * j) % 2 ? [[p00, p10, p11], [p00, p11, p01]] : [[p00, p10, p01], [p10, p11, p01]];
      for (const tri of tris) {
        const u = sub(tri[1], tri[0]), v = sub(tri[2], tri[0]);
        let n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]];
        if (n[2] < 0) n = n.map((c) => -c);
        const nl = n[0] * L[0] + n[1] * L[1] + n[2] * L[2];
        const s = (Math.sign(nl) * nl * nl) / ((n[0] ** 2 + n[1] ** 2 + n[2] ** 2) * (L[0] ** 2 + L[1] ** 2));
        const pts = tri.map((p) => [p[0], Y(p[1])]);
        poly(mesh, pts, "mesh", true);
        if (s < 0.012) { el(mesh, "path", { d: pathOf(pts, true), fill: "url(#hatch-dark)", class: "nostroke" }); counts.dark++; }
        else if (s < 0.05) { el(mesh, "path", { d: pathOf(pts, true), fill: "url(#hatch-mid)", class: "nostroke" }); counts.mid++; }
        else if (s > 0.4) { poly(mesh, pts, "acc thin", true); counts.lit++; }
      }
    }
  }
  poly(g, silPts, "ink");
  line(g, [0, Y(F)], [640, Y(F)], "ink thin");
  arrowHead(g, [92, 92], [44, 48], "ink");
  line(g, [92, 92], [44, 48], "ink thin");
  label(g, [24, 42], "L");
  note(g, [446, 30], "dense hatch: −13%");
  note(g, [446, 48], "light hatch: −6%");
  note(g, [446, 66], "accent: +6% to +13%");
  note(g, [446, 84], "blank: the ridge's own fill");
}

// ---- The palette: color in a perceptual space --------------------------------
function figPalette(svg) {
  const g = el(svg, "g");
  // Left: the OKLCH hue circle, with each biome's range of ridge hues.
  const C = [178, 196], R = 104;
  el(g, "circle", { cx: C[0], cy: C[1], r: R, class: "ink thin" });
  const at = (deg, r) => [C[0] + r * Math.cos((-deg * Math.PI) / 180), C[1] + r * Math.sin((-deg * Math.PI) / 180)];
  for (let d = 0; d < 360; d += 30) line(g, at(d, R - 5), at(d, R + 5), "ink thin");
  for (const d of [0, 90, 180]) note(g, [at(d, R + 22)[0], at(d, R + 22)[1] + 4], `${d}°`, "middle");
  const biomes = [["Desert", 50, 85, 88], ["Temperate", 175, 240, 74], ["Alpine", 215, 275, 88], ["Tundra", 285, 325, 62]];
  for (const [name, a0, a1, r] of biomes) {
    const pts = Array.from({ length: 33 }, (_, i) => at(a0 + ((a1 - a0) * i) / 32, r));
    poly(g, pts, "acc").setAttribute("stroke-width", "3.2");
    line(g, at(a0, r - 5), at(a0, r + 5), "acc thin"); line(g, at(a1, r - 5), at(a1, r + 5), "acc thin");
    const m = (a0 + a1) / 2, lead0 = at(m, r + 4), lead1 = at(m, R + 16), tag = at(m, R + 24);
    line(g, lead0, lead1, "guide");
    note(g, [tag[0], tag[1] + 4], name, Math.cos((m * Math.PI) / 180) < -0.2 ? "end" : Math.cos((m * Math.PI) / 180) > 0.2 ? "start" : "middle");
  }
  dot(g, C, "dot hollow", 3);
  note(g, [C[0], 352], "hue h, ridge ranges by biome", "middle");
  line(g, [318, 30], [318, 350], "rule");
  // Right: the lightness ladder. Bands never overlap, so nearer is always darker.
  const Ly = (L) => 336 - ((L - 0.25) / 0.55) * 290;
  line(g, [352, Ly(0.25)], [352, Ly(0.8)], "ink thin");
  for (const L of [0.3, 0.4, 0.5, 0.6, 0.7, 0.8]) { line(g, [348, Ly(L)], [356, Ly(L)], "ink thin"); note(g, [342, Ly(L) + 4], L.toFixed(1), "end"); }
  label(g, [346, Ly(0.8) - 14], "L", "", "middle");
  const bands = [["far", 0.68, 0.77, 9], ["middle", 0.51, 0.65, 5.5], ["front", 0.30, 0.48, 3]];
  const ridges = [
    (x) => 96 + 22 * Math.sin(x / 31) + 30 * Math.exp(-(((x - 520) / 50) ** 2)) * -1,
    (x) => 168 + 26 * Math.sin(x / 23 + 1.3) - 40 * Math.exp(-(((x - 450) / 45) ** 2)),
    (x) => 244 + 18 * Math.sin(x / 19 + 0.4) - 34 * Math.exp(-(((x - 560) / 40) ** 2)),
  ];
  bands.forEach(([name, l0, l1, gap], i) => {
    poly(g, [[362, Ly(l1)], [370, Ly(l1)], [370, Ly(l0)], [362, Ly(l0)]], "acc thin");
    note(g, [378, (Ly(l0) + Ly(l1)) / 2 + 4], name);
    hatch(svg, `hatch-band-${i}`, gap, 45);
    const pts = []; for (let x = 436; x <= 626; x += 3) pts.push([x, ridges[i](x)]);
    const body = pathOf([...pts, [626, 336], [436, 336]], true);
    el(g, "path", { d: body, class: "mat-bg" });
    el(g, "path", { d: body, fill: `url(#hatch-band-${i})`, class: "mat-fill" });
    poly(g, pts, "ink");
  });
  line(g, [436, 336], [626, 336], "ink thin");
  note(g, [436, 356], "denser lines: a darker shade");
}

// ---- The placement: trees and the plants between them -------------------------
function figPlace(svg) {
  const g = el(svg, "g");
  const X0 = 20, W = 600, base = 372;
  const Rh = (x) => 196 + 30 * Math.sin((x - X0) / 120 + 0.4) + 12 * Math.cos((x - X0) / 47);
  const Y = (h) => base - h;
  const sil = []; for (let x = X0; x <= X0 + W; x += 3) sil.push([x, Y(Rh(x))]);
  poly(g, sil, "ink");
  line(g, [X0, base], [X0 + W, base], "ink thin");
  // Composition: arrangement a = 1 on the front hill, six trees.
  const PHI = 0.6180339887498949, a = 1;
  const c = W * (0.2 + 0.6 * frac(0.37 + 0.29 * a));
  const offs = [0, 0.9, -0.8, 1.7];
  const trees = [
    ...offs.map((o, k) => ({ x: c + 0.035 * W * o, role: k ? "member" : "leader", prom: k ? 0.8 + 0.15 * frac(PHI * (k + 1)) : 1.15, i: k })),
    { x: c + (c < W / 2 ? -0.2 : 0.2) * W, role: "straggler", prom: 0.8, i: 4 },
    { x: c + (c < W / 2 ? 0.38 : -0.38) * W, role: "isolated", prom: 1.0, i: 5 },
  ];
  const drawTree = (p, s) => {
    const seg = (q, dir, len, depth) => {
      const e = [q[0] + dir[0] * len, q[1] + dir[1] * len];
      line(g, q, e, "ink").setAttribute("stroke-width", f1(Math.max(0.7, depth * 0.55)));
      if (depth === 0) return;
      seg(e, rotate(dir, 0.18, depth % 2 ? 1 : -1), len * 0.74, depth - 1);
      seg(e, rotate(dir, 0.62, depth % 2 ? -1 : 1), len * 0.6, depth - 1);
    };
    seg(p, [0, -1], 22 * s, 4);
  };
  for (const t of trees) {
    const x = X0 + t.x;
    const phiI = frac(PHI * (t.i + 1) + 0.25 * a);
    const down = t.role === "straggler" || t.role === "isolated" ? 0.30 + 0.35 * frac(7 * phiI) : 0;
    const h = Rh(x) * (1 - down);
    t.sx = x; t.h = h;
    drawTree([x, Y(h)], t.prom * (1 + 0.9 * down));
  }
  // Vegetation in the open: a gap chosen by width, a tent across it, packed toward the rim.
  const anchors = [X0, ...trees.map((t) => t.sx).sort((p, q) => p - q), X0 + W];
  const rnd = rng(0x7e9a);
  for (let n = 0; n < 150; n++) {
    const u = Array.from({ length: 6 }, rnd);
    const xs = X0 + W * u[0];
    let gi = 0; while (anchors[gi + 1] < xs) gi++;
    const e0 = anchors[gi], e1 = anchors[gi + 1];
    const x = e0 + (e1 - e0) * 0.5 * (1 + u[1] - u[2]);
    const h = 0.97 * Rh(x) * (1 - u[3] ** 4), t = 1 - h / Rh(x);
    const d = 1.0 * (0.35 + 1.25 * t), blade = 4.2 * d * (0.85 + 0.3 * u[4]), lean = (0.25 + 0.3 * u[5]) * blade;
    const p = [x, Y(h)];
    line(g, p, [p[0], p[1] - blade], "tuft"); line(g, p, [p[0] - lean, p[1] - blade * 0.85], "tuft"); line(g, p, [p[0] + lean, p[1] - blade * 0.85], "tuft");
  }
  // The tent between two trees, and the labels.
  const s = trees.find((t) => t.role === "isolated"), lead = trees.find((t) => t.role === "leader");
  const [g0, g1] = s.sx < lead.sx ? [s.sx, Math.min(...trees.filter((t) => t !== s).map((t) => t.sx))] : [Math.max(...trees.filter((t) => t !== s).map((t) => t.sx)), s.sx];
  poly(g, [[g0, 40], [(g0 + g1) / 2, 14], [g1, 40]], "acc thin");
  line(g, [g0, 40], [g1, 40], "guide");
  note(g, [(g0 + g1) / 2, 56], "½(1 + u₁ − u₂)", "middle");
  for (const t of trees.filter((q) => q.role === "isolated" || q.role === "straggler")) note(g, [t.sx, Y(t.h) + 16], t.role, "middle");
  const gx = X0 + c + 0.035 * W * 0.45, gy = Y(Rh(X0 + c)) - 78;
  line(g, [gx, gy + 6], [gx, gy + 22], "guide");
  note(g, [gx, gy], "grove, around its leader", "middle");
}

// Note [1] is collapsed; following its link opens it.
const openNote = () => { const n = document.getElementById(location.hash.slice(1)); if (n && n.tagName === "DETAILS") n.open = true; };
window.addEventListener("hashchange", openNote);
openNote();

const FIGS = { ridge: figRidge, tree: figTree, place: figPlace, palette: figPalette, sun: figSun, shadow: figShadow, cloud: figCloud, split: figSplit, facets: figFacets };
for (const [name, draw] of Object.entries(FIGS)) {
  const svg = document.getElementById(`fig-${name}`);
  if (svg) draw(svg);
}
