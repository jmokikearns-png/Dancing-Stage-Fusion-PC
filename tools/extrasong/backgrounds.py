"""The new gameplay backgrounds (graphics/src/SongBackgrounds.tsx), finished
on the player's PC.

The art is this project's own: art/<name>.png is each background rendered
without its title lettering and finishing touches (graphics: the <name>Base
stills).  The lettering is the song banner's own, so it is cut out of the
player's banner here (banner_lettering.cut) and set as the component sets it,
with its drop shadow; then the halftone and the vignette go over the whole.
The numbers below are the component's; keep them in step.

In songs.toml: background = "@BgGoWest" (the song's banner gives the
lettering); for a stock song ([[stock]]) the disc's own banner does.
"""
import math
import os

import numpy as np
from PIL import Image, ImageFilter

import banner_lettering

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 1280, 960


def rgba(s):
	"""'rgba(r,g,b,a)' or '#rrggbb' -> (r, g, b, a 0..1)."""
	if s.startswith('#'):
		return int(s[1:3], 16), int(s[3:5], 16), int(s[5:7], 16), 1.0
	r, g, b, a = (float(x) for x in s[s.index('(') + 1:s.index(')')].split(','))
	return int(r), int(g), int(b), a


# Each background: its lettering (cut from the banner: banner_lettering.cut's
# arguments; placed: x, y, width, rotation in degrees clockwise, shadow),
# its halftone (opacity, dot spacing, colour, bottom-only mask) and its
# vignette (ellipse size, where it starts, colour).
SPECS = {
	'BgKidsInAmerica': dict(
		lettering=[(dict(mode='bright', min_part=0.01), 150, 58, 980, 0, 'rgba(11,3,32,0.85)')],
		halftone=(0.22, 10, '#000000', True),
		vignette=(0.85, 0.62, 'rgba(4,0,16,0.6)')),
	'BgGoWest': dict(
		lettering=[(dict(mode='white', box=(8, 14, 249, 44), grow=3, min_part=0.05, exclude=[(86, 8, 100, 46), (146, 8, 168, 18)]),
		            46, 196, 860, -7, 'rgba(26,11,11,0.6)'),
		           (dict(mode='white', box=(0, 50, 256, 76), grow=1, min_part=0.05), 590, 555, 660, -7.6, 'rgba(0,0,0,0.6)')],
		halftone=(0.18, 12, '#5a0a0a', False),
		vignette=(0.90, 0.65, 'rgba(40,8,8,0.45)')),
	'BgHeavenEarth': dict(
		lettering=[(dict(mode='outline', hue='navy', box=(0, 0, 256, 44), min_part=0.02, drop='colour'), 70, 366, 1140, 0, 'rgba(13,22,80,0.35)'),
		           (dict(mode='warm', box=(70, 38, 190, 80), min_part=0.3), 810, 672, 440, -8, 'rgba(13,22,80,0.6)')],
		halftone=None,
		vignette=(0.90, 0.70, 'rgba(13,22,80,0.25)')),
	# the stock banner, enlarged 3x as the cut was tuned on
	'BgMickey': dict(
		banner_scale=3,
		lettering=[(dict(mode='outline', hue='green', min_part=0.04, scale=2, drop='red'), 340, 540, 880, -6, 'rgba(42,6,38,0.7)'),
		           (dict(mode='outline', hue='purple', box=(0, 130, 768, 240), min_part=0.05, scale=2), 520, 770, 620, -6, 'rgba(42,6,38,0.7)')],
		halftone=(0.20, 12, '#6a0628', False),
		vignette=(0.88, 0.64, 'rgba(60,0,24,0.55)')),
}


def place(canvas, letters, x, y, w, rot, shadow):
	"""<Img> at (x, y), w wide (height to scale), rotate(rot) about its
	centre, with drop-shadow(10px 12px 0 shadow) (in its own frame)."""
	h = letters.height * w / letters.width
	im = letters.resize((w, max(1, round(h))), Image.LANCZOS)
	r, g, b, a = rgba(shadow)
	pad = int(math.hypot(w, h)) // 2 + 16
	layer = Image.new('RGBA', (w + 2 * pad, im.height + 2 * pad), (0, 0, 0, 0))
	sh = Image.new('RGBA', im.size, (r, g, b, 0))
	sh.putalpha(Image.eval(im.getchannel('A'), lambda v: int(v * a + 0.5)))
	layer.alpha_composite(sh, (pad + 10, pad + 12))
	layer.alpha_composite(im, (pad, pad))
	layer = layer.rotate(-rot, resample=Image.BICUBIC, center=(pad + w / 2, pad + h / 2))
	canvas.alpha_composite(layer, (round(x + w / 2 - (pad + w / 2)), round(y + h / 2 - (pad + h / 2))))


def halftone(rgb, opacity, size, colour, bottom):
	"""Dots of `colour` multiplied over the picture, every `size` pixels."""
	yy, xx = np.mgrid[0:H, 0:W].astype(float)
	dx, dy = (xx % size) + 0.5 - size / 2, (yy % size) + 0.5 - size / 2
	t = np.hypot(dx, dy) / (size * math.sqrt(2) / 2)	# 1 at the tile's corner
	dot = np.clip((0.36 - t) / 0.12, 0, 1)
	if bottom:
		dot *= np.clip((yy / H - 0.55) / 0.45, 0, 1)
	c = np.array(rgba(colour)[:3], float) / 255
	k = (opacity * dot)[..., None]
	return rgb * (1 - k + k * c)


def vignette(rgb, size, start, colour):
	"""radial-gradient(ellipse size size at 50% 45%, transparent start, colour 100%)."""
	yy, xx = np.mgrid[0:H, 0:W].astype(float)
	t = np.hypot((xx + 0.5 - W * 0.5) / (W * size), (yy + 0.5 - H * 0.45) / (H * size))
	r, g, b, a = rgba(colour)
	k = (np.clip((t - start) / (1 - start), 0, 1) * a)[..., None]
	return rgb * (1 - k) + np.array([r, g, b], float) * k


def compose(name, banner, jacket=None):
	"""Background `name`, its lettering cut from `banner` (a PIL image), as
	a 1280x960 RGB picture; None if the banner has no such lettering.
	BgToughEnough takes the song's jacket picture instead."""
	if name == 'BgToughEnough':
		return compose_tough(jacket) if jacket is not None else None
	spec = SPECS[name]
	canvas = Image.open(os.path.join(HERE, 'art', name + '.png')).convert('RGBA')
	if spec.get('banner_scale'):
		k = spec['banner_scale']
		banner = banner.convert('RGB').resize((banner.width * k, banner.height * k), Image.NEAREST)
	for cut_args, x, y, w, rot, shadow in spec['lettering']:
		letters = banner_lettering.cut(banner, **cut_args)
		if letters is None:
			return None
		place(canvas, letters, x, y, w, rot, shadow)
	rgb = np.asarray(canvas.convert('RGB')).astype(float)
	if spec['halftone']:
		rgb = halftone(rgb, *spec['halftone'])
	rgb = vignette(rgb, *spec['vignette'])
	return Image.fromarray(np.clip(rgb + 0.5, 0, 255).astype(np.uint8), 'RGB')


# ---- Tough Enough (graphics: SongBackground with the band's logos) ----
# Our art in three layers (BgToughEnough-under/-rays/-over: the graphics'
# BgToughEnough-* stills); the player's jacket picture gives the photo and
# both logos.
TE_CROP = (-190, 92, 470, 352)		# photoCrop: of the jacket, fills the frame
TE_STAMP = dict(box=(255, 240, 430, 345), key='red', min_part=0.002)
TE_SCRIPT = dict(box=(100, 316, 398, 385), key='white', rotate=4, on_dark=7, min_part=0.008,
                 exclude=[(0, 300, 112, 360), (385, 330, 512, 380), (250, 300, 312, 327)],
                 drop_at=[(300, 246), (270, 20), (358, 23)])


def gradient(w, h, angle, stops):
	"""CSS linear-gradient(angle deg, stops) over a w x h box, as float RGB."""
	a = math.radians(angle)
	dx, dy = math.sin(a), -math.cos(a)
	length = abs(w * dx) + abs(h * dy)
	yy, xx = np.mgrid[0:h, 0:w].astype(float)
	t = ((xx + 0.5 - w / 2) * dx + (yy + 0.5 - h / 2) * dy) / length + 0.5
	pos = [p for p, _ in stops]
	cols = np.array([[int(c[1:3], 16), int(c[3:5], 16), int(c[5:7], 16)] for _, c in stops], float)
	return np.stack([np.interp(t, pos, cols[:, i]) for i in range(3)], -1)


def filled(matte, w, h, rgb):
	"""A matte stretched to w x h, filled with the colours rgb (h x w x 3)."""
	a = np.asarray(matte.getchannel('A').resize((w, h), Image.BILINEAR)).astype(float)
	return Image.fromarray(np.dstack([np.clip(rgb + 0.5, 0, 255), a]).astype(np.uint8), 'RGBA')


def put_rotated(canvas, layer, cx, cy, rot):
	"""layer rotated rot degrees clockwise about its centre, centred at (cx, cy)."""
	layer = layer.rotate(-rot, resample=Image.BICUBIC, expand=True)
	canvas.alpha_composite(layer, (round(cx - layer.width / 2), round(cy - layer.height / 2)))


def compose_tough(jacket):
	"""Tough Enough's background from the jacket picture (a PIL image); None
	if its logos aren't where they are on the jacket this was made for."""
	from logo_matte import cut_matte
	stamp, script = cut_matte(jacket, **TE_STAMP), cut_matte(jacket, **TE_SCRIPT)
	if stamp is None or script is None:
		return None
	art = lambda n: Image.open(os.path.join(HERE, 'art', 'BgToughEnough-%s.png' % n)).convert('RGBA')
	rgb = np.asarray(art('under').convert('RGB')).astype(float)

	# the photo as a four-tone screen print, a dot screen on it, faded at the edges
	cx_, cy_, cw, ch = TE_CROP
	photo = jacket.convert('RGB')
	yy, xx = np.mgrid[0:H, 0:W].astype(float)
	px, py = cx_ + (xx + 0.5) * cw / W, cy_ + (yy + 0.5) * ch / H
	inside = (px >= 0) & (py >= 0) & (px < photo.width) & (py < photo.height)
	big = np.asarray(photo.transform((W, H), Image.AFFINE, (cw / W, 0, cx_, 0, ch / H, cy_), Image.BILINEAR)).astype(float) / 255
	lum = big @ [0.2126, 0.7152, 0.0722]
	lum = np.clip(lum * 1.55 - 0.22, 0, 1)
	k = np.minimum((lum * 4).astype(int), 3)
	tones = np.array([[0.08, 0.02, 0.04], [0.30, 0.00, 0.07], [0.80, 0.07, 0.19], [1.00, 0.86, 0.76]])
	print_rgb = tones[k] * 255
	dx, dy = (xx % 12) + 0.5 - 6, (yy % 12) + 0.5 - 6
	dot = np.clip((0.34 - np.hypot(dx, dy) / (12 * math.sqrt(2) / 2)) / 0.12, 0, 1)
	print_rgb = print_rgb * (1 - 0.35 * dot)[..., None]
	fade = np.clip((1 - np.hypot((xx + 0.5 - 0.66 * W) / (0.70 * W), (yy + 0.5 - 0.46 * H) / (0.85 * H))) / 0.45, 0, 1)
	left = max(0.0, -cx_ / cw * W)
	side = np.clip((xx + 0.5 - (left + 10)) / 210, 0, 1)
	a = (fade * side * inside)[..., None]
	rgb = rgb * (1 - a) + print_rgb * a

	# the spotlight rays, by screen
	rays = np.asarray(art('rays')).astype(float) / 255
	ra = rays[..., 3:4]
	rgb = rgb + ra * rays[..., :3] * 255 * (1 - rgb / 255)

	canvas = Image.fromarray(np.clip(rgb + 0.5, 0, 255).astype(np.uint8), 'RGB').convert('RGBA')
	canvas.alpha_composite(art('over'))

	# the stamp, red, on its label (in 'over'), 520x292 at the starburst
	sw, sh = 520, 292
	put_rotated(canvas, filled(stamp, sw, sh, gradient(sw, sh, 170, [(0, '#ff3a2e'), (0.5, '#d80c1a'), (1, '#a8000c')])), 390, 258, -7)

	# the script, silver with a dark outline and a red shadow, over the ribbon
	sw, sh, m = 820, 190, 24
	fill = filled(script, sw, sh, gradient(sw, sh, 180, [(0, '#ffffff'), (0.45, '#f2f4ff'), (0.70, '#b9c2dc'), (1, '#ffffff')]))
	local = Image.new('RGBA', (sw + 2 * m, sh + 2 * m), (0, 0, 0, 0))
	local.alpha_composite(fill, (m, m))
	ring = local.getchannel('A').filter(ImageFilter.MaxFilter(13))
	edged = Image.new('RGBA', local.size, (0x14, 0x04, 0x0a, 0))
	edged.putalpha(ring)
	edged.alpha_composite(local)
	shadow = Image.new('RGBA', local.size, (0xc8, 0x10, 0x2e, 0))
	shadow.putalpha(edged.getchannel('A'))
	out = Image.new('RGBA', (local.width + 8, local.height + 8), (0, 0, 0, 0))
	out.alpha_composite(shadow, (6, 7))
	out.alpha_composite(edged, (0, 0))
	# out's centre is the box centre shifted by the shadow's growth (+4, +4)
	put_rotated(canvas, out, 430 + 4, 540 + 4, -6)

	rgb = np.asarray(canvas.convert('RGB')).astype(float)
	rgb = vignette(rgb, 0.85, 0.60, 'rgba(10,0,4,0.65)')
	return Image.fromarray(np.clip(rgb + 0.5, 0, 255).astype(np.uint8), 'RGB')
