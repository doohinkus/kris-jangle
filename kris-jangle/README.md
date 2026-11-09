# Kris Jangle

By w33tSnx. Source: https://github.com/doohinkus/kris-jangle

A Christmas song game for the M-VAVE FM-1, built on fm1-base from the
template. It plays the start of a public-domain carol, you replay it on
the keys on piano, and the song builds three notes per level until level
11 is the whole song. Three misses sends you to Grump Mode (a recorded jeer).
The title screen shows a colour battery meter (red low, orange medium,
green full) instead of a number.

| File | What |
| --- | --- |
| `app.h` | Name, store objects and offsets, and the app's half of the platform contract |
| `app.c` | The game: carol data, six-voice engine (bells/music box/choir/piano/flute/sax), screens, saving |
| `fm1.toml` | Build and site settings (BLE owns MIDI out; `--no-ble` builds USB-only) |
| `words.tsv` | Screen-reader words |
| `tests/basic.txt` | Host simulator checks: levels build cumulatively (3 then 6 notes), saving, three misses → Grump Mode |
| `tests/screens.txt` | The manual's screenshots |
| `docs/MANUAL.md` | The manual |
| `site/landing.md` | The site's front page |

## Build, test

From the fm1-base directory:

    tools/build.py kris-jangle --host
    kris-jangle/build/host/kris-jangle_host kris-jangle/tests/basic.txt kris-jangle/build/host
    tools/build.py kris-jangle --web
    tools/build.py kris-jangle

For the toolchain, the SDK and the container the build runs in, see
fm1-base's README.

## Not yet done

- Daily-seed challenge: the platform has no calendar/date API, so a
  same-day-for-everyone seed can't be derived without a new platform function.
- MIDI/BLE out is shipped in Step 1 (USB) and Step 2 (own BLE stack) and is tested
  in the host simulator; a real phone-DAW check is a device follow-up.
- Release packaging: `fm1.toml`'s repo and site URLs point at kris-jangle
  (`https://doohinkus.github.io/kris-jangle/`); next is a release build
  (`tools/build.py kris-jangle --release X.Y`, which runs the release
  checks and writes `firmware.json`), then Hub submission. The screenshot
  path in `fm1.toml` is `docs/img/screen-title.png`, produced by
  `tests/screens.txt`.
