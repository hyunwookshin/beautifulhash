// BeautifulHash site: renders landscapes in the browser with the WebAssembly
// build of core/ (bh.js, bh.wasm). No server: hashing, rendering, favorites, and
// downloads all happen here.
"use strict";

// Example landscapes, each with a short title that hints at the scene without
// naming how it was chosen. `input` is the text that hashes to it, if any;
// `by` is who discovered it (the hash-only ones were found while testing).
const EXAMPLES = [
  { hash: "9e6ed8c1b2283a48a64fca7f8be20b89e01fa378280f91c631ffe62c2f144534", title: "Where the maples burn", by: "Claude" },
  { hash: "486ea46224d1bb4fb680f34f7c9ad96a8f24ec88be73ea8e5a6c65260e9cb8a7", input: "world", title: "Blush on the summit", by: "Hyunwook Shin" },
  { hash: "c06b0cfe0cc5e900c57784484094331f095bf441995c3c31ea6c75691c786c35", input: "flower", title: "Moonlit snow, sleeping pines", by: "Hyunwook Shin" },
  { hash: "e6f933f76267778a781c8ceed4c85da4ef68694fd2217755e5f1a7a02884b98f", input: "academy", title: "Granite under the moon", by: "Hyunwook Shin" },
  { hash: "a65a38fb4042fcbcad48b6ecc6ad70987129d51315698477282cc24a93cd7f23", title: "Rose light on stone", by: "Claude" },
  { hash: "1ec7d6744ab01a7dc28b67e6c477d6f67f24c8ee564a3735fd8202022879d326", input: "nonono", title: "Pink dusk, falling leaves", by: "Hyunwook Shin" },
  { hash: "3508d1ea52591c3d70d419d13f9b2cf1bb665bcc83cc7fd93d1c3ab1b03f8b8f", title: "Thorns against the sky", by: "Claude" },
  { hash: "1620f91609560ef7072fce277eef7962594fdf5e6f5d4e2b2fb94d1761456cac", input: "nauru", title: "One golden tree", by: "Hyunwook Shin" },
  { hash: "dbf10bee497b39492662d30a88224bd29b9e4b39c0bb6b9de047adb80220066b", title: "Leaves under moonlight", by: "Claude" },
  { hash: "048ed6b3c5613162835de5f53371dbc0ac45debff4a7d7b11dd9f3165639c1ef", title: "Lilac sky, quiet pines", by: "Claude" },
  { hash: "5075fb5ef6771b95233e332ffdfc2b947d9924416caf103c3480658403afde2e", title: "Bare branches, first snow", by: "Claude" },
  { hash: "099ae4672fc8922e18dc0a75b1ff155f414ef140cbe583c86e8440de05542029", input: "peel", title: "Clean air, quiet valley", by: "Hyunwook Shin" },
];
const RECOMMENDED = EXAMPLES.slice(0, 4);
const SUGGESTIONS = ["Hello!", "chocolate muffin", "good morning", "my cat Miso", "rainy Tuesday", "happy birthday, Mom", "coffee first", "see you at sunset"];

const $ = (id) => document.getElementById(id);
const HEX = /^[0-9a-fA-F]{64}$/;

// A caption for any landscape: an example's title, else its input, else the hash.
function caption(hex, input) {
  const ex = EXAMPLES.find((e) => e.hash === hex);
  return ex ? ex.title : input ? `“${input}”` : `${hex.slice(0, 12)}…`;
}

// Who discovered a landscape, if it is in the gallery.
function discoverer(hex) {
  const ex = EXAMPLES.find((e) => e.hash === hex);
  return ex ? ex.by : "";
}

// ---- rendering ----------------------------------------------------------------
let render = null;
const ready = BeautifulHash().then((mod) => {
  render = mod.cwrap("bh_render", "string", ["string", "number", "number"]);
});
const urls = new Map();  // hash -> object URL of its SVG (renders are deterministic)

async function svgUrl(hex) {
  hex = hex.toLowerCase();
  if (urls.has(hex)) return urls.get(hex);
  await ready;
  const svg = render(hex, 1024, 768);
  const url = URL.createObjectURL(new Blob([svg], { type: "image/svg+xml" }));
  urls.set(hex, url);
  return url;
}

// Yields to the browser between renders, so the page stays responsive.
const idle = () => new Promise((r) => setTimeout(r, 0));

async function sha256(text) {
  const data = new TextEncoder().encode(text);
  const digest = await crypto.subtle.digest("SHA-256", data);
  return [...new Uint8Array(digest)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

// ---- favorites (this browser only) --------------------------------------------
const FAV_KEY = "beautifulhash.favorites";
function loadFavs() {
  try { return JSON.parse(localStorage.getItem(FAV_KEY)) || []; } catch { return []; }
}
function saveFavs(list) {
  try { localStorage.setItem(FAV_KEY, JSON.stringify(list)); } catch { /* storage unavailable */ }
}
let favs = loadFavs();
const isFav = (hex) => favs.some((f) => f.hash === hex);

// ---- tiles --------------------------------------------------------------------
function tile(hex, input, onClick) {
  const t = document.createElement("button");
  t.className = "tile";
  t.type = "button";
  const cap = caption(hex, input);
  t.innerHTML = `<img class="thumb" alt="" loading="lazy"><span class="cap"><b></b><code>${hex.slice(0, 16)}…</code><small class="by"></small></span>`;
  t.querySelector("b").textContent = cap;
  const by = discoverer(hex);
  t.querySelector(".by").textContent = by ? `Discovered by ${by}` : "";
  t.querySelector(".by").hidden = !by;
  t.querySelector("img").alt = `Landscape: ${cap}`;
  t.addEventListener("click", onClick);
  return t;
}

// Renders tiles' images as they come into view.
const io = new IntersectionObserver(async (entries) => {
  for (const e of entries) {
    if (!e.isIntersecting) continue;
    io.unobserve(e.target);
    await idle();
    e.target.querySelector("img").src = await svgUrl(e.target.dataset.hash);
  }
}, { rootMargin: "200px" });

function lazy(t, hex) {
  t.dataset.hash = hex;
  io.observe(t);
  return t;
}

// ---- the current result -------------------------------------------------------
let current = null;  // { hash, input }

async function show(hex, input) {
  hex = hex.toLowerCase();
  current = { hash: hex, input };
  $("result").hidden = false;
  $("result-loading").hidden = false;
  $("ref-input-row").hidden = !input;
  $("ref-input").textContent = input || "";
  $("ref-hash").textContent = hex;
  $("ref-by-row").hidden = !discoverer(hex);
  $("ref-by").textContent = discoverer(hex);
  $("result-img").alt = `Landscape for hash ${hex}`;
  $("status").textContent = "";
  // Only words that are not in the gallery yet can be suggested for it.
  $("clear").hidden = !$("query").value;
  $("suggest-btn").hidden = !input || !!discoverer(hex);
  updateStar();
  if ($("panel-create").hidden === false) {
    try { history.replaceState(null, "", `#${hex}`); } catch { /* sandboxed */ }
  }
  await idle();
  const url = await svgUrl(hex);
  $("result-img").src = url;
  $("glow").style.setProperty("--glow-img", `url("${url}")`);
  $("result-loading").hidden = true;
}

function updateStar() {
  const on = current && isFav(current.hash);
  $("star").setAttribute("aria-pressed", on ? "true" : "false");
  $("star").querySelector("span").textContent = on ? "Favorited" : "Favorite";
}

async function submit(text) {
  text = text.trim() || $("query").placeholder;
  if (HEX.test(text)) return show(text, "");
  return show(await sha256(text), text);
}

// ---- downloads ----------------------------------------------------------------
function save(url, name) {
  const a = document.createElement("a");
  a.href = url;
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
}

async function downloadPng(hex) {
  const img = new Image();
  img.src = await svgUrl(hex);
  await img.decode();
  const canvas = document.createElement("canvas");
  canvas.width = 2048;
  canvas.height = 1536;
  canvas.getContext("2d").drawImage(img, 0, 0, canvas.width, canvas.height);
  canvas.toBlob((blob) => save(URL.createObjectURL(blob), `beautifulhash-${hex.slice(0, 12)}.png`), "image/png");
}

// ---- favorites strip ----------------------------------------------------------
function renderFavs() {
  const list = $("fav-list");
  list.replaceChildren(...favs.map((f) => lazy(tile(f.hash, f.input, () => show(f.hash, f.input)), f.hash)));
  $("favorites").hidden = favs.length === 0;
}

// ---- examples carousel and lightbox ------------------------------------------
let lbIndex = 0;
async function openLightbox(i) {
  lbIndex = (i + EXAMPLES.length) % EXAMPLES.length;
  const ex = EXAMPLES[lbIndex];
  const hex = ex.hash;
  $("lb-title").textContent = ex.title;
  $("lb-hash").textContent = ex.input ? `“${ex.input}” · ${hex}` : hex;
  $("lb-by").textContent = `Discovered by ${ex.by}`;
  $("lb-img").alt = `Landscape: ${ex.title}`;
  $("lightbox").hidden = false;
  $("lb-close").focus();
  $("lb-img").src = await svgUrl(hex);
}
function closeLightbox() { $("lightbox").hidden = true; }

// ---- bit map tab --------------------------------------------------------------
const NS = [
  { key: "global", name: "Global", from: 0, width: 16,
    fields: [["daylight (day or night)", 2], ["biome", 2], ["season", 2], ["second cloud top", 3], ["second cloud width", 3], ["second cloud center", 3], ["second cloud lead", 1]] },
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

// ---- tabs ---------------------------------------------------------------------
function selectTab(name) {
  for (const b of document.querySelectorAll(".tabs button")) b.setAttribute("aria-selected", b.dataset.tab === name ? "true" : "false");
  $("panel-create").hidden = name !== "create";
  $("panel-bits").hidden = name !== "bits";
  // The bit map has its own link; the create tab keeps the current hash.
  try {
    if (name === "bits") history.replaceState(null, "", "#bits");
    else if (current) history.replaceState(null, "", `#${current.hash}`);
  } catch { /* sandboxed */ }
}

// ---- wiring -------------------------------------------------------------------
function init() {
  // An example opens with its input text when it has one, else its hash.
  const openExample = (ex) => { $("query").value = ex.input || ex.hash; return show(ex.hash, ex.input || ""); };
  for (const ex of RECOMMENDED) {
    const b = document.createElement("button");
    b.type = "button";
    b.className = "chip";
    b.innerHTML = `<b></b><code>${ex.input ? `“${ex.input}”` : `${ex.hash.slice(0, 12)}…`}</code>`;
    b.querySelector("b").textContent = ex.title;
    b.addEventListener("click", () => openExample(ex));
    $("chips").appendChild(b);
  }
  // The empty search box suggests a few plain words; Generate draws the one shown.
  let hint = 0;
  $("query").placeholder = SUGGESTIONS[0];
  setInterval(() => {
    if (document.activeElement === $("query") || $("query").value) return;
    hint = (hint + 1) % SUGGESTIONS.length;
    $("query").placeholder = SUGGESTIONS[hint];
  }, 3200);
  $("search").addEventListener("submit", (e) => { e.preventDefault(); submit($("query").value); });

  $("star").addEventListener("click", () => {
    if (!current) return;
    favs = isFav(current.hash) ? favs.filter((f) => f.hash !== current.hash)
                               : [{ hash: current.hash, input: current.input || "" }, ...favs];
    saveFavs(favs);
    updateStar();
    renderFavs();
  });
  $("dl-svg").addEventListener("click", async () => current && save(await svgUrl(current.hash), `beautifulhash-${current.hash.slice(0, 12)}.svg`));
  $("dl-png").addEventListener("click", () => current && downloadPng(current.hash));
  $("copy-link").addEventListener("click", async () => {
    if (!current) return;
    const link = `${location.origin}${location.pathname}#${current.hash}`;
    try { await navigator.clipboard.writeText(link); $("status").textContent = "Link copied."; }
    catch { $("status").textContent = link; }
  });

  $("carousel").replaceChildren(...EXAMPLES.map((ex, i) => lazy(tile(ex.hash, ex.input, () => openLightbox(i)), ex.hash)));
  const step = () => $("carousel").clientWidth * 0.8;
  $("prev").addEventListener("click", () => $("carousel").scrollBy({ left: -step(), behavior: "smooth" }));
  $("next").addEventListener("click", () => $("carousel").scrollBy({ left: step(), behavior: "smooth" }));

  $("lb-close").addEventListener("click", closeLightbox);
  $("lightbox").addEventListener("click", (e) => { if (e.target === $("lightbox")) closeLightbox(); });
  $("lb-prev").addEventListener("click", () => openLightbox(lbIndex - 1));
  $("lb-next").addEventListener("click", () => openLightbox(lbIndex + 1));
  $("lb-open").addEventListener("click", () => {
    const ex = EXAMPLES[lbIndex];
    closeLightbox();
    openExample(ex);
    $("result").scrollIntoView({ behavior: "smooth", block: "start" });
  });
  document.addEventListener("keydown", (e) => {
    if ($("lightbox").hidden) return;
    if (e.key === "Escape") closeLightbox();
    if (e.key === "ArrowLeft") openLightbox(lbIndex - 1);
    if (e.key === "ArrowRight") openLightbox(lbIndex + 1);
  });

  // The clear button shows while the search box has text; it and the logo
  // both empty the box.
  const clearQuery = () => { $("query").value = ""; $("clear").hidden = true; };
  $("query").addEventListener("input", () => { $("clear").hidden = !$("query").value; });
  $("clear").addEventListener("click", () => { clearQuery(); $("query").focus(); });
  document.querySelector(".wordmark").addEventListener("click", clearQuery);

  for (const el of document.querySelectorAll("[data-tab]")) {
    el.addEventListener("click", (e) => { e.preventDefault(); selectTab(el.dataset.tab); });
  }

  // Suggesting a landscape for the gallery: words (never a raw hash) and the
  // name to credit. There is no server, so Send opens the visitor's mail app
  // with the suggestion filled in.
  const SUGGEST_TO = "shincontact@icloud.com";
  const HASHLIKE = /^(0x)?[0-9a-fA-F]{32,}$/;
  let previewTimer = 0;
  const openSuggest = () => {
    $("suggest").hidden = false;
    $("sg-words").value = current ? current.input : "";
    $("sg-status").textContent = "";
    $("sg-words").dispatchEvent(new Event("input"));
    $("sg-words").focus();
  };
  const closeSuggest = () => { $("suggest").hidden = true; $("suggest-btn").focus(); };
  $("suggest-btn").addEventListener("click", openSuggest);
  $("sg-cancel").addEventListener("click", closeSuggest);
  $("suggest").addEventListener("keydown", (e) => { if (e.key === "Escape") closeSuggest(); });
  $("sg-words").addEventListener("input", () => {
    const words = $("sg-words").value.trim();
    $("sg-words").setCustomValidity(HASHLIKE.test(words) ? "Please enter words, not a hash." : "");
    clearTimeout(previewTimer);
    if (!words || HASHLIKE.test(words)) { $("sg-preview").removeAttribute("src"); return; }
    previewTimer = setTimeout(async () => {
      const hex = await sha256(words);
      if ($("sg-words").value.trim() === words) $("sg-preview").src = await svgUrl(hex);
    }, 350);
  });
  $("suggest-form").addEventListener("submit", async (e) => {
    e.preventDefault();
    const words = $("sg-words").value.trim(), name = $("sg-name").value.trim();
    if (!words || !name || HASHLIKE.test(words)) return;
    const hex = await sha256(words);
    const body = `Words: ${words}\nDiscovered by: ${name}\nSHA-256: ${hex}\nLink: https://beautifulhash.com/#${hex}\n`;
    location.href = `mailto:${SUGGEST_TO}?subject=${encodeURIComponent(`Gallery suggestion: ${words}`)}&body=${encodeURIComponent(body)}`;
    $("sg-status").textContent = "Your mail app should open with the suggestion ready to send.";
  });

  // Analytics consent (Google consent mode): ask once, remember the answer.
  const CONSENT_KEY = "beautifulhash.consent";
  let consent = null;
  try { consent = localStorage.getItem(CONSENT_KEY); } catch { /* storage unavailable */ }
  if (!consent) $("consent").hidden = false;
  const decide = (value) => {
    try { localStorage.setItem(CONSENT_KEY, value); } catch { /* storage unavailable */ }
    if (typeof gtag === "function") gtag("consent", "update", { analytics_storage: value });
    $("consent").hidden = true;
  };
  $("consent-accept").addEventListener("click", () => decide("granted"));
  $("consent-decline").addEventListener("click", () => decide("denied"));

  buildBitmap();
  renderFavs();

  // A shared link opens its hash; otherwise "world", with the search box left empty.
  const fromLink = location.hash.slice(1);
  if (HEX.test(fromLink)) { $("query").value = fromLink; show(fromLink, ""); }
  else show(EXAMPLES[1].hash, EXAMPLES[1].input).then(() => { if (fromLink === "bits") selectTab("bits"); });
}

init();
