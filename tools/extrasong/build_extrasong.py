#!/usr/bin/env python3
"""Build a local mod package that adds a StepMania simfile to Dancing Stage
Fusion's own song list (port/core/dsfcore_extrasong.cpp does the in-RAM
part).

  build_extrasong.py SONG_DIR DISC.cue OUT_PACKAGE_DIR
                     [--title T] [--artist A] [--plate PLATE.png] [--background BG.png]
                     [--master-id 0x8030]

SONG_DIR holds NAME.sm, NAME.ogg (or .mp3/.wav), NAME.png (256x80 banner)
and NAME-bg.png (320x240 background).  Everything is converted to the
game's own formats and written into free space of the player's own disc
image by disc overlays; the output is personal (it contains the song and
data from the disc), never part of this project.

Formats (notes/dsf-extra-songs.md):
* READ_DT song entry: PACK[4] = { PACK{tempo map, charts}, event list,
  LZ(background TIM 320x240 8bpp), language UI set (copied) }.
* Chart: u32 size, u32 kind (style | difficulty<<8 | 1<<16), 14 x 0, then
  one u32 per beat, one byte per 16th, one bit per panel; beat 0 is
  played at measure 3 of the tempo map (measured in the game: at 196 and
  at 98 BPM the steps came exactly one measure late with beat 0 at 2).
* Tempo map: u32 size, (u32 pos in 1/4096 measure, u32 time in 1/75 s).
* Stream (STR.BIN layout): SPU ADPCM, 0x4000-byte blocks alternating L/R,
  ending in a pair of silent flag-1 blocks (psxaudio.c).
* Preview: XA ADPCM 4-bit stereo 37.8 kHz, one sector in 8 (psxaudio.c).
"""
import argparse, hashlib, os, re, shutil, struct, subprocess, sys, tempfile
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import konami_lz
import plate_font

# ---- the disc (SLES-04163, Redump) ----------------------------------------
EXE_LBA, EXE_SIZE = 24, 636928
EXE_SHA1 = 'ade130f4ab7cf5291e6a1606cb8d2fd51a01b1da'
EXE_VBASE = 0x80019800			# vaddr = file offset + this
READDT_LBA, READDT_FREE = 18000, (28573, 32000)	# 0xFF padding at the end of READ_DT.BIN
DUMMY_FREE = (91836, 105336)		# DUMMY.BIN
TOC, TOC_N = 0x80092B50, 235
SONGTAB, SONGS = 0x800AFE38, 20
STRTAB = 0x800932B8
XATAB = 0x800934E4			# entry k = preview id 24 + k
LANG_ENTRIES = 5			# en, fr, de, it, es copies
MUSIC_PACKS = range(201, 206)		# music select: items 10-29 banners, 30-49 plates
RESULT_PACKS = range(206, 211)		# results: items 17-36 plates
RECORD_PACKS = range(28, 33)		# records: slot 1 = SONGTAB {id, mini, plate}
WORKOUT_PACK = 181			# workout results: items 1-20 banners 176x55, slot 21 = 0
UI_DONOR = 14				# song group whose language UI sets are copied (AM-3P)
EVENT_DONOR = 12			# song group whose event list is copied (ORION.78, 200 BPM)


class Disc:
	def __init__(self, cue):
		text = open(cue).read()
		m = re.search(r'FILE\s+"([^"]+)"', text)
		self.bin = os.path.join(os.path.dirname(os.path.abspath(cue)), m.group(1))
		self.f = open(self.bin, 'rb')

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
	return list(q.getdata()), [tuple(pal[3 * i:3 * i + 3]) for i in range(colours)]


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
	bpms = [tuple(float(x) for x in p.split('=')) for p in tags['BPMS'][0].split(',') if p.strip()]
	if len(bpms) != 1 or tags.get('STOPS', [''])[0].strip():
		sys.exit('only constant-tempo songs without stops are supported for now')
	charts = []
	for n in tags.get('NOTES', []):
		f = [x.strip() for x in n.split(':')]
		style, diff, meter, notes = f[0], f[2], int(f[3] or 0), f[5]
		measures = [[r.strip() for r in m.split('\n') if r.strip()] for m in notes.split(',')]
		charts.append(dict(style=style, diff=diff, meter=meter, measures=measures))
	return dict(title=tags.get('TITLE', [''])[0].strip(), artist=tags.get('ARTIST', [''])[0].strip(),
	            offset=float(tags.get('OFFSET', ['0'])[0] or 0), bpm=bpms[0][1],
	            sample_start=float(tags.get('SAMPLESTART', ['0'])[0] or 0),
	            sample_length=float(tags.get('SAMPLELENGTH', ['15'])[0] or 15), charts=charts)


SM_DIFF = {'Beginner': 4, 'Easy': 0, 'Medium': 1, 'Hard': 2}
# Panel bits (single: low nibble, mirrored into the high one; double: P1
# low, P2 high).  Left/Right are bits 0/1 (the most-used double panels are
# P1-Right and P2-Left); Up/Down bits 2/3 come from in-game observation.
PANEL_BITS = {0: 0, 1: 3, 2: 2, 3: 1}	# simfile column (L, D, U, R) -> bit


CHART_START = 3		# tempo-map measure of the chart's first beat

def chart_words(chart, lead_measures, last_measure):
	beats = 4 * last_measure - 8	# the stock charts' length
	words = [0] * beats
	cols = 4 if chart['style'] == 'dance-single' else 8
	for m, rows in enumerate(chart['measures']):
		n = len(rows)
		for r, row in enumerate(rows):
			if (r * 16) % n:
				if any(c in '124' for c in row):
					sys.exit('a note off the 16th-note grid (measure %d)' % m)
				continue
			mask = 0
			for c in range(cols):
				if row[c] in '124':	# taps, hold and roll heads; tails and mines dropped
					mask |= 1 << (PANEL_BITS[c % 4] + (4 if c >= 4 else 0))
			if not mask:
				continue
			if cols == 4:
				mask |= mask << 4
			sixteenth = (m + lead_measures - CHART_START) * 16 + (r * 16) // n
			if sixteenth < 0 or sixteenth // 4 >= beats:
				sys.exit('a step outside the song (measure %d)' % m)
			words[sixteenth // 4] |= mask << (8 * (sixteenth % 4))
	kind = (0 if cols == 4 else 2) | (SM_DIFF[chart['diff']] << 8) | (1 << 16)
	return struct.pack('<II14I', 64 + 4 * beats, kind, *([0] * 14)) + struct.pack('<%dI' % beats, *words)


def tempo_map(lead_measures, last_measure, bpm, offset):
	"""(pos, time) pairs: measure m at (m - lead) * measure_seconds - offset
	(the audio is padded with the lead-in)."""
	ms = 4 * 60.0 / bpm
	pts = [0, 1, 5, last_measure]
	# time: 31-bit signed (bit 31 is a flag the game's maps sometimes set)
	pairs = b''.join(struct.pack('<II', 4096 * m, int(round((m * ms - offset) * 75)) & 0x7FFFFFFF) for m in pts)
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


def raw_sector(lba, sub, data, form2):
	t = lba + 150
	m, s, f = t // 4500, (t // 75) % 60, t % 75
	bcd = lambda v: (v // 10) << 4 | (v % 10)
	head = b'\x00' + b'\xff' * 10 + b'\x00' + bytes([bcd(m), bcd(s), bcd(f), 2])
	sh = bytes(sub) * 2
	body = data + b'\0' * ((2324 if form2 else 2048) - len(data))
	tail = b'\0' * (4 if form2 else 280)	# EDC (and ECC) left zero
	return head + sh + body + tail


# ---- main ---------------------------------------------------------------------
def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('song_dir'); ap.add_argument('cue'); ap.add_argument('out')
	ap.add_argument('--plate', help='a title plate PNG (text on transparent, scaled to 360x40) instead of '
	                'the title and artist set in the game\'s own lettering')
	ap.add_argument('--background', help='gameplay background PNG (any 4:3 size) instead of NAME-bg.png')
	ap.add_argument('--title', help='plate title (default: the simfile\'s #TITLE)')
	ap.add_argument('--artist', help='plate artist (default: the simfile\'s #ARTIST)')
	ap.add_argument('--master-id', type=lambda x: int(x, 0), default=0x8030)
	a = ap.parse_args()

	names = [f for f in os.listdir(a.song_dir) if f.lower().endswith('.sm')]
	base = os.path.join(a.song_dir, names[0][:-3])
	sm = parse_sm(base + '.sm')
	audio = next(base + e for e in ('.ogg', '.mp3', '.wav') if os.path.exists(base + e))
	disc = Disc(a.cue)
	exe = Exe(disc)
	toc = [(exe.w(TOC + 8 * i), exe.w(TOC + 8 * i + 4)) for i in range(TOC_N)]
	master_ids = {exe.w(0x800A3EA0 + 20 * i) for i in range(0x252) if exe.w(0x800A3EA0 + 20 * i + 4)}
	if a.master_id in master_ids:
		sys.exit('song id 0x%x is taken' % a.master_id)
	entry = lambda i: disc.data(toc[i][1], toc[i][0])

	for lba in (READDT_FREE[0], READDT_FREE[1] - 1):
		if disc.user(lba) != b'\xff' * 2048:
			sys.exit('READ_DT free space is not where expected')
	if any(disc.user(DUMMY_FREE[0], 4)) or any(disc.user(DUMMY_FREE[1] - 4, 4)):
		sys.exit('DUMMY.BIN free space is not empty')

	tmp = tempfile.mkdtemp()
	try:
		psxaudio = tool(tmp)
		dur = float(subprocess.check_output(['ffprobe', '-v', 'error', '-show_entries', 'format=duration',
		                                     '-of', 'default=nw=1:nk=1', audio]))
		ms = 4 * 60.0 / sm['bpm']
		first = min(m for c in sm['charts'] for m, rows in enumerate(c['measures']) if any(any(x in '124' for x in r) for r in rows))
		lead = max(0, CHART_START - first)
		last = int((dur + lead * ms + sm['offset']) / ms)	# measure at the end of the (padded) music
		# ---- song entry (5 language copies) ----
		charts = []
		order = [('dance-single', d) for d in ('Easy', 'Medium', 'Hard', 'Beginner')] + \
		        [('dance-double', d) for d in ('Easy', 'Medium', 'Hard')]
		for style, diff in order:
			c = next((c for c in sm['charts'] if c['style'] == style and c['diff'] == diff), None)
			if c:
				charts.append(chart_words(c, lead, last))
		slot0 = build_pack([tempo_map(lead, last, sm['bpm'], sm['offset'])] + charts)
		events = pack_items(entry(68 + 5 * EVENT_DONOR))[1]
		bg = konami_lz.encode(background_tim(Image.open(a.background or base + '-bg.png')))
		song_entries = [build_pack([slot0, events, bg, pack_items(entry(68 + 5 * UI_DONOR + k))[3]])
		                for k in range(LANG_ENTRIES)]

		# ---- graphics ----
		banner = konami_lz.encode(banner_tim(Image.open(base + '.png')))
		mini = konami_lz.encode(mini_tim(Image.open(base + '.png')))
		stock_plate = konami_lz.decode(pack_items(entry(201))[30])
		if a.plate:
			plate = konami_lz.encode(plate_tim(a.plate, stock_plate))
		else:
			font = plate_font.PlateFont(disc, exe, toc, pack_items)
			title, artist = a.title or sm['title'], a.artist or sm['artist']
			lost = font.missing(title + artist)
			if lost:
				sys.exit('the game\'s lettering has no %s: change --title/--artist or give --plate'
				         % ' '.join(lost))
			plate = konami_lz.encode(lettered_plate_tim(font.plate(title, artist), stock_plate))
		workout_banner = konami_lz.encode(workout_tim(Image.open(base + '.png')))
		music, results, records = {}, {}, {}
		it = pack_items(entry(WORKOUT_PACK))
		assert len(it) == 22 and it[21] is None
		records[WORKOUT_PACK] = build_pack(it[:21] + [workout_banner, None])
		for e in MUSIC_PACKS:
			it = pack_items(entry(e))
			assert len(it) == 51 and it[50] is None
			music[e] = build_pack(it[:50] + [banner, plate, None])
		for e in RESULT_PACKS:
			it = pack_items(entry(e))
			assert len(it) == 38 and it[37] is None
			results[e] = build_pack(it[:37] + [plate, None])
		for e in RECORD_PACKS:
			it = pack_items(entry(e))
			st = it[1]
			recs = [struct.unpack_from('<3I', st, 12 * i) for i in range(SONGS)]
			assert struct.unpack_from('<3I', st, 12 * SONGS) == (0, 0, 0)
			# re-base: a 22-record table (21 + terminator), data after it
			old_data_at = 12 * (SONGS + 1)
			new_data_at = 12 * (SONGS + 2)
			blobs = st[old_data_at:]
			blobs += b'\0' * (-len(blobs) % 4)
			mini_at = new_data_at + len(blobs)
			plate_at = mini_at + len(mini) + (-len(mini) % 4)
			table = b''.join(struct.pack('<3I', i, x - old_data_at + new_data_at, y - old_data_at + new_data_at) for i, x, y in recs)
			table += struct.pack('<3I', a.master_id, mini_at, plate_at) + b'\0' * 12
			new_st = table + blobs + mini + b'\0' * (-len(mini) % 4) + plate
			records[e] = build_pack([it[0], new_st] + it[2:])

		# ---- audio ----
		raw = os.path.join(tmp, 'song.s16')
		pcm(audio, 44100, raw, pad_front=lead * ms)
		stream = os.path.join(tmp, 'song.str')
		subprocess.check_call([psxaudio, 'spu', raw, stream])
		stream = open(stream, 'rb').read()
		prev = os.path.join(tmp, 'prev.s16')
		pcm(audio, 37800, prev, start=sm['sample_start'], length=sm['sample_length'], fade=1.0)
		xa = os.path.join(tmp, 'prev.xa')
		subprocess.check_call([psxaudio, 'xa', prev, xa])
		xa = open(xa, 'rb').read()
	finally:
		shutil.rmtree(tmp)

	# ---- layout in the free space ----
	os.makedirs(os.path.join(a.out, 'data'), exist_ok=True)
	overlays = []
	lba = READDT_FREE[0]
	def place(name, blob):
		nonlocal lba
		n = (len(blob) + 2047) // 2048
		if lba + n > READDT_FREE[1]:
			sys.exit('not enough free space in READ_DT')
		at = lba
		open(os.path.join(a.out, 'data', name), 'wb').write(blob + b'\xff' * (n * 2048 - len(blob)))
		overlays.append(('disc_user', at * 2048, name))
		lba += n
		return (len(blob), at)
	entries = [place('song_%d.bin' % k, b) for k, b in enumerate(song_entries)]
	toc_new = {}
	for e, b in list(music.items()) + list(results.items()) + list(records.items()):
		toc_new[e] = place('e%03d.bin' % e, b)
	# stream in DUMMY.BIN, then 10 zero sectors, then the preview
	slba = DUMMY_FREE[0]
	sn = (len(stream) + 2047) // 2048
	open(os.path.join(a.out, 'data', 'stream.bin'), 'wb').write(stream + b'\0' * (sn * 2048 - len(stream)))
	overlays.append(('disc_user', slba * 2048, 'stream.bin'))
	xlba = slba + sn + 10
	xlba += -xlba % 8
	audio_sectors = len(xa) // 2304
	raws = []
	for i in range(audio_sectors * 8):
		lba_i = xlba + i
		if i % 8 == 0:
			k = i // 8
			sub = (1, 0, 0xE4 if k == audio_sectors - 1 else 0x64, 0x01)
			raws.append(raw_sector(lba_i, sub, xa[2304 * k:2304 * (k + 1)], True))
		else:
			raws.append(raw_sector(lba_i, (0xFF, i % 8, 0x04, 0x01), b'', False))
	if xlba + len(raws) > DUMMY_FREE[1]:
		sys.exit('not enough free space in DUMMY.BIN')
	open(os.path.join(a.out, 'data', 'preview.raw'), 'wb').write(b''.join(raws))
	overlays.append(('disc_raw', xlba * 2352, 'preview.raw'))
	xa_entry = (0 << 24 | xlba, 1 << 24 | (xlba + 8 * (audio_sectors - 1)), 0x178)

	# ---- the record ----
	levels = {}
	for c in sm['charts']:
		levels[(c['style'], c['diff'])] = min(c['meter'], 9)
	sp = levels.get(('dance-single', 'Easy'), 0) | levels.get(('dance-single', 'Medium'), 0) << 4 | \
	     levels.get(('dance-single', 'Hard'), 0) << 8 | levels.get(('dance-single', 'Beginner'), 0) << 12
	dp = levels.get(('dance-double', 'Easy'), 0) | levels.get(('dance-double', 'Medium'), 0) << 4 | \
	     levels.get(('dance-double', 'Hard'), 0) << 8
	donor = SONGTAB + 44 * 9	# ORION.78's record: the words we have no source for
	record = [a.master_id, SONGS + 1, 24, 50, sp, dp, 0, 0, exe.w(donor + 32), 0, exe.w(donor + 40)]
	disc_sha = hashlib.sha256(open(disc.bin, 'rb').read()).hexdigest()

	with open(os.path.join(a.out, 'data', 'extrasongs.txt'), 'w') as f:
		f.write('# Read by port/core/dsfcore_extrasong.cpp.  Numbers are hex.\n')
		f.write('song %s\n' % sm['title'])
		f.write('artist %s\n' % sm['artist'])
		f.write('record %s\n' % ' '.join('%x' % w for w in record))
		f.write('stream %x %x %x %x\n' % (slba, len(stream), 44100, 127))
		f.write('entries %s\n' % ' '.join('%x %x' % e for e in entries))
		f.write('gfx 32 33\n')	# music-select pack items 50, 51
		f.write('xa %x %x %x\n' % xa_entry)
		f.write('end\n')
		for e, (size, l) in sorted(toc_new.items()):
			f.write('toc %x %x %x\n' % (e, size, l))
	with open(os.path.join(a.out, 'manifest.toml'), 'w') as f:
		f.write('format_version = 7\nid = "dsf.extrasongs"\nversion = "1.0.0"\n'
		        'name = "Extra songs"\nauthor = "local"\n'
		        'description = "%s added to the song list (made from your own files and disc; never share it)."\n'
		        'resolver = "declarative"\nsave_compatibility = "shared"\n\n' % sm['title'])
		f.write('[[target]]\ngame_id = "SLES-04163"\ndisc_sha256 = "%s"\n\n' % disc_sha)
		f.write('[[feature]]\nid = "extra-songs"\nname = "Extra songs"\n'
		        'description = "Adds the songs in this package to the song list."\ngroup = "Content"\n'
		        'default_enabled = true\n\n')
		f.write('[[resource]]\nfeature = "extra-songs"\nid = "data"\nlabel = "Song data"\n'
		        'format = "folder"\ndefault = "data"\nrequired = true\n\n')
		f.write('[[plugin]]\nfeature = "extra-songs"\nid = "dsf.extrasong"\n\n')
		for target, off, name in overlays:
			p = os.path.join(a.out, 'data', name)
			f.write('[[overlay]]\nfeature = "extra-songs"\ntarget = "%s"\noffset = %d\nfile = "data/%s"\nsha256 = "%s"\n\n'
			        % (target, off, name, hashlib.sha256(open(p, 'rb').read()).hexdigest()))
	print('%s: %d measures (lead %d), %d charts, stream %d sectors @%d, preview %d sectors @%d, READ_DT up to %d'
	      % (sm['title'], last, lead, len(charts), sn, slba, len(raws), xlba, lba))


if __name__ == '__main__':
	main()
