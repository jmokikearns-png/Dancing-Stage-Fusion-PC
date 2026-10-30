#!/usr/bin/env python3
"""Dancing Stage Fusion PS1 disc builder (the back-port).  notes/dsf-ps1-version.md.

Runs on the player's machine, on their own disc: the disc is unpacked with
dumpsxiso, the executable patched, new files placed after the last sector,
and the image rebuilt with mkpsxiso.  Every file the game reads keeps its
sector (the XML dumpsxiso writes pins each one), because the executable
finds its data by sector number: the READ_DT table of contents at
0x80092B50 (235 x {size, lba}), the stream table and the XA preview table.
Nothing it reads or writes is part of this project.

Stage one (this version): the pipeline.  --move-entry N copies READ_DT entry
N into a new file after the end of the disc and points the table of
contents at it, which proves a patched executable and appended data on an
emulator before any song is added.

usage: build_ps1disc.py --extract DIR --out DIR [--move-entry N ...]
  DIR from: dumpsxiso -x DIR -s DIR/dsf.xml <disc.bin>
"""
import argparse
import os
import re
import struct
import subprocess
import sys

EXE_NAME = 'SLES_041.63'
EXE_BASE = 0x80019800		# file offset 0 (the PS-X EXE header) in RAM
TOC = 0x80092B50
TOC_ENTRIES = 235
READ_DT_NAME = 'READ_DT.BIN'
SECTOR = 2048


class Exe:
	def __init__(self, path):
		self.data = bytearray(open(path, 'rb').read())

	def off(self, va):
		return va - EXE_BASE

	def word(self, va):
		return struct.unpack_from('<I', self.data, self.off(va))[0]

	def toc(self, n):
		return struct.unpack_from('<II', self.data, self.off(TOC + 8 * n))	# size, lba

	def set_toc(self, n, size, lba):
		struct.pack_into('<II', self.data, self.off(TOC + 8 * n), size, lba)


class Layout:
	"""dumpsxiso's XML: every file and gap with its sector (offs)."""
	def __init__(self, path):
		self.xml = open(path).read()
		self.files = {m.group(1): (m.group(2), int(m.group(3)))
		              for m in re.finditer(r'<file name="([^"]+)" source="([^"]+)"[^>]*offs="(\d+)"', self.xml)}
		ends = [int(o) + int(n) for n, o in re.findall(r'<dummy sectors="(\d+)"[^>]*offs="(\d+)"', self.xml)]
		for name, (src, offs) in self.files.items():
			ends.append(offs + (os.path.getsize(src) + SECTOR - 1) // SECTOR)
		self.end = max(ends)		# first sector after everything
		self.added = []

	def source(self, name):
		return self.files[name][0]

	def replace_source(self, name, path):
		old = self.files[name][0]
		self.xml = self.xml.replace('source="%s"' % old, 'source="%s"' % path)

	def add_file(self, name, path):
		"""A new data file after the end; returns its first sector."""
		at = self.end
		self.added.append('            <file name="%s" source="%s" type="data" offs="%d"/>' % (name, path, at))
		self.end = at + (os.path.getsize(path) + SECTOR - 1) // SECTOR
		return at

	def write(self, path, image, cue):
		xml = re.sub(r'image_name="[^"]*" cue_sheet="[^"]*"', 'image_name="%s" cue_sheet="%s"' % (image, cue), self.xml)
		xml = xml.replace('        </directory_tree>', '\n'.join(self.added) + '\n        </directory_tree>')
		open(path, 'w').write(xml)


def main():
	ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument('--extract', required=True, help='dumpsxiso output folder (with dsf.xml)')
	ap.add_argument('--out', required=True)
	ap.add_argument('--move-entry', type=int, action='append', default=[], help='READ_DT entry to move past the end (pipeline test)')
	ap.add_argument('--mkpsxiso', default='mkpsxiso')
	a = ap.parse_args()

	os.makedirs(a.out, exist_ok=True)
	layout = Layout(os.path.join(a.extract, 'dsf.xml'))
	exe = Exe(layout.source(EXE_NAME))
	read_dt = open(layout.source(READ_DT_NAME), 'rb').read()
	read_dt_lba = layout.files[READ_DT_NAME][1]

	for n in a.move_entry:
		if not 0 <= n < TOC_ENTRIES:
			sys.exit('no READ_DT entry %d' % n)
		size, lba = exe.toc(n)
		at = (lba - read_dt_lba) * SECTOR
		blob = read_dt[at:at + size]
		path = os.path.join(a.out, 'E%03d.BIN' % n)
		open(path, 'wb').write(blob + b'\0' * (-len(blob) % SECTOR))
		new = layout.add_file('E%03d.BIN' % n, path)
		exe.set_toc(n, size, new)
		print('entry %d: %d bytes, sector %d -> %d' % (n, size, lba, new))

	exe_path = os.path.join(a.out, EXE_NAME)
	open(exe_path, 'wb').write(exe.data)
	layout.replace_source(EXE_NAME, exe_path)
	xml = os.path.join(a.out, 'dsf-ps1.xml')
	image, cue = os.path.join(a.out, 'dsf-ps1.bin'), os.path.join(a.out, 'dsf-ps1.cue')
	layout.write(xml, image, cue)
	subprocess.run([a.mkpsxiso, '-y', xml], check=True)
	print('image: %s (%d sectors)' % (image, os.path.getsize(image) // 2352))


if __name__ == '__main__':
	main()
