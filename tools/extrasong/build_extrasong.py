#!/usr/bin/env python3
"""Build a local mod package that adds StepMania simfiles to Dancing Stage
Fusion's own song list (port/core/dsfcore_extrasong.cpp does the in-RAM
part).

  build_extrasong.py SONGS.toml DISC.cue OUT_PACKAGE_DIR

SONGS.toml lists the songs in song-table order (the first is song 21, and
a song keeps its save slot only while it keeps its place):

  [[song]]
  dir = "Tough Enough"          # relative to SONGS.toml
  master_id = 0x8030            # a free master-table id
  background = "bg.png"         # optional, instead of NAME-bg.png
  banner = "banner.png"         # optional, instead of NAME.png
  title = "..."                 # optional plate/list title (default #TITLE)
  artist = "..."                # optional (default #ARTIST)
  plate = "plate.png"           # optional plate image instead of the lettering
  single = [3, 5, 8, 10]        # optional ratings: Beginner, Light, Standard, Heavy
  double = [5, 8, 10]           # optional ratings: Light, Standard, Heavy
  unlock = { song = 0x8039, difficulties = ["Standard", "Heavy"] }
                                # optional: listed once that song is cleared there
  outro = 2                     # optional: measures of play after the last step
  script = 12                   # optional: the stock song (group 0..19) whose background
                                # animation script is fitted to this song (default ORION.78)

  [[stock]]                     # a new gameplay background for one of the 20
  index = 17                    # song-table index (17 = Mickey)
  background = "bg.png"

A song directory holds NAME.sm, NAME.ogg (or .mp3/.wav), NAME.png (256x80
banner) and NAME-bg.png (320x240 background).  Everything is converted to
the game's own formats.  The new disc sectors go into one file the plugin
appends after the end of the player's own disc image; the output is
personal (it contains the songs and data from the disc), never part of
this project.

Formats (notes/dsf-extra-songs.md):
* READ_DT song entry: PACK[4] = { PACK{tempo map, charts}, event list,
  LZ(background TIM 320x240 8bpp), language UI set (copied) }.
* Chart: u32 size, u32 kind (style | difficulty<<8 | 1<<16), 14 x 0, then
  one u32 per beat, one byte per 16th, one bit per panel; beat 0 is
  played at measure 3 of the tempo map (measured in the game: at 196 and
  at 98 BPM the steps came exactly one measure late with beat 0 at 2).
* Tempo map: u32 size, then (u32 pos in 1/4096 measure, u32 time in
  1/75 s) points the game interpolates between; a stop is two points at
  one position (as on the stock songs).
* Stream (STR.BIN layout): SPU ADPCM, 0x4000-byte blocks alternating L/R,
  ending in a pair of silent flag-1 blocks (psxaudio.c).
* Preview: XA ADPCM 4-bit stereo 37.8 kHz, eight channels interleaved
  sector by sector, as in XA.STR (psxaudio.c).
* Graphics the menus decode from their packs (banners, plates) are 16-byte
  markers there; the pictures themselves go to gfx.bin, which the plugin
  serves through the game's LZ decoder.
"""
import argparse, hashlib, math, os, re, shutil, struct, subprocess, sys, tempfile, tomllib
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import konami_lz
import plate_font

# ---- the disc (SLES-04163, Redump) ----------------------------------------
EXE_LBA, EXE_SIZE = 24, 636928
EXE_SHA1 = 'ade130f4ab7cf5291e6a1606cb8d2fd51a01b1da'
EXE_VBASE = 0x80019800			# vaddr = file offset + this
TOC, TOC_N = 0x80092B50, 235
SONGTAB, SONGS = 0x800AFE38, 20
MASTER, MASTER_N = 0x800A3EA0, 0x252
PREVIEW_IDS = 50			# music ids: stock streams 1-23, stock previews 25-49
STREAM_IDS = PREVIEW_IDS + 12		# extra streams after the extra previews (the plugin
					# makes the driver take these ids as streams, 0x8002C398)
LANG_ENTRIES = 5			# en, fr, de, it, es copies
MUSIC_PACKS = range(201, 206)		# music select: items 10-29 banners, 30-49 plates
RESULT_PACKS = range(206, 211)		# results: items 17-36 plates
RECORD_PACKS = range(28, 33)		# records: slot 1 = SONGTAB {id, mini, plate}
WORKOUT_PACK = 181			# workout results: items 1-20 banners 176x55, slot 21 = 0
UI_DONOR = 14				# song group whose language UI sets are copied (AM-3P)
EVENT_DONOR = 12			# default script donor (songs.toml script = group): ORION.78
MAX_SONGS = 32				# the save keeps records for 32 songs
DIFFS = ('Beginner', 'Light', 'Standard', 'Heavy')
SM_NAME = {'Beginner': 'Beginner', 'Light': 'Easy', 'Standard': 'Medium', 'Heavy': 'Hard'}
MARKER = b'DSFX'


class Disc:
	def __init__(self, cue):
		text = open(cue).read()
		m = re.search(r'FILE\s+"([^"]+)"', text)
		self.bin = os.path.join(os.path.dirname(os.path.abspath(cue)), m.group(1))
		self.f = open(self.bin, 'rb')
		self.sectors = os.path.getsize(self.bin) // 2352

	def raw(self, lba):
		self.f.seek(lba * 2352)
		return self.f.read(2352)

	def user(self, lba, n=1):
		return b''.join(self.raw(l)[24:24 + 2048] for l in range(lba, lba + n))

	def data(self, lba, size):
		return self.user(lba, (size + 2047) // 2048)[:size]


class Exe:
	def __init__(self, disc):
		self.b = disc.data(EXE_LBA, EXE_SIZE)
		if hashlib.sha1(self.b).hexdigest() != EXE_SHA1:
			sys.exit('this is not the supported disc (SLES-04163 executable differs)')

	def w(self, va):
		return struct.unpack_from('<I', self.b, va - EXE_VBASE)[0]


# ---- containers -------------------------------------------------------------
def pack_items(b):
	n = struct.unpack_from('<I', b, 0)[0] // 4
	offs = list(struct.unpack_from('<%dI' % n, b, 0))
	out = []
	for s in offs:
		if not s:
			out.append(None)
			continue
		e = min([x for x in offs if x > s] + [len(b)])
		out.append(b[s:e])
	return out


def build_pack(items):
	"""PACK: u32 offsets (0 = empty slot), items 4-aligned after the table."""
	n = len(items)
	offs, body, pos = [], b'', 4 * n
	for it in items:
		if it is None:
			offs.append(0)
			continue
		it = it + b'\0' * (-len(it) % 4)
		offs.append(pos)
		body += it
		pos += len(it)
	return struct.pack('<%dI' % n, *offs) + body


class Blobs:
	"""Decoded pictures the plugin serves for markers (gfx.bin)."""
	def __init__(self):
		self.data, self.index = b'', []

	def marker(self, tim):
		k = len(self.index)
		self.index.append((len(self.data), len(tim)))
		self.data += tim + b'\0' * (-len(tim) % 4)
		return MARKER + struct.pack('<III', k, k ^ 0xFFFFFFFF, len(tim))


# ---- TIM ----------------------------------------------------------------------
def rgb15(r, g, b, stp=False):
	v = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
	if stp:
		v |= 0x8000
	return v


def make_tim(indices, w, h, bpp, clut, clut_wide):
	"""TIM with CLUT at (0,0) and image at (0,0): the game places them."""
	flags = 0x08 | (0 if bpp == 4 else 1)
	pal = struct.pack('<%dH' % clut_wide, *(list(clut) + [0] * (clut_wide - len(clut))))
	cl = struct.pack('<IHHHH', 12 + len(pal), 0, 0, clut_wide, 1) + pal
	if bpp == 4:
		px = bytes((indices[i] & 15) | ((indices[i + 1] & 15) << 4) for i in range(0, w * h, 2))
		hw = w // 4
	else:
		px = bytes(indices)
		hw = w // 2
	im = struct.pack('<IHHHH', 12 + len(px), 0, 0, hw, h) + px
	return struct.pack('<II', 0x10, flags) + cl + im


def quantize(img, colours):
	q = img.convert('RGB').quantize(colours, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
	pal = q.getpalette()[:colours * 3]
	pal += [0] * (colours * 3 - len(pal))
	return list(q.tobytes()), [tuple(pal[3 * i:3 * i + 3]) for i in range(colours)]


def banner_tim(img):
	"""256x80 4bpp, 16 colours, no 0x0000 (the game's banners keep it free)."""
	idx, pal = quantize(img.resize((256, 80), Image.LANCZOS), 16)
	clut = [max(1, rgb15(*c)) for c in pal]
	return make_tim(idx, 256, 80, 4, clut, 16)


def workout_tim(img):
	"""176x55 4bpp workout-results banner: as the music select's, 16
	colours, no 0x0000 and no STP bit."""
	idx, pal = quantize(img.resize((176, 55), Image.LANCZOS), 16)
	clut = [max(1, rgb15(*c)) for c in pal]
	return make_tim(idx, 176, 55, 4, clut, 16)


def mini_tim(img):
	"""88x28 4bpp records-list banner, colours with the STP bit as on disc."""
	idx, pal = quantize(img.resize((88, 28), Image.LANCZOS), 16)
	return make_tim(idx, 88, 28, 4, [rgb15(*c, stp=True) for c in pal], 16)


def background_tim(img):
	idx, pal = quantize(img.resize((320, 240), Image.LANCZOS), 256)
	return make_tim(idx, 320, 240, 8, [rgb15(*c, stp=True) for c in pal], 256)


def plate_tim(plate_png, stock_plate_tim):
	"""360x40 4bpp title plate on the stock plates' own palette: index 0 the
	background key, then the navy outline-to-white ramp (by brightness)."""
	t = stock_plate_tim
	clut_wide = struct.unpack_from('<IHHHH', t, 8)[3]
	clut = list(struct.unpack_from('<%dH' % clut_wide, t, 20))
	def lum(c):
		return ((c & 31) * 3 + ((c >> 5) & 31) * 6 + ((c >> 10) & 31)) / 10
	ramp = sorted(range(1, 16), key=lambda i: lum(clut[i]))
	im = Image.open(plate_png).convert('RGBA').resize((360, 40), Image.LANCZOS)
	idx = []
	for r, g, b, a in im.getdata():
		if a < 96:
			idx.append(0)
		else:
			l = (r * 3 + g * 6 + b) / 10 / 255
			idx.append(ramp[min(14, int(l * 15))])
	return make_tim(idx, 360, 40, 4, clut, clut_wide)


def lettered_plate_tim(indices, stock_plate_tim):
	"""Plate indices from plate_font on the stock plates' own palette."""
	t = stock_plate_tim
	clut_wide = struct.unpack_from('<IHHHH', t, 8)[3]
	clut = list(struct.unpack_from('<%dH' % clut_wide, t, 20))
	return make_tim(indices, 360, 40, 4, clut, clut_wide)


# ---- simfile ----------------------------------------------------------------
def parse_sm(path):
	s = open(path, encoding='utf-8', errors='replace').read()
	s = re.sub(r'//[^\n]*', '', s)
	tags = {}
	for m in re.finditer(r'#([A-Z]+):([^;]*);', s):
		tags.setdefault(m.group(1), []).append(m.group(2))
	def pairs(tag):
		out = []
		for p in tags.get(tag, [''])[0].split(','):
			if p.strip():
				b, v = p.split('=')[:2]
				out.append((float(b), float(v)))
		return sorted(out)
	charts = []
	for n in tags.get('NOTES', []):
		f = [x.strip() for x in n.split(':')]
		style, diff, meter, notes = f[0], f[2], int(f[3] or 0), f[5]
		measures = [[r.strip() for r in m.split('\n') if r.strip()] for m in notes.split(',')]
		charts.append(dict(style=style, diff=diff, meter=meter, measures=measures))
	bpms = pairs('BPMS')
	if not bpms or bpms[0][0] > 0:
		sys.exit('%s: no tempo at beat 0' % path)
	return dict(title=tags.get('TITLE', [''])[0].strip(), artist=tags.get('ARTIST', [''])[0].strip(),
	            offset=float(tags.get('OFFSET', ['0'])[0] or 0), bpms=bpms,
	            stops=[s for s in pairs('STOPS') if s[1] > 0],
	            sample_start=float(tags.get('SAMPLESTART', ['0'])[0] or 0),
	            sample_length=float(tags.get('SAMPLELENGTH', ['15'])[0] or 15), charts=charts)


class Timing:
	"""Simfile time (seconds; beat 0 at -OFFSET) of a beat, with the tempo
	changes and stops; before beat 0 the first tempo continues.  A stop at
	a beat delays everything after that beat."""
	def __init__(self, sm):
		self.offset, self.bpms, self.stops = sm['offset'], sm['bpms'], sm['stops']

	def time(self, beat, after_stop=False):
		t = -self.offset
		if beat < 0:
			return t + beat * 60.0 / self.bpms[0][1]
		for i, (b, bpm) in enumerate(self.bpms):
			end = self.bpms[i + 1][0] if i + 1 < len(self.bpms) else math.inf
			if beat <= b:
				break
			t += (min(beat, end) - b) * 60.0 / bpm
		for b, d in self.stops:
			if b < beat or (after_stop and b == beat):
				t += d
		return t


SM_DIFF = {'Beginner': 4, 'Easy': 0, 'Medium': 1, 'Hard': 2}
# Panel bits (single: low nibble, mirrored into the high one; double: P1
# low, P2 high).  Left/Right are bits 0/1 (the most-used double panels are
# P1-Right and P2-Left); Up/Down bits 2/3 come from in-game observation.
PANEL_BITS = {0: 0, 1: 3, 2: 2, 3: 1}	# simfile column (L, D, U, R) -> bit


CHART_START = 3		# tempo-map measure of the chart's first beat
OUTRO = 2		# measures of play after a song's last step (songs.toml: outro)

def chart_words(chart, lead_measures, last_measure, report):
	"""The chart on the game's 16th-note grid; a step between 16ths moves
	to the nearest one (reported)."""
	beats = 4 * last_measure - 8	# the stock charts' length
	words = [0] * beats
	cols = 4 if chart['style'] == 'dance-single' else 8
	moved = 0
	for m, rows in enumerate(chart['measures']):
		n = len(rows)
		for r, row in enumerate(rows):
			mask = 0
			for c in range(cols):
				if row[c] in '124':	# taps, hold and roll heads; tails and mines dropped
					mask |= 1 << (PANEL_BITS[c % 4] + (4 if c >= 4 else 0))
			if not mask:
				continue
			if (r * 16) % n:
				moved += 1
			if cols == 4:
				mask |= mask << 4
			sixteenth = (m + lead_measures - CHART_START) * 16 + int(round(r * 16.0 / n))
			if sixteenth < 0 or sixteenth // 4 >= beats:
				sys.exit('a step outside the song (measure %d)' % m)
			words[sixteenth // 4] |= mask << (8 * (sixteenth % 4))
	if moved:
		report.append('%s %s: %d steps moved to the 16th-note grid' % (chart['style'], chart['diff'], moved))
	kind = (0 if cols == 4 else 2) | (SM_DIFF[chart['diff']] << 8) | (1 << 16)
	return struct.pack('<II14I', 64 + 4 * beats, kind, *([0] * 14)) + struct.pack('<%dI' % beats, *words)


# ---- background animation scripts (song entry slot 1) -------------------------
# u32 size, u16 5, u16 k, u32 n, n x s32 cue position (1/4096 measure; the
# first is -99999, the second 0), n x 8 words: what the stage does from that
# cue on (background layers, camera, effects).  Only the positions are time.

def parse_events(ev):
	w = struct.unpack_from('<%dI' % (len(ev) // 4), ev, 0)
	n = w[2]
	if w[0] != len(ev) or len(ev) != 12 + 36 * n:
		sys.exit('an event list that is not 12 + 36 * cues bytes')
	pos = [struct.unpack('<i', struct.pack('<I', x))[0] for x in w[3:3 + n]]
	recs = [w[3 + n + 8 * i:3 + n + 8 * i + 8] for i in range(n)]
	return w[1], pos, recs


def build_events(head, pos, recs):
	n = len(pos)
	return struct.pack('<III', 12 + 36 * n, head, n) + struct.pack('<%di' % n, *pos) + \
		b''.join(struct.pack('<8I', *r) for r in recs)


def stock_density(slot0):
	"""Steps per tempo-map measure of a stock song (its single Standard chart,
	else its busiest single chart), and its last tempo-map measure."""
	items = pack_items(slot0)
	tm = items[0]
	npts = (struct.unpack_from('<I', tm, 0)[0] - 4) // 8
	last = struct.unpack_from('<I', tm, 4 + 8 * (npts - 1))[0] / 4096.0
	best = None
	for c in items[1:]:
		kind = struct.unpack_from('<I', c, 4)[0]
		if kind & 0xFF:
			continue
		words = struct.unpack_from('<%dI' % ((len(c) - 64) // 4), c, 64)
		d = [0] * (int(last) + 2)
		for i, wd in enumerate(words):
			M = CHART_START + i // 4
			if M < len(d):
				d[M] += sum(1 for q in range(4) if (wd >> (8 * q)) & 0xFF)
		key = (((kind >> 8) & 0xFF) == SM_DIFF['Medium'], sum(d))
		if best is None or key > best[0]:
			best = (key, d)
	return best[1], last


def structure(d, last):
	"""Anchors of a song's shape from its steps per measure: where the steps
	start and end, and the breaks between (2+ measures well below the song's
	usual density), as (start, end) pairs."""
	busy = [M for M, v in enumerate(d) if v]
	if not busy:
		return 0.0, float(last), []
	first, end = busy[0], busy[-1] + 1
	nz = sorted(v for v in d[first:end] if v)
	low = 0.4 * nz[len(nz) // 2]
	breaks, M = [], first
	while M < end:
		if d[M] <= low:
			k = M
			while k < end and d[k] <= low:
				k += 1
			if k - M >= 2:
				breaks.append((M, k))
			M = k
		else:
			M += 1
	return float(first), float(end), breaks


def anchor_map(d_dens, d_last, n_dens, n_last):
	"""Piecewise-linear map from donor measures to new-song measures through
	matching anchors: the steps' start and end, and breaks paired in order
	by their place in the song (within 15% of its length)."""
	df, de, db = structure(d_dens, d_last)
	nf, ne, nb = structure(n_dens, n_last)
	rel = lambda x, f, e: (x - f) / max(e - f, 1.0)
	pts, j = [(0.0, 0.0), (df, nf)], 0
	for a0, a1 in db:
		best = None
		for k in range(j, len(nb)):
			gap = abs(rel(a0, df, de) - rel(nb[k][0], nf, ne))
			if gap < 0.15 and (best is None or gap < best[0]):
				best = (gap, k)
		if best:
			k = best[1]
			pts += [(float(a0), float(nb[k][0])), (float(a1), float(nb[k][1]))]
			j = k + 1
	pts += [(de, ne), (max(d_last, de + 1), max(n_last, ne + 1))]
	xs, ys = [], []
	for x, y in sorted(pts):
		if (not xs or (x > xs[-1] and y > ys[-1])):
			xs.append(x); ys.append(y)
	def f(p):
		if p <= xs[0]:
			return ys[0]
		for i in range(1, len(xs)):
			if p <= xs[i]:
				return ys[i - 1] + (p - xs[i - 1]) * (ys[i] - ys[i - 1]) / (xs[i] - xs[i - 1])
		return ys[-1] + (p - xs[-1]) * (ys[-1] - ys[-2]) / (xs[-1] - xs[-2])
	return f, list(zip(xs, ys))


def retime_events(ev, donor_slot0, dens, last):
	"""A stock song's script on another song's structure: each cue moves to
	the same place in the new song's shape (anchor_map), snapped to its
	beats."""
	head, pos, recs = parse_events(ev)
	d_dens, d_last = stock_density(donor_slot0)
	f, _ = anchor_map(d_dens[:int(d_last) + 1], d_last, dens[:int(last) + 1], last)
	out_pos, out_recs, prev = pos[:2], recs[:2], 0.0
	for p, r in zip(pos[2:], recs[2:]):
		x = p / 4096.0
		grid = 1.0 if x == int(x) else 0.25
		y = round(f(x) / grid) * grid
		y = max(y, prev + (0.5 if grid == 1.0 else 0.25))	# two cues met: keep them apart
		if y > last - 0.25:
			y = last - 0.25
			if y <= prev:
				continue
		out_pos.append(int(round(y * 4096))); out_recs.append(r); prev = y
	return build_events(head, out_pos, out_recs), len(pos) - len(out_pos)


def tempo_map(lead, last, timing, pad):
	"""Points (measure, time): tempo-map measure M is simfile beat 4(M - lead),
	played pad seconds later (the audio is padded with the lead-in)."""
	T = lambda M, after=False: timing.time(4.0 * (M - lead), after) + pad
	pts = {0.0, 1.0, float(last)}
	for b, _ in timing.bpms[1:]:
		pts.add(b / 4.0 + lead)
	stops = {b / 4.0 + lead for b, _ in timing.stops}
	out = []
	for M in sorted(pts | stops):
		if M > last:
			continue
		out.append((M, T(M)))
		if M in stops:
			out.append((M, T(M, True)))
	# time: 31-bit signed (bit 31 is a flag the game's maps sometimes set)
	pairs = b''.join(struct.pack('<II', int(round(4096 * M)), int(round(t * 75)) & 0x7FFFFFFF) for M, t in out)
	return struct.pack('<I', 4 + len(pairs)) + pairs


# ---- audio ------------------------------------------------------------------
def tool(tmp):
	exe = os.path.join(tmp, 'psxaudio')
	subprocess.check_call(['cc', '-O2', '-o', exe, os.path.join(HERE, 'psxaudio.c')])
	return exe


def pcm(src, rate, out, start=None, length=None, pad_front=0.0, fade=0.0):
	af = []
	if pad_front > 0:
		af.append('adelay=%d|%d' % (int(pad_front * 1000), int(pad_front * 1000)))
	if fade > 0 and length:
		af.append('afade=t=in:d=%g,afade=t=out:st=%g:d=%g' % (fade, length - fade, fade))
	cmd = ['ffmpeg', '-loglevel', 'error', '-y']
	if start is not None:
		cmd += ['-ss', '%g' % start]
	if length:
		cmd += ['-t', '%g' % length]
	cmd += ['-i', src, '-ac', '2', '-ar', str(rate)]
	if af:
		cmd += ['-af', ','.join(af)]
	cmd += ['-f', 's16le', out]
	subprocess.check_call(cmd)


# Media sectors: Mode 2 without the sync/header, 2336 bytes from the
# duplicated subheader (psx_mod_append_disc_extent).
def data_sectors(blob):
	n = (len(blob) + 2047) // 2048
	blob += b'\0' * (n * 2048 - len(blob))
	sub = bytes((0, 0, 0x08, 0)) * 2
	return b''.join(sub + blob[2048 * i:2048 * (i + 1)] + b'\0' * 280 for i in range(n))


def xa_sector(sub, data):
	return bytes(sub) * 2 + data + b'\0' * (2324 - len(data)) + b'\0' * 4


# ---- one song -----------------------------------------------------------------
class Song:
	def __init__(self, cfg, root):
		self.cfg = cfg
		self.dir = os.path.join(root, cfg['dir'])
		names = [f for f in os.listdir(self.dir) if f.lower().endswith('.sm')]
		if len(names) != 1:
			sys.exit('%s: expected one .sm file' % self.dir)
		self.base = os.path.join(self.dir, names[0][:-3])
		self.sm = parse_sm(self.base + '.sm')
		self.audio = next((self.base + e for e in ('.ogg', '.mp3', '.wav') if os.path.exists(self.base + e)), None)
		if not self.audio:
			sys.exit('%s: no audio file' % self.dir)
		self.id = cfg['master_id']
		self.title = cfg.get('title', self.sm['title'])
		self.artist = cfg.get('artist', self.sm['artist'])
		self.path = lambda k: os.path.join(root, cfg[k])

	def chart(self, style, diff):
		return next((c for c in self.sm['charts'] if c['style'] == style and c['diff'] == SM_NAME[diff]), None)

	def ratings(self, style):
		"""Nibbles as the song table holds them: Light, Standard, Heavy,
		Beginner (single only)."""
		names = DIFFS if style == 'dance-single' else DIFFS[1:]
		given = self.cfg.get('single' if style == 'dance-single' else 'double')
		lv = {}
		for i, d in enumerate(names):
			c = self.chart(style, d)
			if not c:
				continue
			lv[d] = given[i] if given else c['meter']
			if not 1 <= lv[d] <= 10:
				sys.exit('%s: %s %s rating %d is outside 1..10' % (self.title, style, d, lv[d]))
		return lv.get('Light', 0) | lv.get('Standard', 0) << 4 | lv.get('Heavy', 0) << 8 | lv.get('Beginner', 0) << 12

	def build(self, entry, psxaudio, tmp, font, stock_plate, blobs):
		sm, report = self.sm, []
		timing = Timing(sm)
		dur = float(subprocess.check_output(['ffprobe', '-v', 'error', '-show_entries', 'format=duration',
		                                     '-of', 'default=nw=1:nk=1', self.audio]))
		bpm0 = sm['bpms'][0][1]
		first = min(m for c in sm['charts'] for m, rows in enumerate(c['measures'])
		            if any(any(x in '124' for x in r) for r in rows))
		lead = max(0, CHART_START - first)
		pad = lead * 4 * 60.0 / bpm0
		# the last whole measure inside the (padded) music
		last = lead
		while timing.time(4.0 * (last + 1 - lead), True) + pad <= dur + pad:
			last += 1
		self.last, self.lead = last, lead
		# Play ends OUTRO measures after the last step, as on the stock songs
		# (0.5 to 5 after theirs), so the rest of the music plays under CLEARED;
		# never past the music's end.
		last_step = max(m + lead + r / float(len(rows)) for c in sm['charts']
		                for m, rows in enumerate(c['measures'])
		                for r, row in enumerate(rows) if any(x in '124' for x in row))
		self.length = min(last + 1, int(math.ceil(last_step + self.cfg.get('outro', OUTRO))))
		# ---- song entry (5 language copies) ----
		charts = []
		for style, diffs in (('dance-single', ('Light', 'Standard', 'Heavy', 'Beginner')),
		                     ('dance-double', ('Light', 'Standard', 'Heavy'))):
			for d in diffs:
				c = self.chart(style, d)
				if c:
					charts.append(chart_words(c, lead, last, report))
		slot0 = build_pack([tempo_map(lead, last, timing, pad)] + charts)
		# the background animation script: a stock song's, on this song's structure
		donor = self.cfg.get('script', EVENT_DONOR)
		if not 0 <= donor < SONGS:
			sys.exit('%s: script %d is not one of the 20 songs' % (self.title, donor))
		dens = [0] * (last + 2)
		ref = self.chart('dance-single', 'Standard') or max((c for c in sm['charts'] if c['style'] == 'dance-single'),
		                                                     key=lambda c: c['meter'])
		for m, rows in enumerate(ref['measures']):
			if m + lead < len(dens):
				dens[m + lead] += sum(1 for row in rows if any(x in '124' for x in row[:4]))
		donor_entry = pack_items(entry(68 + 5 * donor))
		events, dropped = retime_events(donor_entry[1], donor_entry[0], dens, float(self.length))
		self.script = (donor, len(parse_events(events)[1]), dropped)
		bg_png = self.path('background') if 'background' in self.cfg else self.base + '-bg.png'
		bg = konami_lz.encode(background_tim(Image.open(bg_png)))
		self.entries = [build_pack([slot0, events, bg, pack_items(entry(68 + 5 * UI_DONOR + k))[3]])
		                for k in range(LANG_ENTRIES)]
		# ---- graphics ----
		banner_img = Image.open(self.path('banner') if 'banner' in self.cfg else self.base + '.png')
		if 'plate' in self.cfg:
			plate = plate_tim(self.path('plate'), stock_plate)
		else:
			lost = font.missing(self.title + self.artist)
			if lost:
				sys.exit('%s: the game\'s lettering has no %s: change title/artist or give a plate'
				         % (self.title, ' '.join(lost)))
			plate = lettered_plate_tim(font.plate(self.title, self.artist), stock_plate)
		self.banner = blobs.marker(banner_tim(banner_img))
		self.plate = blobs.marker(plate)
		self.mini = blobs.marker(mini_tim(banner_img))
		self.workout_index = len(blobs.index)
		self.workout = blobs.marker(workout_tim(banner_img))
		# ---- audio ----
		raw = os.path.join(tmp, 'song.s16')
		pcm(self.audio, 44100, raw, pad_front=pad)
		stream = os.path.join(tmp, 'song.str')
		subprocess.check_call([psxaudio, 'spu', raw, stream])
		self.stream = open(stream, 'rb').read()
		prev = os.path.join(tmp, 'prev.s16')
		pcm(self.audio, 37800, prev, start=sm['sample_start'], length=sm['sample_length'], fade=1.0)
		xa = os.path.join(tmp, 'prev.xa')
		subprocess.check_call([psxaudio, 'xa', prev, xa])
		self.xa = open(xa, 'rb').read()
		bpm_text = '/'.join('%g' % b for _, b in sm['bpms'][:4]) + ('...' if len(sm['bpms']) > 4 else '')
		print('%-34s %3d measures, play to %d (lead %d), %d charts, BPM %s%s' % (
		      self.title[:34], last, self.length, lead, len(charts), bpm_text,
		      ', %d stops' % len(sm['stops']) if sm['stops'] else ''))
		print('    background script from stock song %d, %d cues%s' % (
		      self.script[0], self.script[1], ', %d past the end dropped' % self.script[2] if self.script[2] else ''))
		for r in report:
			print('    ' + r)


# ---- main ---------------------------------------------------------------------
def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('songs'); ap.add_argument('cue'); ap.add_argument('out')
	a = ap.parse_args()
	cfg = tomllib.load(open(a.songs, 'rb'))
	root = os.path.dirname(os.path.abspath(a.songs))
	songs = [Song(c, root) for c in cfg['song']]
	if SONGS + len(songs) > MAX_SONGS:
		sys.exit('the game holds %d songs: at most %d extra' % (MAX_SONGS, MAX_SONGS - SONGS))

	disc = Disc(a.cue)
	exe = Exe(disc)
	toc = [(exe.w(TOC + 8 * i), exe.w(TOC + 8 * i + 4)) for i in range(TOC_N)]
	taken = {exe.w(MASTER + 20 * i) for i in range(MASTER_N) if exe.w(MASTER + 20 * i + 4)}
	blank = sum(1 for i in range(MASTER_N) if not exe.w(MASTER + 20 * i + 4))
	ids = [s.id for s in songs]
	if len(set(ids)) != len(ids) or taken & set(ids):
		sys.exit('song ids must be unique and free in the master table')
	if blank < len(songs):
		sys.exit('the master table has only %d blank slots' % blank)
	for s in songs:
		u = s.cfg.get('unlock')
		if u and (u['song'] not in ids or not set(u['difficulties']) <= set(DIFFS)):
			sys.exit('%s: unlock names an unknown song or difficulty' % s.title)
	entry = lambda i: disc.data(toc[i][1], toc[i][0])

	tmp = tempfile.mkdtemp()
	blobs = Blobs()
	try:
		psxaudio = tool(tmp)
		font = plate_font.PlateFont(disc, exe, toc, pack_items)
		stock_plate = konami_lz.decode(pack_items(entry(201))[30])
		for s in songs:
			s.build(entry, psxaudio, tmp, font, stock_plate, blobs)
	finally:
		shutil.rmtree(tmp)

	# ---- the menus' packs: markers after the stock items ----
	packs = {}
	it = pack_items(entry(WORKOUT_PACK))
	assert len(it) == 22 and it[21] is None
	packs[WORKOUT_PACK] = build_pack(it[:21] + [s.workout for s in songs] + [None])
	for e in MUSIC_PACKS:
		it = pack_items(entry(e))
		assert len(it) == 51 and it[50] is None
		packs[e] = build_pack(it[:50] + [x for s in songs for x in (s.banner, s.plate)] + [None])
	for e in RESULT_PACKS:
		it = pack_items(entry(e))
		assert len(it) == 38 and it[37] is None
		packs[e] = build_pack(it[:37] + [s.plate for s in songs] + [None])
	for e in RECORD_PACKS:
		it = pack_items(entry(e))
		st = it[1]
		recs = [struct.unpack_from('<3I', st, 12 * i) for i in range(SONGS)]
		assert struct.unpack_from('<3I', st, 12 * SONGS) == (0, 0, 0)
		# re-base: one record per song plus the terminator, data after it
		old_at, new_at = 12 * (SONGS + 1), 12 * (SONGS + len(songs) + 1)
		data = st[old_at:]
		data += b'\0' * (-len(data) % 4)
		table = b''.join(struct.pack('<3I', i, x - old_at + new_at, y - old_at + new_at) for i, x, y in recs)
		for s in songs:
			mini_at = new_at + len(data)
			data += s.mini
			table += struct.pack('<3I', s.id, mini_at, mini_at + len(s.mini))
			data += s.plate
		packs[e] = build_pack([it[0], table + b'\0' * 12 + data] + it[2:])
	# stock songs' new backgrounds: their language entries (the TOC entries
	# their song record's group points to) with item 2 replaced
	for st in cfg.get('stock', []):
		k = st['index']
		if not 0 <= k < SONGS:
			sys.exit('stock index %d is not one of the 20 songs' % k)
		group = exe.w(SONGTAB + 44 * k + 36)
		bg = konami_lz.encode(background_tim(Image.open(os.path.join(root, st['background']))))
		for p in sorted({exe.w(group + 4 * l) for l in range(6)}):
			if not (TOC <= p < TOC + 8 * TOC_N and (p - TOC) % 8 == 0):
				sys.exit('stock song %d: its entries are not in the TOC' % k)
			e = (p - TOC) // 8
			it = pack_items(entry(e))
			assert len(it) == 4
			packs[e] = build_pack(it[:2] + [bg, it[3]])
		print('stock song %d: new background in %d language entries' % (k, len({exe.w(group + 4 * l) for l in range(6)})))

	# ---- media: the sectors appended after the disc ----
	base = disc.sectors
	media, lba = [], base
	def put(sectors):
		nonlocal lba
		at = lba
		media.append(sectors)
		lba += len(sectors) // 2336
		return at
	for s in songs:
		s.entry_lbas = [(len(b), put(data_sectors(b))) for b in s.entries]
	toc_new = {e: (len(b), put(data_sectors(b))) for e, b in sorted(packs.items())}
	for s in songs:
		s.stream_lba = put(data_sectors(s.stream) + data_sectors(b'\0' * 2048 * 10))
	# previews: eight to a block, channel c on every eighth sector from c
	for blk in range(0, len(songs), 8):
		group = songs[blk:blk + 8]
		counts = [len(s.xa) // 2304 for s in group]
		start, file_no = lba, 1 + blk // 8
		sectors = []
		for i in range(8 * max(counts)):
			c, k = i % 8, i // 8
			if c < len(group) and k < counts[c]:
				sub = (file_no, c, 0xE4 if k == counts[c] - 1 else 0x64, 0x01)
				sectors.append(xa_sector(sub, group[c].xa[2304 * k:2304 * (k + 1)]))
			else:
				sectors.append(xa_sector((0xFF, c, 0x04, 0x01), b''))
		put(b''.join(sectors))
		for c, s in enumerate(group):
			s.xa_entry = (c << 24 | (start + c), file_no << 24 | (start + c + 8 * (counts[c] - 1)), 0x178)
	if lba + 150 > 450000:
		sys.exit('the songs do not fit before 99:59:74')

	os.makedirs(os.path.join(a.out, 'data'), exist_ok=True)
	with open(os.path.join(a.out, 'data', 'media.bin'), 'wb') as f:
		for m in media:
			f.write(m)
	open(os.path.join(a.out, 'data', 'gfx.bin'), 'wb').write(blobs.data)

	# ---- the list ----
	donor = SONGTAB + 44 * 9	# ORION.78's record: the words we have no source for
	# Record word 8, low half: the song's length in measures; play ends there
	# and CLEARED shows over the rest of the music (ORION.78: 80, its last
	# step at 75 and tempo map end at 79; with ORION's 80 a longer song
	# stopped at measure 80).
	with open(os.path.join(a.out, 'data', 'extrasongs.txt'), 'w', encoding='latin-1') as f:
		f.write('# Read by port/core/dsfcore_extrasong.cpp.  Numbers are hex.\n')
		f.write('media %x %x\n' % (base, lba - base))
		for k, (off, size) in enumerate(blobs.index):
			f.write('blob %x %x %x\n' % (k, off, size))
		for k, s in enumerate(songs):
			record = [s.id, SONGS + 1 + k, STREAM_IDS + k, PREVIEW_IDS + k,
			          s.ratings('dance-single'), s.ratings('dance-double'), 0, 0,
			          (exe.w(donor + 32) & 0xFFFF0000) | s.length, 0, exe.w(donor + 40)]
			f.write('song %s\n' % s.title)
			f.write('artist %s\n' % s.artist)
			f.write('record %s\n' % ' '.join('%x' % w for w in record))
			f.write('stream %x %x %x %x\n' % (s.stream_lba, len(s.stream), 44100, 127))
			f.write('entries %s\n' % ' '.join('%x %x' % e for e in s.entry_lbas))
			f.write('gfx %x %x\n' % (50 + 2 * k, 51 + 2 * k))	# music-select pack items
			f.write('xa %x %x %x\n' % s.xa_entry)
			f.write('workout %x\n' % s.workout_index)
			u = s.cfg.get('unlock')
			if u:
				mask = sum(1 << DIFFS.index(d) for d in u['difficulties'])
				f.write('unlock %x %x\n' % (u['song'], mask))
			f.write('end\n')
		for e, (size, l) in sorted(toc_new.items()):
			f.write('toc %x %x %x\n' % (e, size, l))

	media_path = os.path.join(a.out, 'data', 'media.bin')
	media_size = os.path.getsize(media_path)
	media_sha = hashlib.sha256(open(media_path, 'rb').read()).hexdigest()
	disc_sha = hashlib.sha256(open(disc.bin, 'rb').read()).hexdigest()
	with open(os.path.join(a.out, 'manifest.toml'), 'w') as f:
		f.write('format_version = 9\nid = "dsf.extrasongs"\nversion = "1.0.0"\n'
		        'name = "Extra songs"\nauthor = "local"\n'
		        'description = "%d songs added to the song list (made from your own files and disc; never share it)."\n'
		        'resolver = "declarative"\nsave_compatibility = "shared"\nprepare = "dsf.extrasongs.media"\n\n' % len(songs))
		f.write('[[target]]\ngame_id = "SLES-04163"\ndisc_sha256 = "%s"\n\n' % disc_sha)
		f.write('[[feature]]\nid = "extra-songs"\nname = "Extra songs"\n'
		        'description = "Adds the songs in this package to the song list."\ngroup = "Content"\n'
		        'default_enabled = true\n\n')
		f.write('[[resource]]\nfeature = "extra-songs"\nid = "data"\nlabel = "Song data"\n'
		        'format = "folder"\ndefault = "data"\n\n')	# not required: a preparer must supply every required resource
		f.write('[[resource]]\nfeature = "extra-songs"\nid = "media"\nlabel = "Song audio and graphics"\n'
		        'format = "file"\nhidden = true\nrequired = true\nsize = %d\nsha256 = "%s"\n\n' % (media_size, media_sha))
		f.write('[[plugin]]\nfeature = "extra-songs"\nid = "dsf.extrasong"\n')
	print('%d songs, media %d sectors from LBA %d, %d pictures' % (len(songs), lba - base, base, len(blobs.index)))


if __name__ == '__main__':
	main()
