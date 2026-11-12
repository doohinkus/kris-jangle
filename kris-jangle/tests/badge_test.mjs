// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// The badge link: the QR codes the FM-1 shows (qr_data.h, made by tools/gen_qr.py)
// and the page they open on the phone (web/sound/). The checks:
//   1. the header holds a code a carol and a level, and every code's address is the page's
//      address with the carol and the level in it;
//   2. the page's own numbers (the carols, the levels) are the header's;
//   3. the page's files are all there, the manifest's icons are, and the service worker
//      caches every file the page needs (so the page works offline).
//   node tests/badge_test.mjs
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const app = path.join(path.dirname(fileURLToPath(import.meta.url)), "..");
const read = (p) => fs.readFileSync(path.join(app, p), "utf8");

let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "ok  " : "FAIL"} ${msg}`); if (!ok) fails++; };

// --- the codes -------------------------------------------------------------
const header = read("qr_data.h");
const num = (name) => {
  const m = new RegExp(`#define ${name} (\\d+)`).exec(header);
  return m ? parseInt(m[1], 10) : -1;
};
const modules = num("QR_MODULES");
const carols = num("QR_CAROLS");
const levels = num("QR_LEVELS");
const bytes = num("QR_BYTES");
const variants = num("QR_VARIANTS");
check(modules === 33, `QR_MODULES is 33 (${modules})`);
check(carols === 4, `QR_CAROLS is 4 (${carols})`);
check(levels === 11, `QR_LEVELS is 11 (${levels})`);
check(bytes === Math.ceil((modules * modules) / 8), `QR_BYTES is ${bytes}, the modules packed`);
check(variants === carols * levels, `QR_VARIANTS is ${variants}, a code a carol and a level`);

const codes = [...header.matchAll(/\{\s*([^}]*)\},\s*\/\* c(\d+) l(\d+): (\S+) \*\//g)];
check(codes.length === variants, `the header holds ${codes.length} codes`);
codes.forEach((m, i) => {
  const carol = parseInt(m[2], 10);
  const level = parseInt(m[3], 10);
  const url = m[4];
  const n = (m[1].match(/0x[0-9A-F]{2}u/g) || []).length;
  if (n !== bytes) check(false, `code ${i}: ${n} bytes, not ${bytes}`);
  const want = `?c=${carol}&l=${level}`;
  if (!url.endsWith(want)) check(false, `code ${i}: the address ${url} does not end in ${want}`);
});
check(codes.every((m, i) => {
  const carol = parseInt(m[2], 10);
  const level = parseInt(m[3], 10);
  return carol === Math.floor(i / levels) && level === (i % levels) + 1;
}), "the codes are in order: carol by carol, level by level");

// --- the page --------------------------------------------------------------
const page = read("web/sound/app.js");
const pageCarols = /const CAROLS = \[([^\]]*)\]/.exec(page);
check(!!pageCarols, "the page has its carols");
check(pageCarols && (pageCarols[1].match(/"/g) || []).length / 2 === carols,
  `the page knows ${carols} carols`);
const pageLevels = /const LEVELS = (\d+)/.exec(page);
check(pageLevels && parseInt(pageLevels[1], 10) === levels, `the page knows ${levels} levels`);
// the names the page shows: one a carol, in the game's own spelling
const titles = /const CAROL_TITLES = \[([^\]]*)\]/.exec(page);
check(!!titles, "the page has the carols' names");
check(titles && (titles[1].match(/"/g) || []).length / 2 === carols,
  `the page names ${carols} carols`);
const base = codes.length ? codes[0][4].split("?")[0] : "";
check(!!base && base.endsWith("/sound/"), `the page's address is ${base}`);

for (const f of ["web/sound/index.html", "web/sound/app.js", "web/sound/sw.js", "web/sound/manifest.webmanifest"]) {
  check(fs.existsSync(path.join(app, f)), `${f} is there`);
}

// the tree is gone, and the text PNGs with it: the carol's name and the level
// are the page's own text now. The art the page draws must all be there.
for (const f of ["art/card.png", "art/plus.png", "art/sparkle.png", "art/divider.png",
                 "art/head_badge.png", "art/head_level.png", "art/grump.png",
                 "art/frosty1.png", "art/frosty2.png", "art/santa.png", "art/title.png",
                 "art/orn_flake.png", "art/orn_spark.png"]) {
  check(fs.existsSync(path.join(app, "web/sound", f)), `${f} is there`);
}
// the text art is gone with the HTML text: the name and the level are drawn
// by the browser in Barlow, in the page's own ink
for (const f of ["art/plaque.png", "art/seal.png", "art/carol_0.png", "art/lvl_1.png"]) {
  check(!fs.existsSync(path.join(app, "web/sound", f)), `${f} is gone`);
}
check(/art\/card\.png/.test(read("web/sound/index.html")), "the page shows the ice card");
check(/id="badge-name"/.test(read("web/sound/index.html")) &&
      /badge-name"\)\.textContent/.test(page), "the carol's name is the page's own text");
check(/id="badge-num"/.test(read("web/sound/index.html")) &&
      /badge-num"\)\.textContent/.test(page), "the level is the page's own text");
// the tree's art is gone with the tree
for (const f of ["art/fir_big.png", "art/fir_wide.png", "art/fir.json",
                 "art/bauble_green.png", "art/star.png", "art/tag.png", "art/ridge.png"]) {
  check(!fs.existsSync(path.join(app, "web/sound", f)), `${f} is gone`);
}
// the page names Frosty's firmware poses, not the old snowman
check(/art\/frosty1\.png/.test(page), "the page shows Frosty (the firmware's pose)");
check(!/art\/snowman\.png/.test(page), "the page does not use the old snowman");
check(!/art\/frosty\.png/.test(page), "the page does not use the old frosty");

// the page's fonts: Barlow, self-hosted (the page works offline)
for (const f of ["web/sound/barlow-medium.woff2", "web/sound/barlow-bold.woff2"]) {
  check(fs.existsSync(path.join(app, f)), `${f} is there`);
}
check(/@font-face/.test(read("web/sound/index.html")), "the page declares its font");

// the jingle: the game's own reward jingle, clipped, and the page plays it
check(fs.existsSync(path.join(app, "web/sound/reward.mp3")), "the jingle is there");
check(/reward\.mp3/.test(page), "the page plays the jingle");

// the manifest: it must parse, and its icons must be there
let manifest = null;
try { manifest = JSON.parse(read("web/sound/manifest.webmanifest")); } catch (e) { /* reported below */ }
check(!!manifest, "the manifest parses");
if (manifest) {
  for (const icon of manifest.icons || []) {
    check(fs.existsSync(path.join(app, "web/sound", icon.src)), `the icon ${icon.src} is there`);
  }
  const sizes = (manifest.icons || []).map((i) => i.sizes);
  check(sizes.includes("192x192") && sizes.includes("512x512"),
    "the manifest has a 192 and a 512 icon");
}

// the service worker: it must cache every file the page needs
const sw = read("web/sound/sw.js");
const listed = [...sw.matchAll(/^\s*"([^"]+)",?$/gm)].map((m) => m[1]);
const onDisk = [];
(function walk(dir) {
  for (const e of fs.readdirSync(path.join(app, dir), { withFileTypes: true })) {
    const rel = path.join(dir, e.name);
    if (e.isDirectory()) walk(rel);
    else if (e.name !== "sw.js") onDisk.push(path.relative("web/sound", rel));
  }
})("web/sound");
for (const f of onDisk) {
  check(listed.includes(f) || listed.includes("./" + f), `the service worker caches ${f}`);
}

console.log(fails ? `FAIL ${fails} checks failed` : "all checks passed");
process.exit(fails ? 1 : 0);
