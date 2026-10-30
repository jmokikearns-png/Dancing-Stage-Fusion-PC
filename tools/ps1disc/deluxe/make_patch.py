#!/usr/bin/env python3
"""Build the Deluxe patch set: compiles payload.c / entry.S for the PS1 and
writes patches.h for dlxbuild.c (the builder that runs on the player's disc).

Everything in patches.h is this project's own: the compiled payload and the
new instruction words.  The stock words are listed only so the builder can
refuse a disc that is not SLES-04163 as expected."""
import os, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
CC = 'mipsel-linux-gnu-gcc'
CFLAGS = ['-march=r3000', '-mabi=32', '-msoft-float', '-O2', '-G0', '-fno-pic', '-mno-abicalls',
          '-ffreestanding', '-fno-builtin', '-nostdlib', '-fno-toplevel-reorder', '-Wall', '-Werror']
BLOCK, BLOCK_END = 0x80098AB0, 0x8009DC04	# in the executable's zero block 0x80098AA8.., 16-aligned

EXE_LBA, EXE_BASE = 24, 0x80019800		# SLES_041.63 (file offset 0 in RAM)
OVL_LBA, OVL_BASE = 28460, 0x800E8000		# READ_DT entry 227, the options overlay
REC_LBA = 28479					# READ_DT entry 228, the RECORDS overlay (also at 0x800E8000)

# The stock words of the sites the extra songs patch, taken from a local copy
# of the disc at build time (only to check the player's disc against).
STOCK_EXE = os.environ.get('DLX_STOCK_EXE', '/home/user/work/analysis/ps1-tooling/extract/SLES_041.63')
STOCK_READ_DT = os.environ.get('DLX_STOCK_READ_DT', '/home/user/work/analysis/ps1-tooling/extract/READ_DT.BIN')
WORKOUT_SCRATCH = 0x80198000	# the workout results' decode buffer: past the bigger pack, below 0x801A4000
SELECT_BASE = 0x80162000	# the music select pack, 0x2000 below 0x80160000 + 0x12000 more room (see SONG_EXE)


def j(op, target):
	return (op << 26) | ((target >> 2) & 0x3FFFFFF)


def build(out):
	objs = []
	for src in ('entry.S', 'payload.c'):
		o = os.path.join(out, os.path.splitext(src)[0] + '.o')
		subprocess.run([CC] + CFLAGS + ['-c', os.path.join(HERE, src), '-o', o], check=True)
		objs.append(o)
	elf = os.path.join(out, 'payload.elf')
	subprocess.run([CC] + CFLAGS + ['-T', os.path.join(HERE, 'payload.ld'), '-Wl,--no-warn-rwx-segments',
	                '-o', elf] + objs, check=True, cwd=out)
	blob = os.path.join(out, 'payload.bin')
	subprocess.run(['mipsel-linux-gnu-objcopy', '-O', 'binary', '-j', '.blob', elf, blob], check=True)
	syms = {}
	for line in subprocess.run(['mipsel-linux-gnu-nm', elf], capture_output=True, text=True, check=True).stdout.splitlines():
		a, _, name = line.split()
		syms[name] = int(a, 16)
	data = open(blob, 'rb').read()
	if syms.get('dlx_blob_start') != BLOCK:
		sys.exit('the payload links at 0x%08X, not 0x%08X' % (syms.get('dlx_blob_start', 0), BLOCK))
	if BLOCK + len(data) > BLOCK_END:
		sys.exit('payload is %d bytes: does not fit' % len(data))
	return data, syms


def main():
	out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'build')
	os.makedirs(out, exist_ok=True)
	data, s = build(out)
	# (va, stock word, new word)
	exe = [
		(0x80051954, 0x1062000B, 0x1460000B),	# beq v1, v0 -> bne v1, zero: TYPE2 and TYPE3 colour notes by beat
		(0x80054D28, 0x14620116, 0x10600116),	# bne v1, v0 -> beq v1, zero: palette builder 1
		(0x80055700, 0x146200E0, 0x106000E0),	# palette builder 2
		(0x800519F0, 0x3C0B800C, j(2, s['dlx_note_site'])),
		(0x800519F4, 0x256BA310, 0),
		(0x8005517C, 0x0801551C, j(2, s['dlx_build1_site'])),
		(0x80055A7C, 0x08015723, j(2, s['dlx_build2_site'])),
		(0x80084BC0, 0x2C420002, 0x2C420003),	# the save check lets 2 through
	]
	ovl = [
		(0x800EA168, 0x0C03A3BD, j(3, s['dlx_step_mark_site'])),
		# STEP MARK COLORS steps mod 3: v1 = value + dir; v1 < 0: += 3; v1 >= 3: -= 3
		(0x800E9E68, 0x24630002, 0x04610002),	# bgez v1, +2
		(0x800E9E6C, 0x000317C2, 0x28620003),	# slti v0, v1, 3
		(0x800E9E70, 0x00621021, 0x24630003),	# addiu v1, v1, 3
		(0x800E9E74, 0x00021043, 0x14400002),	# bnez v0, +2
		(0x800E9E78, 0x00021040, 0x00000000),	# nop
		(0x800E9E7C, 0x00621823, 0x2463FFFD),	# addiu v1, v1, -3
	]
	exe_img = open(STOCK_EXE, 'rb').read()
	ew = lambda va: struct.unpack_from('<I', exe_img, va - EXE_BASE)[0]
	rd = open(STOCK_READ_DT, 'rb').read()
	rec_img = rd[(REC_LBA - 18000) * 2048:]
	rw = lambda va: struct.unpack_from('<I', rec_img, va - OVL_BASE)[0]
	sel_hi, sel_lo = SELECT_BASE >> 16, SELECT_BASE & 0xFFFF
	song_exe = []
	# the music select pack (READ_DT 201..205) loads at 0x80174000 into 192 KB;
	# with the extra songs' banners and plates it needs ~262 KB: it loads at
	# SELECT_BASE instead (0x80160000..0x80174000 is untouched during the
	# GAME / DOUBLE / WORKOUT selects -- but the dancers are kept there
	# between songs: dlx_load_site makes them load again), the loader and
	# the banner decoder's four bases (lui 0x8017 + ori 0x4000) changed together
	for lui, ori in ((0x8007E180, 0x8007E1A8), (0x8004E940, 0x8004E944), (0x8004E954, 0x8004E95C),
	                 (0x8004E970, 0x8004E97C), (0x8004F364, 0x8004F368)):
		song_exe.append((lui, ew(lui), (ew(lui) & 0xFFFF0000) | sel_hi))
		song_exe.append((ori, ew(ori), (ew(ori) & 0xFFFF0000) | sel_lo))
	song_exe += [
		(0x8002C398, ew(0x8002C398), j(2, s['dlx_kind_site'])), (0x8002C39C, ew(0x8002C39C), 0),
		(0x80050C40, ew(0x80050C40), j(2, s['dlx_avail_site'])), (0x80050C44, ew(0x80050C44), 0),
		(0x8008CE50, ew(0x8008CE50), j(2, s['dlx_workout_slot'])), (0x8008CE54, ew(0x8008CE54), 0),
		(0x8008AA00, ew(0x8008AA00), j(3, s['dlx_workout_lz'])),
		(0x80023FC0, ew(0x80023FC0), j(2, s['dlx_load_site'])), (0x80023FC4, ew(0x80023FC4), 0),
	]
	# the workout loader's inline slot formula -> a call (x, y, clut x, clut y at 0x28..0x34(sp))
	addiu = lambda rt, rs, imm: (9 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)
	sw = lambda rt, off, base: (0x2B << 26) | (base << 21) | (rt << 16) | (off & 0xFFFF)
	V0, A0, A1, A2, A3, S0, SP = 2, 4, 5, 6, 7, 16, 29
	code = [addiu(V0, SP, 0x34), sw(V0, 0x10, SP), (S0 << 21) | (A0 << 11) | 0x21,
	        addiu(A1, SP, 0x28), addiu(A2, SP, 0x2C), j(3, s['dlx_workout_slot']), addiu(A3, SP, 0x30),
	        (0x0F << 26) | (A1 << 16) | (WORKOUT_SCRATCH >> 16), (0x0D << 26) | (A1 << 21) | (A1 << 16) | (WORKOUT_SCRATCH & 0xFFFF)]
	for i in range((0x8008A9F4 - 0x8008A9A0) // 4):
		va = 0x8008A9A0 + 4 * i
		song_exe.append((va, ew(va), code[i] if i < len(code) else 0))
	# Its decode buffer 0x8018C000 lies inside the bigger workout pack (0x80174000
	# + 134 KB): the pictures it decodes there (the screen's, <= 1.8 KB, and the
	# 4.9 KB banners, up to 16 KB) would overwrite the banners not yet decoded.
	# They go to WORKOUT_SCRATCH instead, 0x3400 past the pack's end (0x80174000
	# + 134 KB), in memory untouched on that screen (traced on the original
	# disc: 0x80195000..0x801A4000); lui 0x8018 + ori 0xC000 pairs, 0x8008AA48,
	# after the loop, keeps it.
	for lui, ori in ((0x8008A86C, 0x8008A874), (0x8008A880, 0x8008A884), (0x8008AA08, 0x8008AA0C)):
		assert ew(lui) & 0xFFFF == 0x8018 and ew(ori) & 0xFFFF == 0xC000
		song_exe.append((lui, ew(lui), (ew(lui) & 0xFFFF0000) | (WORKOUT_SCRATCH >> 16)))
		song_exe.append((ori, ew(ori), (ew(ori) & 0xFFFF0000) | (WORKOUT_SCRATCH & 0xFFFF)))
	song_rec = [
		(0x800E8FA4, rw(0x800E8FA4), 0x00404821),	# sw v0, 0(s0) -> move t1, v0: the end mark is
		(0x800E8FA8, rw(0x800E8FA8), rw(0x800E8FA8)),	#   stored after the test, so 32 songs
		(0x800E8FAC, rw(0x800E8FAC), 0xAE090000),	#   do not end the list (sw t1, 0(s0))
		(0x800E90BC, rw(0x800E90BC), j(3, s['dlx_records_site'])),
		# the card buffer 0x80124000 -> 0x80126000, past the bigger pack (0x80100000..)
		(0x800E8E1C, rw(0x800E8E1C), (rw(0x800E8E1C) & 0xFFFF0000) | 0x6000),
	]
	assert rw(0x800E8E18) == 0x3C048012 and rw(0x800E8E1C) == 0x34844000
	h = ['/* Generated by make_patch.py: the Deluxe patch set.  Do not edit. */',
	     '#define DLX_EXE_LBA %d' % EXE_LBA, '#define DLX_EXE_BASE 0x%08Xu' % EXE_BASE,
	     '#define DLX_OVL_LBA %d' % OVL_LBA, '#define DLX_OVL_BASE 0x%08Xu' % OVL_BASE,
	     '#define DLX_BLOCK 0x%08Xu' % BLOCK, '#define DLX_BLOCK_END 0x%08Xu' % BLOCK_END,
	     'static const unsigned char dlx_payload[%d] = {' % len(data)]
	for i in range(0, len(data), 16):
		h.append('\t' + ''.join('0x%02X,' % b for b in data[i:i + 16]))
	h.append('};')
	h.append('#define DLX_REC_LBA %d' % REC_LBA)
	h.append('#define DLX_SELECT_BASE 0x%08Xu' % SELECT_BASE)
	for k in ('dlx_num_extra', 'dlx_extra', 'dlx_tables'):
		h.append('#define DLX_SYM_%s 0x%08Xu' % (k[4:].upper(), s[k]))
	for name, lst in (('dlx_exe_words', exe), ('dlx_ovl_words', ovl), ('dlx_song_exe_words', song_exe), ('dlx_song_rec_words', song_rec)):
		h.append('static const struct DlxWord %s[%d] = {' % (name, len(lst)))
		h += ['\t{ 0x%08Xu, 0x%08Xu, 0x%08Xu },' % w for w in lst]
		h.append('};')
	open(os.path.join(HERE, 'patches.h'), 'w').write('\n'.join(h) + '\n')
	print('payload %d bytes at 0x%08X..0x%08X (block ends 0x%08X)' % (len(data), BLOCK, BLOCK + len(data), BLOCK_END))
	for k in sorted(s, key=s.get):
		if k.startswith('dlx_'):
			print('  0x%08X %s' % (s[k], k))


if __name__ == '__main__':
	main()
