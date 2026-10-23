// BeautifulHash bit map page: draws which namespace reads each of the 256
// hash bits, and the ledger of their fields.
"use strict";

const $ = (id) => document.getElementById(id);
const NS = [
  { key: "global", name: "Global", from: 0, width: 6,
    fields: [["daylight (day or night)", 2], ["biome", 2], ["season", 2]] },
  { key: "global-extra", name: "Global (extra)", from: 6, width: 10,
    fields: [["second cloud top", 3], ["second cloud width", 3], ["second cloud center", 3], ["second cloud lead", 1]],
    note: "Global's spare bits: the second cloud, and easter eggs such as the aurora" },
  { key: "sky", name: "Sky", from: 16, width: 24,
    fields: [["cloud top", 3], ["cloud height", 2], ["cloud width", 3], ["cloud center", 3], ["bottom width", 2], ["skew", 2], ["leading side", 1], ["sky tint", 8]] },
  { key: "celestial", name: "Celestial", from: 40, width: 8, fields: [["sun or moon azimuth", 4], ["elevation", 4]] },
  { key: "mountains", name: "Mountains", from: 48, width: 88,
    fields: [["ridge layout header", 1], ["ridge shapes, placement, colors", 87]], note: "two- or three-ridge layout; both read all 88" },
  { key: "hills", name: "Hills", from: 136, width: 40, fields: [["one or two hills", 1], ["crest, edges, shape, color per hill", 39]] },
  { key: "trees", name: "Trees", from: 176, width: 48,
    fields: [["count code", 4], ["arrangement", 2], ["size", 2], ["leaf palette", 4], ["girth, height, forks per tree", 36]],
    note: "count code is 3 or 4 bits and the leaf palette 4 or 0; the rest goes to the trees" },
  { key: "vegetation", name: "Vegetation", from: 224, width: 24, fields: [["palette", 4], ["density", 2], ["spread", 2], ["seed", 16]] },
  { key: "fog", name: "Fog", from: 248, width: 8, fields: [["density", 2], ["tint", 3], ["mist", 3]] },
];

function buildBitmap() {
  const fieldAt = (ns, bit) => {
    let at = ns.from;
    for (const [label, w] of ns.fields) { if (bit < at + w) return label; at += w; }
    return "";
  };
  const bits = $("bits");
  bits.appendChild(document.createElement("div"));
  for (let c = 0; c < 16; c++) {
    const l = document.createElement("div");
    l.className = "collabel";
    l.textContent = "+" + c.toString(16).toUpperCase();
    bits.appendChild(l);
  }
  for (let r = 0; r < 16; r++) {
    const rl = document.createElement("div");
    rl.className = "rowlabel";
    rl.textContent = String(r * 16);
    bits.appendChild(rl);
    for (let c = 0; c < 16; c++) {
      const bit = r * 16 + c;
      const ns = NS.find((n) => bit >= n.from && bit < n.from + n.width);
      const cell = document.createElement("div");
      cell.className = "cell";
      cell.tabIndex = 0;
      cell.textContent = bit;
      cell.style.background = `var(--ns-${ns.key})`;
      const label = `bit ${bit} · ${ns.name} · ${fieldAt(ns, bit)}`;
      cell.title = label;
      cell.setAttribute("aria-label", label);
      const showHint = () => { $("bit-hint").textContent = label; };
      cell.addEventListener("mouseenter", showHint);
      cell.addEventListener("focus", showHint);
      bits.appendChild(cell);
    }
  }
  for (const ns of NS) {
    $("legend").insertAdjacentHTML("beforeend", `<span><i class="sw" style="background: var(--ns-${ns.key})"></i>${ns.name}</span>`);
    const fields = ns.fields.map(([l, w]) => `${l} ${w}`).join(" · ") + (ns.note ? `<br>(${ns.note})` : "");
    $("ledger").insertAdjacentHTML("beforeend", `<tr><td><span class="ns"><i class="sw" style="background: var(--ns-${ns.key})"></i>${ns.name}</span></td>
      <td class="num">${ns.from}–${ns.from + ns.width - 1}</td><td class="num">${ns.width}</td><td class="fields">${fields}</td></tr>`);
  }
}

buildBitmap();
