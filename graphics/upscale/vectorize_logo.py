#!/usr/bin/env python3
"""Trace the game logo (the player's own copy, upscaled) into clean vector
layers for ThemeArt.tsx's ThemeTitleLogoDSF: the logo is flat layers drawn
over each other (green edge, yellow rim, dark ink, letter fill), so each is
split off by colour as a nested mask and traced with potrace into smooth
curves.  The result is Konami's artwork too: it goes to the build's temporary
public dir, never into the project.

Also writes the letters' shading as its own smooth texture (OUT.json's
name with -fill.png): every pixel takes the nearest letter-fill colour, then
a blur, so the shading has no ink bleeding in and no upscaling blotches; the
traced fill shape gives it its edge.

  vectorize_logo.py LOGO.png OUT.json"""
import json, re, subprocess, sys, tempfile, os
import numpy as np
from PIL import Image
from scipy import ndimage

def absorb(mask, inside, max_area):
	"""Small parts of `inside` enclosed in `mask`'s area become part of it:
	highlights or specks in a letter that only match the next layer's colour."""
	lab, n = ndimage.label(inside & ~mask)
	if n == 0:
		return mask
	area = ndimage.sum(np.ones_like(lab), lab, index=np.arange(1, n + 1))
	small = np.zeros(n + 1, bool)
	small[1:] = area <= max_area
	return mask | small[lab]

def trace(mask, tmp):
	pbm = os.path.join(tmp, 'm.pbm')
	Image.fromarray(np.where(mask, 0, 255).astype(np.uint8)).convert('1').save(pbm)
	svg = subprocess.run(['potrace', pbm, '-b', 'svg', '-o', '-', '--flat', '-t', '30', '-a', '1.1', '-O', '0.4'],
		capture_output=True, text=True, check=True).stdout
	transform = re.search(r'<g transform="([^"]+)"', svg).group(1)
	paths = re.findall(r'<path d="([^"]+)"', svg)
	return {'transform': transform, 'd': ' '.join(p.replace('\n', ' ') for p in paths)}

def main():
	src, dst = sys.argv[1], sys.argv[2]
	im = np.asarray(Image.open(src).convert('RGBA')).astype(int)
	R, G, B, A = (im[:, :, i] for i in range(4))
	lum = (R * 3 + G * 6 + B) / 10
	green = (B < 45) & (R < 150) & (G > 130)
	yellow = (R > 175) & (G > 165) & (B < 32)	# the rim; highlights in the letters are greener (B 40-60)
	ink = (lum < 85) & (R < 60)
	edge = A > 128
	area = (im.shape[0] * im.shape[1]) / 70000	# one logo pixel's area at this scale (400x175 art)
	rim = edge & ~green
	inked = rim & ~yellow
	inked = absorb(inked, ndimage.binary_fill_holes(inked) & ~inked, 12 * area)
	fill = inked & ~ink
	fill = absorb(fill, ndimage.binary_fill_holes(fill) & ~fill & inked, 6 * area)
	with tempfile.TemporaryDirectory() as tmp:
		layers = [dict(name=n, **trace(m, tmp)) for n, m in (('edge', edge), ('rim', rim), ('ink', inked), ('fill', fill))]
	tex = dst[:-5] + '-fill.png' if dst.endswith('.json') else dst + '-fill.png'
	core = ndimage.binary_erosion(fill, iterations=max(1, round(im.shape[1] / 400)))
	_, (iy, ix) = ndimage.distance_transform_edt(~core, return_indices=True)
	rgb = im[:, :, :3][iy, ix].astype(np.float32)
	sigma = 0.9 * im.shape[1] / 400
	rgb = np.stack([ndimage.gaussian_filter(rgb[:, :, c], sigma) for c in range(3)], -1)
	Image.fromarray(np.clip(rgb + 0.5, 0, 255).astype(np.uint8), 'RGB').save(tex)
	json.dump({'w': im.shape[1], 'h': im.shape[0], 'fill_texture': os.path.basename(tex), 'layers': layers}, open(dst, 'w'))

if __name__ == '__main__':
	main()
