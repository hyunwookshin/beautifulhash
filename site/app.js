// BeautifulHash site: renders landscapes in the browser with the WebAssembly
// build of core/ (bh.js, bh.wasm). No server: hashing, rendering, favorites, and
// downloads all happen here.
"use strict";

// Example landscapes, each with a short title that hints at the scene without
// naming how it was chosen. `input` is the text that hashes to it, if any;
// `by` is who discovered it (the hash-only ones were found while testing).
const EXAMPLES = [
  { hash: "9e6ed8c1b2283a48a64fca7f8be20b89e01fa378280f91c631ffe62c2f144534", title: "Where the maples burn", by: "Claude" },
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
// The landscape shown on arrival: "world", kept out of the gallery so its
// arrows count on from it (world+1, world+2, …) instead of walking the gallery.
const WELCOME = { hash: "486ea46224d1bb4fb680f34f7c9ad96a8f24ec88be73ea8e5a6c65260e9cb8a7", input: "world" };
const SUGGESTIONS = ["Hello!", "chocolate muffin", "good morning", "my cat Miso", "rainy Tuesday", "Happy Birthday", "coffee first", "see you at sunset"];

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
// The credit shown for it; the site's author goes uncredited on the site.
const credit = (hex) => discoverer(hex) === "Hyunwook Shin" ? "" : discoverer(hex);

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
  const by = credit(hex);
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
// What the arrows walk: the gallery or the favorites, looping, when the current
// landscape came from one of them; otherwise "seed", which counts past the
// input: world, world+1, world+2, and back.
let mode = "seed";

async function show(hex, input, source) {
  hex = hex.toLowerCase();
  current = { hash: hex, input };
  if (source !== "arrow") {
    mode = source === "gallery" ? "gallery" : source === "favorite" ? "favorites"
         : isFav(hex) ? "favorites" : EXAMPLES.some((e) => e.hash === hex) ? "gallery" : "seed";
  }
  highlight(source === "arrow");
  for (const b of document.querySelectorAll("[data-step='-1']")) b.disabled = mode === "seed" && !SEED_STEP.test(input || hex);
  $("result").hidden = false;
  // A landscape drawn ahead of time (a neighbor, or one seen before) shows at once.
  $("result-loading").hidden = urls.has(hex);
  $("ref-input-row").hidden = !input;
  $("ref-input").textContent = input || "";
  $("ref-hash").textContent = hex;
  $("ref-by-row").hidden = !credit(hex);
  $("ref-by").textContent = credit(hex);
  $("result-img").alt = `Landscape for hash ${hex}`;
  $("status").textContent = "";
  // Only words that are not in the gallery yet can be suggested for it.
  $("clear").hidden = !$("query").value;
  $("suggest-btn").hidden = !input || !!discoverer(hex);
  updateStar();
  // The welcome landscape keeps a bare address; any other puts its hash there to share.
  try { history.replaceState(null, "", source ? `#${hex}` : location.pathname); } catch { /* sandboxed */ }
  await idle();
  const url = await svgUrl(hex);
  $("result-img").src = url;
  updateViewer();
  $("glow").style.setProperty("--glow-img", `url("${url}")`);
  $("result-loading").hidden = true;
  track(hex, input, source);
  prefetch();
}

// Moves to the next (d = 1) or previous (d = -1) landscape.
const SEED_STEP = /^(.*)\+(\d+)$/s;
// The landscape one step away, as { hash, input, query }, or null if none.
async function target(d) {
  if (!current) return null;
  const list = mode === "gallery" ? EXAMPLES : mode === "favorites" ? favs : null;
  if (list && list.length) {
    const i = list.findIndex((x) => x.hash === current.hash);
    const next = list[((i < 0 ? (d > 0 ? -1 : 0) : i) + d + list.length) % list.length];
    return { hash: next.hash, input: next.input || "", query: mode === "gallery" ? next.input || next.hash : null };
  }
  const m = SEED_STEP.exec(current.input || current.hash);
  const root = m ? m[1] : current.input || current.hash, n = (m ? +m[2] : 0) + d;
  if (n < 0) return null;
  const text = n ? `${root}+${n}` : root;
  return HEX.test(text) ? { hash: text.toLowerCase(), input: "", query: text } : { hash: await sha256(text), input: text, query: text };
}
async function stepBy(d) {
  if (mode !== "seed" && !(mode === "gallery" ? EXAMPLES : favs).length) mode = "seed";
  const t = await target(d);
  if (!t) return;
  if (t.query !== null) { $("query").value = t.query; $("clear").hidden = false; }
  return show(t.hash, t.input, "arrow");
}

// Draws the landscapes on either side of the current one while it is being
// looked at, so a swipe or an arrow shows them at once; a swipe drags them in
// beside it. Stale results (the visitor moved on meanwhile) are dropped.
let prefetchToken = 0;
async function prefetch() {
  const token = ++prefetchToken, from = current && current.hash;
  for (const d of [1, -1]) {
    await idle();
    const t = canStep(d) ? await target(d) : null;
    const url = t ? await svgUrl(t.hash) : "";
    if (token !== prefetchToken || !current || current.hash !== from) return;
    for (const id of d > 0 ? ["peer-next", "vpeer-next"] : ["peer-prev", "vpeer-prev"]) {
      if (url) $(id).src = url; else $(id).removeAttribute("src");
    }
  }
}

// Outlines the current landscape's tile in the strip the arrows walk, and
// scrolls that strip to it when an arrow moved there.
function highlight(scroll) {
  const strip = mode === "gallery" ? $("carousel") : mode === "favorites" ? $("fav-list") : null;
  for (const t of document.querySelectorAll(".tile")) {
    const on = !!current && t.parentElement === strip && t.dataset.hash === current.hash;
    t.classList.toggle("selected", on);
    if (on && scroll) strip.scrollTo({ left: t.offsetLeft - (strip.clientWidth - t.offsetWidth) / 2, behavior: "smooth" });
  }
}

// Reports each landscape a visitor asks for to Google Analytics as
// landscape_generated: how it was asked for and what kind of scene came out.
// The typed words never leave the page; only their length is sent. The
// default landscape shown on load is not reported.
const THEME_NAMES = ["alpine", "temperate", "tundra", "desert"];
const SEASON_NAMES = ["winter", "spring", "summer", "fall"];
function track(hex, input, source) {
  if (!source || typeof gtag !== "function") return;
  const g = parseInt(hex.slice(0, 2), 16);
  try {
    gtag("event", "landscape_generated", {
      source,
      input_kind: input ? "words" : "hash",
      input_length: input.length,
      in_gallery: !!discoverer(hex),
      theme: THEME_NAMES[(g >> 4) & 3],
      season: SEASON_NAMES[(g >> 2) & 3],
      daylight: (g >> 6) === 3 ? "night" : "day",
    });
  } catch { /* analytics blocked */ }
}

function updateStar() {
  const on = current && isFav(current.hash);
  for (const b of [$("star"), $("v-fav")]) {
    b.setAttribute("aria-pressed", on ? "true" : "false");
    b.querySelector("span").textContent = on ? "Favorited" : "Favorite";
  }
}
function toggleFav() {
  if (!current) return;
  favs = isFav(current.hash) ? favs.filter((f) => f.hash !== current.hash)
                             : [{ hash: current.hash, input: current.input || "" }, ...favs];
  saveFavs(favs);
  updateStar();
  renderFavs();
  prefetch();
}
// Shares the current landscape's link: the device's share sheet where there is
// one, else the clipboard. Returns a word for the visitor, or "" if nothing to say.
async function shareLink() {
  if (!current) return "";
  const url = `${location.origin}${location.pathname}#${current.hash}`;
  if (navigator.share) {
    try { await navigator.share({ title: "BeautifulHash", text: caption(current.hash, current.input), url }); return ""; }
    catch (e) { if (e.name === "AbortError") return ""; }
  }
  try { await navigator.clipboard.writeText(url); return "Link copied."; } catch { return url; }
}
function updateViewer() {
  if ($("viewer").hidden || !current) return;
  $("viewer-img").src = $("result-img").src;
  $("viewer-img").alt = $("result-img").alt;
  $("viewer-bg").style.backgroundImage = `url("${$("result-img").src}")`;
  $("viewer-title").textContent = caption(current.hash, current.input);
  $("viewer-hash").textContent = `${current.hash.slice(0, 16)}…`;
}
// Whether stepping d (1 or -1) moves anywhere: only the plain input stays put going back.
const canStep = (d) => !!current && (mode !== "seed" || d > 0 || SEED_STEP.test(current.input || current.hash));

async function submit(text) {
  text = text.trim() || $("query").placeholder;
  const source = $("query").value.trim() ? "typed" : "suggestion";
  if (HEX.test(text)) return show(text, "", source);
  return show(await sha256(text), text, source);
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

// PNG sizes. Banners and wide screens are full-width bands of the landscape,
// starting 62% of the way down, which keeps the summits, the near ridge, and
// the trees; portrait screens are full-height columns, starting centered. The
// visitor moves the band in the crop dialog.
const SIZES = {
  png: { w: 2048, h: 1536, name: "" },
  linkedin: { w: 1584, h: 396, name: "-linkedin", label: "LinkedIn banner" },
  x: { w: 1500, h: 500, name: "-x", label: "X header" },
  facebook: { w: 1640, h: 624, name: "-facebook", label: "Facebook cover" },
  desktop: { w: 3840, h: 2160, name: "-desktop", label: "Desktop background" },
};
// Phone and tablet backgrounds, by maker, at each screen's native portrait
// resolution. A foldable's inner screen is nearly square.
const DEVICES = {
  phones: [
    ["iPhone", [
      ["iPhone 16 Pro Max", 1320, 2868], ["iPhone 16 Pro", 1206, 2622], ["iPhone 16 and 15", 1179, 2556], ["iPhone SE", 750, 1334]]],
    ["Pixel", [
      ["Pixel 9 Pro XL", 1344, 2992], ["Pixel 9 Pro", 1280, 2856], ["Pixel 9", 1080, 2424]]],
    ["Galaxy", [
      ["Galaxy S25 Ultra", 1440, 3120], ["Galaxy S25", 1080, 2340], ["Other Android", 1080, 2400]]],
  ],
  tablets: [
    ["Foldables", [
      ["Galaxy Z Fold6, inner", 1856, 2160], ["Galaxy Z Fold6, cover", 968, 2376], ["Galaxy Z Flip6", 1080, 2640],
      ["Pixel 9 Pro Fold, inner", 2076, 2152], ["Pixel 9 Pro Fold, cover", 1080, 2424]]],
    ["Tablets", [
      ["iPad Pro 13″", 2064, 2752], ["iPad Pro 11″", 1668, 2420], ["iPad Air 11″", 1640, 2360], ["iPad mini", 1488, 2266],
      ["Galaxy Tab S10 Ultra", 1848, 2960]]],
  ],
};
// Card formats at 300 dpi, each with 1/8 inch (A6: 3 mm) of bleed on every side.
const CARDS = [
  ["5 × 7 in, landscape", 2175, 1575], ["5 × 7 in, portrait", 1575, 2175],
  ["4 × 6 in postcard, landscape", 1875, 1275], ["4 × 6 in postcard, portrait", 1275, 1875],
  ["4.25 × 5.5 in (A2), landscape", 1725, 1350], ["5.5 × 5.5 in, square", 1725, 1725],
  ["A6, landscape", 1819, 1311], ["A6, portrait", 1311, 1819],
];
const slug = (name) => name.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
for (const groups of Object.values(DEVICES)) {
  for (const [, list] of groups) for (const [label, w, h] of list) SIZES[slug(label)] = { w, h, name: `-${slug(label)}`, label };
}
for (const [label, w, h] of CARDS) SIZES[`card-${slug(label)}`] = { w, h, name: `-card-${slug(label)}`, label: `Card, ${label}` };
// The visitor's own screen, in device pixels: CSS pixels times the pixel
// ratio. Phones and tablets get it upright; a desktop screen stays wide. A
// browser reports the scaled resolution, so a phone set to a lower display
// resolution gets that one, which is the one its wallpaper shows at.
function thisScreen() {
  const dpr = window.devicePixelRatio || 1, a = Math.round(screen.width * dpr), b = Math.round(screen.height * dpr);
  const touch = matchMedia("(pointer: coarse)").matches, small = Math.min(screen.width, screen.height) < 600;
  const label = touch ? (small ? "This phone" : "This tablet") : "This screen";
  return touch ? { w: Math.min(a, b), h: Math.max(a, b), name: "-screen", label } : { w: a, h: b, name: "-screen", label };
}
// A size narrower than 4:3 is a column (it crops across), else a band (it crops
// down); bandOf is its share of the landscape along that axis.
const isColumn = (size) => size.w / size.h < 1024 / 768;
const bandOf = (size) => isColumn(size) ? (size.w / size.h) * (768 / 1024) : (size.h / size.w) * (1024 / 768);
const clampCenter = (c, size) => Math.min(1 - bandOf(size) / 2, Math.max(bandOf(size) / 2, c));
const startCenter = (size) => clampCenter(isColumn(size) ? 0.5 : 0.62, size);
async function downloadPng(hex, size, center = 0.62) {
  const img = new Image();
  img.src = await svgUrl(hex);
  await img.decode();
  const canvas = document.createElement("canvas");
  canvas.width = size.w;
  canvas.height = size.h;
  // Draw the whole SVG scaled to cover the canvas, so it rasterizes sharp, and
  // shift it so the chosen center lands mid-canvas.
  const col = isColumn(size), k = col ? size.h / 768 : size.w / 1024;
  const shift = (len, full) => Math.min(0, Math.max(len - full * k, len / 2 - center * full * k));
  canvas.getContext("2d").drawImage(img, col ? shift(size.w, 1024) : 0, col ? 0 : shift(size.h, 768), 1024 * k, 768 * k);
  canvas.toBlob((blob) => save(URL.createObjectURL(blob), `beautifulhash-${hex.slice(0, 12)}${size.name}.png`), "image/png");
}

// ---- favorites strip ----------------------------------------------------------
function renderFavs() {
  const list = $("fav-list");
  list.replaceChildren(...favs.map((f) => lazy(tile(f.hash, f.input, () => show(f.hash, f.input, "favorite")), f.hash)));
  $("favorites").hidden = favs.length === 0;
  highlight(false);
}

// A vertical wheel scrolls a strip sideways while it can still move that way,
// and dragging pans it; a drag never counts as a click on a tile.
function dragScroll(el) {
  el.addEventListener("wheel", (e) => {
    if (Math.abs(e.deltaX) > Math.abs(e.deltaY)) return;
    const max = el.scrollWidth - el.clientWidth;
    if ((e.deltaY < 0 && el.scrollLeft <= 0) || (e.deltaY > 0 && el.scrollLeft >= max - 1)) return;
    e.preventDefault();
    el.scrollLeft += e.deltaY;
  }, { passive: false });
  let x0 = 0, left0 = 0, moved = false, down = false;
  el.addEventListener("pointerdown", (e) => {
    if (e.pointerType !== "mouse" || e.button !== 0) return;
    down = true; moved = false; x0 = e.clientX; left0 = el.scrollLeft;
  });
  el.addEventListener("pointermove", (e) => {
    if (!down) return;
    if (!moved && Math.abs(e.clientX - x0) > 5) { moved = true; el.classList.add("dragging"); }
    if (moved) el.scrollLeft = left0 - (e.clientX - x0);
  });
  const up = () => { down = false; el.classList.remove("dragging"); };
  el.addEventListener("pointerup", up);
  el.addEventListener("pointerleave", up);
  el.addEventListener("click", (e) => { if (moved) { e.stopPropagation(); e.preventDefault(); moved = false; } }, true);
  el.addEventListener("dragstart", (e) => e.preventDefault());
}

// ---- wiring -------------------------------------------------------------------
function init() {
  // The bit map was once a tab at #bits; it is its own page now.
  if (location.hash === "#bits") { location.replace("bitmap"); return; }

  // An example opens with its input text when it has one, else its hash.
  const openExample = (ex, source) => { $("query").value = ex.input || ex.hash; return show(ex.hash, ex.input || "", source); };
  for (const ex of RECOMMENDED) {
    const b = document.createElement("button");
    b.type = "button";
    b.className = "chip";
    b.innerHTML = `<b></b><code>${ex.input ? `“${ex.input}”` : `${ex.hash.slice(0, 12)}…`}</code>`;
    b.querySelector("b").textContent = ex.title;
    b.addEventListener("click", () => openExample(ex, "chip"));
    $("chips").appendChild(b);
  }
  // The empty search box suggests a few plain words; Convert draws the one shown.
  let hint = 0;
  $("query").placeholder = SUGGESTIONS[0];
  setInterval(() => {
    if ($("query").value) return;
    hint = (hint + 1) % SUGGESTIONS.length;
    $("query").placeholder = SUGGESTIONS[hint];
  }, 3200);
  $("search").addEventListener("submit", (e) => { e.preventDefault(); submit($("query").value); });

  $("star").addEventListener("click", toggleFav);
  // Download menu: the standard PNG, the SVG, a banner for a social profile, or a
  // background. Phones and tablets open a second page in place of the first,
  // with a back row, so the menu keeps its width on a phone.
  const menu = $("dl-menu");
  const PAGE_TITLES = { phones: "Phones", tablets: "Foldables and tablets" };
  for (const [page, groups] of Object.entries(DEVICES)) {
    const div = document.createElement("div");
    div.className = "dl-page";
    div.dataset.page = page;
    div.hidden = true;
    div.innerHTML = `<button role="menuitem" class="dl-back" data-back><svg viewBox="0 0 24 24" width="16" height="16" aria-hidden="true" class="line"><path d="M15 6l-6 6 6 6"/></svg><span>${PAGE_TITLES[page]}</span></button>`
      + groups.map(([maker, list]) => `<div class="dl-sep" role="separator">${maker}</div>`
        + list.map(([label, w, h]) => `<button role="menuitem" data-dl="${slug(label)}"><span>${label}<small>${w} × ${h}</small></span></button>`).join("")).join("");
    menu.appendChild(div);
  }
  const items = () => [...menu.querySelectorAll(".dl-page:not([hidden]) [role=menuitem]")];
  const showPage = (page, focus) => {
    for (const p of menu.querySelectorAll(".dl-page")) p.hidden = p.dataset.page !== page;
    (focus ? menu.querySelector(`[data-sub="${focus}"]`) : items()[0])?.focus();
  };
  const setMenu = (open) => {
    menu.hidden = !open;
    $("dl-btn").setAttribute("aria-expanded", String(open));
    if (open) {
      // Measured on each opening: the window may have moved to another monitor.
      SIZES.screen = thisScreen();
      $("dl-screen-name").textContent = SIZES.screen.label;
      $("dl-screen-size").textContent = `${SIZES.screen.w} × ${SIZES.screen.h}`;
      showPage("main");
    }
  };
  $("dl-btn").addEventListener("click", () => setMenu(menu.hidden));
  document.addEventListener("click", (e) => { if (!menu.hidden && !e.target.closest(".dl")) setMenu(false); });
  menu.addEventListener("keydown", (e) => {
    const list = items(), i = list.indexOf(document.activeElement), page = menu.querySelector(".dl-page:not([hidden])").dataset.page;
    if (e.key === "Escape") { e.stopPropagation(); if (page === "main") { setMenu(false); $("dl-btn").focus(); } else showPage("main", page); }
    if (e.key === "ArrowDown" || e.key === "ArrowUp") { e.preventDefault(); list[(i + (e.key === "ArrowDown" ? 1 : list.length - 1)) % list.length].focus(); }
    if (e.key === "ArrowRight" && document.activeElement.dataset.sub) { e.preventDefault(); showPage(document.activeElement.dataset.sub); }
    if (e.key === "ArrowLeft" && page !== "main") { e.preventDefault(); showPage("main", page); }
  });
  menu.addEventListener("click", async (e) => {
    const item = e.target.closest("[role=menuitem]");
    if (!item) return;
    // The page swap hides the clicked row; keep the outside-click check from closing the menu.
    e.stopPropagation();
    if (item.dataset.sub) return showPage(item.dataset.sub);
    if (item.hasAttribute("data-back")) return showPage("main", item.closest(".dl-page").dataset.page);
    setMenu(false);
    if (!current) return;
    const kind = item.dataset.dl;
    if (kind === "svg") save(await svgUrl(current.hash), `beautifulhash-${current.hash.slice(0, 12)}.svg`);
    else if (kind === "png") await downloadPng(current.hash, SIZES.png);
    else if (kind === "card") return openCard();
    else return openCrop(kind);
    tracked(kind);
  });
  const tracked = (kind) => { try { if (typeof gtag === "function") gtag("event", "landscape_downloaded", { format: kind }); } catch { /* analytics blocked */ } };

  // Crop dialog: a band (or a column) at the size's proportions over the whole landscape.
  let crop = null;  // { kind, size, center }
  const placeBand = () => {
    const band = bandOf(crop.size), col = isColumn(crop.size), b = $("crop-band").style;
    b.top = col ? "0" : `${(crop.center - band / 2) * 100}%`;
    b.height = col ? "100%" : `${band * 100}%`;
    b.left = col ? `${(crop.center - band / 2) * 100}%` : "0";
    b.width = col ? `${band * 100}%` : "100%";
    $("crop-stage").classList.toggle("column", col);
    $("crop-band").setAttribute("aria-valuenow", String(Math.round(crop.center * 100)));
  };
  const openCrop = async (kind) => {
    const size = SIZES[kind];
    crop = { kind, size, center: startCenter(size) };
    $("crop-title").textContent = size.label;
    $("crop-size").textContent = `${size.w} × ${size.h}`;
    $("crop-img").src = await svgUrl(current.hash);
    placeBand();
    $("crop").hidden = false;
    $("crop-band").focus();
  };
  // A card's crop returns to the card dialog; anything else, to the menu button.
  const closeCrop = () => {
    $("crop").hidden = true;
    if (crop.kind.startsWith("card-")) { $("card").hidden = false; $("card-dl").focus(); } else $("dl-btn").focus();
  };
  // Dragging moves the band by the pointer's travel; a press outside it centers it there.
  let drag = null;
  $("crop-stage").addEventListener("pointerdown", (e) => {
    const at = (e) => {
      const r = $("crop-stage").getBoundingClientRect();
      return isColumn(crop.size) ? (e.clientX - r.left) / r.width : (e.clientY - r.top) / r.height;
    };
    const p = at(e);
    if (e.target !== $("crop-band")) { crop.center = clampCenter(p, crop.size); placeBand(); }
    drag = { at, p, center: crop.center };
    $("crop-stage").setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  $("crop-stage").addEventListener("pointermove", (e) => {
    if (!drag) return;
    crop.center = clampCenter(drag.center + drag.at(e) - drag.p, crop.size);
    placeBand();
  });
  const endDrag = () => { drag = null; };
  $("crop-stage").addEventListener("pointerup", endDrag);
  $("crop-stage").addEventListener("pointercancel", endDrag);
  $("crop-band").addEventListener("keydown", (e) => {
    const step = { ArrowUp: -0.02, ArrowLeft: -0.02, ArrowDown: 0.02, ArrowRight: 0.02, PageUp: -0.1, PageDown: 0.1 }[e.key];
    if (step === undefined) return;
    e.preventDefault();
    crop.center = clampCenter(crop.center + step, crop.size);
    placeBand();
  });
  $("crop").addEventListener("keydown", (e) => { if (e.key === "Escape") closeCrop(); });
  $("crop").addEventListener("click", (e) => { if (e.target === $("crop")) closeCrop(); });
  $("crop-cancel").addEventListener("click", closeCrop);
  $("crop-save").addEventListener("click", async () => {
    await downloadPng(current.hash, crop.size, crop.center);
    tracked(crop.kind);
    if (crop.kind.startsWith("card-")) $("card-note").textContent = `Downloaded: ${crop.size.w} × ${crop.size.h} pixels. Now upload it below.`;
    closeCrop();
  });
  // Personalized cards: step 1 picks a format and downloads it through the crop
  // dialog; step 2 links to card makers to upload it to.
  $("card-format").replaceChildren(...CARDS.map(([label, w, h]) => new Option(`${label} · ${w} × ${h}`, `card-${slug(label)}`)));
  const CARD_NOTE = $("card-note").textContent;
  let cardOpener = null;
  const openCard = () => {
    if (!current) return;
    cardOpener = document.activeElement;
    $("card-note").textContent = CARD_NOTE;
    $("card").hidden = false;
    $("card-format").focus();
  };
  const closeCard = () => { $("card").hidden = true; (cardOpener && cardOpener.isConnected && !cardOpener.closest("[hidden]") ? cardOpener : $("dl-btn")).focus(); };
  $("card-btn").addEventListener("click", openCard);
  $("card-dl").addEventListener("click", () => { $("card").hidden = true; openCrop($("card-format").value); });
  $("card-done").addEventListener("click", closeCard);
  $("card-close").addEventListener("click", closeCard);
  $("card").addEventListener("click", (e) => { if (e.target === $("card")) closeCard(); });
  $("card").addEventListener("keydown", (e) => { if (e.key === "Escape") closeCard(); });
  // Full screen: a tap on the image shows the expand button for a few seconds
  // (hover shows it with a mouse). The viewer fills the window, and the whole
  // screen where the browser allows it.
  let touchedTimer = 0;
  $("result-img").addEventListener("click", () => {
    $("result").querySelector(".frame").classList.add("touched");
    clearTimeout(touchedTimer);
    touchedTimer = setTimeout(() => $("result").querySelector(".frame").classList.remove("touched"), 3000);
  });
  // On a touch screen, a pill fades in and out to say how to swipe: on the
  // page when the image first comes into view and on leaving full screen, and
  // on entering full screen.
  const touch = matchMedia("(pointer: coarse)").matches;
  const pill = (id, text) => {
    const el = $(id);
    if (text) el.textContent = text;
    el.classList.remove("show");
    void el.offsetWidth;  // restart the animation
    el.classList.add("show");
  };
  const tip = (id) => { if (touch) pill(id); };
  new IntersectionObserver((entries, io) => {
    if (entries.some((e) => e.isIntersecting)) { tip("tip-page"); io.disconnect(); }
  }, { threshold: 0.6 }).observe($("result").querySelector(".frame"));

  // Full screen: the landscape with its caption and a column of actions, as in
  // a feed. A tap hides or shows them; ×, Escape, or leaving the browser's full
  // screen closes it.
  const closeViewer = () => {
    if ($("viewer").hidden) return;
    $("viewer").hidden = true;
    if (document.fullscreenElement) document.exitFullscreen().catch(() => {});
    $("expand").focus({ preventScroll: true });
    tip("tip-page");
  };
  $("expand").addEventListener("click", () => {
    $("viewer").hidden = false;
    $("viewer").classList.remove("bare");
    updateViewer();
    $("viewer-close").focus();
    $("viewer").requestFullscreen?.().catch(() => {});
    tip("tip-full");
  });
  $("viewer-close").addEventListener("click", closeViewer);
  $("v-fav").addEventListener("click", toggleFav);
  $("v-share").addEventListener("click", async () => { const said = await shareLink(); if (said) pill("viewer-toast", said); });
  $("v-save").addEventListener("click", async () => { if (!current) return; await downloadPng(current.hash, SIZES.png); tracked("png"); pill("viewer-toast", "Saved."); });
  $("viewer").addEventListener("keydown", (e) => { if (e.key === "Escape") closeViewer(); });
  document.addEventListener("fullscreenchange", () => { if (!document.fullscreenElement) closeViewer(); });

  // Swipes step through landscapes: sideways on the page, up and down in full
  // screen (up for the next, as in a feed). The image follows the finger; past
  // a quarter of the way, or on a quick flick, it slides off and the next one
  // slides in from the other side once drawn. A swipe never counts as a tap.
  let swiped = false;
  const onSwipe = (el, axis, track) => {
    let start = null, busy = false;
    const size = () => (axis === "x" ? el.clientWidth : el.clientHeight);
    const move = (px, ms) => {
      track.style.transition = ms ? `transform ${ms}ms cubic-bezier(0.2, 0.8, 0.2, 1)` : "none";
      track.style.transform = px ? `translate${axis.toUpperCase()}(${px}px)` : "";
    };
    const wait = (ms) => new Promise((r) => setTimeout(r, ms));
    el.addEventListener("pointerdown", (e) => {
      if (busy || e.pointerType === "mouse" || e.target.closest("button")) return;
      start = { x: e.clientX, y: e.clientY, t: e.timeStamp, locked: false };
    });
    el.addEventListener("pointermove", (e) => {
      if (!start) return;
      const dx = e.clientX - start.x, dy = e.clientY - start.y;
      const [along, across] = axis === "x" ? [dx, dy] : [dy, dx];
      if (!start.locked) {
        if (Math.abs(along) > 8 && Math.abs(along) > Math.abs(across)) start.locked = true;
        else if (Math.abs(across) > 8) { start = null; return; }
        else return;
      }
      // A step that goes nowhere still gives a little, then resists.
      move(canStep(along < 0 ? 1 : -1) ? along : along * 0.25, 0);
    });
    el.addEventListener("pointercancel", () => { if (start?.locked) move(0, 200); start = null; });
    el.addEventListener("pointerup", async (e) => {
      if (!start) return;
      const s0 = start;
      start = null;
      if (!s0.locked) return;
      swiped = true;
      setTimeout(() => { swiped = false; }, 400);
      const along = axis === "x" ? e.clientX - s0.x : e.clientY - s0.y, d = along < 0 ? 1 : -1;
      const far = Math.abs(along) > size() / 4 || (e.timeStamp - s0.t < 300 && Math.abs(along) > 40);
      if (!far || !canStep(d)) return move(0, 200);
      // Slide the neighbor fully in, then make it the current image in place.
      busy = true;
      move(-d * size(), 200);
      await wait(200);
      const peer = track.querySelector(d > 0 ? ".peer.next" : ".peer.prev"), img = track.querySelector("img:not(.peer)");
      if (peer.getAttribute("src")) img.src = peer.src;
      move(0, 0);
      busy = false;
      await stepBy(d);
    });
  };
  onSwipe($("result").querySelector(".frame"), "x", $("track"));
  onSwipe($("viewer"), "y", $("vtrack"));
  $("result-img").addEventListener("click", (e) => { if (swiped) e.stopImmediatePropagation(); }, true);
  $("viewer").addEventListener("click", (e) => {
    if (swiped || e.target.closest("button")) return;
    $("viewer").classList.toggle("bare");
  });

  $("copy-link").addEventListener("click", async () => {
    const said = await shareLink();
    if (said) $("status").textContent = said;
  });

  // A gallery tile loads straight into the result, as a favorite does.
  $("carousel").replaceChildren(...EXAMPLES.map((ex) => lazy(tile(ex.hash, ex.input, () => {
    openExample(ex, "gallery");
    $("result").scrollIntoView({ behavior: "smooth", block: "start" });
  }), ex.hash)));
  // The gallery scrolls sideways with a mouse wheel too, and by dragging.
  dragScroll($("carousel"));
  dragScroll($("fav-list"));
  // The strips' own arrows scroll them; the result's arrows step through landscapes.
  for (const b of document.querySelectorAll("[data-rail]")) b.addEventListener("click", () => {
    const strip = b.parentElement.querySelector(".carousel, .thumbs");
    strip.scrollBy({ left: Number(b.dataset.rail) * strip.clientWidth * 0.8, behavior: "smooth" });
  });
  for (const b of document.querySelectorAll("[data-step]")) b.addEventListener("click", () => stepBy(Number(b.dataset.step)));
  document.addEventListener("keydown", (e) => {
    if ((e.key !== "ArrowLeft" && e.key !== "ArrowRight") || e.altKey || e.ctrlKey || e.metaKey || e.shiftKey) return;
    if (!$("crop").hidden || !$("viewer").hidden || e.target.closest?.("input, textarea, select, [role=menu], [role=slider], .carousel, .thumbs")) return;
    e.preventDefault();
    stepBy(e.key === "ArrowRight" ? 1 : -1);
  });


  // The clear button shows while the search box has text; it and the logo
  // both empty the box and put the cursor in it.
  const clearQuery = () => { $("query").value = ""; $("clear").hidden = true; $("query").focus({ preventScroll: true }); };
  $("query").addEventListener("input", () => { $("clear").hidden = !$("query").value; });
  $("clear").addEventListener("click", clearQuery);

  // The logo starts over, as a fresh visit would: the welcome landscape, no hash.
  document.querySelector(".wordmark").addEventListener("click", (e) => {
    e.preventDefault();
    clearQuery();
    show(WELCOME.hash, WELCOME.input);
    window.scrollTo({ top: 0, behavior: "smooth" });
  });

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

  renderFavs();

  $("query").focus({ preventScroll: true });

  // A shared link opens its hash; otherwise "world", with the search box left empty.
  const fromLink = location.hash.slice(1);
  if (HEX.test(fromLink)) { $("query").value = fromLink; show(fromLink, "", "link"); }
  else show(WELCOME.hash, WELCOME.input);
}

init();
