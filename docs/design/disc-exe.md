# disc-exe: SLES_041.63 (main executable) analysis

Status: IN PROGRESS (resumed run; earlier partial outputs under /home/user/work/analysis/disc-exe/ are being re-verified).

## Extraction (verified)
- Source: LBA 24, 311 Form1 sectors, 636928 bytes -> /home/user/work/analysis/disc-exe/extract/SLES_041.63
- SHA-1 ade130f4ab7cf5291e6a1606cb8d2fd51a01b1da, MD5 101756f00fda88c6002799f7e1348555
- Header: "PS-X EXE", pc0 0x8002397C, gp0 0, t_addr 0x8001A000, t_size 634880 (0x9B000), s_addr 0x801FFFF0, region "Sony Computer Entertainment Inc. for Europe area"
- Text segment flat binary: extract/SLES_041.63.text.bin (file offset 0x800 -> vaddr 0x8001A000; vaddr = file_off + 0x80019800)
- Script: scripts/extract_exe.py (python3 -I)

(sections below are filled in as analysis proceeds)

## Progress checkpoint (resumed run) - verified so far
- PsyQ: banner "Library Programs (c) 1993-1997 Sony Computer Entertainment Inc." @0x800A78B8; $Id intr.c 1.75 1997/02/07 @0x8001E674, sys.c 1.140 1998/01/12 @0x8001E7A4 (libgpu), bios.c 1.86 1997/03/28 @0x8001EC2C (libcd). Large-signature matches intersect only at PsyQ 4.4 (LIBGPU SYS.OBJ 12080 B: 4.4/4.5; LIBCD BIOS.OBJ 5984 B: 4.1-4.4; LIBPAD PADMAIN 3536 B: 4.4/4.5).
- READ_DT TOC @0x80092B50: 235 x {u32 size, u32 lba}; contiguous 18000..28572, slack 0xFF in all 233 partial sectors, 28573..31999 all 0xFF padding (scripts/verify_readdt_toc.py, slack_check.py).
- STR.BIN stream table @0x800932B8: 23 x {lba, size, 44100, 127}; each stream followed by exactly 10 zero sectors; SPU-ADPCM (100% valid frame headers), stereo 0x4000 interleave (flag-6 frames at byte 16 and 16400) [interleave = hypothesis until decoded].
- STR.BIN voice banks: u32 table @0x80093428 (23 entries, one per stream) -> 11 distinct 536-sector banks, byte-identical copies (seek locality), 43 VAGp clips each (etc_, game_b/g, result_, failed_). Code 0x8002A0D4 indexes them (cat 5: bank+idx*8, 8 sectors; cat 6: bank+0xD0+idx*16, 16 sectors).
- XA segment table @0x800934F0: 25 x 12 B {ch<<24|start, file<<24|end, 0x178}; all 25 match disc subheader (file,channel) sector sets exactly.
- Disc song table @0x800AFE38: 20 x 44 B; ids not in master name table @0x800A56C4 (285 x 20 B); each points to 6-slot language group -> READ_DT TOC entries 68..167.
