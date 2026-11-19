// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// Kris Jangle's badge page: the FM-1's QR code opens it with the carol and the
// level in the address (?c=0&l=5). It shows the badge on the ice card and can
// share one as an image.
//
// The page is the reference's own world: its indigo-to-rose sky (the CSS in
// index.html), its snowflake stars, its ice card with the green seal. The
// headings are drawn as images (art/gen_page_art.py), the way the device draws
// them; the carol's name and the level are the page's own text, in Barlow.
"use strict";

const CAROLS = ["JINGLE BELLS", "DECK THE HALLS", "MERRILY ON HIGH", "O CHRISTMAS TREE"];
const LEVELS = 11;
// The car's own name, in the game's own spelling.
const CAROL_TITLES = ["Jingle Bells", "Deck the Halls", "Merrily on High", "O Christmas Tree"];
// The characters on the badge: Frosty in his two poses (the ones the firmware
// animates), and the Grump he was saved from.
const ART = { 0: "art/frosty1.png", 1: "art/santa.png", 2: "art/frosty1.png", 3: "art/frosty1.png" };
const WHO = { 0: "Frosty", 1: "Santa", 2: "Frosty", 3: "Frosty" };
const KEY = "kj-badges";
const SOUND_KEY = "kj-sound";
const SPEAK_KEY = "kj-speak";

const $ = (id) => document.getElementById(id);
const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");

function clampInt(v, lo, hi, dflt) {
  const n = parseInt(v, 10);
  return Number.isFinite(n) && n >= lo && n <= hi ? n : dflt;
}

function badges() {
  try {
    const raw = JSON.parse(localStorage.getItem(KEY) || "[]");
    return Array.isArray(raw) ? raw.filter((k) => typeof k === "string") : [];
  } catch (e) {
    return [];
  }
}

function saveBadges(list) {
  try { localStorage.setItem(KEY, JSON.stringify(list)); } catch (e) { /* private mode */ }
}

function hasBadge(list, c, l) { return list.indexOf(c + "-" + l) >= 0; }

function addBadge(list, c, l) {
  const key = c + "-" + l;
  if (list.indexOf(key) < 0) { list.push(key); saveBadges(list); return true; }
  return false;
}

// The carol's name, for the sentence: "Jingle Bells", not "JINGLE BELLS" and
// not "Jingle bells" — the small words stay small. The names are the game's
// own, so "O CHRISTMAS TREE" keeps its O.
const SMALL_WORDS = ["a", "an", "and", "at", "for", "in", "of", "on", "the", "to"];
function titleCase(s) {
  const words = s.toLowerCase().split(" ");
  return words.map((w, i) => (i > 0 && SMALL_WORDS.indexOf(w) >= 0 ? w : w.charAt(0).toUpperCase() + w.slice(1)))
    .join(" ");
}
// The car's own name, in the game's own spelling.
function carolName(c) { return CAROL_TITLES[c] || titleCase(CAROLS[c]); }

// ---------------------------------------------------------------- the sky ---
// The stars: the reference's plus signs, with a four-pointed sparkle among
// them. They twinkle slowly, the way the stars do on the device.
function makeStars() {
  const sky = $("stars");
  if (reduceMotion.matches) return;
  const spots = [[8, 6, 14], [30, 14, 10], [52, 4, 18], [74, 18, 12], [18, 30, 12],
                 [88, 34, 14], [40, 26, 10], [64, 40, 12], [6, 46, 10], [94, 10, 10],
                 [22, 18, 10], [46, 46, 10]];
  spots.forEach(([x, y, s], i) => {
    const el = document.createElement("i");
    el.className = i % 3 === 0 ? "star4" : "plus";
    el.style.left = x + "%";
    el.style.top = y + "%";
    el.style.width = s + "px";
    el.style.height = s + "px";
    el.style.animationDelay = ((i * 37) % 500) / 100 + "s";
    el.style.animationDuration = (3 + (i % 5)) + "s";
    sky.appendChild(el);
  });
}

// ---------------------------------------------------------------- the badge ---
function showBadge(c, l, fresh) {
  $("badge-card").classList.remove("hidden");
  $("side-section").classList.remove("hidden");
  $("picker-card").classList.add("hidden");
  $("badge-name").textContent = CAROLS[c];
  $("badge-num").textContent = String(l);
  $("badge-who").src = ART[c];
  $("badge-who").alt = WHO[c];
  $("congrats").innerHTML =
    "Congratulations, you saved <b>" + carolName(c) + "</b> from the Grump. " +
    "Save more carols to spread holiday cheer.";

  // The eleven pips: the levels this carol has been taken to.
  const pips = $("pips");
  pips.innerHTML = "";
  for (let i = 1; i <= LEVELS; i++) {
    const p = document.createElement("i");
    if (i <= l) p.className = "on";
    pips.appendChild(p);
  }

  if (fresh) playJingle();
  announce(CAROLS[c] + ", level " + l + ". Saved from the Grump.");
}

// The screen reader, behind the "Hear it" button.
function announce(text) {
  if (!("speechSynthesis" in window)) return;
  if (localStorage.getItem(SPEAK_KEY) === "off") return;
  const u = new SpeechSynthesisUtterance(text);
  u.rate = 0.95;
  speechSynthesis.speak(u);
}

// ----------------------------------------------------------------- sound ---
// The game's own reward jingle, clipped: art/reward-jingle.wav cut to 2.72 s
// and encoded to web/sound/reward.mp3. A QR scan is not a user gesture, so the
// browser will not play it until the page is touched: the sound is armed and
// fires on the first tap.
let audio = null;
let armed = false;

function playJingle() {
  if (localStorage.getItem(SOUND_KEY) === "off") return;
  if (!audio) {
    audio = new Audio("reward.mp3");
    audio.preload = "auto";
  }
  audio.currentTime = 0;
  audio.play().catch(() => { /* the browser will not have it: the tap will arm it */ });
}

function armSound() {
  if (armed) return;
  armed = true;
  const once = () => {
    playJingle();
    removeEventListener("pointerdown", once);
    removeEventListener("keydown", once);
  };
  addEventListener("pointerdown", once, { passive: true });
  addEventListener("keydown", once);
}

// ----------------------------------------------------------------- share ---
function loadImage(src) {
  return new Promise((res, rej) => {
    const im = new Image();
    im.onload = () => res(im);
    im.onerror = rej;
    im.src = src;
  });
}

// The badge as an image, to share: the same card the page shows, big enough to
// be sharp in a chat or on a wall.
//
// The card and its contents are the page's own images, and the places they go
// are the page's own numbers (the plaque's custom properties, read back here),
// so the shared image and the page cannot drift apart.
//
// The card is drawn at exactly twice the page's size: the page shows it at
// 340x364 (its art is two image pixels for every CSS pixel, and the card's cut
// is 680x728), so twice that is the art's own pixels and nothing is resampled.
// The carol's name and the level are text here, drawn by the canvas in the
// site's face — the same text the page shows, at twice its size.
const INK = "#0e2255";                     // the ink on the card's paper

async function badgeImage(c, l) {
  const W = 1080, H = 1080;                        // a square, for the feeds
  const cv = document.createElement("canvas");
  cv.width = W; cv.height = H;
  const g = cv.getContext("2d");
  g.imageSmoothingEnabled = false;                  // the pixels stay square

  // the reference's own sky, the stops the CSS uses
  const sky = g.createLinearGradient(0, 0, 0, H);
  [["#071f4f", 0], ["#0c255d", 0.1], ["#112965", 0.2], ["#1a3173", 0.3],
   ["#2b3a88", 0.4], ["#374193", 0.5], ["#584c98", 0.6], ["#77559a", 0.7],
   ["#865999", 0.8], ["#9f6497", 0.9], ["#bd6f8e", 1]].forEach(([c0, at]) => sky.addColorStop(at, c0));
  g.fillStyle = sky;
  g.fillRect(0, 0, W, H);

  // the card, twice the page's size, so its pixels stay whole
  const cs = getComputedStyle(document.querySelector(".plaque"));
  const PW = parseFloat(cs.width) || 340;
  const PH = parseFloat(cs.height) || 364;
  const k = 2;                                     // the card's scale here
  const TW = PW * k, TH = PH * k;
  const TX = (W - TW) / 2, TY = (H - TH - 150) / 2;
  const num = (name, dflt) => {
    const v = parseFloat(cs.getPropertyValue(name));
    return Number.isFinite(v) ? v : dflt;
  };
  // a part at the page's own place, drawn at its own size, centred on x
  const put = async (src, cx, top) => {
    try {
      const im = await loadImage(src);
      g.drawImage(im, TX + cx * k - im.width * k / 2, TY + top * k, im.width * k, im.height * k);
    } catch (e) { /* the art is missing: the rest is drawn anyway */ }
  };
  try {
    const card = await loadImage("art/card.png");
    g.drawImage(card, TX, TY, TW, TH);
  } catch (e) { /* no card: the sky is left bare */ }

  // The site's face, for the name and the level: the canvas needs it loaded
  // before it can measure or draw it.
  try { await document.fonts.load("700 52px 'Barlow Semi Condensed'"); } catch (e) { /* fall back */ }
  g.textAlign = "center";
  g.textBaseline = "middle";
  g.fillStyle = INK;

  // the carol's name, with a snowflake each side — the row the page flexes
  const nameSize = (parseFloat(cs.getPropertyValue("--name-size")) || 19) * k;
  const nameY = TY + (num("--name-top", 67) + 12) * k;
  g.font = "700 " + nameSize + "px 'Barlow Semi Condensed', Arial, sans-serif";
  const name = CAROLS[c];
  const nameW = g.measureText(name).width;
  g.fillText(name, TX + PW / 2 * k, nameY);
  const gap = (parseFloat(cs.getPropertyValue("--row-gap")) || 8) * k;
  const flakeW = 22 * k;
  try {
    const fl = await loadImage("art/orn_flake.png");
    const side = nameW / 2 + gap + flakeW / 2;
    [TX + PW / 2 * k - side, TX + PW / 2 * k + side].forEach((fx) => {
      g.drawImage(fl, fx - flakeW / 2, nameY - flakeW / 2, flakeW, flakeW);
    });
  } catch (e) { /* no flake */ }

  // the level, with a sparkle each side
  const numSize = (parseFloat(cs.getPropertyValue("--num-size")) || 52) * k;
  const numY = TY + (num("--num-top", 100) + num("--num-size", 52) / 2) * k;
  g.font = "700 " + numSize + "px 'Barlow Semi Condensed', Arial, sans-serif";
  const lv = String(l);
  const lvW = g.measureText(lv).width;
  g.fillText(lv, TX + PW / 2 * k, numY);
  try {
    const sp = await loadImage("art/orn_spark.png");
    const side = lvW / 2 + gap + 14 * k / 2;
    [TX + PW / 2 * k - side, TX + PW / 2 * k + side].forEach((sx) => {
      g.drawImage(sp, sx - 14 * k / 2, numY - 14 * k / 2, 14 * k, 14 * k);
    });
  } catch (e) { /* no sparkle */ }

  // the word LEVEL, in its own place
  await put("art/head_level.png", PW / 2, num("--lvlword-top", 157));

  // Frosty (or Santa), at the page's own place
  try {
    const who = await loadImage(ART[c]);
    g.drawImage(who, TX + num("--who-left", 48) * k, TY + num("--who-top", 165) * k,
                who.width * k, who.height * k);
  } catch (e) { /* no sprite */ }

  // the Grump, mirrored, at his own place
  try {
    const grump = await loadImage("art/grump.png");
    g.save();
    g.translate(TX + (num("--grump-left", 219) + grump.width) * k, TY + num("--grump-top", 78) * k);
    g.scale(-1, 1);
    g.drawImage(grump, 0, 0, grump.width * k, grump.height * k);
    g.restore();
  } catch (e) { /* no grump */ }

  // the line, in the site's own face, white with the carol's name in gold —
  // the reference's congrats, with its shadow behind it
  g.textBaseline = "alphabetic";
  const lineFont = "600 34px 'Barlow Semi Condensed', Arial, sans-serif";
  g.font = lineFont;
  const said1 = "Congratulations, you saved ";
  const said2 = " from the Grump.";
  const full1 = said1 + carolName(c) + said2;
  g.shadowColor = "rgba(10, 14, 38, .9)";
  g.shadowOffsetY = 2;
  g.shadowBlur = 3;
  g.fillStyle = "#f4f0f6";
  g.fillText(full1, W / 2, TY + TH + 66);
  g.fillText("Save more carols to spread holiday cheer.", W / 2, TY + TH + 110);
  g.shadowColor = "transparent";
  // the carol's name again, in gold, over its own place in the line
  const preW = g.measureText(said1).width;
  const midW = g.measureText(carolName(c)).width;
  const x0 = W / 2 - g.measureText(full1).width / 2;
  g.fillStyle = "#ffc83c";
  g.fillText(carolName(c), x0 + preW + midW / 2, TY + TH + 66);
  return cv;
}

async function shareBadge(c, l) {
  const cv = await badgeImage(c, l);
  const name = "kris-jangle-" + CAROLS[c].toLowerCase().replace(/[^a-z]+/g, "-") + "-level-" + l + ".png";
  cv.toBlob(async (blob) => {
    if (!blob) return;
    const file = new File([blob], name, { type: "image/png" });
    if (navigator.canShare && navigator.canShare({ files: [file] })) {
      try {
        await navigator.share({ files: [file], title: "Kris Jangle", text: CAROLS[c] + ", level " + l });
        return;
      } catch (e) { /* the share was cancelled: fall through to the download */ }
    }
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = name;
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 5000);
  }, "image/png");
}

// ---------------------------------------------------------------- picker ---
function fillPickers() {
  const carol = $("pick-carol");
  const level = $("pick-level");
  CAROLS.forEach((name, i) => {
    const o = document.createElement("option");
    o.value = String(i);
    o.textContent = titleCase(name);
    carol.appendChild(o);
  });
  for (let i = 1; i <= LEVELS; i++) {
    const o = document.createElement("option");
    o.value = String(i);
    o.textContent = "Level " + i;
    level.appendChild(o);
  }
  carol.addEventListener("change", () => { location.search = "?c=" + carol.value + "&l=" + level.value; });
  level.addEventListener("change", () => { location.search = "?c=" + carol.value + "&l=" + level.value; });
}

// ---------------------------------------------------------------- layout ---
// The card is 340 wide; a narrower phone gets the whole badge scaled down as
// one, so its pixels stay square. The scale is a fraction of the art's own
// size, and the whole badge moves with it.
//
// The landscape layout has its own scale (half size, beside the notes), so
// this only runs where the card is on its own.
function fitPlaque() {
  const wrap = document.querySelector(".plaquewrap");
  const plaque = document.querySelector(".plaque");
  if (!wrap || !plaque) return;
  // the room the card has: the page's own width, less its padding. (The card
  // may still be hidden when this first runs, so the page is measured, not
  // the card.)
  const page = document.querySelector(".page");
  if (!page) return;
  const cs = getComputedStyle(page);
  const room = page.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight);
  const need = 340;
  if (!(room > 0)) return;                   // not laid out yet: try again later
  const ps = room >= need ? 1 : room / need;
  if (ps === 1) {
    plaque.classList.remove("scaled");
    plaque.style.removeProperty("--ps");
    wrap.style.width = "";
    wrap.style.height = "";
  } else {
    plaque.classList.add("scaled");
    plaque.style.setProperty("--ps", ps);
    wrap.style.width = room + "px";
    wrap.style.height = Math.round(364 * ps) + "px";
  }
}

// ------------------------------------------------------------------ main ---
function main() {
  fillPickers();
  makeStars();
  armSound();
  addEventListener("resize", fitPlaque);

  const q = new URLSearchParams(location.search);
  const c = clampInt(q.get("c"), 0, CAROLS.length - 1, -1);
  const l = clampInt(q.get("l"), 1, LEVELS, -1);
  const list = badges();
  if (c >= 0 && l >= 0) {
    addBadge(list, c, l);
    showBadge(c, l, true);
  } else {
    $("picker-card").classList.remove("hidden");
  }
  fitPlaque();

  // The sound and the voice toggles, kept across visits.
  const soundBtn = $("sound");
  const soundOff = localStorage.getItem(SOUND_KEY) === "off";
  soundBtn.setAttribute("aria-pressed", String(!soundOff));
  soundBtn.textContent = soundOff ? "Muted" : "Sound";
  soundBtn.addEventListener("click", () => {
    const off = localStorage.getItem(SOUND_KEY) === "off";
    if (off) { localStorage.setItem(SOUND_KEY, "on"); soundBtn.textContent = "Sound"; soundBtn.setAttribute("aria-pressed", "true"); }
    else { localStorage.setItem(SOUND_KEY, "off"); soundBtn.textContent = "Muted"; soundBtn.setAttribute("aria-pressed", "false"); }
  });
  const speakBtn = $("speak");
  const speakOff = localStorage.getItem(SPEAK_KEY) === "off";
  speakBtn.setAttribute("aria-pressed", String(!speakOff));
  speakBtn.textContent = speakOff ? "Silent" : "Hear it";
  speakBtn.addEventListener("click", () => {
    const off = localStorage.getItem(SPEAK_KEY) === "off";
    if (off) { localStorage.setItem(SPEAK_KEY, "on"); speakBtn.textContent = "Hear it"; speakBtn.setAttribute("aria-pressed", "true"); }
    else { localStorage.setItem(SPEAK_KEY, "off"); speakBtn.textContent = "Silent"; speakBtn.setAttribute("aria-pressed", "false"); }
  });

  $("share").addEventListener("click", () => {
    const q2 = new URLSearchParams(location.search);
    shareBadge(clampInt(q2.get("c"), 0, CAROLS.length - 1, 0), clampInt(q2.get("l"), 1, LEVELS, 1));
  });
  $("download").addEventListener("click", () => $("share").click());
  $("pick-go").addEventListener("click", () => {
    location.search = "?c=" + $("pick-carol").value + "&l=" + $("pick-level").value;
  });
}

main();

// A hook for the tests: the badge as a canvas, on its own.
window.kjBadge = badgeImage;

if ("serviceWorker" in navigator) {
  window.addEventListener("load", () => navigator.serviceWorker.register("sw.js").catch(() => {}));
}
