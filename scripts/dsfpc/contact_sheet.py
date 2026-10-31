#!/usr/bin/env python3
"""Contact sheet of a video: one frame every STEP seconds, labelled with its time.
usage: contact_sheet.py VIDEO OUT.png START END STEP [COLS]"""
import subprocess, sys, tempfile, os
from PIL import Image, ImageDraw
video, out = sys.argv[1], sys.argv[2]
start, end, step = float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])
cols = int(sys.argv[6]) if len(sys.argv) > 6 else 6
tw, th = 240, 180
frames = []
with tempfile.TemporaryDirectory() as td:
    t = start
    while t <= end:
        p = os.path.join(td, '%08.2f.png' % t)
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-ss', str(t), '-i', video, '-frames:v', '1', '-y', p])
        if os.path.exists(p):
            im = Image.open(p).convert('RGB').resize((tw, th))
            ImageDraw.Draw(im).text((4, 4), '%.1f' % t, fill=(255, 255, 0))
            frames.append(im)
        t += step
rows = (len(frames) + cols - 1) // cols
sheet = Image.new('RGB', (cols * tw, max(rows, 1) * th))
for i, im in enumerate(frames):
    sheet.paste(im, ((i % cols) * tw, (i // cols) * th))
sheet.save(out)
print(out, len(frames))
