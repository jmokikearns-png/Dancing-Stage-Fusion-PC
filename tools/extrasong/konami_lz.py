"""Konami LZ (the scheme of the System 573 / PS1 BEMANI games), encoder and
decoder.  Control bytes give 8 flags each, LSB first.  Flag 0: one literal
byte.  Flag 1, byte d:
  d < 0x80          long copy:  distance ((d & 3) << 8) | next byte, length (d >> 2) + 3
  0x80 <= d < 0xC0  short copy: distance (d & 0xF) + 1, length (d >> 4) - 6
  0xC0 <= d < 0xFF  literal run of d - 0xB8 bytes
  d == 0xFF         end"""


def decode(src):
	out = bytearray()
	i = ctl = 0
	while True:
		ctl >>= 1
		if not ctl & 0x100:
			ctl = src[i] | 0xFF00
			i += 1
		d = src[i]
		i += 1
		if not ctl & 1:
			out.append(d)
			continue
		if d == 0xFF:
			return bytes(out)
		if d >= 0xC0:
			n = d - 0xB8
			out += src[i:i + n]
			i += n
			continue
		if d < 0x80:
			dist = ((d & 3) << 8) | src[i]
			i += 1
			n = (d >> 2) + 3
		else:
			dist = (d & 0xF) + 1
			n = (d >> 4) - 6
		p = len(out) - dist
		for k in range(n):
			out.append(out[p + k])


def encode(data):
	"""Greedy, with hash chains over 3-byte prefixes (window 1023)."""
	data = bytes(data)
	n = len(data)
	out = bytearray()
	flags_at, nflags = None, 8
	heads = {}

	def flag(bit):
		nonlocal flags_at, nflags
		if nflags == 8:
			flags_at = len(out)
			out.append(0)
			nflags = 0
		if bit:
			out[flags_at] |= 1 << nflags
		nflags += 1

	def best_match(i):
		best_len, best_dist = 0, 0
		# short copies: distance 1..16, length 2..5
		for dist in range(1, min(16, i) + 1):
			l = 0
			while l < 5 and i + l < n and data[i + l] == data[i + l - dist]:
				l += 1
			if l >= 2 and l > best_len:
				best_len, best_dist = l, dist
		if i + 3 <= n:
			for j in reversed(heads.get(data[i:i + 3], [])[-64:]):
				dist = i - j
				if dist > 1023:
					break
				l = 0
				while l < 34 and i + l < n and data[i + l] == data[j + l]:
					l += 1
				if l >= 3 and l > best_len:
					best_len, best_dist = l, dist
					if l == 34:
						break
		return best_len, best_dist

	def remember(i):
		if i + 3 <= n:
			heads.setdefault(data[i:i + 3], []).append(i)

	lits = bytearray()

	def flush_literals():
		nonlocal lits
		while len(lits) >= 8:
			k = min(70, len(lits))
			flag(1)
			out.append(0xB8 + k)
			out.extend(lits[:k])
			lits = lits[k:]
		for b in lits:
			flag(0)
			out.append(b)
		lits = bytearray()

	i = 0
	while i < n:
		l, dist = best_match(i)
		short_ok = l <= 5 and dist <= 16
		if l >= 3 or (l == 2 and short_ok):
			flush_literals()
			flag(1)
			if short_ok and l <= 5 and dist <= 16 and l >= 2:
				out.append(((l + 6) << 4) | (dist - 1))
			else:
				out.append(((l - 3) << 2) | (dist >> 8))
				out.append(dist & 0xFF)
			for k in range(l):
				remember(i + k)
			i += l
		else:
			lits.append(data[i])
			remember(i)
			i += 1
	flush_literals()
	flag(1)
	out.append(0xFF)
	return bytes(out)
