// Renders hashes with the WebAssembly build and prints SHA-256 of each SVG, one
// per line ("<hash> <sha256>"), for comparison with the native build
// (wasm/golden.txt; `make wasm-check`).
import { createRequire } from "node:module";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
const require = createRequire(import.meta.url);
const BeautifulHash = require("../site/bh.js");
const mod = await BeautifulHash();
const render = mod.cwrap("bh_render", "string", ["string", "number", "number"]);
for (const line of readFileSync(process.argv[2], "utf8").split("\n")) {
  const hex = line.trim().split(/\s+/)[0];
  if (!hex) continue;
  const svg = render(hex, 1024, 768);
  console.log(hex, createHash("sha256").update(svg).digest("hex"));
}
