# Kris Jangle

By w33tSnx. Source: https://github.com/doohinkus/kris-jangle

A Christmas song game for the M-VAVE FM-1. The FM-1 plays the start
of a public-domain carol and you replay it on the 27 keys. The song
builds three notes per level until level 11 is the whole song. Three
misses sends you to Grump Mode (a recorded jeer). It plays in a winter
dusk — a deep blue sky fading to peach, the first stars, firs of different
sizes on the ridge — in a chunky pixel face (Terminus). The title screen
is laid out in bordered boxes — PLAY MODE and SONG lists on top,
a battery lightbulb beside the name and a KEEP SCORE box below —
and on the sound pages and during YOUR TURN holding a key shows a large
note modal with its name and MIDI number. For grown-ups, SELECT on the sound
page turns on a music staff that writes the notes the demo plays and you
play, with their real values — a treble clef, leaning note heads, sharps,
ledger lines, bar lines — written in black ink straight on the dusk sky, the
firs on the ridge stepping aside so the ink reads, a miss in deep crimson —
on by default. With the staff on, the note modal stays hidden: the staff
writes the notes instead.

Four carols: Jingle Bells, Deck the Halls, Ding Dong Merrily on High,
and O Christmas Tree. Pick one in the SONG box on the HOME screen
(turn SELECT); Deck the Halls by default. Replay is on piano or the
selected sound. Three misses → Grump Mode; finish level 11 and PLAY
offers the next song.

LFO shows the badge: a QR code on the FM-1's screen that opens a page
on the phone with the carol and the level in the address. The page is the
game's own world — the dusk sky in its ten bands, the chunky face (its
headings are drawn as images, the way the device draws them), and the
badge itself on a snowflake of ice with the game's paper inside it. It is
laid out for a phone held either way up. It can share the badge as an
image. It works offline after the first load. Finish a whole song and the
badge comes on its own — the trophy for the song. The codes are made at
build time (there is no QR encoder on the FM-1): one a carol and a level,
44 of them, 6 KB of flash.

- Try it first in the browser emulator:
  https://doohinkus.github.io/kris-jangle/emu/
- Install from Chrome or Edge (back up first — firmwares share the
  settings sectors):
  https://doohinkus.github.io/kris-jangle/install/
- The badge page: https://doohinkus.github.io/kris-jangle/sound/
- Full manual: `docs/MANUAL.md` / https://doohinkus.github.io/kris-jangle/manual/

| File | What |
| --- | --- |
| `app.h` | Name, store objects and offsets, and the app's half of the platform contract |
| `app.c` | The game: carol data, six-voice engine (bells/choir/kazoo/piano/flute/sax), screens, saving |
| `fm1.toml` | Build and site settings |
| `words.tsv` | Screen-reader words |
| `tests/basic.txt` | Host simulator checks: levels build cumulatively (3 then 6 notes), saving, three misses → Grump Mode |
| `tests/staff.txt` | The staff: on by default, toggled on the sound page, kept with SAVE, writing the demo's and the player's notes with their values (misses red, a black key's sharp, the bar scrolling) |
| `tests/staff_upgrade.txt` | A pre-staff (CCR4) settings record keeps its bytes and the staff comes in on (`tests/mk_ccr4_image.py` makes the flash image) |
| `tests/screens.txt` | The manual's screenshots |
| `tests/scan.txt` | The badge screen: LFO shows the QR, the code is the one for the carol and the level, PLAY goes back |
| `tests/song.txt` | The whole song, and the badge at its end: the celebration, then the QR of the song, then the next song (`tests/mk_song_script.py` makes the script from the notes in `app.c`) |
| `tests/badge_test.mjs` | The badge page's checks: the codes a carol and a level, the page's numbers, the art the page draws, the offline cache |
| `tools/gen_qr.py` | Makes the QR codes (`qr_data.h`), one a carol and a level; `--check` says if the header is stale |
| `tools/qrcodegen.py` | Project Nayuki's QR generator (MIT), kept as it is: the codes are made with it |
| `web/sound/` | The badge page: the ice card, the share image, and the offline cache |
| `art/gen_art.py` | The generated pixel art: the frosted title banner, the fir, the music note, the sound page's icons (in `art/palette.py`'s colours) |
| `art/gen_page_art.py` | The badge page's art: the card cut out of the reference drawing, the pixel headings, the paper ornaments, the sky's stars and the icons |
| `art/reference-badge.jpeg` | The badge page's reference drawing: the mock the page was designed from. `art/gen_page_art.py` cuts the ice card out of it (the sky flooded away, the paper flattened, the mock's own text and characters erased, the green `SAVED` seal kept) |
| `art/png2c.py` | The converter: the PNGs (and the hand-drawn ones) into `sprite_data.h` |
| `docs/MANUAL.md` | The manual |
| `site/landing.md` | The site's front page |
