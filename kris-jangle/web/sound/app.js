// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// Kris Jangle's badge page: the FM-1's QR code opens it with the carol and the
// level in the address (?c=0&l=5). It shows the badge, keeps the badges earned
// in this browser on a tree, and can share one as an image.
"use strict";

const CAROLS = ["JINGLE BELLS", "DECK THE HALLS", "MERRILY ON HIGH", "O CHRISTMAS TREE"];
const LEVELS = 11;
const COLORS = ["#ffc83c", "#8ce0a0", "#78c8f0", "#ff8a8a"];   // a colour a carol
const ART = { 0: "art/frosty.png", 1: "art/santa.png", 2: "art/snowman.png", 3: "art/frosty.png" };
const KEY = "kj-badges";

const $ = (id) => document.getElementById(id);

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
  try { localStorage.setItem(KEY, JSON.stringify(list)); } catch (e) { /* private mode: the tree is only for this visit */ }
}

function hasBadge(list, c, l) { return list.indexOf(c + "-" + l) >= 0; }

function addBadge(list, c, l) {
  const key = c + "-" + l;
  if (list.indexOf(key) < 0) { list.push(key); saveBadges(list); }
  return list;
}

// The badge card: the carol, the level and the character.
function showBadge(c, l) {
  $("badge-card").classList.remove("hidden");
  $("picker-card").classList.add("hidden");
  $("badge-carol").textContent = CAROLS[c];
  $("badge-level").textContent = String(l);
  $("badge-art").src = ART[c];
  const say = () => {
    if (!("speechSynthesis" in window)) return;
    const u = new SpeechSynthesisUtterance(CAROLS[c].toLowerCase() + ", level " + l);
    u.rate = 0.95;
    speechSynthesis.speak(u);
  };
  if ("speechSynthesis" in window) setTimeout(say, 250);
}

// The tree: one ornament a badge, in the tree's own order (carol by carol).
function drawTree(list) {
  const tree = $("tree");
  tree.querySelectorAll(".orn").forEach((n) => n.remove());
  const cols = 6;
  const rows = Math.ceil((CAROLS.length * LEVELS) / cols);
  const top = 46;
  const step = 26;
  let n = 0;
  for (let c = 0; c < CAROLS.length; c++) {
    for (let l = 1; l <= LEVELS; l++, n++) {
      const col = n % cols;
      const row = Math.floor(n / cols);
      const y = top + row * step;
      const inRow = Math.min(cols, CAROLS.length * LEVELS - row * cols);
      const half = y <= 140 ? (78 * (y - 30)) / 110 : (115 * (y - 110)) / 140;   /* the tree's width there */
      const x = 120 + (inRow > 1 ? (col - (inRow - 1) / 2) * ((2 * half - 10) / (inRow - 1)) : 0);
      const earned = hasBadge(list, c, l);
      const o = document.createElement("div");
      o.className = "orn";
      o.style.left = (x - 11) + "px";
      o.style.top = (y - 11) + "px";
      o.style.width = o.style.height = "22px";
      o.style.lineHeight = "18px";
      if (earned) {
        o.style.background = COLORS[c];
        o.textContent = String(l);
        o.title = CAROLS[c] + ", level " + l;
        o.addEventListener("click", () => showBadge(c, l));
      } else {
        o.style.background = "rgba(255,255,255,0.10)";
        o.style.borderColor = "rgba(255,255,255,0.20)";
      }
      tree.appendChild(o);
    }
  }
  $("tree-note").textContent = list.length
    ? list.length + (list.length === 1 ? " badge" : " badges") + " on your tree."
    : "No badges yet. Scan one on the FM-1.";
}

// The badge as an image, to share.
function loadImage(src) {
  return new Promise((res, rej) => {
    const im = new Image();
    im.onload = () => res(im);
    im.onerror = rej;
    im.src = src;
  });
}

async function badgeImage(c, l) {
  const S = 480;
  const cv = document.createElement("canvas");
  cv.width = S; cv.height = S;
  const g = cv.getContext("2d");
  const sky = g.createLinearGradient(0, 0, 0, S);
  [["#0e1638", 0], ["#182456", 0.2], ["#263474", 0.4], ["#36448a", 0.6],
   ["#64609e", 0.78], ["#b88a94", 0.9], ["#d4a08c", 1]].forEach(([c0, at]) => sky.addColorStop(at, c0));
  g.fillStyle = sky;
  g.fillRect(0, 0, S, S);
  g.fillStyle = "rgba(10,14,56,0.90)";
  g.fillRect(24, 56, S - 48, S - 112);
  g.strokeStyle = "#4a5aa0";
  g.lineWidth = 4;
  g.strokeRect(24, 56, S - 48, S - 112);
  let art = null;
  try { art = await loadImage(ART[c]); } catch (e) { art = null; }
  if (art) g.drawImage(art, 44, 150, art.width * 1.5, art.height * 1.5);
  g.textAlign = "center";
  g.fillStyle = COLORS[c];
  g.font = "bold 26px ui-monospace, monospace";
  g.fillText(CAROLS[c], 310, 176);
  g.fillStyle = "#eee8ee";
  g.font = "bold 54px ui-monospace, monospace";
  g.fillText("LEVEL " + l, 310, 246);
  g.font = "18px ui-monospace, monospace";
  g.fillStyle = "#b0d6f0";
  g.fillText("KRIS JANGLE", 310, 288);
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

function fillPickers() {
  const carol = $("pick-carol");
  const level = $("pick-level");
  CAROLS.forEach((name, i) => {
    const o = document.createElement("option");
    o.value = String(i);
    o.textContent = name;
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

function main() {
  fillPickers();
  const q = new URLSearchParams(location.search);
  const c = clampInt(q.get("c"), 0, CAROLS.length - 1, -1);
  const l = clampInt(q.get("l"), 1, LEVELS, -1);
  const list = badges();
  if (c >= 0 && l >= 0) {
    addBadge(list, c, l);
    showBadge(c, l);
  } else {
    $("picker-card").classList.remove("hidden");
  }
  drawTree(list);
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
