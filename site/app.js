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

async function show(hex, input, source) {
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
  try { history.replaceState(null, "", `#${hex}`); } catch { /* sandboxed */ }
  await idle();
  const url = await svgUrl(hex);
  $("result-img").src = url;
  $("glow").style.setProperty("--glow-img", `url("${url}")`);
  $("result-loading").hidden = true;
  track(hex, input, source);
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
  $("star").setAttribute("aria-pressed", on ? "true" : "false");
  $("star").querySelector("span").textContent = on ? "Favorited" : "Favorite";
}

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

// PNG sizes. Banners are full-width bands of the landscape; the visitor picks
// the band in the crop dialog, starting 62% of the way down, which keeps the
// summits, the near ridge, and the trees.
const SIZES = {
  png: { w: 2048, h: 1536, name: "" },
  linkedin: { w: 1584, h: 396, name: "-linkedin", label: "LinkedIn banner" },
  x: { w: 1500, h: 500, name: "-x", label: "X header" },
  facebook: { w: 1640, h: 624, name: "-facebook", label: "Facebook cover" },
};
// The band's height as a fraction of the landscape's, and the allowed centers.
const bandOf = (size) => (size.h / size.w) * (1024 / 768);
const clampCenter = (c, size) => Math.min(1 - bandOf(size) / 2, Math.max(bandOf(size) / 2, c));
async function downloadPng(hex, size, center = 0.62) {
  const img = new Image();
  img.src = await svgUrl(hex);
  await img.decode();
  const canvas = document.createElement("canvas");
  canvas.width = size.w;
  canvas.height = size.h;
  // Draw the whole SVG at the banner's width, so it rasterizes sharp, and shift it up.
  const k = size.w / 1024, top = Math.min(0, Math.max(size.h - 768 * k, size.h / 2 - center * 768 * k));
  canvas.getContext("2d").drawImage(img, 0, top, 1024 * k, 768 * k);
  canvas.toBlob((blob) => save(URL.createObjectURL(blob), `beautifulhash-${hex.slice(0, 12)}${size.name}.png`), "image/png");
}

// ---- favorites strip ----------------------------------------------------------
function renderFavs() {
  const list = $("fav-list");
  list.replaceChildren(...favs.map((f) => lazy(tile(f.hash, f.input, () => show(f.hash, f.input, "favorite")), f.hash)));
  $("favorites").hidden = favs.length === 0;
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
  // The empty search box suggests a few plain words; Generate draws the one shown.
  let hint = 0;
  $("query").placeholder = SUGGESTIONS[0];
  setInterval(() => {
    if ($("query").value) return;
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
  // Download menu: the standard PNG, the SVG, or a banner for a social profile.
  const menu = $("dl-menu"), items = [...menu.querySelectorAll("[role=menuitem]")];
  const setMenu = (open) => {
    menu.hidden = !open;
    $("dl-btn").setAttribute("aria-expanded", String(open));
    if (open) items[0].focus();
  };
  $("dl-btn").addEventListener("click", () => setMenu(menu.hidden));
  document.addEventListener("click", (e) => { if (!menu.hidden && !e.target.closest(".dl")) setMenu(false); });
  menu.addEventListener("keydown", (e) => {
    const i = items.indexOf(document.activeElement);
    if (e.key === "Escape") { setMenu(false); $("dl-btn").focus(); }
    if (e.key === "ArrowDown" || e.key === "ArrowUp") { e.preventDefault(); items[(i + (e.key === "ArrowDown" ? 1 : items.length - 1)) % items.length].focus(); }
  });
  for (const item of items) item.addEventListener("click", async () => {
    setMenu(false);
    if (!current) return;
    const kind = item.dataset.dl;
    if (kind === "svg") save(await svgUrl(current.hash), `beautifulhash-${current.hash.slice(0, 12)}.svg`);
    else if (kind === "png") await downloadPng(current.hash, SIZES.png);
    else return openCrop(kind);
    tracked(kind);
  });
  const tracked = (kind) => { try { if (typeof gtag === "function") gtag("event", "landscape_downloaded", { format: kind }); } catch { /* analytics blocked */ } };

  // Crop dialog: a band at the banner's proportions over the whole landscape.
  let crop = null;  // { kind, size, center }
  const placeBand = () => {
    const band = bandOf(crop.size);
    $("crop-band").style.top = `${(crop.center - band / 2) * 100}%`;
    $("crop-band").style.height = `${band * 100}%`;
    $("crop-band").setAttribute("aria-valuenow", String(Math.round(crop.center * 100)));
  };
  const openCrop = async (kind) => {
    const size = SIZES[kind];
    crop = { kind, size, center: clampCenter(0.62, size) };
    $("crop-title").textContent = size.label;
    $("crop-size").textContent = `${size.w} × ${size.h}`;
    $("crop-img").src = await svgUrl(current.hash);
    placeBand();
    $("crop").hidden = false;
    $("crop-band").focus();
  };
  const closeCrop = () => { $("crop").hidden = true; $("dl-btn").focus(); };
  // Dragging moves the band by the pointer's travel; a press outside it centers it there.
  let drag = null;
  $("crop-stage").addEventListener("pointerdown", (e) => {
    const rect = $("crop-stage").getBoundingClientRect(), y = (e.clientY - rect.top) / rect.height;
    if (e.target !== $("crop-band")) { crop.center = clampCenter(y, crop.size); placeBand(); }
    drag = { y, center: crop.center, h: rect.height };
    $("crop-stage").setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  $("crop-stage").addEventListener("pointermove", (e) => {
    if (!drag) return;
    const rect = $("crop-stage").getBoundingClientRect();
    crop.center = clampCenter(drag.center + (e.clientY - rect.top) / drag.h - drag.y, crop.size);
    placeBand();
  });
  const endDrag = () => { drag = null; };
  $("crop-stage").addEventListener("pointerup", endDrag);
  $("crop-stage").addEventListener("pointercancel", endDrag);
  $("crop-band").addEventListener("keydown", (e) => {
    const step = { ArrowUp: -0.02, ArrowDown: 0.02, PageUp: -0.1, PageDown: 0.1 }[e.key];
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
    closeCrop();
  });
  $("copy-link").addEventListener("click", async () => {
    if (!current) return;
    const link = `${location.origin}${location.pathname}#${current.hash}`;
    try { await navigator.clipboard.writeText(link); $("status").textContent = "Link copied."; }
    catch { $("status").textContent = link; }
  });

  $("carousel").replaceChildren(...EXAMPLES.map((ex, i) => lazy(tile(ex.hash, ex.input, () => openLightbox(i)), ex.hash)));
  // The gallery scrolls sideways with a mouse wheel too, and by dragging.
  dragScroll($("carousel"));
  dragScroll($("fav-list"));
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
    openExample(ex, "gallery");
    $("result").scrollIntoView({ behavior: "smooth", block: "start" });
  });
  document.addEventListener("keydown", (e) => {
    if ($("lightbox").hidden) return;
    if (e.key === "Escape") closeLightbox();
    if (e.key === "ArrowLeft") openLightbox(lbIndex - 1);
    if (e.key === "ArrowRight") openLightbox(lbIndex + 1);
  });

  // The clear button shows while the search box has text; it and the logo
  // both empty the box and put the cursor in it.
  const clearQuery = () => { $("query").value = ""; $("clear").hidden = true; $("query").focus({ preventScroll: true }); };
  $("query").addEventListener("input", () => { $("clear").hidden = !$("query").value; });
  $("clear").addEventListener("click", clearQuery);

  document.querySelector(".wordmark").addEventListener("click", (e) => { e.preventDefault(); clearQuery(); });

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
  else show(EXAMPLES[1].hash, EXAMPLES[1].input);
}

init();
