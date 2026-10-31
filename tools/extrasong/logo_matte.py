#!/usr/bin/env python3
"""Cut a logo out of a picture as a clean, upscaled alpha matte.

  logo_matte.py IN.png OUT.png --box X0 Y0 X1 Y1 --key red|white
                [--scale 4] [--rotate DEG] [--exclude X0 Y0 X1 Y1 ...] [--on-dark R]
                [--min-part F] [--drop-at X Y ...]

--key red keeps strongly red pixels (a stamp), --key white keeps bright,
grey pixels (lettering).  --on-dark R keeps only what lies on a dark
surface: the dark pixels closed by R (filling the letters on it), shrunk
by a little to drop the surface's edge.  --min-part F drops specks: parts
smaller than F times the largest part; --drop-at X Y the part at that
point of OUT (as printed by a run without it).  The matte is scaled up
smoothly and its edge sharpened, so it can be recoloured and outlined at
any size (port/graphics SongBackground masks a colour with it).  OUT is
white on transparent.  For the player's own artwork only: the result goes
with their files, not into this project.
"""
import argparse

import numpy as np
from PIL import Image, ImageFilter

from banner_lettering import dilate, part_sizes


def matte(rgb, key):
	r, g, b = (rgb[..., i].astype(float) for i in range(3))
	if key == 'red':
		# how far red stands out from the other two channels
		a = (r - np.maximum(g, b)) / 255.0
		return np.clip((a - 0.12) / 0.22, 0, 1)
	lum = (r * 0.3 + g * 0.59 + b * 0.11) / 255.0
	sat = (np.maximum(np.maximum(r, g), b) - np.minimum(np.minimum(r, g), b)) / 255.0
	return np.clip((lum - 0.42) / 0.22, 0, 1) * np.clip((0.35 - sat) / 0.15, 0, 1)


def cut_matte(img, box, key, scale=4, rotate=0.0, exclude=(), on_dark=0, min_part=0.0, drop_at=()):
	"""The logo in `box` of `img` (a PIL image) as a white-on-transparent
	RGBA matte, `scale` times larger; None if there is nothing there."""
	src = np.asarray(img.convert('RGB'))
	m = matte(src, key)
	if on_dark:
		lum = src.astype(float) @ [0.3, 0.59, 0.11] / 255.0
		dark = Image.fromarray(((lum < 0.24) * 255).astype(np.uint8), 'L')
		k = 2 * on_dark + 1
		surface = dark.filter(ImageFilter.MaxFilter(k)).filter(ImageFilter.MinFilter(k)).filter(ImageFilter.MinFilter(5))
		m = m * (np.asarray(surface) > 0)
	for x0, y0, x1, y1 in exclude:
		m[y0:y1, x0:x1] = 0
	x0, y0, x1, y1 = box
	m = m[y0:y1, x0:x1]
	im = Image.fromarray((m * 255).astype(np.uint8), 'L')
	im = im.resize((im.width * scale, im.height * scale), Image.BICUBIC)
	im = im.filter(ImageFilter.GaussianBlur(scale * 0.45))
	if rotate:
		im = im.rotate(rotate, Image.BICUBIC, expand=True)
	# a crisp edge: steepen the soft ramp around the half-way level
	v = np.asarray(im).astype(float) / 255.0
	v = np.clip((v - 0.5) * 4 + 0.5, 0, 1)
	if min_part:
		labels, sizes = part_sizes(v > 0.3)
		if len(sizes):
			keep = np.isin(labels, 1 + np.nonzero(sizes >= min_part * sizes.max())[0])
			v = v * dilate(keep, 2)
	ys, xs = np.nonzero(v > 0.02)
	if not len(ys):
		return None
	pad = 2 * scale
	v = v[max(0, ys.min() - pad):ys.max() + pad + 1, max(0, xs.min() - pad):xs.max() + pad + 1]
	if drop_at:
		labels, _ = part_sizes(v > 0.02)
		for x, y in drop_at:
			if y < labels.shape[0] and x < labels.shape[1] and labels[y, x]:
				v = v * (labels != labels[y, x])
	out = np.zeros(v.shape + (4,), np.uint8)
	out[..., :3] = 255
	out[..., 3] = (v * 255).astype(np.uint8)
	return Image.fromarray(out, 'RGBA')


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('src'); ap.add_argument('out')
	ap.add_argument('--box', type=int, nargs=4, required=True)
	ap.add_argument('--key', choices=('red', 'white'), required=True)
	ap.add_argument('--scale', type=int, default=4)
	ap.add_argument('--rotate', type=float, default=0.0, help='degrees, counter-clockwise, applied after cutting')
	ap.add_argument('--exclude', type=int, nargs=4, action='append', default=[], help='box (in IN) to leave out')
	ap.add_argument('--on-dark', type=int, default=0, help='closing radius of the dark surface the lettering is on')
	ap.add_argument('--min-part', type=float, default=0.0, help='drop parts smaller than this fraction of the largest')
	ap.add_argument('--drop-at', type=int, nargs=2, action='append', default=[], help='OUT point whose part to drop')
	a = ap.parse_args()
	out = cut_matte(Image.open(a.src), a.box, a.key, a.scale, a.rotate, a.exclude, a.on_dark, a.min_part, a.drop_at)
	if out is None:
		raise SystemExit('nothing found')
	out.save(a.out)
	print('%s: %dx%d' % (a.out, out.width, out.height))


if __name__ == '__main__':
	main()
