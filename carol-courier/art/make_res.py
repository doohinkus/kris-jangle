#!/usr/bin/env python3
"""Build Carol Courier's resource payload from the art/*.wav clips.

Usage: python3 carol-courier/art/make_res.py   (run from the fm1-base root)
Reads the WAV sources below (44.1 kHz stereo, 16-bit) and writes
art/carol_res.bin, the payload tools/build.py places in the resource
region (fm1.toml resources = "art/carol_res.bin"). Add a clip by adding
its source and id here; the app (app.c) looks clips up by id.

Each clip is downmixed to mono, leading/trailing silence (below a small
threshold) is trimmed with a short pad, downsampled to 22050 Hz by
pair-averaging, peak-normalised and stored as unsigned 8-bit samples.
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
OUT_RATE = 22050
TRIM = 400        # |sample| below this (of 32767) counts as silence
PAD = 1102        # ~50 ms of silence kept at each end (at 22050 Hz)

CLIPS = [(1, "santa-intro.wav"), (2, "grump-jeer.wav")]


def convert(src):
    w = wave.open(str(ROOT / "art" / src))
    assert w.getsampwidth() == 2 and w.getframerate() == 44100, f"{src}: want 44.1 kHz 16-bit"
    n = w.getnframes()
    raw = w.readframes(n)
    chans = w.getnchannels()
    w.close()
    pcm = array.array("h")
    pcm.frombytes(raw)
    if chans == 2:
        mono = [(pcm[2 * i] + pcm[2 * i + 1]) / 2.0 for i in range(n)]
    else:
        mono = [float(v) for v in pcm]
    live = [i for i, v in enumerate(mono) if abs(v) > TRIM]
    mono = mono[live[0]:live[-1] + 1]
    down = [(mono[2 * i] + mono[2 * i + 1]) / 2.0 if 2 * i + 1 < len(mono) else mono[2 * i]
            for i in range((len(mono) + 1) // 2)]
    peak = max(abs(v) for v in down) or 1.0
    gain = 120.0 / peak
    data = bytes(max(0, min(255, int(round(128 + v * gain)))) for v in down)
    return bytes([128] * PAD) + data + bytes([128] * PAD)


blobs = []
for clip_id, src in CLIPS:
    data = convert(src)
    blobs.append((clip_id, data))
    print(f"{src} -> clip {clip_id}: {len(data)} samples at {OUT_RATE} Hz "
          f"({len(data) / OUT_RATE:.2f} s)")

header = struct.pack("<4I", 0x444E5343, 1, len(blobs), 0)
entries = b""
body = b""
for clip_id, data in blobs:
    entries += struct.pack("<HHII", clip_id, OUT_RATE, len(header) + 12 * len(blobs) + len(body), len(data))
    body += data
DST.write_bytes(header + entries + body)
print(f"payload {len(header) + len(entries) + len(body)} B -> {DST.name}")
