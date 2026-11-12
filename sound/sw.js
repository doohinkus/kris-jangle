// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// Kris Jangle's badge page, offline: the page, its script and its art are
// cached on the first visit, so the page works with no network (the address
// still carries the carol and the level).
const CACHE = "kj-sound-v5";
const FILES = [
  "./",
  "index.html",
  "app.js",
  "manifest.webmanifest",
  "reward.mp3",
  "barlow-medium.woff2",
  "barlow-bold.woff2",
  "art/card.png",
  "art/divider.png",
  "art/frosty1.png",
  "art/frosty2.png",
  "art/grump.png",
  "art/head_badge.png",
  "art/head_level.png",
  "art/icon-192.png",
  "art/icon-512.png",
  "art/orn_flake.png",
  "art/orn_spark.png",
  "art/plus.png",
  "art/santa.png",
  "art/sparkle.png",
  "art/title.png",
  "art/title_page.png"
];

self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(FILES)).then(() => self.skipWaiting()));
});

self.addEventListener("activate", (e) => {
  e.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (e) => {
  if (e.request.method !== "GET") return;
  // The page itself: the network first, so a new version shows at once; the
  // cache is the fallback when there is none (offline).
  if (e.request.mode === "navigate") {
    e.respondWith(
      fetch(e.request)
        .then((res) => {
          const copy = res.clone();
          caches.open(CACHE).then((c) => c.put("index.html", copy));
          return res;
        })
        .catch(() => caches.match("index.html"))
    );
    return;
  }
  // The art and the script: the cache first, and a fresh copy in the
  // background when there is a network.
  e.respondWith(
    caches.match(e.request).then((hit) => {
      const net = fetch(e.request).then((res) => {
        if (res.ok && e.request.url.startsWith(self.location.origin)) {
          const copy = res.clone();
          caches.open(CACHE).then((c) => c.put(e.request, copy));
        }
        return res;
      }).catch(() => hit);
      return hit || net;
    })
  );
});
