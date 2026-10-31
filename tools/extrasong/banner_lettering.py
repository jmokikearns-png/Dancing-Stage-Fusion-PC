#!/usr/bin/env python3
"""Cut a song banner's own lettering out, colours and outlines included, as
an upscaled RGBA picture for a gameplay background (port/graphics
SongBackgrounds), so the background's title is set exactly as the banner's.

  banner_lettering.py BANNER.png OUT.png --mode M [--box X0 Y0 X1 Y1] [--scale 4]
                      [--min-part F] [--grow N]

Modes pick what is lettering on that banner:
  bright   light pixels (and their coloured fringes) on a dark ground
  white    white letter fills, grown by --grow pixels to take their outline
  outline  dark outlines of one hue (--hue navy, green or purple), with
           whatever they enclose
  warm     yellow-orange fills with their orange outline, with what they
           enclose
--box limits the cut to part of the banner (in banner pixels); --min-part
drops specks smaller than that fraction of the largest part; --drop red or
--drop colour removes background caught inside the letters (red to
maroon, or any bright saturated colour); --exclude X0 Y0 X1 Y1 leaves a box out.  For the
player's own art only: the result goes with their files, not into this
project.
"""

import argparse
from collections import deque

import numpy as np
from PIL import Image, ImageFilter


# ---- the few image operations needed (as scipy.ndimage's, 4-connected) ----
def dilate(m, iterations=1):
	m = np.asarray(m, bool)
	for _ in range(iterations):
		n = m.copy()
		n[1:] |= m[:-1]
		n[:-1] |= m[1:]
		n[:, 1:] |= m[:, :-1]
		n[:, :-1] |= m[:, 1:]
		m = n
	return m


def fill_holes(m):
	"""m with every background region that doesn't reach the edge filled."""
	m = np.asarray(m, bool)
	bg = ~m
	reach = np.zeros_like(m)
	reach[0, :], reach[-1, :], reach[:, 0], reach[:, -1] = bg[0, :], bg[-1, :], bg[:, 0], bg[:, -1]
	while True:
		n = dilate(reach) & bg
		if (n == reach).all():
			return ~reach
		reach = n


def part_sizes(m):
	"""Label 4-connected parts: (labels, sizes), labels 1..n, sizes[i] of part i+1."""
	h, w = m.shape
	labels = np.zeros((h, w), np.int32)
	sizes, flat = [], m.ravel()
	lab = labels.ravel()
	for start in np.flatnonzero(flat):
		if lab[start]:
			continue
		n = len(sizes) + 1
		lab[start] = n
		q, count = deque([start]), 0
		while q:
			i = q.popleft()
			count += 1
			y, x = divmod(i, w)
			for j, ok in ((i - w, y > 0), (i + w, y < h - 1), (i - 1, x > 0), (i + 1, x < w - 1)):
				if ok and flat[j] and not lab[j]:
					lab[j] = n
					q.append(j)
		sizes.append(count)
	return labels, np.array(sizes)


def mask(rgb, mode, grow, hue):
	r, g, b = (rgb[..., i].astype(float) / 255 for i in range(3))
	mx, mn = np.maximum(np.maximum(r, g), b), np.minimum(np.minimum(r, g), b)
	lum, sat = r * 0.3 + g * 0.59 + b * 0.11, mx - mn
	if mode == 'bright':
		m = (mx > 0.72) & (lum > 0.45)
		return dilate(m)
	if mode == 'white':
		m = (lum > 0.72) & (sat < 0.3)
		return fill_holes(dilate(m, grow))
	if mode == 'outline':
		dark = lum < 0.5
		m = {'navy': dark & (b > r + 0.1) & (b > g),
		     'green': dark & (g > r + 0.06) & (g > b),
		     'purple': dark & (r > g + 0.1) & (b > g + 0.1)}[hue]
		return fill_holes(dilate(m))
	if mode == 'warm':
		m = (r > 0.8) & (g > 0.35) & (b < 0.35)
		return fill_holes(dilate(m))
	raise SystemExit('unknown mode')


def cut(im, mode, box=None, scale=4, min_part=0.0, grow=2, hue='navy', drop=None, exclude=()):
	"""The lettering of banner `im` (a PIL image), upscaled `scale` times,
	as RGBA; None if there is none."""
	im = im.convert('RGB')
	if box:
		im = im.crop(box)
	rgb = np.asarray(im)
	m = mask(rgb, mode, grow, hue)
	if drop:
		r, g, b = (rgb[..., i].astype(float) / 255 for i in range(3))
		mx, mn = np.maximum(np.maximum(r, g), b), np.minimum(np.minimum(r, g), b)
		bad = (r > g + 0.15) & (r > b + 0.05) & (g < 0.45) if drop == 'red' else (mx - mn > 0.35) & (mx > 0.6) & ~((b > r) & (b > g) & (mx < 0.5))
		m &= ~bad
	for x0, y0, x1, y1 in exclude:
		ox, oy = (box[0], box[1]) if box else (0, 0)
		m[max(0, y0 - oy):max(0, y1 - oy), max(0, x0 - ox):max(0, x1 - ox)] = False
	if min_part:
		labels, sizes = part_sizes(m)
		if len(sizes):
			m = np.isin(labels, 1 + np.nonzero(sizes >= min_part * sizes.max())[0])
	ys, xs = np.nonzero(m)
	if not len(ys):
		return None
	x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
	s = scale
	col = im.crop((x0, y0, x1, y1)).resize(((x1 - x0) * s, (y1 - y0) * s), Image.LANCZOS)
	alpha = Image.fromarray((m[y0:y1, x0:x1] * 255).astype(np.uint8), 'L')
	alpha = alpha.resize(col.size, Image.BICUBIC).filter(ImageFilter.GaussianBlur(s * 0.35))
	v = np.asarray(alpha).astype(float) / 255
	v = np.clip((v - 0.5) * 3 + 0.5, 0, 1)
	return Image.fromarray(np.dstack([np.asarray(col), (v * 255).astype(np.uint8)]), 'RGBA')


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('banner'); ap.add_argument('out')
	ap.add_argument('--mode', required=True, choices=('bright', 'white', 'outline', 'warm'))
	ap.add_argument('--box', type=int, nargs=4)
	ap.add_argument('--scale', type=int, default=4)
	ap.add_argument('--min-part', type=float, default=0.0)
	ap.add_argument('--grow', type=int, default=2)
	ap.add_argument('--hue', choices=('navy', 'green', 'purple'), default='navy')
	ap.add_argument('--drop', choices=('red', 'colour'))
	ap.add_argument('--exclude', type=int, nargs=4, action='append', default=[], help='box (in the banner) to leave out')
	a = ap.parse_args()
	out = cut(Image.open(a.banner), a.mode, a.box, a.scale, a.min_part, a.grow, a.hue, a.drop, a.exclude)
	if out is None:
		raise SystemExit('nothing found')
	out.save(a.out)
	print('%s: %dx%d' % (a.out, out.width, out.height))


if __name__ == '__main__':
	main()
