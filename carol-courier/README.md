# Carol Courier

A Christmas phrase game for the M-VAVE FM-1, built on fm1-base from the
template. It plays a 4-note phrase of a public-domain carol, you replay it
on the keys, three misses sends you to Grump Mode (kazoo), and a clean
phrase earns a word-pair Carol Code you can pass to a friend.

| File | What |
| --- | --- |
| `app.h` | Name, store objects and offsets, and the app's half of the platform contract |
| `app.c` | The game: carol data, four-voice engine (music box, bells, choir-ish, kazoo), screens, codes, saving |
| `fm1.toml` | Build and site settings (no Bluetooth in the MVP: smallest build) |
| `words.tsv` | Screen-reader words |
| `tests/basic.txt` | Host simulator checks: clean phrase scores 650, code words recorded, saving, three misses → Grump Mode |
| `tests/screens.txt` | The manual's screenshots |
| `docs/MANUAL.md` | The manual |
| `site/landing.md` | The site's front page |

## Build, test

From the fm1-base directory:

    tools/build.py carol-courier --host
    carol-courier/build/host/carol-courier_host carol-courier/tests/basic.txt carol-courier/build/host
    tools/build.py carol-courier --web
    tools/build.py carol-courier

For the toolchain, the SDK and the container the build runs in, see
fm1-base's README.

## Not yet done

- Daily-seed challenge: the platform has no calendar/date API, so a
  same-day-for-everyone seed can't be derived without a new platform
  function. Codes already give the sharing mechanic.
- Full choir patch and MIDI/BLE out are stretch goals.
- Release packaging (`firmware.json`, screenshot, Hub submission) follows
  the template README's steps; the screenshot path in `fm1.toml` is
  `docs/img/screen-title.png`, produced by `tests/screens.txt`.
