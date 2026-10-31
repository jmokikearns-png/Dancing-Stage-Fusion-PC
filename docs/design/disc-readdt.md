# READ_DT.BIN analysis (key: disc-readdt)

Status: Phase 1 identification COMPLETE (resumed run, 2026-10-16). Every TOC entry is identified. Unidentified leftovers come
to 25 small leaves (10.5 KB in total).
Analysis dir: `/home/user/work/analysis/disc-readdt/` (disc-derived, analysis-only, never to be distributed).
Scripts: `/home/user/work/analysis/disc-readdt/scripts/`, run with `/home/user/work/tools/venv-disc/bin/python -I <script>`.

## 0. TL;DR
* READ_DT.BIN does **not** start with a TOC. Its file table lives in the executable **SLES_041.63 at 0x80092B50**
  (file offset 0x79350): 235 records of `{u32 size_in_bytes, u32 absolute_LBA}`, terminated by `{0,0}`. Entries are
  contiguous and sector-aligned. The unused tail of each entry's last sector is 0xFF. Sectors 10573..13999 are 0xFF padding.
* Compression is the **Konami LZ** scheme (the same algorithm as the Konami System 573 "comp573" decoder). 3,459 LZ streams
  decode cleanly. 3,440 of them produce valid TIMs, 1 produces a TIM with a bad length field, and 18 produce the dancer
  model blobs. Every stream ends with its 0xFF terminator at its slot end (<=3 pad bytes).
* There are three container formats, all demonstrated: **PACK** (u32 offset table), **LZSET** (N LZ streams + tail offset
  table), and **SONGTAB** (20 x {id, lzA, lzB}). A few blobs are unaligned back-to-back LZ streams.
* Content: 4 VAB + 16 VAG sound effects/voices, 3,785 TIMs (4/8 bpp only), multilingual Latin-1 text (en/fr/de/it/es),
  18 dancer models (custom format) + 5 TMDs, 58 dance-motion banks, 9 code overlays, and **all 20 songs' step charts and
  tempo maps**.
* **Step charts were found and decoded** (131 charts = 20 songs x {single Beginner/Light/Standard/Heavy} + 17 songs x
  {double Light/Standard/Heavy}). Each chart stores one u32 per beat, one byte per 16th note, and one bit per panel. The
  tempo map's (position, time) pairs use 1/4096-measure positions and **1/75 s ticks**. BPMs computed this way match the
  published BPMs (e.g. PARANOIA MAX 190, ORION.78 200, 321STARS 192, AM-3P 130, Come Into My World 125, Freak Like Me 92).
* No song audio is in READ_DT.BIN. A second table in the EXE (0x800932B8) lists 23 streams in **STR.BIN** with a 44100
  value, which is probably the song audio (a lead for the STR.BIN task).

## 1. Extraction (verified)
* `scripts/extract_readdt.py`: LBA 18000..31999 from the .bin. It checks sync, BCD MSF, mode byte 2, the subheader copy and
  the EDC on every sector.
  Result: 14000 sectors, 0 EDC errors. Subheaders: `file=0 ch=0 submode=0x08 ci=0` x13999, plus the last sector with
  submode 0x89 (EOF|EOR|data).
  Output: `READ_DT.BIN`, 28,672,000 B, SHA-1 `96f16598d5092f021b80b96aeed512776e1755c0`. A re-run this session gave the
  identical SHA-1.
* `SLES_041.63` was copied from LBA 24 (636,928 B, SHA-1 `ade130f4ab7cf5291e6a1606cb8d2fd51a01b1da`; 'PS-X EXE',
  t_addr 0x8001A000). Re-extracted and compared in this session.

## 2. Structure
### 2.1 No in-file header
Sector 0 (entry 0) is a 2048-byte block of 16-bit colours (`0000 ffff defb bdf7 9cf3 5aeb ...` = grey ramp). It is not
an offset table. Of its 64 rows of 16 u16, 59 start with 0x0000 (the transparent colour), so it is probably a CLUT bank
(64 x 16-colour palettes). Confidence: uncertain on purpose, verified on layout.

### 2.2 The TOC is in the executable (verified)
```
SLES_041.63 file off 0x79350 (RAM 0x80092B50):
00079350: 0008 0000 5046 0000 4071 0200 5146 0000   {0x800, 18000} {0x27140=160064, 18001}
00079360: 70c4 0000 a046 0000 b074 0200 b946 0000   {50288, 18080} {160944, 18105}
...
00079aa8: 0000 0000 0000 0000                       terminator after 235 records (RAM 0x800932A8)
```
* Units: the first word is the **size in bytes** (e.g. entry 1 = 160064 equals the VAB header's own file-size field).
  The second word is the **absolute disc LBA** (18000 = READ_DT start). Relative sector = LBA-18000, byte offset =
  rel*2048.
* Code consumes the table by record address. For example, 0x80089f94..a4 loads 0x80092B58/0x80092B60 (records 1/2), and
  0x800889dc/0x80088b94 use 0x80092C20/0x80092C28 (records 26/27). Found by `scripts/exe_xref.py`, a lui/addiu pair scan
  checked with capstone in `scripts/disasm.py`.
* Alignment: every entry starts on a sector boundary, and record[i+1].lba = record[i].lba + ceil(size/2048) for all 234
  gaps (gap column = 0 in `entries_raw.csv`). The slack after each entry's data is 0xFF (233 entries 'ff'; 2 'none',
  i.e. their size fills the last sector exactly).
  Entries end at rel. sector 10573. Rel. sectors 10573..13999 (3427 sectors, 7.0 MB) are all 0xFF.
  All-zero sectors (10) lie inside entries 229/231 (BSS-like zero runs in overlays).
* CSV: **`entries.csv`** (one row per record: table address, LBA, rel sector, byte offset, size, sectors, entropy, top
  container, LZ/TIM counts, leaf kinds, group label, language). Raw version: `entries_raw.csv`.

### 2.3 Containers and compression (verified)
* **Konami LZ** (`scripts/konami_lz.py`). The control byte is read LSB-first. Flag 0 = literal. Flag 1 = byte d:
  d<0x80 -> 2-byte ref, dist=((d&3)<<8|next), len=(d>>2)+3; 0x80..0xBF -> dist=(d&15)+1, len=(d>>4)-6;
  0xC0..0xFE -> literal run of d-0xB8 bytes; 0xFF = end.
  Source: the same algorithm as `decode_lz` in 987123879113/gobbletools `sys573/sys573tool/comp573.pyx`
  (https://github.com/987123879113/gobbletools/blob/master/sys573/sys573tool/comp573.pyx; it writes the lengths as
  "+2 then copy len+1", which equals ours).
  Evidence: 3,459 streams decode with no error. The terminator lands at the end of the slot (<=3 pad bytes) in every
  case. 3,440 outputs are structurally valid TIMs whose sizes equal header-computed sizes (e.g. 77344 = 8+12+512+12+76800
  for 320x240 8bpp). Of the rest, 1 is entry 58/0 (a TIM with a bad length field) and 18 are the dancer-model blobs of
  entries 33-50. Stream count: 3,257 inside PACK/LZSET + 200 in SONGTABs + 2 concatenated = 3,459.
* **PACK**: u32 offset table at the start. The first offset equals the table size, and 0 = empty slot. A sub-block runs
  from its offset to the next larger offset. Offsets are not always in ascending slot order (entries 168-175).
  Example, entry 78: `10000000 4c1c0000 90200000 44150100`.
* **LZSET**: `{u32 N, u32 tbl, u32 X, u32 tbl+4N}`, then N LZ streams from 0x10, then N+X u32 offsets at `tbl`. X extra
  items are stored raw. The blob length is exactly tbl+4(N+X). Example: entry 51 holds 6 streams -> six 320x240 8bpp TIMs.
* **SONGTAB** (entries 28-32, slot 1, 49,744 B, identical in all 5 copies): 20 x `{u32 id/flags, u32 offA, u32 offB}`
  (first offA = 0xFC). Each offset starts an LZ stream: 20 x 88x28 4bpp + 20 x 360x40 4bpp TIMs (song title strips).
  The id words (0x1d7, 0x701c, 0x19, ... 0x8015) are uninterpreted.
* **LZ_CONCAT**: entry 58, slot 4 (45,596 B) = two back-to-back unaligned LZ streams (21767 + 23828 B) -> two 320x240
  8bpp TIMs.
* Entropy (whole entry, bits/byte): VAB/VAG 6.3-6.6 (ADPCM), LZ-packed graphics 6.8-7.8, raw motion banks 5.9, code
  4.6-5.8, stubs 0.02-0.16. Per-sector values are in `sector_scan.csv`.

## 3. Entry map (verified unless marked)
| entries | n | content | evidence |
|---|---|---|---|
| 0 | 1 | CLUT bank? 64 x 16 colours | colour ramps; 59/64 rows start with 0x0000 |
| 1-4 | 4 | **VAB** 'pBAV' v7 (1/2/1/2 programs, 7/21/14/32 tones, 7/14/11/31 VAGs) | header + VAG-size table sums exactly to the entry size; vgmstream reads them |
| 5-15 | 11 | **VAG** 'VAGp' `fai_01..fai_11` (16.5-22.05 kHz mono) | header data size + 48 = entry size; `audio/e005.wav` decodes (rms 7327, no clipping) |
| 16-20 | 5 | **VAG** `res_01..res_05` (13-17.2 kHz) | same; `audio/e016.wav` |
| 21-25 | 5 | Lesson mode: PACK[24] = {313 strings, language LZSET, 22 LZ TIMs}, one per language **en,fr,de,it,es** | decoded strings, e.g. string 1 "If you have never played Dancing Stage start with this lesson!" / "Si tu n'as jamais joué à Dancing Stage..." / "Wenn Sie nie..." / "Se non hai mai..." / "¡Si nunca has jugado..."; Latin-1 |
| 26 | 1 | 36 animation frames 152x88 8bpp + misc TIMs | `png/sheet_e026.png` |
| 27 | 1 | Options screen: TIMs for all 5 languages + 5 string tables + font | `png/sheet_e027.png` (EXIT/QUITTER/BEENDEN/ESCI/SALIR) |
| 28-32 | 5 | Records / high-score screen per language: strings ("Displays high scores for each track."), SONGTAB, 29 LZ TIMs | text + LZ->TIM |
| 33-50 | 18 | Dancer characters: LZ PACK{16 x 64x64 4bpp textures, ~40 KB model data} | `png/sheet_e033.png` (face/clothes textures). The model format is custom (not TMD): header `0,0,0x1c,0x25(=37 parts?)` then (off,count,0x1000) triples. **uncertain** |
| 51-67 | 17 | Stage background sets: LZSET of 6 x 320x240 8bpp TIMs (1-2 CLUT rows) | `png/sheet_e051.png`. Entry 58/0 is a TIM whose image-block length field (38412) is half the real data (76812) |
| 68-167 | 100 | **20 songs x 5 language copies** (see section 5) | `png/songs/sheet_songs.png` |
| 168-175 | 8 | **Dance motion banks**: PACK[17]; each slot is an 'S' bank `u16 0x53, u16 n, u32 8, n x {name_off, data_off}`, names `normal_m5..`, `capoera1a..m`, `hiphop1a..n`, `jazz1`, `jazz2`, `lock1`, `mhouse1`, `n31`, `sino_`, `soul1`, `soul2`, `thouse2`, `thouse3`, `y11`, `y31`, `hopping1`, `hiphop2` | header test passes for all 55 slots (`MOTION_BANK` in `leaves.csv`). The motion data encoding itself is not analysed |
| 176-180 | 5 | Character select (16 portraits 256x256 4bpp; names YUNI, EMI, AKIRA, RAGE, ...) per language | `png/sheet_e176.png` |
| 181-185 | 5 | Workout results screen | `png/sheet_e181.png` |
| 186-190 | 5 | Workout setting screen | `png/sheet_e186.png` |
| 191-195 | 5 | CLEARED / FAILED / GAME OVER banners | `png/sheet_e191.png` |
| 196-200 | 5 | Gameplay / How-to-play: arrow and receptor TIMs, a **TMD** (id 0x41, 22 objects, all tables in range), a motion bank (`S`, 5 entries), how-to-play TIMs | `png/sheet_e196.png` |
| 201-205 | 5 | Music select: song banners 256x80, 4bpp | `png/sheet_e201.png` |
| 206-210 | 5 | Results screen: AA/A/B/C/D, PERFECT/GREAT/GOOD/ALMOST/BOO/MAX COMBO, song-title strips | `png/sheet_e206.png` |
| 211-215 | 5 | Title screen (320x256 8bpp logo layers) + mode menu (GAME/DOUBLE/WORKOUT/LESSON MODE...) | `png/sheet_e211.png` |
| 216-220 | 5 | Warning screen 640x480 4bpp per language | `png/sheet_e216.png` |
| 221-225 | 5 | Title-screen layers, 5 byte-identical copies (SHA-1 e15bc0a5decf) | SHA-1 |
| 226-229 | 4 | **Code overlays loaded at 0x800E8000**. Entry 226 = boot overlay: CAUTION screen, KONAMI logos, "SOFTWARE TERMINATED / CONSOLE MAY HAVE BEEN MODIFIED" text, memory-card prefix "BESLES-04163system", 14 embedded LZ TIMs (640x480 4bpp). Entry 228 has "WORKOUT DIARY / HIGH SCORES" | jal/prologue voting gives base 0x800E8000 (`scripts/overlay_base.py`); many jal targets fall in the main EXE (125-267); `png/sheet_e226_overlay_tims.png` |
| 230, 233, 234 | 3 | **Code overlays loaded at 0x801C4000**. 230 = licence/credits text ("I Believe In A Thing Called Love", ISRC...). 233 contains "inst demo" and MDEC_rest / MDEC_in_sync strings (movie/MDEC helper) | voting base 0x801C4000; function-pointer tables 0x801Cxxxx at the start |
| 231, 232 | 2 | Stub overlays: `08 00 e0 03 01 00 02 24` (jr ra; li v0,1), then zeros | hexdump |

No PS-X EXE header, SEQ/SEP ('pQES'), standalone MDEC/BS frame, or raw TIM at an entry start was found, except as listed
above. A scan of all leaves found 5 TMDs (entries 196-200 only).

## 4. TIM statistics (verified; `tim_stats.txt`, `tims_all.csv`)
* 3,785 TIMs: 3,174 x 4 bpp (CLUT 16 x 1 row) + 611 x 8 bpp (CLUT 256 x 1 row: 555, 2 rows: 55, 3 rows: 1).
  No 16- or 24-bit TIMs.
* Most frequent sizes: 92x18 4bpp (525, text strips), 64x64 4bpp (373), 64x16 (315), 360x40 4bpp (300, song titles),
  320x240 8bpp (199, backgrounds), 256x256 4bpp (131), 176x55 / 256x80 4bpp (song banners, 100 each),
  640x480 4bpp (warning/caution screens).
* VRAM x of the image rectangle: 0 (2035), 996 (525), 960 (435), 1008 (315)... CLUT position: (0,0) for 1,829 TIMs
  (patched in at runtime, presumably). The rest sit in VRAM rows y=496-511 (e.g. (320,508-511), (992-1008,496-498)).
* 39 standalone 512-byte leaves look like 256-colour CLUTs (first colour 0x0000). Classed as `CLUT256?`.
* PNGs from my own converter (`scripts/tim2png.py`: 4/8 bpp indexed with CLUT row 0, 15-bit -> RGBA, 0x0000 =
  transparent) are in `png/` (contact sheets per entry and `png/single/`). All rendered images look right (no shear or
  stride errors). Some 4 bpp textures look white with CLUT row 0; their real palette row is selected at runtime.

## 5. Songs and step charts (evidence-backed)
### 5.1 Song group layout (entries 68+5g .. 72+5g, g = 0..19)
Outer PACK[4]:
* slot 0: PACK{ tempo map, charts... }
* slot 1: event list
* slot 2: LZ -> 320x240 8bpp background TIM, which identifies the song (`png/songs/`)
* slot 3: LZSET of 14 UI TIMs + 1 raw 516-byte item

Slots 0-2 are byte-identical in all 5 copies of a song (151+20+20 leaves). Slot 3 differs per copy but is identical across
the 20 songs for the same copy index. So the copies are per-language (en,fr,de,it,es), and only the UI graphics change.
Song order (g): 0 I Believe In A Thing Called Love, 1 Come Into My World, 2 DIVE, 3 Freak Like Me, 4 Gotta Get Thru This,
5 Healing Vision ~Angelic mix~, 6 KAKUMEI, 7 Keep On Movin', 8 Can't Stop Fallin' In Love -Speed Mix-, 9 Make It Better,
10 Mickey, 11 Move Your Feet, 12 ORION.78 (civilization mix), 13 PARANOIA MAX ~DIRTY MIX~, 14 AM-3P, 15 Still In My Heart,
16 A Stupid Barber, 17 Sunlight, 18 321 STARS, 19 Try 2 Luv. U. These are the PS1 version's 20 songs (RemyWiki lists 20).

### 5.2 Chart format (verified structure; `scripts/charts.py`, `chart_stats.csv`, `charts_dump_song02.txt`)
```
e133 (PARANOIA MAX) chart @ READ_DT+0xA2E054: e0040000 00000100 00000000 x14 | then 296 x u32
u32 size (= 64 + 4*beats, true for all 131 charts)
u32 kind: byte0 style (0 = single, 2 = double), byte1 difficulty code, byte2 = 1 (always)
14 x u32 0
beats x u32: one u32 per beat (quarter note); byte k (k=0..3, little-endian order) = step mask on the k-th 16th of the beat
```
Evidence:
* **16th-note grid.** Beginner (code 4) uses only byte 0 (1903/1903 steps on the beat). Light uses byte 0 (5622) and
  byte 2 (219, 8th notes), with bytes 1/3 almost unused (2/12). Heavy uses byte 1 = 200 and byte 3 = 329 (16ths).
* **Length agrees with the tempo map.** The number of beats equals 4 x (last tempo-map measure) - 8 for every song
  (e.g. DIVE: 240 beats vs 62 measures).
* **Single vs double.** In single charts the high nibble always equals the low nibble (15,384/15,384 step bytes, e.g.
  0x11, 0x22, 0x44, 0x88). Double charts use 8 independent bits (0x01..0x80).
* **Difficulty codes.** Code 4 = Beginner (single only), 0 = Light, 1 = Standard, 2 = Heavy. Step counts rise strictly in
  the order 4 < 0 < 1 < 2 for all 37 song x style sets. Songs 0, 1 and 4 have no double charts. RemyWiki (via search
  summary) confirms the tiers Beginner/Light/Standard/Heavy and that PS1 Fusion has no freeze arrows.
* Step-byte popcounts: single 1/2/4 arrows, double 1/2/8. No 3-arrow values occur.
* **Special 0xFF / 0xF mask.** It occurs 89 times in single and 86 in double, only in songs 4 and 12. The counts per
  difficulty are identical between single and double (10/35/41), so it is a layout-independent marker, not a real
  4-/8-panel jump. Its meaning is **unknown**.
* **Bit -> arrow mapping: uncertain (statistical).** Doubles per-bit totals are b0 1231, b1 2474, b2 1085, b3 1486,
  b4 2421, b5 1183, b6 997, b7 1502. The two most-used panels are b1 and b4. The most frequent double jump is 0x12
  (b1+b4, 199), and the most frequent single jump is 0x3 (b0+b1, 570), ahead of 0xC (249).
  If the low nibble is P1 (left pad), this fits **bit0 = Left, bit1 = Right**, with bits 2/3 = Up/Down in an
  undetermined order: centre panels used most, L+R the commonest jump, centre jump in doubles.
  This is NOT the SSQ-style L,D,U,R order.
  Next step: confirm from the gameplay overlay's step parser, or by matching a published chart.

### 5.3 Tempo map (verified against published BPMs)
```
e133 tempo @ READ_DT+0xA2E030: 24000000 | 00000000 a4ffff7f | 00100000 02000000 | 00500000 7d010000 | 00c00400 be1b0000
```
`u32 size`, then pairs `(u32 pos, u32 time)`: pos in 1/4096 measure (0x1000 = 1 measure), time in **1/75 s** ticks.
The map is piecewise linear. In 2 songs the time word carries a 0x80000000 bias (e.g. 0x7fffffa4 = -92, 0x80000097 = 151).
Computed BPMs (4 beats/measure), with the published value in parentheses:
PARANOIA MAX 190.0 (190), ORION.78 200.0 (200), 321 STARS 192.0 (192), AM-3P 130.1 (130), Come Into My World 125.0 (125),
Freak Like Me 91.8 (92), Healing Vision 49/196 (46-196), I Believe... 128.1 then 67.2 at the end (68-128).
The published values come from RemyWiki CS_DS_Fusion_PS1 (via web-search summaries; the site itself was blocked by the
proxy). Others: DIVE 155, Can't Stop Fallin' 170, KAKUMEI 148 with slow-downs, Mickey ~150 and Move Your Feet ~119
(variable tempo, typical of licensed songs). The table is in `tempo.csv`. Song lengths are 71-110 s.

### 5.4 Event list (slot 1): uncertain
`u32 size, u16 5, u16 k (k in 2..17), u32 count, -99999 (0xfffe7961), 0, count x u32 positions (1/4096 measure)`, then
records such as `0x100000ff 0x10000004 0x00ffff0a`. The values k = 2..17 match the motion-bank slot indices 1..16 of
entries 168-175, so a dancer-choreography timeline is plausible. Not proven.

## 6. Leads for other tasks
* SLES_041.63 at 0x800932B8 holds 23 records `{u32 LBA, u32 size, u32 44100, u32 127}` with LBAs 32001..89875, all inside
  STR.BIN (LBA 32000). Sizes are 3.6-5.7 MB. At a 44.1 kHz stereo SPU-ADPCM rate (50,400 B/s) that is 72-113 s, the
  length of a song (compare tempo-map lengths 71-110 s). This is probably the streamed song audio: 20 songs + 3 others.
  **uncertain**, for the STR.BIN task.
* Song audio is not in READ_DT.BIN (only SFX VAB/VAG).

## 7. Files (all under /home/user/work/analysis/disc-readdt/)
* `READ_DT.BIN`, `SLES_041.63` (extracts)
* `entries.csv` (final TOC + classification), `entries_raw.csv`, `entry_summary.csv`, `sector_scan.csv`
* `nodes.csv` (container tree), `leaves.csv` (leaf identification + test used), `leaves/` (raw unidentified-at-walk leaves)
* `tims.csv`, `tims_all.csv`, `tim_stats.txt`
* `chart_stats.csv`, `tempo.csv`, `charts.csv` (first-pass candidates), `charts_dump_song02.txt`
* `png/` (contact sheets per entry, `png/songs/` 20 backgrounds + sheet, `png/single/`), `audio/e005.wav`, `audio/e016.wav`
* Scripts: `extract_readdt.py`, `sector_scan.py`, `toc.py`, `exe_xref.py`, `disasm.py`, `konami_lz.py`, `analyze.py`,
  `readdt_lib.py`, `tree.py`, `tim_scan.py`, `tim2png.py`, `make_png.py`, `song_banners.py`, `charts.py`, `classify.py`,
  `overlay_base.py`, `find_charts.py`

## 8. Open questions / next steps
1. Arrow-bit order (bits 2/3 = Up/Down, and confirming bits 0/1 = L/R). Disassemble the step reader in the 0x800E8000
   gameplay overlay (227-229), or match one published chart.
2. Meaning of the 0xFF step marker (songs 4 and 12), and of the event list (slot 1).
3. Dancer model format (entries 33-50, /1) and motion-bank data encoding. Either can be skipped for a StepMania port that
   uses its own visuals.
4. SONGTAB id words, the 516-byte raw items in the language LZSETs, entry 0 and the 512-byte CLUT-like leaves.
5. For Phase 2, the chart converter: per song, read tempo.csv pairs -> #BPMS/#OFFSET (the time of the beat-0 position),
   and chart u32/beat -> 16th-note rows in .sm. Link each song to its audio stream once STR.BIN is mapped.

## Sources
* Konami LZ reference: https://github.com/987123879113/gobbletools/blob/master/sys573/sys573tool/comp573.pyx
* Song list, BPMs, tiers, no freeze arrows: https://remywiki.com/CS_DS_Fusion_PS1 (seen through web-search summaries;
  direct fetch blocked by the proxy). Also https://psxdatacenter.com/games/P/D/SLES-04163.html (no freeze arrows, per
  the search summary).
