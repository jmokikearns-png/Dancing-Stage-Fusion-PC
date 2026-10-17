#!/usr/bin/env python3
"""Advance widths of a font at a pixel size, for a StepMania bitmap font page
of 256 cells, which StepMania maps as Windows-1252.  Writes JSON (the
character and width of each cell) for the FontSheet composition and the
StepMania .ini.
usage: font_metrics.py FONT.woff2 SIZE_PX OUT.json"""
import json, sys
from fontTools.ttLib import TTFont

font = TTFont(sys.argv[1])
size = float(sys.argv[2])
upm = font['head'].unitsPerEm
cmap = font.getBestCmap()
hmtx = font['hmtx']
widths, chars = [], []
for cell in range(256):
    try:
        ch = ord(bytes([cell]).decode('cp1252'))
    except UnicodeDecodeError:
        ch = 0
    if cell < 32 or cell == 0x7F or ch == 0 or ch not in cmap:
        widths.append(0)
        chars.append(0)
        continue
    adv = hmtx[cmap[ch]][0]
    widths.append(int(round(adv * size / upm)))
    chars.append(ch)
os2 = font['OS/2']
out = {
    'size': size,
    'widths': widths,
    'chars': chars,
    'capHeight': round(os2.sCapHeight * size / upm, 2),
    'ascent': round(font['hhea'].ascent * size / upm, 2),
    'descent': round(-font['hhea'].descent * size / upm, 2),
}
json.dump(out, open(sys.argv[3], 'w'))
print('widths: max %d, "W" %d, "a" %d; cap %.1f px' % (max(widths), widths[ord('W')], widths[ord('a')], out['capHeight']))
