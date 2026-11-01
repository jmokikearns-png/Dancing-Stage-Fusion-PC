"""Decode a stock song's STR.BIN stream from the disc image to a WAV (for a local video edit only)."""
import struct, sys, wave
import numpy as np
BIN = '/home/user/work/original/disc/Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).bin'
f = open(BIN, 'rb')
def sectors(lba, n):
    out = bytearray()
    for i in range(n):
        f.seek((lba + i) * 2352); raw = f.read(2352)
        out += raw[24:24 + 2048]
    return bytes(out)
exe = sectors(24, 636928 // 2048)
load = struct.unpack_from('<I', exe, 0x18)[0]
def w(addr): return struct.unpack_from('<I', exe, 0x800 + addr - load)[0]
F0 = [0, 60, 115, 98, 122]; F1 = [0, 0, -52, -55, -60]
def decode_channel(data):
    out = np.empty(len(data) // 16 * 28, np.int16); s1 = s2 = 0; k = 0
    for o in range(0, len(data), 16):
        h = data[o]; sh = h & 15; fl = min(h >> 4, 4)
        if sh > 12: sh = 9
        for b in data[o + 2:o + 16]:
            for nib in (b & 15, b >> 4):
                v = (nib << 12) & 0xFFFF
                if v & 0x8000: v -= 0x10000
                v = (v >> sh) + ((s1 * F0[fl] + s2 * F1[fl] + 32) >> 6)
                v = max(-32768, min(32767, v)); out[k] = v; k += 1; s2, s1 = s1, v
    return out
if __name__ == '__main__':
    if sys.argv[1] == 'list':
        for i in range(20):
            r = 0x800AFE38 + 44 * i
            print(i, hex(w(r)), w(r + 8))
        sys.exit()
    n = int(sys.argv[1]); dst = sys.argv[2]
    lba, size = w(0x800932B8 + 16 * (n - 1)), w(0x800932B8 + 16 * (n - 1) + 4)
    data = sectors(lba, (size + 2047) // 2048)[:size]
    L, R = bytearray(), bytearray()
    for b in range(0, len(data) - 0x7FFF, 0x8000):
        L += data[b:b + 0x4000]; R += data[b + 0x4000:b + 0x8000]
    l, r = decode_channel(bytes(L)), decode_channel(bytes(R))
    pcm = np.stack([l, r], 1)
    with wave.open(dst, 'wb') as wv:
        wv.setnchannels(2); wv.setsampwidth(2); wv.setframerate(44100); wv.writeframes(pcm.tobytes())
    print('stream', n, 'lba', lba, 'size', size, 'seconds', len(l) / 44100)
