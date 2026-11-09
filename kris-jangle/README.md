# Kris Jangle

By w33tSnx. Source: https://github.com/doohinkus/kris-jangle

A Christmas song game for the M-VAVE FM-1. The FM-1 plays the start
of a public-domain carol and you replay it on the 27 keys. The song
builds three notes per level until level 11 is the whole song. Three
misses sends you to Grump Mode (a recorded jeer). The title screen
shows a colour battery meter (red low, orange medium, green full).

Four carols: Jingle Bells, Deck the Halls, Ding Dong Merrily on High,
and O Christmas Tree. Pick one with `SONG:` on the HOME screen
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
| `app.c` | The game: carol data, six-voice engine (bells/music box/choir/piano/flute/sax), screens, saving |
| `fm1.toml` | Build and site settings |
| `words.tsv` | Screen-reader words |
| `tests/basic.txt` | Host simulator checks: levels build cumulatively (3 then 6 notes), saving, three misses → Grump Mode |
| `tests/screens.txt` | The manual's screenshots |
| `docs/MANUAL.md` | The manual |
| `site/landing.md` | The site's front page |
