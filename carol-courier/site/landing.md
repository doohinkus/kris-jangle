# CAROL COURIER

Carol Courier is a Christmas game for the M-VAVE FM-1. It plays the start of
a carol — Jingle Bells, Deck the Halls, or We Wish You a Merry Christmas —
and you replay it on the keys, on piano throughout. The song builds as you
level up, three notes at a time, until level 11 is the whole song. Three
misses sends you to Grump Mode, where the Grump jeers at you — finish a
whole song and it offers you the next one. It speaks to a screen reader,
keeps your best scores, and runs in the browser emulator before you flash it.

![Carol Courier title](manual/img/screen-title.png "Carol Courier on the FM-1's screen")

This is version {{VERSION}}. You install it at your own risk, and the installer can put M-VAVE's own firmware back.

- [Try it in the browser](emu/). Play it right here, with sound. It runs the same code as the FM-1, so you don't need one.
- [Install](install/) it from Chrome or Edge, with the FM-1 connected by USB.
- The [Manual](manual/) explains the screen, the controls and saving.
- [Download {{PKG}}](firmware/{{PKG}}) to use the command-line installer: `python3 tools/fm1_install.py {{PKG}}`.
- The [Source](https://github.com/doohinkus/kris-jangle) is under MPL-2.0. It's built on fm1-base and felucca-bsp, which come from Felucca by Leo Kuroshita, Hügelton Instruments.

M-VAVE and FM-1 are trademarks of their owners. Carol Courier is not affiliated with M-VAVE.
