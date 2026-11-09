#!/usr/bin/env python3
"""Build Carol Courier's resource payload from the art/*.wav clips.

Usage: python3 carol-courier/art/make_res.py   (run from the fm1-base root)
Reads the WAV sources below (44.1 kHz stereo, 16-bit) and writes
art/carol_res.bin, the payload tools/build.py places in the resource
region (fm1.toml resources = "art/carol_res.bin"). Add a clip by adding
its source and id here; the app (app.c) looks clips up by id.

piano-c4.wav is CC0 1.0 from Sam Gossner's VCSL (Versilian Studios,
https://github.com/sgossner/VCSL): Chordophones/Zithers/Grand Piano,
Kawai/Sustains/GPiano_sus_C4_v2_rr1_Player.wav, via
github.com/doohinkus/Felucca assets/samples-cc0/PIANO. Only the first
~0.9 s is used, pitch-shifted from C4 over F3..G5.
flute-c5.wav is CC0 1.0 from Sam Gossner's VSCO-2 Community Edition
(https://github.com/sgossner/VSCO-2-CE): Woodwinds/Flute/susvib/
LDFlute_susvib_C5_v1_1.wav, via the same Felucca directory.
sax-a3.wav is CC0 1.0 from VCSL: Aerophones/Reed Aerophones/Tenor
Saxophone/Non-Vibrato/BrettTenor_NV_Main_A#3_vl2_rr1.wav, same source.
Both are pitch-shifted like the piano; flute and sax are sustained
wind, so their attack and decay ride on the device envelope.

Each clip is downmixed to mono, leading/trailing silence (below a small
threshold) is trimmed with a short pad, downsampled to its rate below
(22050 Hz by averaging; voice and effects at 11025 Hz, which are still
intelligible and keep the whole payload inside the resource region:
app.h's APP_RES_SECTORS * 4096 - 32), peak-normalised and stored as
unsigned 8-bit samples.
Payload layout (little-endian):

    u32 magic "CSND" (0x444E5343)
    u32 version (1)
    u32 count (clips)
    u32 reserved (0)
    per clip: u16 id, u16 rate (Hz), u32 offset, u32 samples   (12 bytes)
    clip data, back to back from the first offset

The app (app.c) validates this and plays clips by id; if the resource
region is missing or does not match, it plays without them.
"""
import pathlib
import struct
import wave
import array

ROOT = pathlib.Path(__file__).resolve().parent.parent  # carol-courier/
DST = ROOT / "art" / "carol_res.bin"
TRIM = 400        # |sample| below this (of 32767) counts as silence

CLIPS = [(1, "santa-intro.wav", 11025), (2, "grump-jeer.wav", 11025),
         (3, "reward-jingle.wav", 22050), (4, "grump-lose.wav", 11025),
         (5, "piano-c4.wav", 22050), (6, "flute-c5.wav", 11025),
         (7, "sax-a3.wav", 11025)]  # id, source, output rate


def convert(src, out_rate):
    pad = 1102 * out_rate // 22050       # ~50 ms of silence kept at each end
    w = wave.open(str(ROOT / "art" / src))
    in_rate = w.getframerate()
    sampwidth = w.getsampwidth()
    chans = w.getnchannels()
    n = w.getnframes()
    # Instrument clips are short excerpts: piano 0.9 s, flute 0.6 s, sax 0.5 s.
    limit = {"piano-c4.wav": 22000, "flute-c5.wav": 15400, "sax-a3.wav": 14000}.get(src)
    if limit:
        n = min(n, limit)
    raw = w.readframes(n)
    w.close()
    # de-interleave any channel count and depth into mono 16-bit-range floats,
    # resampling 48 kHz sources down to 44.1 kHz by averaging.
    if sampwidth == 2:
        p = array.array("h")
        p.frombytes(raw)
        vals = [float(v) for v in p]
    elif sampwidth == 3:
        vals = []
        for i in range(0, len(raw) - 2, 3):
            v = raw[i] | (raw[i + 1] << 8) | (raw[i + 2] << 16)
            if v & 0x800000:
                v -= 0x1000000  # sign-extend 24-bit
            vals.append(float(v) / (1 << 24) * 32768.0)
    else:
        raise AssertionError(f"{src}: want 16/24-bit")
    if chans == 2:
        mono = [(vals[2 * i] + vals[2 * i + 1]) / 2.0 for i in range(len(vals) // 2)]
    else:
        mono = vals
    if in_rate != 44100:
        # 48 kHz -> 44.1 kHz: linear interpolation per frame, mono before.
        out = []
        for i in range(int(len(mono) * 44100 / in_rate)):
            f = i * in_rate / 44100.0
            j = int(f)
            frac = f - j
            if j + 1 < len(mono):
                out.append(mono[j] * (1.0 - frac) + mono[j + 1] * frac)
            else:
                out.append(mono[j])
        mono = out
    trim = 60 if src in ("piano-c4.wav", "flute-c5.wav", "sax-a3.wav") else TRIM
    live = [i for i, v in enumerate(mono) if abs(v) > trim]
    if live:
        mono = mono[live[0]:live[-1] + 1]
    factor = 44100 // out_rate
    down = [sum(mono[i:i + factor]) / len(mono[i:i + factor])
            for i in range(0, len(mono), factor)]
    peak = max(abs(v) for v in down) or 1.0
    gain = 120.0 / peak
    data = bytes(max(0, min(255, int(round(128 + v * gain)))) for v in down)
    return bytes([128] * pad) + data + bytes([128] * pad)


blobs = []
for clip_id, src, rate in CLIPS:
    data = convert(src, rate)
    blobs.append((clip_id, rate, data))
    print(f"{src} -> clip {clip_id}: {len(data)} samples at {rate} Hz "
          f"({len(data) / rate:.2f} s)")

header = struct.pack("<4I", 0x444E5343, 1, len(blobs), 0)
entries = b""
body = b""
for clip_id, rate, data in blobs:
    entries += struct.pack("<HHII", clip_id, rate, len(header) + 12 * len(blobs) + len(body), len(data))
    body += data
DST.write_bytes(header + entries + body)
print(f"payload {len(header) + len(entries) + len(body)} B -> {DST.name}")
