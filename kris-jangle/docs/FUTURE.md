# Future features (deferred ideas)

Ideas worth doing later, with the decisions already made so they don't get
re-litigated. Nothing here is implemented; each entry is a starting point,
not a spec.

## User song upload: learn any song from a .MID file

A web page that takes a standard MIDI file the user drops in and turns it
into a learnable song on the FM-1, so the choice isn't limited to the four
built-in carols.

Decisions (made 2026-11-09, revised 2026-11-19, no implementation yet):

- **Convert only, no piano-roll editor.** File in, song out; fixes mean
  re-exporting the .MID. (An editor UI would roughly double the effort.)
- **The page reads the .MID; the FM-1 applies the game's rules.** The SMF
  format — the chunks, the variable-length deltas, the running status — is
  the fiddly mechanical part and is cheapest in JavaScript; the conversion
  rules that must agree with the firmware (the eighth-note grid, the
  27-key range, the 32 slots) stay on the device, where the game's model
  lives. (Revised: the FM-1 was going to parse the .MID itself.)
- **The page picks the track.** It reads the file, lists the tracks with
  their note counts, and the user chooses the melody. The page then sends
  that track's notes (pitch and length in ticks) with the tempo and the
  beat; the device converts and stores.
- **USB first**, over the app's own SysEx frames (`APP_SYSEX`), which the
  web backup page already proves works over Web MIDI. The page finds the
  FM-1 by its port name and asks for its own hello.
- **BLE deferred.** BLE MIDI supports SysEx (the codec in
  `bt/ble/ble_midi.c` chunks it), but the JieLi stack's MIDI service
  *drops* received SysEx (`ble/midi/le_midi.c`: "sysex: skip to the end
  flag"), the platform routes only USB SysEx to the app, and browsers
  can't send BLE MIDI via Web MIDI — Web Bluetooth GATT is
  Chromium-desktop-only. BLE needs stack changes plus a native tool or
  GATT writer; revisit only if USB proves insufficient. A BLE *command*
  ("load the song") would be cheap later: the platform delivers
  controllers over Bluetooth, and the app only has to stop ignoring them.
- Conversion rules follow the built-in carols: quantize to the eighth-note
  `CAROL_DUR` grid, shift into the 27-key range (MIDI 53–79) and drop
  what still doesn't fit (not clamp — a clamped note would be a wrong
  note), cap at 32 notes (level 11 = whole song). A rest becomes the
  previous note's longer duration: the model has none.
- **Four user songs**, in one store object at a **new** offset (`OBJ_SONG`,
  0xDD000 — the free sector between the panel and the Bluetooth records;
  the existing offsets do not move, so saved scores survive). The object
  joins `APP_BACKUP_OBJS`, so a backup carries the user's songs.
- **The song's name is the .MID file's name**, sent by the page; the
  track's own name inside the file is often meaningless ("Voice 1").

Open questions for when this is picked up:

- How custom song names are announced by the screen reader (`words.tsv`
  entries or a fallback).
- Whether the page gets a preview of the melody before it sends (it can:
  it has the notes) and how much of the editor's UI is worth building.

Rough effort: the page's SMF reader and UI M, the device's conversion,
storage and song picker M, tests/docs S.

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

**Partly done (2026-11-10):** the QR half is built. LFO on the FM-1 shows a
QR code for the carol and the level (44 codes, made at build time by
`tools/gen_qr.py` with Nayuki's MIT generator, into `qr_data.h`), and the
page at `/kris-jangle/sound/` reads the carol and the level from the address,
shows the badge, keeps the badges earned on a tree in `localStorage`, and
shares one as an image (offline after the first load). What is left is the
sound half: the page listening with Web Audio and decoding the FM-1's tone,
which makes a badge verified and carries the exact score.

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
  game's own art (`art/frosty1.png`, `frosty2.png`, `grump.png`, ...),
  the carol, level and score, with a little animation and a chime. A
  badge collection in `localStorage` shows a Christmas tree with the
  ornaments earned so far. **Done (2026-11-11):** the badge is the ice
  card, cut out of the page's reference drawing
  (`art/reference-badge.jpeg`) — its sky flooded away, its paper
  flattened, its own text and characters erased, its green `SAVED` seal
  and snow kept — with the carol's name and the level as the page's own
  text (Barlow, in the reference's ink), the word LEVEL drawn as an
  image (`art/gen_page_art.py`), the levels cleared, Frosty (the
  firmware's own two poses) and the Grump, and the reference's
  indigo-to-rose sky with its plus-sign stars; the chime is the game's
  own reward jingle, clipped.
- **Share / Pass It On.** Render the badge to a canvas and offer
  `navigator.share` / download; show the challenge as a code and a QR
  code (generated client-side) so a friend can scan it and see what to
  play. Web Speech API announces the badge. **The share image is done**
  (the card at 1080 px, drawn from the page's own images and the
  page's own places, so the two cannot drift apart); the code and the
  client-side QR are not.
- **Original MPL-2.0 code.** No GPL copy; the QR is generated with an
  MIT/BSD tool and the decoder is original.

Open questions for when this is picked up:

- The payload: a challenge (carol+level), a result (with score), a save
  record, or all behind a type byte?
- The FM-1 trigger: a dedicated QR screen with PLAY, or a button combo
  from any screen?
- The badge design: which art, which animation, and how the collection
  tree is laid out. **Answered (2026-11-11):** the game's own art
  (Frosty's two firmware poses and the Grump) on a snowflake of ice with
  the game's paper inside it; the level pips for the progress; the
  game's face, drawn as images.
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
