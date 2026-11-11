#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""Generate Kris Jangle's pixel art: the title banner, the fir on the ridge,
the music note on the staff panel and the sound page's icons.

The drawings are code, so they are reproducible and reviewable; the PNGs are
plain files, so they can be opened and edited in Aseprite afterwards (edit
the PNG, delete the line here that writes it, and the converter still uses
your version).

    python3 kris-jangle/art/gen_art.py        (from the fm1-base root)
    python3 kris-jangle/art/png2c.py          (then: the sprites into sprite_data.h)

Everything is drawn in art/palette.py's colours, the same ones app.c paints
the sky in.
"""
import pathlib
import sys

from PIL import Image, ImageChops, ImageDraw

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1] / "tools"))
import palette as P                                          # noqa: E402
import gen_font                                              # noqa: E402

FONTS = HERE.parents[1] / "assets" / "fonts"


def render_text(text, bdf, scale):
    """the text as a Pillow image (mode L): the glyphs' pixels are 255"""
    height, glyphs = gen_font.render_bdf(FONTS / bdf, scale, 32, 95)
    pad = gen_font.PAD * scale
    adv = glyphs[ord("A") - 32][0]
    img = Image.new("L", (pad + adv * len(text), height), 0)
    for i, c in enumerate(text):
        _, bw, pixels = glyphs[ord(c) - 32]
        # the glyph's box is wider than its advance (the font's padding), so the
        # letters are pasted through their own mask: no letter erases the one
        # before it
        cell = Image.frombytes("L", (bw, height), bytes(pixels)).point(lambda v: 255 if v else 0)
        img.paste(cell, (i * adv, 0), cell)
    return img


def grow(mask, n=1):
    """the mask grown by n pixels in every direction (a frosted edge)"""
    out = mask.copy()
    for _ in range(n):
        # an AFFINE transform maps the output (x, y) to the input
        # (a*x + b*y + c, d*x + e*y + f): the shifts below pull in the four
        # neighbours, one pixel each way
        m = ImageChops.lighter(out, out.transform(out.size, Image.AFFINE, (1, 0, 1, 0, 1, 0)))
        m = ImageChops.lighter(m, out.transform(out.size, Image.AFFINE, (1, 0, -1, 0, 1, 0)))
        m = ImageChops.lighter(m, out.transform(out.size, Image.AFFINE, (1, 0, 0, 0, 1, 1)))
        m = ImageChops.lighter(m, out.transform(out.size, Image.AFFINE, (1, 0, 0, 0, 1, -1)))
        out = m
    return out


def title_banner():
    """'KRIS JANGLE' in the chunky Terminus face, frosted: a white letter with
    an icy edge and a soft shadow. At the font's own size, so the banner fits
    the title screen's top strip."""
    text = "KRIS JANGLE"
    mask = render_text(text, "ter-u16b.bdf", 1)
    edge = grow(mask, 1)
    shadow = Image.new("L", mask.size, 0)
    shadow.paste(edge, (1, 1))
    w, h = mask.size
    img = Image.new("RGBA", (w + 2, h + 3), (0, 0, 0, 0))
    px = img.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            if x < w and y < h and mask.load()[x, y]:
                px[x, y] = P.WHITE + (255,)            # the letter
            elif x < w and y < h and edge.load()[x, y]:
                px[x, y] = P.FROST + (255,)            # the frost around it
            elif x < w and y < h and shadow.load()[x, y]:
                px[x, y] = P.PANEL + (130,)            # the shadow behind it
    img.save(HERE / "title.png")
    print(f"title.png: {img.size[0]}x{img.size[1]}")


def fir():
    """a fir on the ridge, with snow on its tiers"""
    w, h = 24, 34
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    tiers = [(0, 10, 4), (7, 12, 5), (14, 14, 6), (21, 16, 7)]   # top, height, half-width
    for top, height, half in tiers:
        d.polygon([(w // 2, top), (w // 2 - half, top + height), (w // 2 + half, top + height)], fill=P.PINE + (255,))
        d.polygon([(w // 2, top), (w // 2 - half, top + height), (w // 2 - half + 3, top + height)],
                  fill=P.PINE_DARK + (255,))
        d.rectangle([w // 2 - half + 1, top + 1, w // 2 + half - 1, top + 2], fill=P.SNOW + (255,))
    d.rectangle([w // 2 - 1, 30, w // 2 + 1, h - 1], fill=P.PINE_DARK + (255,))
    img.save(HERE / "pine.png")
    print(f"pine.png: {w}x{h}")


def note():
    """a chunky eighth note: the head, the stem and the flag"""
    w, h = 14, 22
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([1, h - 8, 9, h - 1], fill=P.WHITE + (255,))
    d.rectangle([8, 2, 9, h - 4], fill=P.WHITE + (255,))
    d.polygon([(9, 2), (13, 6), (13, 9), (9, 5)], fill=P.WHITE + (255,))
    img.save(HERE / "note.png")
    print(f"note.png: {w}x{h}")


def _smooth(points, samples=24):
    """a Catmull-Rom spline through the points: a smooth curve, no corners"""
    pts = [points[0]] + list(points) + [points[-1]]
    out = []
    for i in range(len(pts) - 3):
        p0, p1, p2, p3 = pts[i], pts[i + 1], pts[i + 2], pts[i + 3]
        for s in range(samples):
            t = s / samples
            t2, t3 = t * t, t * t * t
            out.append(tuple(
                0.5 * (2 * p1[k] + (-p0[k] + p2[k]) * t
                       + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                       + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3)
                for k in (0, 1)))
    out.append(points[-1])
    return out


def clef():
    """the treble clef for the staff panel: a G clef, 24x46.

    The glyph is drawn at (11, 159) on screen, so the coil's centre (local
    (7.4, 27)) lands on the G4 line at y 186, the top hook clears the top line
    (local 9) and the descender's curl hangs below the bottom line (local 33).

    It is one stroke, the way a G clef is written: up from the bottom hook,
    the stem, over the top and down the left into the coil, round the bowl,
    up the right (crossing the stem) and in to the centre, where it ends.
    The stroke is two pixels wide, drawn eight times over and cut back down,
    so the curves come out clean instead of staircased. The ink is the game's
    own near-black: the staff floats on the dusk sky, so the clef is written
    on it, not lit on top of it.
    """
    w, h = 24, 46
    s = 8                                                # the supersample
    img = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    ink = P.INK + (255,)

    def big(pts):
        return [(x * s, y * s) for x, y in pts]

    stroke = [
        (8.6, 38.4), (6.8, 39.6), (6.6, 41.8), (8.6, 44.0), (11.6, 44.6),
        (14.2, 43.4), (16.0, 40.6),                       # the bottom hook
        (16.0, 34.0), (16.0, 24.0), (16.0, 12.0), (16.0, 1.2),   # the stem, up
        (14.8, 0.2), (13.4, 0.0), (11.8, 1.2), (10.6, 3.4), (10.2, 6.4),
        (10.4, 10.0), (11.0, 13.0), (11.6, 14.6),         # the hook's left side
        (10.4, 16.6), (9.2, 17.8), (6.2, 18.4), (3.2, 20.2), (1.4, 23.2),
        (1.2, 27.0), (2.6, 30.6), (5.2, 33.2), (8.8, 34.2), (12.6, 33.4),
        (15.4, 31.0), (16.8, 27.4), (16.6, 24.0),         # the bowl, up the right
        (14.4, 21.4), (11.6, 20.8), (9.2, 21.8), (8.4, 24.2), (8.2, 27.2),
        (9.4, 29.4), (8.0, 30.4), (6.2, 29.8),            # the coil, in to the centre
    ]
    d.line(big(_smooth(stroke, samples=12)), fill=ink, width=2 * s, joint="curve")

    img = img.resize((w, h), Image.LANCZOS)
    px = img.load()
    for y in range(h):
        for x in range(w):
            if px[x, y][3] < 128:
                px[x, y] = (0, 0, 0, 0)
            else:
                px[x, y] = P.INK + (255,)
    img.save(HERE / "clef.png")
    print(f"clef.png: {w}x{h}")


def icons():
    """the sound page's icons, 16x16 each"""
    def piano(d):
        d.rectangle([1, 4, 14, 13], fill=P.WHITE + (255,))
        for x in (3, 6, 9, 12):
            d.rectangle([x, 4, x + 1, 7], fill=P.INK + (255,))
        d.rectangle([1, 14, 14, 15], fill=P.INK + (255,))

    def choir(d):
        for x in (2, 7, 12):
            d.ellipse([x, 3, x + 2, 5], fill=P.WHITE + (255,))
            d.polygon([(x, 6), (x + 2, 6), (x + 3, 12), (x - 1, 12)], fill=P.WHITE + (255,))

    def kazoo(d):
        d.polygon([(2, 9), (12, 6), (13, 9), (3, 12)], fill=P.GOLD + (255,))
        d.ellipse([11, 6, 14, 9], fill=P.GOLD + (255,))

    def flute(d):
        d.rectangle([1, 7, 14, 8], fill=P.WHITE + (255,))
        for x in (4, 7, 10):
            d.rectangle([x, 5, x, 6], fill=P.INK + (255,))
        d.rectangle([13, 5, 14, 10], fill=P.WHITE + (255,))

    def sax(d):
        d.rectangle([3, 2, 4, 10], fill=P.GOLD + (255,))
        d.rectangle([3, 10, 8, 11], fill=P.GOLD + (255,))
        d.polygon([(8, 8), (13, 9), (13, 14), (8, 13)], fill=P.GOLD + (255,))

    def bells(d):
        d.polygon([(8, 3), (13, 11), (3, 11)], fill=P.GOLD + (255,))
        d.rectangle([7, 11, 9, 13], fill=P.GOLD + (255,))
        d.ellipse([6, 13, 10, 15], fill=P.GOLD + (255,))

    def staff(d):
        for y in (4, 6, 8, 10, 12):
            d.rectangle([1, y, 14, y], fill=(150, 150, 158, 255))
        d.ellipse([4, 7, 8, 11], fill=P.WHITE + (255,))
        d.rectangle([7, 3, 8, 9], fill=P.WHITE + (255,))

    def midi(d):
        d.ellipse([2, 2, 13, 13], fill=P.INK + (255,))
        d.ellipse([3, 3, 12, 12], fill=(60, 60, 70, 255))
        for x, y in ((5, 6), (7, 4), (9, 4), (10, 7), (7, 9)):
            d.rectangle([x, y, x + 1, y + 1], fill=(180, 180, 190, 255))

    for name, fn in (("piano", piano), ("choir", choir), ("kazoo", kazoo),
                     ("flute", flute), ("sax", sax), ("bells", bells),
                     ("staff", staff), ("midi", midi)):
        img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        fn(ImageDraw.Draw(img))
        img.save(HERE / f"icon_{name}.png")
    print("icons: 8 icons, 16x16")


def main():
    """draw every sprite, or just the ones named on the command line"""
    names = sys.argv[1:] or ["title_banner", "fir", "note", "clef", "icons"]
    for name in names:
        globals()[name]()


if __name__ == "__main__":
    main()
