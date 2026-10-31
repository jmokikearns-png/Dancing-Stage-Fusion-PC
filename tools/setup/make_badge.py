#!/usr/bin/env python3
"""The "PC" badge Setup.exe puts on the program's icon (setup/icon/badge.rgba).

The icon itself is the game's memory card icon, which Setup takes from the
player's own disc; this file holds only the badge, drawn over it at each icon
size from 32 up: raw 8-bit RGBA rows, sizes 32, 48, 64, 128, 256 one after
another, transparent outside the badge.
    make_badge.py OUT_FILE
"""
import sys
from PIL import Image, ImageDraw, ImageFont

SIZES = (32, 48, 64, 128, 256)
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'


def badge(size):
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # A rounded tag in the bottom-right corner, white "PC" on dark blue.
    bw, bh = int(size * 0.50), int(size * 0.30)
    x0, y0 = size - bw - max(1, size // 32), size - bh - max(1, size // 32)
    d.rounded_rectangle((x0, y0, x0 + bw, y0 + bh), radius=max(2, size // 14),
                        fill=(18, 24, 48, 255), outline=(255, 255, 255, 255),
                        width=max(1, size // 40))
    f = ImageFont.truetype(FONT, int(bh * 0.72))
    tw, th = d.textbbox((0, 0), 'PC', font=f)[2:]
    d.text((x0 + (bw - tw) // 2, y0 + (bh - th) // 2 - int(bh * 0.08)), 'PC',
           font=f, fill=(255, 255, 255, 255))
    return im


with open(sys.argv[1], 'wb') as out:
    for s in SIZES:
        out.write(badge(s).tobytes())
