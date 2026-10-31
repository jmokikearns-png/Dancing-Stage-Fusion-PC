#!/usr/bin/env python3
"""Write a solid-colour RGBA PNG (no dependencies).  make_png.py OUT W H R G B [A]"""
import struct, sys, zlib

def png(path, w, h, rgba):
    raw = b''.join(b'\x00' + bytes(rgba) * w for _ in range(h))
    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    data = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
    with open(path, 'wb') as f:
        f.write(data)

if __name__ == '__main__':
    a = sys.argv[1:]
    png(a[0], int(a[1]), int(a[2]), [int(x) for x in a[3:6]] + [int(a[6]) if len(a) > 6 else 255])
