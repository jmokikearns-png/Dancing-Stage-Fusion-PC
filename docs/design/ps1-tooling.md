# PS1 tooling for analysis and exactness validation (key: ps1-tooling)

STATUS: IN PROGRESS (resumed run, 2026-10-16). Sections a, b done; c, d being filled.

Confidence tags: **[verified]** = run/checked here on the real disc or binaries; **[sourced]** = from
cited documentation/source code; **[uncertain]** = inference, not confirmed.

## Layout

- Tools: `/home/user/work/tools/ps1-tooling/` (src/, install/, prefix/, ghidra/, jpsxdec/, docs/, logs/, dl/)
- Disc-derived outputs (never distribute): `/home/user/work/analysis/ps1-tooling/`
- Scripts: `/home/user/work/scripts/ps1-tooling/` + installer `/home/user/work/scripts/setup_ps1_tools.sh`

## a) Disc extraction - DONE

- mkpsxiso/dumpsxiso 2.30 from https://github.com/Lameguy64/mkpsxiso @54fb164 -> `tools/ps1-tooling/install/bin`. [verified]
- `dumpsxiso -x analysis/ps1-tooling/extract -s analysis/ps1-tooling/extract/dsf.xml <disc.bin>` [verified]
- `verify_extract.py` (own ISO9660+XA parser): all 6 files match direct sector reads. [verified, re-run 2026-10-16]
- Round trip: `mkpsxiso -y -o rebuilt.bin -c rebuilt.cue extract/dsf.xml` gives a BIT-IDENTICAL image
  (cmp equal; MD5 9cdceb1e..., SHA-1 1cd6d586..., CRC32 78592E6B = Redump). [verified]
- XA.STR is stored by dumpsxiso as 14848 x 2336 bytes (raw[16:2352]: subheader+data+EDC). [verified]
- Pitfalls [verified]: 7z cannot open raw .bin; bchunk default gives 2048 B/sector ISO -> 7z/isoinfo XA.STR = lossy
  2048-byte view (md5 7232e997...); bchunk 1.2.2 `-p` actually writes raw[0:2336] (help text says offset 24).
- XA.STR contains 2148 Form1 filler sectors, file=0xFF, submode 0x04 (audio bit, Form1), silent ADPCM. [verified]

## b) Media tools - DONE

- reference decoder `scripts/ps1-tooling/xa_ref_decode.py` (psx-spx algorithm) == ffmpeg == vgmstream bit-exact
  for file1/ch0 (7,483,392 samples); jPSXdec differs (max 17 LSB, RMS 3.1). [verified]
- all 12699 Form2 audio sectors: 4-bit stereo 37.8 kHz; no shift>12; no header-copy mismatches. [verified]
- STR.BIN: vgmstream TXTH (PSX codec, 2ch, 44100, interleave 0x4000) decodes stream 0; L/R corr 0.588 vs <0.06
  for other interleaves. ffmpeg/jPSXdec: no. [verified]

## c/d) in progress: Ghidra 12.1.4 + ghidra_psx_ldr 2026.10.03 installed, headless test running; PCSX-Redux
building; Mednafen 1.29.0 (apt) starts headless, stops on missing firmware/scph5502.bin [verified];
DuckStation = CC-BY-NC-ND-4.0 @d4e5a09 [verified from LICENSE+SPDX].
