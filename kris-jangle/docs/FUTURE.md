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

Rough effort: converter S–M, upload + storage M, BLE M–L extra, tests/docs S.
