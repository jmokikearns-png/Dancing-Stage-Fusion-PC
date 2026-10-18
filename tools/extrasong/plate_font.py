"""Title plates in the game's own lettering.

The music select, results and records screens show each song's title over
its artist on a 360x40 4bpp plate: bold wide letters, white fill shaded
towards lavender, a navy outline (palette index 1).  This module cuts the
letters out of the stock plates on the player's own disc (so nothing of
Konami's is stored here) and sets new text with them.

Cutting: each plate line is split into connected areas of fill (palette
index >= 8); areas that overlap horizontally are one letter (i, j, !, :).
They are matched, left to right, to the line's characters from the
executable's master song table; a line whose count differs is skipped.
The other non-key pixels (outline, shading) go to the letter whose fill
they are nearest to.  For each character the most common shape among its
width is kept, cut where it stood beside a space or a line end when there
is such a sample.  Spacing is fitted per character (see _fit_spacing); the
two baselines are the most common ones on the stock plates.
"""
import struct
from collections import Counter, defaultdict, deque

import konami_lz

MASTER = 0x800A3EA0
MASTER_N = 0x252
GFXTAB = 0x800AA154
SONGS = 20
MUSIC_PACK = 201
FILL = 11		# palette index from which a pixel counts as letter fill
DOT = 12		# fill areas smaller than this are dots of a letter
PLATE_W, PLATE_H = 360, 40
SPLIT = 20		# letters whose fill starts above this row are on the title line

# The stock plates' second lines.  The executable has no artist strings;
# these are the songs' published credits, used only to label the letters.
ARTISTS = {
	0x1d7: 'DJ SIMON', 0x701c: 'Sho-T', 0x19: 'KTz', 0x10e: 'NAOKI', 0x109: 'Be For U',
	0x12b: '2MB', 0x173: 'dj TAKA with NAOKI', 0x2e: 'N.M.R', 0xe: 'mitsu-O!', 0xd5: '2MB',
	0x1d: '190', 0xea: 'NAOKI', 0x6001: 'S.F.M.P.', 0x802f: 'Kylie Minogue', 0x700e: 'Sugababes',
	0x8025: 'Daniel Beddingfield', 0x8023: 'The Darkness', 0x800f: 'Toni Basil',
	0x8018: 'Junior Senior', 0x8015: 'DJ SAMMY',
}


def _tim(t):
	"""4bpp TIM -> (palette words, width, height, indices)."""
	cl = struct.unpack_from('<IHHHH', t, 8)
	pal = list(struct.unpack_from('<%dH' % cl[3], t, 20))
	off = 8 + cl[0]
	im = struct.unpack_from('<IHHHH', t, off)
	w, h = im[3] * 4, im[4]
	px = t[off + 12:off + 12 + w * h // 2]
	return pal, w, h, [(px[i >> 1] >> (4 * (i & 1))) & 15 for i in range(w * h)]


def _cstr(exe, va):
	b = exe.b[va - 0x80019800:]
	return b[:b.index(0)].decode('latin-1')


def _letters(idx, w, y0, y1):
	"""Fill areas of rows y0..y1, merged when they overlap horizontally
	within a line (title: top above SPLIT), as (line, pixels, left, right)
	in reading order."""
	seen, areas = set(), []
	for y in range(y0, y1):
		for x in range(w):
			if idx[y * w + x] >= FILL and (x, y) not in seen:
				stack, area = [(x, y)], []
				seen.add((x, y))
				while stack:
					cx, cy = stack.pop()
					area.append((cx, cy))
					for dx in (-1, 0, 1):
						for dy in (-1, 0, 1):
							nx, ny = cx + dx, cy + dy
							if (y0 <= ny < y1 and 0 <= nx < w and (nx, ny) not in seen
									and idx[ny * w + nx] >= FILL):
								seen.add((nx, ny))
								stack.append((nx, ny))
				areas.append(area)
	line = lambda a: 0 if min(p[1] for p in a) < SPLIT else 1
	box = lambda a: (min(p[0] for p in a), max(p[0] for p in a))
	areas.sort(key=lambda a: (line(a), box(a)[0]))
	# dots (i, j, !, ?, :) join the letter whose columns they lie within
	big = [a for a in areas if len(a) >= DOT]
	merged = [[line(a), list(a), box(a)[0], box(a)[1]] for a in big]
	for a in areas:
		if len(a) >= DOT:
			continue
		lo, hi = box(a)
		host = [m for m in merged if m[0] == line(a) and m[2] - 1 <= lo and hi <= m[3] + 1]
		if host:
			host[0][1] += a
		else:
			merged.append([line(a), list(a), lo, hi])
	merged.sort(key=lambda m: (m[0], m[2]))
	merged = [tuple(m) for m in merged]
	return merged


def _owners(idx, w, y0, y1, letters):
	"""Every non-key pixel of the line -> the letter whose fill is nearest
	(breadth-first from all fills at once, through non-key pixels)."""
	owner, queue = {}, deque()
	for n, (_, a, _, _) in enumerate(letters):
		for p in a:
			owner[p] = n
			queue.append(p)
	while queue:
		x, y = queue.popleft()
		for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
			nx, ny = x + dx, y + dy
			if y0 <= ny < y1 and 0 <= nx < w and (nx, ny) not in owner and idx[ny * w + nx]:
				owner[(nx, ny)] = owner[(x, y)]
				queue.append((nx, ny))
	return owner


class PlateFont:
	def __init__(self, disc, exe, toc, pack_items):
		names = {}
		for i in range(MASTER_N):
			a = MASTER + 20 * i
			if exe.w(a + 4):
				names[exe.w(a)] = _cstr(exe, exe.w(a + 8))
		items = pack_items(disc.data(toc[MUSIC_PACK][1], toc[MUSIC_PACK][0]))
		samples = defaultdict(list)
		pairs, bases = [], ([], [])
		self.palette = None
		for k in range(SONGS):
			sid, _, plate_item = (exe.w(GFXTAB + 12 * k + 4 * j) for j in range(3))
			if sid not in names or sid not in ARTISTS:
				continue
			pal, w, h, idx = _tim(konami_lz.decode(items[plate_item]))
			self.palette = self.palette or pal
			letters = _letters(idx, w, 0, h)
			owner = _owners(idx, w, 0, h, letters)
			shapes = defaultdict(list)
			for (x, y), n in owner.items():
				shapes[n].append((x, y, idx[y * w + x]))
			for line, text in enumerate((names[sid], ARTISTS[sid])):
				mine = [n for n, l in enumerate(letters) if l[0] == line]
				# (character, follows a space, inside small-print brackets)
				chars, depth = [], 0
				for i, c in enumerate(text):
					if c == '(':
						depth += 1
					if c != ' ':
						chars.append((c, i > 0 and text[i - 1] == ' ', depth > 0))
					if c == ')':
						depth -= 1
				if len(mine) != len(chars):
					continue
				base = Counter(max(p[1] for p in letters[n][1]) for n in mine).most_common(1)[0][0]
				bases[line].append(base)
				tall = max(base - min(p[1] for p in letters[n][1]) for n in mine)
				small = any(c == '(' and base - min(p[1] for p in letters[n][1]) < 0.85 * tall
				            for n, (c, _, _) in zip(mine, chars))
				prev = None
				for q, (n, (ch, after_space, bracketed)) in enumerate(zip(mine, chars)):
					# a letter beside a space or a line end lost no outline to a neighbour
					free = (q == 0 or after_space) + (q == len(chars) - 1 or chars[q + 1][1])
					_, _, lo, hi = letters[n]
					if not (bracketed and small):
						shape = tuple(sorted((x - lo, y - base, v) for x, y, v in shapes[n]))
						samples[ch].append((shape, hi - lo + 1, free))
						if prev is not None:
							pairs.append((prev[1], ch, after_space, lo - letters[prev[0]][3] - 1))
						prev = (n, ch)
					else:
						prev = None
		if not samples or not bases[0] or not bases[1]:
			raise SystemExit('could not read the lettering of the stock title plates')
		self.glyphs = {}
		for ch, s in samples.items():
			width = Counter(w for _, w, _ in s).most_common(1)[0][0]
			shape = max((x for x in s if x[1] == width), key=lambda x: (x[2], len(x[0])))
			self.glyphs[ch] = shape[:2]
		self._fit_spacing(pairs)
		self.bases = [Counter(b).most_common(1)[0][0] for b in bases]
		if 'G' not in self.glyphs and 'C' in self.glyphs:
			self._add_g()

	# The stock plates never use a capital G.  It is the C with a bar and a
	# right stem, drawn here (rows relative to the baseline, columns x >= 2
	# of the C; '.' transparent, hex digits palette indices) and shaded like
	# the bar of the E.
	G_RIGHT = {
		-5: '1111111111',
		-4: '11cbbbff51',
		-3: '11a888ff51',
		-2: '211111ff51',
		-1: '91111aff51',
		0: 'ffffffffb1',
		1: '4555555211',
		2: '1111111111',
	}

	def _add_g(self):
		shape, width = self.glyphs['C']
		cells = {(x, y): v for x, y, v in shape}
		for y, row in self.G_RIGHT.items():
			for i, c in enumerate(row):
				cells.pop((2 + i, y), None)
				if c != '.':
					cells[(2 + i, y)] = int(c, 16)
		self.glyphs['G'] = (tuple(sorted((x, y, v) for (x, y), v in cells.items())), width)
		self.left['G'] = self.left['C']
		self.right['G'] = self.right.get('O', self.right['C'])

	def _fit_spacing(self, pairs):
		"""Proportional spacing: the gap between two letters' fills is the
		first's right bearing plus the second's left bearing (plus a word
		space after a space), fitted by least squares to the stock gaps."""
		import numpy as np
		chars = sorted(self.glyphs)
		col = {c: i for i, c in enumerate(chars)}
		n = len(chars)
		rows, rhs = [], []
		for a, b, sp, gap in pairs:
			r = [0.0] * (2 * n + 1)
			r[col[a]] += 1; r[n + col[b]] += 1
			if sp:
				r[2 * n] = 1
			rows.append(r); rhs.append(gap)
		# weak pull of every bearing towards the common gap's half
		half = float(np.median([g for _, _, sp, g in pairs if not sp])) / 2
		for i in range(2 * n):
			r = [0.0] * (2 * n + 1); r[i] = 0.1
			rows.append(r); rhs.append(0.1 * half)
		sol = np.linalg.lstsq(np.array(rows), np.array(rhs), rcond=None)[0]
		self.right = {c: sol[col[c]] for c in chars}
		self.left = {c: sol[n + col[c]] for c in chars}
		self.space = sol[2 * n]
		# a bearing far from the rest comes from a sample that misleads the
		# fit (the stock "(" mostly opens small print): use the median
		for side in (self.right, self.left):
			mid = float(np.median(list(side.values())))
			for c, v in side.items():
				if abs(v - mid) > 3:
					side[c] = mid
		err = [abs(self.right[a] + self.left[b] + (self.space if sp else 0) - g) for a, b, sp, g in pairs]
		self.spacing_error = (float(np.mean(err)), float(max(err)))

	def missing(self, text):
		return sorted({c for c in text if c != ' ' and c not in self.glyphs})

	def _layout(self, text, squeeze):
		x, prev, space, out = 0.0, None, False, []
		for c in text:
			if c == ' ':
				space = prev is not None
				continue
			if prev is not None:
				x += max(0.0, self.right[prev] + self.left[c] + (self.space if space else 0) - squeeze)
			out.append((int(round(x)), c))
			x += self.glyphs[c][1]
			prev, space = c, False
		return out, int(round(x))

	def plate(self, title, artist):
		"""360x40 palette indices: the title over the artist, each centred."""
		out = [0] * (PLATE_W * PLATE_H)
		for text, base in ((title, self.bases[0]), (artist, self.bases[1])):
			squeeze = 0.0
			while self._layout(text, squeeze)[1] > PLATE_W - 4 and squeeze < 3:
				squeeze += 0.5
			placed, width = self._layout(text, squeeze)
			if width > PLATE_W - 4:
				raise SystemExit('"%s" is too long for a title plate' % text)
			left = (PLATE_W - width) // 2
			for x0, c in placed:
				for dx, dy, v in self.glyphs[c][0]:
					X, Y = left + x0 + dx, base + dy
					if 0 <= X < PLATE_W and 0 <= Y < PLATE_H and v > out[Y * PLATE_W + X]:
						out[Y * PLATE_W + X] = v
		return out
