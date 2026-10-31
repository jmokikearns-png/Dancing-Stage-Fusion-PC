# Dancing Stage Fusion: the PS1 version (back-port)

The PC port's game changes (extra songs, TYPE3 colours, the Heaven and
Earth unlock, the RECORDS / WORKOUT fixes, autoplay) put back onto a
PlayStation disc. No PC settings, no high resolution, no QUIT.

## Release model

Same rule as the PC port: no Konami data and no licensed music is ever
distributed. The release is a builder that runs on the player's machine:
their own Redump image of SLES-04163, plus the song pack inputs they provide
(simfiles, audio, art), in, a patched disc image out. Nothing it outputs is
shared by this project.

## Findings (2026-10-30)

All measured on the PC runtime (Linux debug build, OpenBIOS) unless noted.

- **Save data: no change needed.** The save keeps records for up to 32 songs
  ([0x800B1AFC] + 0x90 + idx * 0x50, idx < 0x20; dsf-extra-songs.md).
- **Resident tables.** The PC plugin keeps the extended song, stream,
  graphics, group, entry and XA tables in 0x1580 bytes (M_SIZE in
  dsfcore_extrasong.cpp) plus menu strings and small tables, all in mod
  memory at 0x9F000000, outside the PS1's 2 MB. On a disc they need a home
  in RAM along with the MIPS code that replaces the C++ hooks: ~10 KB
  estimated.
- **Resident home: 0x80098AA8..0x8009DC04 (20.3 KB), inside the
  executable's image.** Zero in the file (SLES_041.63) and in every RAM
  snapshot; the code only addresses the table that starts right after it
  (0x8009DC04, from 0x8007A290). Filled with markers at the music select
  and played through a song and its results: untouched (2026-10-30). Data
  put there loads with the executable and the file keeps its size.
- **Not 0x800DEA00..0x800E8000.** It looked free (below), but the startup
  code (0x8002397C) clears BSS 0x800B4C70..0x800DE9E8 and then calls
  InitHeap (0x8003D6C8) from 0x800DE9EC up to the stack: that block is the
  heap's start. Untouched in the tests, but anything allocating would use it.
- **Free RAM candidate (heap start, see above): 0x800DEA00..0x800E8000 (37.5 KB).** Zero in every
  RAM snapshot taken at the music select (twice), mid-song (twice) and the
  results. Then filled with a marker pattern at the music select (every 4th
  byte) and played through a song, its results and back: no byte changed.
  Not yet checked: boot and the attract demo, OPTIONS, RECORDS, WORKOUT,
  DOUBLE / 2P, the memory card screens, a song failed.
- **Pack sizes with the 12 extra songs' pictures in them** (each picture
  compressed with the game's LZ, port/tools/extrasong/konami_lz.py):
  music select e201..e205 172.9..174.1 KB -> ~256 KB (buffer 192 KB: ~64 KB
  short); results e206..e210 48..50 KB -> ~65 KB; WORKOUT results e181
  89 KB -> 131 KB. Their buffers still to measure. The 302 KB block free
  during the music select ends exactly at the select buffer's start
  (0x80174000), so loading the pack ~64 KB lower is the first thing to try.
- **Music select packs are the tight spot.** e201..e205 (one per language)
  are 168.4..169.5 KB and load into a 192 KB buffer (0x80174000..0x801A4000):
  ~23 KB spare. The PC build keeps them at stock size by serving the extra
  songs' banners and plates from outside RAM; on a disc those pictures have
  to be in the packs, and 12 songs' banners and plates very likely need more
  than 23 KB (to measure: their compressed sizes from the pack builder).
  During the music select a 302 KB block at 0x80128800 is zero in both
  snapshots (it is used during songs), so the select pack buffer can likely
  move there for that screen; to be confirmed with a marker test like above.
- **Other free blocks** (zero in every phase): 0x80098AA5..0x8009DC04
  (20.3 KB), 0x800BE094..0x800C4748 (25.7 KB), 0x800C5434..0x800CC44C
  (28.0 KB), 0x801BDD3E..0x801C4000 (24.7 KB), 0x801CA000..0x801D0000
  (24.0 KB). Unverified beyond the snapshots. Below 0x80010000 is the
  BIOS's: these measurements ran on OpenBIOS, so that area says nothing
  about Sony's BIOS and is off limits.

## The disc

- The game finds its data by sector address: the READ_DT TOC (0x80092B50,
  235 x {size, lba}), the stream table (0x800932B8) and the XA preview table
  (0x800934E4), all in the executable. New songs' sectors can be appended
  after the last sector and those tables pointed at them, as the PC pack's
  media.bin already does; no file system changes beyond the executable.
- mkpsxiso 2.30 round-trips the disc bit-identically (ps1-tooling.md), so
  the patched SLES_041.63 can go back through mkpsxiso, and the new sectors
  appended after.
- To check: the PC pack's media.bin sectors (data Mode 2 Form 1, XA audio
  Form 2) must carry correct subheaders and EDC/ECC for a real drive; the
  PC runtime does not check them.

## The code

- Each C++ mod hook (port/core/dsfcore_*.cpp) becomes MIPS: the table
  setup done at install becomes static data built into the executable by
  the builder, and the runtime hooks (LZ_GUEST, AVAIL_GUEST, WORKOUT_SLOT,
  the TYPE3 instruction plugins, the RECORDS loop fix, the unlock) become
  small routines in the resident block, reached by patched jal/j words.
- Toolchain: mipsel-linux-gnu-gcc / -as are installed.

## Test bench

`psx-beetle` (Beetle PSX, the psxrecomp oracle; docs/beetle-linux.md) is
built at tools/recomp-src/psxrecomp/runtime/build-beetle/psx-beetle and
boots the original disc headless to the language screen (2026-10-30):

    SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy \
      psx-beetle <bios> --disc <cue> --port 4380

with the same debug protocol as the PC runtime (ping, screenshot, read_ram,
input). OpenBIOS stands in for the BIOS here; real hardware or a Sony BIOS
in DuckStation is the final check.

## Builder

`port/tools/ps1disc/build_ps1disc.py` (2026-10-30, stage one): unpacks
nothing itself (takes dumpsxiso's folder and XML), patches the executable,
adds files after the last sector and rebuilds with mkpsxiso, every stock
file at its sector. `--move-entry 201` copies the English music select pack
to a new file at sector 105486 and points the table of contents there:
in psx-beetle the game seeks to 105486 (CD command trace) and the music
select shows as stock (`beetle_select.py`). So a patched executable plus
appended data works end to end on an accurate emulator.

## Deluxe test 1 (2026-10-30)

The PS1 version is named Dancing Stage Fusion Deluxe.  Test 1 =
STEP MARK COLORS TYPE3 (port/tools/ps1disc/deluxe/): payload.c + entry.S
linked at 0x80098AB0 (16-aligned: the linker aligns the output section, and
a blob written at 0x80098AA8 put every entry 8 bytes off -- found as stack
corruption in the attract demo); exe branch flips for the three `== 1`
tests, j stubs at the note-palette site and both builder exits, the save
check 2 -> 3; overlay 227 (raw in READ_DT, LBA 28460, loads at 0x800E8000):
mod-3 stepping and a jal to the label routine.  dlxbuild.c (Windows .exe)
patches a copy of the player's image in place and re-seals EDC/ECC, no
mkpsxiso needed while nothing moves.  Beetle: menu cycle, TYPE3 song, kept
across restart, 160 s of attract demo clean.  Not yet: DuckStation with a
Sony BIOS, hardware.

## Next steps

1. Finish the RAM map: marker tests of the resident block, the select
   area below 0x80174000, and the results / workout buffers, in every mode.
2. One extra song end to end on the disc: its tables in the resident block
   and the executable's constants pointed at them (what dsfcore_extrasong's
   install does at run time, done by the builder), its sectors appended
   (media.bin's 2336-byte sectors as an XA "mixed" file), its pictures in
   the packs; music select, play, results, in psx-beetle.
3. Port the remaining hooks to MIPS, one feature at a time.
