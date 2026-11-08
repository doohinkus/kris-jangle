# CAROL COURIER

Carol Courier is a Christmas song game for the M-VAVE FM-1. It plays the
start of a public-domain carol and you replay it on the 27 keys, on piano
(music box) throughout. The song builds as you level up: level 1 asks for
the first 3 notes, and each level adds 3 more on top of all the notes
before, so level 11 is the whole 32-note song. Accurate notes earn 100
points each, a clean level adds 250, and three misses sends you to Grump
Mode — a recorded jeer from the Grump — before you retry the level. A clean level gives
you a word-pair Carol Code you can pass to a friend so they face the same
challenge. Finish the whole song and you get a celebration screen offering
the next song. On the title screen, Santa's recorded voice is just a
button away: press SEQ to hear his introduction (any other key or button
stops it).

## Contents

1. Install
2. The screen
3. Controls
4. Carol Codes
5. Saving
6. The screen reader
7. Troubleshooting

---

## 1. Install

To try it first, open the emulator. It runs the same code in your browser,
with sound, and you can play it with a mouse, a touch screen or your computer
keyboard.

To install it from the browser, use Chrome or Edge. Connect the FM-1 by USB,
open the installer and press Install. The browser asks once to allow MIDI, so
allow it. Don't unplug the cable while it writes. Back up first: firmwares
share the settings sectors, and installing overwrites them.

To go back to the stock firmware, use the installer's last section, which
writes M-VAVE's own firmware back. If the FM-1 doesn't start, it starts in
SAFE MODE after two failed starts, and the web installer can reach it there.

---

## 2. The screen

![Carol Courier title screen](img/screen-title.png "The title screen")

| Screen | What it shows |
| --- | --- |
| Title | The name, battery level (0–3 bars), and the controls that matter. |
| Carol select | The carol (turn PRESET to change), your best score on it, and your current Carol Code. |
| Code entry | The two code words, one at a time. Turn PRESET to change the word, SELECT for the next word or to go. |
| Listen | The carol's name and which note of the level's length is playing. In EASY and MEDIUM, the key blinks as each note sounds. |
| Your turn | Notes played so far (out of the level's length), score, misses (3 = Grump Mode) and combo. In EASY, the key to press stays lit the whole turn. |
| Grump Mode | The Grump jeers (recorded clip), then you retry it. |
| Result | Score, level, and your Carol Code to pass on. |
| Song complete | The whole song played: score and code, and PLAY offers the next song. |

---

## 3. Controls

| Control | What it does |
| --- | --- |
| Keys | Play the level's notes, in the selected sound (PIANO is the default: a sampled grand piano pitch‑shifted over the 27 keys). Each key also sends its note, with velocity, as MIDI out over USB and Bluetooth (channel 1) while MIDI is on. The Listen playback is not sent — only what you play. On Bluetooth, choose the FM-1 in your app's MIDI input list (it appears as the device name). |
| PLAY | Start from the title or select screen, skip Listen, replay a level, go to the next level from the result screen, or start the next song from the song-complete screen. |
| PRESET | Choose the carol on the select screen; choose a code word on the code screen. |
| SELECT | On the code screen: next word, then start the challenge the code describes. |
| ARP | On the select screen: open code entry. |
| GLO | Cycle the play mode: EASY, MEDIUM, HARD. The mode is shown at the top of every screen. |
| FX | Turn MIDI out on or off, on both USB and Bluetooth. The screen reader says MIDI ON or MIDI OFF. The choice is kept with SAVE. |
| EDIT | Open SELECT SOUND: turn PRESET to change the sound (BELLS, MUSICBOX, CHOIR, KAZOO, PIANO, FLUTE, SAX), play keys to audition, PLAY to finish. Kept with SAVE. |
| ENV | Open ENVELOPE: turn PRESET to choose ATTACK/DECAY/RELEASE, turn SELECT to change it (0..100), PLAY to finish. Kept with SAVE. |
| LFO | Open GRAIN: turn PRESET to change the grain shimmer (0..100, 0 is clean), PLAY to finish. Kept with SAVE. |
| MASTER | Volume. This is the only volume control — sound is always on. |
| SEQ | On the title screen only: play Santa's intro voice. |
| OCT−, OCT+ | Unassigned in this game; they do nothing. |
| SAVE | Save your best scores, codes and settings. SAVE lights while there's something to save. |
| HOME | Back to the title screen. |

Play modes (GLO cycles them; the mode is saved with SAVE — and the title
screen names each control):

| Mode | Listen | Your turn |
| --- | --- | --- |
| EASY | The sounding note's key blinks. | The key to press next stays dimly lit, so you can see what to play while you play it. |
| MEDIUM | The sounding note's key blinks. | No key is shown; you play from memory of the Listen phase. |
| HARD | No key is shown; you play from the sound alone. | No key is shown; sound alone. |

In every mode the key you physically press still lights, and the unlit
buttons still glow. Grump Mode's replay follows the same rules as Listen.

---

## 4. Carol Codes

A Carol Code is two words, like `SLEIGH-TREETOP`. The first word picks the
carol and the starting level (there are 11 levels per carol), and the second
repeats the level as a check. Codes from older versions of the game will
not line up with the new levels. To use a friend's code: on the select
screen press ARP, turn PRESET to each word, press SELECT to confirm the
first word, then SELECT again to start.

---

## 5. Saving

Press SAVE to keep your best score and level for each carol, your last code,
the play mode, the MIDI on/off choice, the selected sound, and the envelope
and grain values. They come back the next time the FM-1 starts.
Note: on FM-1s with the newer flash chip, some firmwares can't persist
saves — if yours vanish after a power cycle, that's a known platform issue,
not a broken FM-1.

---

## 6. The screen reader

Carol Courier tells a screen reader what's on its screen through the
[FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/). When the
reader starts listening, it hears the name, and then each change as it
happens — including code words, so codes can be entered by ear.

---

## 7. Troubleshooting

If you hear no sound, turn MASTER up. Sound is always on — there is no mute.

If the installer doesn't find the FM-1, use Chrome or Edge, allow MIDI when
the browser asks, and close any other programs using the FM-1's MIDI port.

If you keep landing in Grump Mode, slow down: the window is generous, and
in EASY the key to press stays lit (in MEDIUM it blinks during Listen, and
HARD shows no guidance). Grump Mode is an
interlude, not a game over — you always retry the same level.
