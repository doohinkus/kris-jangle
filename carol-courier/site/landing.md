# CAROL COURIER

Carol Courier is a Christmas game for the M-VAVE FM-1. It plays a 4-note phrase
of a carol — Jingle Bells, Deck the Halls, or We Wish You a Merry Christmas —
and you replay it on the keys. Three misses in a phrase sends you to Grump
Mode, a kazoo interlude, and a clean phrase earns you a word-pair Carol Code
(like SLEIGH-TREETOP) that you can text to a friend so they can beat your
score. Three tiers cycle as you level up: music box with an LED walkthrough,
sleigh bells with an LED walkthrough, then a choir-ish stack with no LEDs and
a shorter window. It speaks to a screen reader, keeps your best scores, and
runs in the browser emulator before you flash it.

![Carol Courier title](manual/img/screen-title.png "Carol Courier on the FM-1's screen")

This is version {{VERSION}}. You install it at your own risk, and the installer can put M-VAVE's own firmware back.

- [Try it in the browser](emu/). Play it right here, with sound. It runs the same code as the FM-1, so you don't need one.
- [Install](install/) it from Chrome or Edge, with the FM-1 connected by USB.
- The [Manual](manual/) explains the screen, the controls, saving and codes.
- [Download {{PKG}}](firmware/{{PKG}}) to use the command-line installer: `python3 tools/fm1_install.py {{PKG}}`.
- The [Source](https://github.com/charlesvestal/fm1-base) is under MPL-2.0. It's built on fm1-base and felucca-bsp, which come from Felucca by Leo Kuroshita, Hügelton Instruments.

M-VAVE and FM-1 are trademarks of their owners. Carol Courier is not affiliated with M-VAVE.
