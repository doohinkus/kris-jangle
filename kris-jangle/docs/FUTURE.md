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
