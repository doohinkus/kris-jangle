#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""A flash image holding a pre-staff (CCR4) settings record, to check the
upgrade path in the host simulator: the record's bytes (scores, the play
mode, MIDI, the sound) must survive, and the staff byte comes in as its
default, off.

The settings object's sectors are device/storage.c's: a 32-byte header at the
sector's start (the magic, the object's type, a copy index, a sequence, the
payload's length, the payload's CRC-32 and the header's own CRC-32), the
payload at +256; copy A at the app's APP_STORE_MAP offset, copy B a sector
later. The record is 40 bytes (a pre-staff firmware saved sizeof bytes, and
the struct's alignment padding made it one longer than its fields). Make and
use it:

    python3 kris-jangle/tests/mk_ccr4_image.py OUTFILE
    FM1B_FLASH=OUTFILE kris-jangle/build/host/kris-jangle_host \
        kris-jangle/tests/staff_upgrade.txt OUTDIR
"""
import struct
import sys
import zlib

ST_MAGIC = 0x30583058
SETTINGS = 0xFC000                    # kris-jangle/app.h APP_STORE_MAP
SECTOR = 4096
HDR = struct.Struct("<IHHIII8xI")     # magic, type, slot, seq, len, crc, rsv[2], hcrc

# the CCR4 record (kris-jangle/app.c's settings_v4_t): best_score 550 on
# Jingle Bells, level 1, MEDIUM (1), PIANO (3). A pre-staff firmware saved
# sizeof bytes — 40, the struct's alignment padding the last byte — so the
# record on a real flash has this byte too (any value; the loader ignores it)
payload = struct.pack(
    "<I4I4H2B2B2BB3BB",
    0x43435234,                    # "CCR4"
    550, 0, 0, 0,                  # best_score[4]
    1, 0, 0, 0,                    # best_level[4]
    0, 0,                          # last_w1, last_w2
    1, 1,                          # sound_on, leds_on
    1, 0,                          # rsv[0] = mode (MEDIUM), rsv[1] = MIDI on
    3,                             # snd = PIANO
    25, 50, 50,                    # env_a, env_d, env_r
    0,                             # grain
) + b"\x00"


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: mk_ccr4_image.py OUTFILE")
    image = bytearray(b"\xff" * 0x100000)         # 1 MiB, erased
    header = HDR.pack(ST_MAGIC, 0, 0, 1, len(payload), zlib.crc32(payload), 0)
    assert len(header) == 32
    header = header[:28] + struct.pack("<I", zlib.crc32(header[:28])) + b""
    assert len(header) == 32
    sector = SETTINGS
    image[sector:sector + 32] = header
    image[sector + 256:sector + 256 + len(payload)] = payload
    with open(sys.argv[1], "wb") as f:
        f.write(image)


if __name__ == "__main__":
    main()
