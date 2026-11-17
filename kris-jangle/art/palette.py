# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""Kris Jangle's palette: the colours the game draws with and the art is
generated in. One place, so the generated sprites sit in the sky rather than
on it; app.c's sky, panel and ground colours are the same numbers.

    SKY      the ten dusk bands, top to bottom
    PANEL    the boxes and the note modal (dark blue glass over the sky)
    PINE     the firs on the ridge
    SNOW     the ground and the flakes
    FROST    the icy edge on the title banner
    PAPER    the paper the badge screen's code is on (app.c's QR screen)
    WOOD     the wooden frames around the boxes (art/frame_*.png)
    BOX_IN   the boxes' interior (dark chocolate, was the navy PANEL)
"""

SKY = [
    (14, 22, 56), (24, 36, 86), (38, 52, 116), (54, 68, 138), (74, 84, 154),
    (100, 96, 162), (128, 106, 160), (156, 120, 154), (184, 138, 148), (212, 160, 140),
]
PANEL = (10, 14, 38)
WOOD = (150, 92, 48)
WOOD_DARK = (96, 56, 28)
WOOD_LITE = (196, 134, 78)
BOX_IN = (58, 36, 24)
SNOW_LITE = (240, 244, 252)
SNOW_SHADE = (176, 186, 220)
PINE = (26, 30, 66)
PINE_DARK = (18, 22, 52)
SNOW = (238, 232, 238)
SNOW_SHADE = (176, 166, 200)
FROST = (176, 214, 240)
WHITE = (255, 255, 255)
INK = (10, 14, 38)
GOLD = (255, 200, 60)
PAPER = (250, 244, 232)         # the QR screen's paper: the code is dark on light
