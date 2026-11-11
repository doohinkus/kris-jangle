#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""Make the host script that plays Jingle Bells from the first note to the last:
tests/song.txt. The song is 32 notes and the levels build up (3, 6, ... 32),
so the script plays every level's notes in order, and the song's end is what
the badge (the QR code) is for.

The notes and their lengths come from app.c (the CAROLS and CAROL_DUR tables),
so the script follows the game when the tune changes.

    python3 tests/mk_song_script.py            (from the app's directory)
    Writes: tests/song.txt
"""
import pathlib
import re

APP = pathlib.Path(__file__).resolve().parents[1]
EIGHTH_MS = 180            # app.c's EIGHTH_MS
PHRASE_GAP_MS = 200        # app.c's PHRASE_GAP_MS
NOTES_PER_LEVEL = 3
LEVELS = 11
KEY0 = 53                  # the first key's MIDI number (app.c: 53 + k)


def table(src, marker):
    """the numbers of a table in app.c: the block between the marker and its "};" """
    m = re.search(re.escape(marker) + r"[^{]*\{(.*?)\n\};", src, re.S)
    if not m:
        raise SystemExit(f"app.c: no table {marker}")
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    return [int(n) for n in re.findall(r"\b(\d+)\b", body)]


def main():
    src = (APP / "app.c").read_text(encoding="utf-8")
    durs = table(src, "static const uint8_t CAROL_DUR")
    pitches = table(src, "static const uint8_t CAROLS[NCAROLS]")
    song = pitches[:32]
    dur = durs[:32]
    if len(song) != 32 or len(dur) != 32:
        raise SystemExit(f"app.c: the song is {len(song)} notes, not 32")

    out = []
    out.append("# Kris Jangle in the host simulator: the whole song, and the badge at its end.")
    out.append("#   kris-jangle/build/host/kris-jangle_host kris-jangle/tests/song.txt kris-jangle/build/host")
    out.append("# Made by tests/mk_song_script.py (the notes come from app.c).")
    out.append("wait 800")
    out.append("expect state 0")
    out.append("expect carol 0")
    out.append("expect snd 1")
    out.append("# PLAY starts the first level")
    out.append("tap PLAY")
    for level in range(1, LEVELS + 1):
        n = min(NOTES_PER_LEVEL * level, len(song))      # level 11 is the whole song (32 notes)
        listen = sum(dur[:n]) * EIGHTH_MS + PHRASE_GAP_MS * ((n + 3) // 4)
        out.append(f"# level {level}: {n} notes")
        out.append(f"wait {listen + 2000}")
        out.append("expect state 2")
        for i in range(n):
            out.append(f"key {song[i] - KEY0} down")
            out.append("wait 40")
            out.append(f"key {song[i] - KEY0} up")
            out.append("wait 40")
        if level < LEVELS:
            out.append("expect state 4")
            out.append(f"expect level {level}")
            out.append("tap PLAY")
        else:
            out.append("# the song is done: the celebration, then the badge (the QR code) on its own")
            out.append("expect state 5")
            out.append("shot songdone.png")
            out.append("wait 4500")
            out.append("expect state 7")
            out.append("expect qr_carol 0")
            out.append("expect qr_level 11")
            out.append("expect qr_variant 10")
            out.append("shot trophy.png")
            out.append("# PLAY on the badge goes to the next song")
            out.append("tap PLAY")
            out.append("wait 500")
            out.append("expect state 1")
            out.append("expect carol 1")
    (APP / "tests" / "song.txt").write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"tests/song.txt: {LEVELS} levels, {sum(NOTES_PER_LEVEL * l for l in range(1, LEVELS + 1))} notes, "
          f"{len(out)} lines")


if __name__ == "__main__":
    main()
