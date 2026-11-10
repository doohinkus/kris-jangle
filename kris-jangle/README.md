# Kris Jangle

By w33tSnx. Source: https://github.com/doohinkus/kris-jangle

A Christmas song game for the M-VAVE FM-1. The FM-1 plays the start
of a public-domain carol and you replay it on the 27 keys. The song
builds three notes per level until level 11 is the whole song. Three
misses sends you to Grump Mode (a recorded jeer). It plays in a winter
dusk — a deep blue sky fading to peach, the first stars, a row of firs on
the ridge — in a chunky pixel face (Terminus). The title screen
is laid out in bordered boxes — PLAY MODE and SONG lists on top,
a battery lightbulb beside the name and a KEEP SCORE box below —
and on the sound pages and during YOUR TURN holding a key shows a large
note modal with its name and MIDI number. For grown-ups, SELECT on the sound
page turns on a music staff that writes the notes the demo plays and you
play, with their real values — sharps, ledger lines, bar lines, misses in
red — off by default.

Four carols: Jingle Bells, Deck the Halls, Ding Dong Merrily on High,
and O Christmas Tree. Pick one in the SONG box on the HOME screen
(turn SELECT); Jingle Bells by default. Replay is on piano or the
selected sound. Three misses → Grump Mode; finish level 11 and PLAY
offers the next song.

- Try it first in the browser emulator:
  https://doohinkus.github.io/kris-jangle/emu/
- Install from Chrome or Edge (back up first — firmwares share the
  settings sectors):
  https://doohinkus.github.io/kris-jangle/install/
- Full manual: `docs/MANUAL.md` / https://doohinkus.github.io/kris-jangle/manual/

| File | What |
| --- | --- |
| `app.h` | Name, store objects and offsets, and the app's half of the platform contract |
| `app.c` | The game: carol data, six-voice engine (bells/choir/kazoo/piano/flute/sax), screens, saving |
| `fm1.toml` | Build and site settings |
| `words.tsv` | Screen-reader words |
| `tests/basic.txt` | Host simulator checks: levels build cumulatively (3 then 6 notes), saving, three misses → Grump Mode |
| `tests/staff.txt` | The staff: toggled on the sound page, kept with SAVE, writing the demo's and the player's notes with their values (misses red, a black key's sharp, the bar scrolling) |
| `tests/staff_upgrade.txt` | A pre-staff (CCR4) settings record keeps its bytes and the staff comes in off (`tests/mk_ccr4_image.py` makes the flash image) |
| `tests/screens.txt` | The manual's screenshots |
| `art/gen_art.py` | The generated pixel art: the frosted title banner, the fir, the music note, the sound page's icons (in `art/palette.py`'s colours) |
| `art/png2c.py` | The converter: the PNGs (and the hand-drawn ones) into `sprite_data.h` |
| `docs/MANUAL.md` | The manual |
| `site/landing.md` | The site's front page |
