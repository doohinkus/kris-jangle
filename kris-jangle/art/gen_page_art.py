#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""Kris Jangle's badge page's art: the ice card the badge sits on, the pixel
headings, the ornaments, the snowflakes and the icons.

The card is cut out of the reference drawing (art/reference-badge.jpeg, the
mock the page was designed from): the sky is flooded away from the borders,
the paper inside the ice frame is flattened to its own cream, everything that
was drawn on the paper (the mock's text, characters and ornaments) is erased,
and the seal is kept. The page then puts its own text, characters and
ornaments on the flat paper, at the reference's places.

The other drawings are code, so they are reproducible and reviewable; the PNGs
are plain files, so they can be opened and edited in Aseprite afterwards (edit
the PNG, delete the line here that writes it, and the page still uses your
version).

    python3 kris-jangle/art/gen_page_art.py        (from the fm1-base root)

**The scale.** Every piece of art here is drawn at SCALE (2) image pixels for
every pixel of the game's own grid, and the page shows each image at its
natural size. So one game pixel is two CSS pixels on the phone, and the
browser's own device-pixel scaling takes it to whole screen pixels: nothing is
ever resampled, so the pixels stay square. The card is cut at the reference's
own resolution and resized to the same rule: 680 image pixels wide, shown at
340 CSS pixels, two image pixels to the CSS pixel like everything else.

**The colours.** The game's palette (art/palette.py) where the page wants what
the game has — the snow, the frost, the gold — and a few of the page's own
(below) for the card's ice and its ornaments, which the game has no use for.
"""
import math
import pathlib
import shutil
import sys
from collections import deque

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1] / "tools"))
import palette as P                                          # noqa: E402
from gen_art import render_text, grow                        # noqa: E402

PAGE = HERE.parent / "web" / "sound" / "art"                 # where the page's art goes

SCALE = 2                    # image pixels for every pixel of the game's grid
FACE = "ter-u16b.bdf"        # the game's "B" face (font_set = "terminus")
LIGHT = "ter-u16n.bdf"       # the game's "S" face, the lighter one

# The page's own colours, sampled off the reference. The game has no use for
# these, so they do not go in art/palette.py (which is the colours the device
# paints with).
ICE = (126, 190, 232)        # the headings' ice
ICE_DEEP = (54, 100, 170)    # their edge
ORN = (154, 204, 236)        # the paper ornaments' blue
PAPER_FLAT = (248, 238, 224)  # the card's paper, painted flat over the erased art

# The card as the page shows it: CARD_W image pixels wide, two for every CSS
# pixel. The cut keeps the reference's own proportions.
CARD_W = 680


def scaled(img):
    """the image at the page's scale: every pixel of the game's grid becomes
    SCALE image pixels."""
    return img.resize((img.size[0] * SCALE, img.size[1] * SCALE), Image.NEAREST)


def pixel_text(text, bdf, scale, color, edge=None, shadow=None):
    """the text as an RGBA image: the letter in `color`, an `edge` of frost
    around it (grown a pixel) and a `shadow` behind, the way the title banner
    is drawn. The letter is drawn at the game's own size and then scaled, so
    the result is the game's pixels."""
    mask = render_text(text, bdf, scale)
    edge_mask = grow(mask, 1) if edge else None
    shadow_mask = None
    if shadow:
        shadow_mask = Image.new("L", mask.size, 0)
        shadow_mask.paste(grow(mask, 1), (1, 1))
    w, h = mask.size
    img = Image.new("RGBA", (w + 2, h + 3), (0, 0, 0, 0))
    px = img.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            if x < w and y < h and mask.load()[x, y]:
                px[x, y] = color + (255,)                   # the letter
            elif edge_mask is not None and x < w and y < h and edge_mask.load()[x, y]:
                px[x, y] = edge + (255,)                    # the frost around it
            elif shadow_mask is not None and x < w and y < h and shadow_mask.load()[x, y]:
                px[x, y] = shadow + (130,)                  # the shadow behind it
    return scaled(img)


def save(img, name):
    PAGE.mkdir(parents=True, exist_ok=True)
    img.save(PAGE / name)
    print(f"{name}: {img.size[0]}x{img.size[1]}")


def snowflake(size, color, arm=None):
    """a snowflake: six arms from the centre, each with a pair of branches."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2
    r = size / 2 - 1
    w = max(1, size // 12)
    arm = arm or r
    ink = color + (255,) if len(color) == 3 else color
    for i in range(6):
        a = i * math.pi / 3
        dx, dy = math.cos(a), math.sin(a)
        d.line([(c, c), (c + dx * arm, c + dy * arm)], fill=ink, width=w)
        for at, ln in ((0.45, 0.28), (0.72, 0.20)):
            bx, by = c + dx * arm * at, c + dy * arm * at
            for s in (-1, 1):
                b = a + s * math.pi / 4
                d.line([(bx, by), (bx + math.cos(b) * arm * ln, by + math.sin(b) * arm * ln)],
                       fill=ink, width=w)
    return img


def sparkle4(size, color):
    """a four-pointed sparkle: a diamond each way from the centre."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2
    r = size / 2 - 1
    for i in range(4):
        a = i * math.pi / 2
        dx, dy = math.cos(a), math.sin(a)
        d.polygon([(c + dx * r, c + dy * r),
                   (c + dx * 4 - dy * 2, c + dy * 4 + dx * 2),
                   (c, c), (c + dx * 4 + dy * 2, c + dy * 4 - dx * 2)],
                  fill=color + (255,) if len(color) == 3 else color)
    d.rectangle([c - 1, c - 1, c + 1, c + 1],
                fill=color + (255,) if len(color) == 3 else color)
    return img


def plus(size, color):
    """a plus sign, the way the reference's stars are drawn."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2
    w = max(2, size // 5)
    d.rectangle([c - w / 2, 0, c + w / 2, size], fill=color + (255,))
    d.rectangle([0, c - w / 2, size, c + w / 2], fill=color + (255,))
    return img


# ------------------------------------------------------------- the card ---
# The reference, and where its card sits in it. The cut was worked out against
# the reference itself; the numbers below are its result, so the script is
# reproducible without re-deriving them.
CARD_CUT = (168, 271, 975, 1135)     # the card's box in the reference, x0 y0 x1 y1

# The paper ornaments, redrawn where the reference has them (its characters
# and text sat on the others' spots, so a few are nudged clear of the page's
# own content). Six-arm flakes, plus two small sparkles, in the card's own
# image pixels: x, y, size.
ORNAMENTS = [
    (347, 93, 44),      # under the frame's top emblem
    (136, 257, 46),     # the paper's left edge
    (110, 320, 40),     # below it
    (300, 545, 40),     # between Frosty's spot and the seal
    (210, 200, 22),     # a small sparkle beside the number
    (445, 330, 18),     # another, below the number's right
]


def cut_card():
    """the card: the reference's ice frame, its paper flattened, its seal
    kept, its ornaments redrawn. Everything else the mock had on the paper —
    the text, the characters, the seal's neighbours — the page draws itself,
    so it comes off here."""
    im = Image.open(HERE / "reference-badge.jpeg").convert("RGB")
    W, H = im.size
    a = np.asarray(im).astype(int)
    R, G, B = a[:, :, 0], a[:, :, 1], a[:, :, 2]
    LUM = (R + G + B) / 3

    # the sky's gradient, sampled off the margins (the same numbers the page's
    # CSS uses, so the card sits on its own sky)
    margins = np.concatenate([a[:, 8:56], a[:, W - 56:W - 8]], axis=1)
    grad = np.median(margins, axis=1)
    lum_grad = grad.mean(axis=1)

    def flood_from_border(seed):
        flood = np.zeros((H, W), bool)
        dq = deque()
        for x in range(W):
            for y in (0, H - 1):
                if seed[y, x] and not flood[y, x]:
                    flood[y, x] = True
                    dq.append((y, x))
        for y in range(H):
            for x in (0, W - 1):
                if seed[y, x] and not flood[y, x]:
                    flood[y, x] = True
                    dq.append((y, x))
        while dq:
            y, x = dq.popleft()
            for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if 0 <= ny < H and 0 <= nx < W and seed[ny, nx] and not flood[ny, nx]:
                    flood[ny, nx] = True
                    dq.append((ny, nx))
        return flood

    def components(mask):
        lab = np.zeros((H, W), np.int32)
        sizes = {}
        cur = 0
        ys, xs = np.nonzero(mask)
        for y0, x0 in zip(ys, xs):
            if lab[y0, x0]:
                continue
            cur += 1
            q = deque([(y0, x0)])
            lab[y0, x0] = cur
            n = 0
            while q:
                y, x = q.popleft()
                n += 1
                for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                    if 0 <= ny < H and 0 <= nx < W and mask[ny, nx] and not lab[ny, nx]:
                        lab[ny, nx] = cur
                        q.append((ny, nx))
            sizes[cur] = n
        return lab, sizes

    def grow_m(m, r):
        return np.asarray(Image.fromarray((m * 255).astype(np.uint8))
                          .filter(ImageFilter.MaxFilter(2 * r + 1))) > 127

    def shrink_m(m, r):
        return np.asarray(Image.fromarray((m * 255).astype(np.uint8))
                          .filter(ImageFilter.MinFilter(2 * r + 1))) > 127

    def close_m(m, r):
        return shrink_m(grow_m(m, r), r)

    # -- the card: everything the sky flood cannot reach ----------------------
    # The tolerance is generous, but the ridge's dark under-edge must not count
    # as sky: navy much darker than its own row's sky is card, not sky.
    d = np.abs(a - grad[:, None, :]).sum(axis=2)
    darknavy = (B - R > 15) & (LUM < lum_grad[:, None] - 40)
    card = ~flood_from_border((d < 90) & ~darknavy)
    card = close_m(card, 4)
    lab, sizes = components(card)
    card = lab == max(sizes, key=sizes.get)
    # the pips' gold hangs off the card's bottom; it goes. Only near the edge:
    # the mock's Frosty wears gold too, and his is card.
    gold = (R - B > 60) & (R > 180) & (G > 120)
    card &= ~(gold & ~shrink_m(card, 30))
    lab, sizes = components(card)
    card = lab == max(sizes, key=sizes.get)
    ys, xs = np.nonzero(card)
    X0, Y0 = int(xs.min()), int(ys.min())

    # -- the ice: the card's blue, thick where it is frame, thin where it is
    # ornament ---------------------------------------------------------------
    blue = (B - R > 45) & (B > 140) & (G > 110) & grow_m(card, 2)
    band = shrink_m(card, 4) & ~shrink_m(card, 65)      # the frame's own ring
    lab, sizes = components(blue)
    ring_band = np.zeros((H, W), bool)
    for cid, n in sizes.items():
        m = lab == cid
        if n > 2000 and (m & band).sum() > n * 0.3:
            ring_band |= m
    barrier_full = close_m(ring_band, 9)                # the flood's wall round the paper
    lab, sizes = components(shrink_m(barrier_full, 2))
    barrier_ice = np.zeros((H, W), bool)
    for cid, n in sizes.items():
        if n > 800:
            barrier_ice |= lab == cid
    protect = grow_m(barrier_ice, 12)                   # the frame and what sits on it

    # -- the seal: the mock's green stamp, kept -------------------------------
    green = (G - R > 15) & (G - B > 10) & (G > 100)
    lab, sizes = components(green)
    seal_green = np.zeros((H, W), bool)
    for cid, n in sizes.items():
        m = lab == cid
        if n > 200 and np.nonzero(m)[1].min() > X0 + 350:
            seal_green |= m
    whiteish = (R > 236) & (G > 234) & (B > 226)
    seal_keep = grow_m(seal_green, 8) | (whiteish & grow_m(seal_green, 30))

    # -- flatten the paper ----------------------------------------------------
    reached = flood_from_border(~barrier_full)
    core = shrink_m(card, 18)                           # certainly inside the paper
    snowzone = whiteish & shrink_m(card, 4) & ~shrink_m(card, 42)   # the snow on the frame
    base = (core | (~reached & card)) & ~protect & ~seal_keep & ~snowzone
    # The paper's own blue — the mock's ornaments — dies too, but the frame's
    # blue must not: the ice is thick, the ornaments' arms are thin, and an
    # erosion tells them apart.
    frameblue = blue & (protect | band)
    exemption = grow_m(shrink_m(frameblue, 2), 10) | grow_m(barrier_ice, 6)
    kill = blue & core & ~exemption & ~grow_m(seal_green, 10)
    flatten = base & ~blue
    flatten |= kill
    # the Grump's feet left thin white dashes above the seal; they die, the
    # snow on the seal (a thick mass) stays
    zone = np.zeros((H, W), bool)
    zone[Y0 + 425:Y0 + 482, X0 + 440:X0 + 690] = True
    zw = whiteish & zone
    flatten |= zw & ~shrink_m(zw, 2) & ~grow_m(seal_green, 2) & card

    out = a.copy()
    out[flatten] = PAPER_FLAT

    # the sky-coloured nicks the flood left inside the frame: filled from the
    # ice around them
    nick = (d < 55) & shrink_m(card, 6) & card
    lab, sizes = components(nick)
    for cid, n in sizes.items():
        if n < 40:
            continue
        m = lab == cid
        ring = grow_m(m, 12) & ~grow_m(m, 3) & card & ~nick
        ys4, xs4 = np.nonzero(ring)
        out[m] = np.median(out[ys4, xs4], axis=0)

    # -- cut, and draw the paper's ornaments ----------------------------------
    alpha = shrink_m(card, 2)
    rgba = np.dstack([out, alpha * 255]).astype(np.uint8)
    cut = Image.fromarray(rgba, "RGBA").crop((X0, Y0, int(xs.max()) + 1, int(ys.max()) + 1))
    ch = round(cut.size[1] * CARD_W / cut.size[0])
    card_img = cut.resize((CARD_W, ch), Image.LANCZOS)
    for x, y, s in ORNAMENTS:
        card_img.alpha_composite(snowflake(s, ORN), (int(x - s / 2), int(y - s / 2)))
        card_img.alpha_composite(sparkle4(max(8, s // 3), P.WHITE),
                                 (int(x - max(8, s // 3) / 2), int(y - max(8, s // 3) / 2)))
    save(card_img, "card.png")


def headings():
    """the page's headings: the game's face, in the reference's white with the
    blue rim and the deep shadow behind."""
    save(pixel_text("YOUR BADGE", FACE, 1, P.WHITE, edge=ICE, shadow=ICE_DEEP), "head_badge.png")


def title():
    """the page's title: the game's name, in the reference's white with the
    blue rim. The device's own banner (art/title.png) is white and frosted,
    which is right on the device's screen and wrong here."""
    save(pixel_text("KRIS JANGLE", LIGHT, 1, P.WHITE, edge=ICE, shadow=ICE_DEEP), "title_page.png")


def level_word():
    """the word LEVEL, in the ice with a white rim, the way the reference
    draws it."""
    save(pixel_text("LEVEL", FACE, 1, ICE, edge=P.WHITE, shadow=ICE_DEEP), "head_level.png")


def divider():
    """the rule under the title: a line with a snowflake in the middle."""
    w, h = 120, 12
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rectangle([0, h // 2 - 1, w // 2 - 10, h // 2], fill=P.WHITE + (200,))
    d.rectangle([w // 2 + 10, h // 2 - 1, w - 1, h // 2], fill=P.WHITE + (200,))
    fl = snowflake(11, ICE + (255,))
    img.alpha_composite(fl, (w // 2 - 5, 0))
    save(scaled(img), "divider.png")


def ornaments():
    """the paper ornaments the page sets beside the carol's name and the
    level: a snowflake and a sparkle, in the ornaments' blue."""
    save(scaled(snowflake(22, ORN)), "orn_flake.png")
    save(scaled(sparkle4(14, ORN)), "orn_spark.png")


def sky_bits():
    """the sky's stars: the reference's plus signs, and the four-pointed
    sparkles that catch the light beside the card."""
    save(scaled(plus(7, P.WHITE)), "plus.png")
    save(scaled(sparkle4(16, P.WHITE)), "sparkle.png")


def characters():
    """the game's own characters, copied into the page: Frosty in his two
    poses (the ones the firmware animates), the Grump, Santa and the title
    banner. The page uses the same files the device draws."""
    for name in ("frosty1", "frosty2", "grump", "santa", "title"):
        src = HERE / f"{name}.png"
        if src.exists():
            shutil.copyfile(src, PAGE / f"{name}.png")
            print(f"{name}.png: copied")


def icons():
    """the page's icons: the game's Frosty on the night sky, for the home
    screen and for the phone's share sheet. Maskable, so the art sits in the
    middle of the square with room to be cropped."""
    for size in (192, 512):
        img = Image.new("RGBA", (size, size), (14, 22, 56, 255))
        d = ImageDraw.Draw(img)
        for i, col in enumerate(P.SKY):                     # the sky's ten bands
            d.rectangle([0, size * i // 10, size, size * (i + 1) // 10], fill=col + (255,))
        d.rectangle([0, size * 86 // 100, size, size], fill=P.SNOW + (255,))
        try:
            who = Image.open(HERE / "frosty1.png").convert("RGBA")
        except OSError:
            continue
        h = int(size * 0.60)                                # the safe zone
        w = int(who.width * h / who.height)
        who = who.resize((w, h), Image.NEAREST)
        img.paste(who, ((size - w) // 2, int(size * 0.20)), who)
        save(img, f"icon-{size}.png")


def main():
    """draw every piece of the page's art, or just the ones named on the
    command line"""
    names = sys.argv[1:] or ["cut_card", "headings", "title", "level_word", "divider",
                             "ornaments", "sky_bits", "characters", "icons"]
    for name in names:
        globals()[name]()


if __name__ == "__main__":
    main()
