import glob, sys
from PIL import Image, ImageDraw
d, out, cols = sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 10
DIV = float(sys.argv[4]) if len(sys.argv) > 4 else 50
fs = sorted(glob.glob(d + '/*.png'))
w, h = Image.open(fs[0]).size
sheet = Image.new('RGB', (w * cols, (h + 14) * ((len(fs) + cols - 1) // cols)))
dr = ImageDraw.Draw(sheet)
for i, p in enumerate(fs):
    x, y = (i % cols) * w, (i // cols) * (h + 14)
    sheet.paste(Image.open(p), (x, y))
    t = int(p.rsplit("/", 1)[1][:-4]) / DIV
    dr.text((x + 3, y + h), '%.1f' % t, fill=(255, 255, 0))
sheet.save(out)
