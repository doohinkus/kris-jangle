# Future features (deferred ideas)

Ideas worth doing later, with the decisions already made so they don't get
re-litigated. Nothing here is implemented; each entry is a starting point,
not a spec.

## User song upload: learn any song from a .MID file

A web page that takes a standard MIDI file the user drops in and turns it
into a learnable song on the FM-1, so the choice isn't limited to the four
built-in carols.

Decisions (made 2026-11-09, no implementation yet):

- **Convert only, no piano-roll editor.** File in, song out; fixes mean
  re-exporting the .MID. (An editor UI would roughly double the effort.)
- **Auto-pick the top voice** (most-populated / highest track). No track
  picker in v1.
- **USB first.** The web backup page already proves SysEx-over-Web-MIDI
  works; a new SysEx command plus store objects is the delivery path.
- **BLE deferred.** BLE MIDI supports SysEx (the codec in `bt/ble/ble_midi.c`
  chunks it), but the platform only routes received SysEx from USB to the
  app, and browsers can't send BLE MIDI via Web MIDI — Web Bluetooth GATT
  is Chromium-desktop-only. BLE needs stack changes plus a native tool or
  GATT writer; revisit only if USB proves insufficient.
- Conversion rules follow the built-in carols: quantize to the eighth-note
  `CAROL_DUR` grid, transpose/clamp into the 27-key range (MIDI 53–79),
  cap at 32 notes (level 11 = whole song).

Open questions for when this is picked up:

- User songs as **extra slots** (grows `NCAROLS` — storage layout change;
  existing saved scores must survive) vs. **replacing a built-in carol**.
- How custom song names are announced by the screen reader (`words.tsv`
  entries or a fallback).

Rough effort: converter S–M, upload + storage M, BLE M-L extra, tests/docs S.

## Sound link: send app data over audio (transmit side)

A small transmitter that plays a chosen blob of app data (a save
record, a revived "Pass It On" challenge, a user song from the .MID
upload idea) as an audio signal from the FM-1's output, so another
device can record it. The receiver is a separate effort; this is the
FM-1's half.

Decisions (made 2026-11-09, no implementation yet):

- **Generic app data.** The library carries a byte blob; the app
  decides what goes in it. The transmitter is receiver-agnostic: it is
  designed so another FM-1 or a phone/computer can record and decode
  it, but no receiver is built here.
- **Proven transport.** Reuse the PO-32 framing and DBPSK modem
  (preamble 124 x 0x55 + sync 14 19 9D CF; packets tag(2 LE) + len(1) +
  payload + trailer(2); stateful whitening + CRC, init 0x1D0F; final
  tail C3 71 ...; ~3.6 kHz differential PSK at ~1202 baud, 3 carrier
  cycles/symbol, phase flip on 0, LSB-first). The format is documented
  by libpo (MIT).
- **Code in the app.** `kris-jangle/soundtx.h` + `soundtx.c`, included
  from `app.c` (the unity build); no platform changes. Pure C99, no
  libm; the same code runs in the host simulator and the browser
  emulator.
- **Original MPL-2.0 code.** Do not copy Drumlab's GPL `po32tx.c`.
  Reimplement from libpo's MIT docs (`core/docs/PROTOCOL.md`,
  `PO_FORMATS.md`); if libpo is used in tests, keep its MIT notice.

Proposed shape (to confirm when picked up):

- v1 sends one packet: a blob of at most 255 bytes (a save record is
  ~40, a challenge code a few, a user song ~64). A kris-jangle tag
  (e.g. 4A 4B "KJ" on the wire) marks the packet; the PO-32 sync word
  is reused so the preamble detector is shared.
- A small API: `soundtx_begin`, `soundtx_send(t, data, len)`,
  `soundtx_play`, `soundtx_render(t, out, n, amp)` (mono samples at
  peak `amp`; kris-jangle's `app_render` is 24-bit, so `amp` is about
  5,000,000 for ~0.6 FS).
- A blob-level header (magic, type, length, CRC32) is recommended for
  versioning and end-to-end integrity, but the blob's meaning is the
  app's.

Open questions for when this is picked up:

- The v1 payload: a save backup, the revived "Pass It On" challenge, or
  both behind a type byte?
- The UI trigger (a title-screen action or a button combo) and whether
  the game audio is muted during a send.
- The tag value, and whether to reuse the PO-32 sync word or use a
  kris-jangle-specific one.
- Whether v1 needs blobs over 255 bytes (chunking) or one packet is
  enough.
- The receiver: a web page (Web Audio API) or a native app.

Rough effort: modem + framing library M, app hookup + UI + words S-M,
host round-trip test S-M, receiver (separate issue) M-L.

## Sound link: phone/web receiver (QR + Web Audio + trophy)

The other half of the sound link: the FM-1 shows a QR code that opens
a static webpage on the player's phone; the page listens with the Web
Audio API, the player presses a button combo on the FM-1, and the page
decodes the tone and shows a Christmas badge. No app install, no
pairing, no server: the page is a PWA on GitHub Pages and works
offline after the first load.

Decisions (made 2026-11-09, no implementation yet):

- **Phone first, computer second.** The phone's microphone is the
  receiver; a laptop browser can also receive, and can select the FM-1's
  USB audio input for a clean capture and send the challenge back over
  Web MIDI (the user-song-upload path).
- **Static page, PWA.** `https://doohinkus.github.io/kris-jangle/sound/`
  (GitHub Pages, HTTPS, no server). A manifest and service worker cache
  the page and its art, so it works offline after the first load.
- **Web Audio capture.** `getUserMedia` with echo cancellation, noise
  suppression and auto gain control off (the phone's processing would
  otherwise chew up the tone); an AudioWorklet streams samples to a JS
  decoder.
- **Decoder.** Delay-and-multiply DPSK (no carrier recovery), timing
  recovered from the alternating preamble, then dewhiten, CRC check,
  payload out. Rate-agnostic: estimate the carrier from the preamble
  (the phone may capture at 44.1 or 48 kHz) and handle fractional
  symbol timing (1202 baud is ~36.7 samples/symbol).
- **Auto-record and signal lock.** The page listens continuously and
  detects the preamble itself; a level meter and a "locked" indicator
  let the player move the phone closer. No "press record at the right
  time".
- **Robustness.** The FM-1 sends the tone 2-3 times; the blob carries a
  CRC32; the page decodes the first clean copy. If decoding fails, the
  page shows the challenge as text (or the old Carol Code word pair) so
  the player can type it into the FM-1.
- **FM-1 side.** A QR screen (the QR is generated offline and embedded
  as a C array; fm1-base has no QR encoder and Drumlab's is GPL). The
  combo (e.g. GLO+SEQ) mutes the game audio and plays the tone, with a
  progress bar. The payload is a small app blob: magic, type, carol,
  level, score, CRC32.
- **Themed trophies.** The page shows a Christmas badge using the
  game's own art (`art/frosty.png`, `santa.png`, ...), the carol, level
  and score, with a little animation and a chime. A badge collection in
  `localStorage` shows a Christmas tree with the ornaments earned so
  far.
- **Share / Pass It On.** Render the badge to a canvas and offer
  `navigator.share` / download; show the challenge as a code and a QR
  code (generated client-side) so a friend can scan it and see what to
  play. Web Speech API announces the badge.
- **Original MPL-2.0 code.** No GPL copy; the QR is generated with an
  MIT/BSD tool and the decoder is original.

Open questions for when this is picked up:

- The payload: a challenge (carol+level), a result (with score), a save
  record, or all behind a type byte?
- The FM-1 trigger: a dedicated QR screen with PLAY, or a button combo
  from any screen?
- The badge design: which art, which animation, and how the collection
  tree is laid out.
- The Pass It On mechanism: text code, QR, Web MIDI send-back, or all.
- The PWA icons and caching strategy (which art to use, how much to
  cache).
- The decoder: a single JS file, or a C decoder compiled to WASM and
  shared with the host test?
- The computer receiver: USB audio input and Web MIDI send-back as a
  first-class feature or a later extra?

Rough effort: FM-1 QR + transmit UI M; webpage + decoder M-L; badge
collection + share M; PWA S; testing with host WAV and emulator loopback
M; computer receiver / BLE MIDI M-L extra.
