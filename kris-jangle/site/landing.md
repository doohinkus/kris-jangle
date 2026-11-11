# KRIS JANGLE

Kris Jangle is a Christmas game for the M-VAVE FM-1. It plays the start of
a carol — Jingle Bells, Deck the Halls, Ding Dong Merrily on High, or O Christmas Tree —
and you replay it on the keys, on piano (or the selected sound) throughout. It plays in a
winter dusk — a deep blue sky fading to peach, the first stars, firs of
different sizes on the ridge — in a chunky pixel face. The title screen
is laid out in bordered boxes picking the play mode and the carol, the LISTEN and YOUR TURN
stats sit in boxed grids, and on the sound pages and during YOUR TURN holding a key pops a large note modal with its name and MIDI
number. The song builds as you
level up, three notes at a time, until level 11 is the whole song. Three
misses sends you to Grump Mode, where the Grump jeers at you — finish a
whole song and it offers you the next one. For grown-ups, the sound page
hides a music staff: turn SELECT and a treble staff writes the notes the
demo plays and you play, with their real values — sharps, ledger lines, bar
lines, a miss in red — off by
default. It speaks to a screen reader,
keeps your best scores, and runs in the browser emulator before you flash it.

![Kris Jangle title](manual/img/screen-title.png "Kris Jangle on the FM-1's screen")

This is version {{VERSION}}. You install it at your own risk, and the installer can put M-VAVE's own firmware back.

- [Try it in the browser](emu/). Play it right here, with sound. It runs the same code as the FM-1, so you don't need one.
- [Install](install/) it from Chrome or Edge, with the FM-1 connected by USB.
- The [Manual](manual/) explains the screen, the controls and saving.
- [Download {{PKG}}](firmware/{{PKG}}) to use the command-line installer: `python3 tools/fm1_install.py {{PKG}}`.
- The [Source](https://github.com/doohinkus/kris-jangle) is under MPL-2.0. It's built on fm1-base and felucca-bsp, which come from Felucca by Leo Kuroshita, Hügelton Instruments.

M-VAVE and FM-1 are trademarks of their owners. Kris Jangle is not affiliated with M-VAVE.
