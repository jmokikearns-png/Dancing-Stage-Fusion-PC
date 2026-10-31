# Extra songs in Dancing Stage Fusion (PS1, SLES-04163)

Adding a StepMania simfile to the game's own song list, as song 21+.
Code: `port/core/dsfcore_extrasong.cpp` (the in-RAM part, a mod plugin
`dsf.extrasong`) and `port/tools/extrasong/` (the package builder).
The package a player builds contains their song and data from their own
disc. It is personal and is never part of this project.

## Status (2026-10-17)

Tested with "Tough Enough" (Vanilla Ninja):

- **Linux build:** the song shows in the music select (#20 in the
  alphabetical list) with its banner, plate and difficulty stars. The
  preview plays. Gameplay runs: the stream comes from my sectors and the
  background shows. Failing a song and saving don't crash.
- **Windows build (embedded dsfcore):** the music select and gameplay
  both work, after the install race below was fixed.

Also checked on the Linux build (life gauge 0x800BA33A held at 1000 to
force clears): the results screen plate, the RECORDS high scores (mini
banner, grade, plays), DOUBLE mode and WORKOUT mode with its results.

Not tested yet: sync judged by playing on real hardware (the measurement
below is of the runtime's own picture and sound).

**Sync (fixed 2026-10-17).** The first pack had every step exactly one
measure late (the player reported it as badly out of sync). How it was
measured, on the Windows build with a debug core (`DSFCORE_DEBUG_PORT`): each frame's
screenshot is tagged with the SPU output sample clock; the recorded audio
is cross-correlated with the source song to place song time 0; an arrow
counts as arriving when it covers the receptors' fixed outline pixels.
The arrows were the right chart (DSF "Standard" = the simfile's Easy) on
the right beats, but late by 1.220 s, and one measure at 196.02 BPM is
1.224 s. A test build at half tempo was late by 2.444 s, one measure at
98 BPM, so the offset is in measures, not time. After the builder moved
the chart start: −0.008 s mean, 0.026 s spread over 17 arrows.

## How the game finds a song

| Table | Address | Shape | Notes |
|---|---|---|---|
| Song table | 0x800AFE38 | 20 × 44 B | see the song record below |
| Song accessors | 0x800759A0..0x80075EAC | | the count function at 0x80075BDC returns 0x14 |
| Music select list | | up to 32 entries | sorted case-insensitively by master-table title |
| Stream table | 0x800932B8 | 23 × {lba, size, 44100, 127} | select_stream(n) at 0x8002CA40 uses base 0x800932A8 (1-based) |
| Graphics table | 0x800AA154 | 20 × {id, banner item, plate item} | read by 0x80050BAC |
| XA preview table | 0x800934E4 | {ch<<24 \| start, file<<24 \| end, 0x178} | preview music ids 24..49 |
| Master name table | 0x800A3EA0 | 0x252 × {id, short*, title*, alt*, artist*} | 30 entries have short = 0 (blank) |
| READ_DT TOC | 0x80092B50 | 235 × {size, lba} | |
| Save records | [0x800B1AFC] + 0x90 + idx·0x50 | | idx < 0x20, so the save has room for 32 songs |

Song record words:

- w0: master id.
- w1: number.
- w2: stream number (1-based).
- w3: preview music id.
- w4: single ratings, as nibbles:
  - bits 0-3 Light
  - bits 4-7 Standard
  - bits 8-11 Heavy
  - bits 12-15 Beginner
- w5: double ratings.
- w6, w7: availability flags.
- w8: 0x0004xxxx; the low half is the song's length in measures, and play
  ends there (ORION.78: 80, tempo map to 79). Copying ORION's made every
  longer extra song stop at measure 80 (fixed 2026-10-23).
- w9: pointer to a 6-pointer language group (JA EN FR DE IT ES). Each pointer
  points to {size, lba} of that language's song entry.
- w10: 0x2F8.

Graphics table: an id missing from this table leaves the caller's outputs
uninitialised, which crashes (SYSCALL fatal).

Music ids:

- The driver sends ids below 25 to STR streams and the rest to XA (0x8002C398).
- It reads from start − 24 as pre-roll.
- 0x800296E0 `sltiu v0,(id-1),0x31` rejects ids above 49, so the plugin raises
  it for a new preview id.

## Data on the disc

### Song entry (READ_DT e68 + 5·g + language)

It is a PACK[4]:

- slot 0: PACK{tempo map, charts}. The chart order is single Light, Standard,
  Heavy, Beginner, then double Light, Standard, Heavy.
- slot 1: event list. Borrowed from ORION (g = 12), so the stage animations
  are ORION's.
- slot 2: LZ background TIM, 320×240 8bpp.
- slot 3: language UI set. Copied from g = 14.

### Chart format

- u32 size (64 + 4·beats).
- u32 kind = style | diff<<8 | 1<<16.
  - Styles: 0 single, 2 double.
  - Difficulties: 4 Beginner, 0 Light, 1 Standard, 2 Heavy.
- 14 zero words.
- One u32 per beat, with one byte per 16th note.
- Beat 0 plays at measure 3 of the tempo map (measured; see Status), and
  the stock charts have beats = 4·last − 8. (Measure 2 was my first guess,
  from the stock charts alone, and it was wrong.)
- Panel bits: L = 0, R = 1, U = 2, D = 3.
  - Singles mirror the low nibble into the high nibble.
  - Doubles use the low nibble for P1 and the high nibble for P2.
- There are no holds: hold heads become taps.

### Tempo map

- u32 size.
- Then pairs of (position in 1/4096 measure, time in 1/75 s). The time is a
  31-bit signed value; bit 31 is a flag.
- The builder supports one BPM and no stops only.

### Stream (STR.BIN layout)

- SPU ADPCM in 0x4000-byte blocks, alternating L and R.
- Each channel's first frame is silent with flag 0. The next has flag 6, and
  the rest have flag 2.
- The stream ends with a pair of silent flag-1 blocks (0x8000 bytes),
  followed by 10 zero sectors.

### Preview (XA)

- 4-bit stereo at 37.8 kHz, 18 × 128 B sound groups per sector, one sector in 8.
- Audio subheader: (file, ch, 0x64, 0x01). The last sector uses 0xE4.
- Filler sectors: (0xFF, ch, 0x04, 0x01).

### Packs

- Music select (e201–205): items 10–29 are banners (256×80 4bpp), items
  30–49 are plates (360×40). Slot 50 is 0. They load at 0x80174000, with
  room up to 0x801A4000.
- Results (e206–210): items 17–36 are plates, indexed idx + 0x11. Slot 37 is 0.
- Workout results (e181, loads at 0x80174000): items 1–20 are 176×55 4bpp
  banners, item 21 is 0. The builder appends the new banner as item 21 and
  points the TOC at the bigger pack (`toc b5` in extrasongs.txt).
- Records (e28–32): slot 1 is SONGTAB, 20 × {id, offA mini 88×28, offB plate}
  plus a zero terminator. The offsets are relative to SONGTAB.

### Palettes

- Banners have no 0x0000 colour and no STP bit.
- Background and mini-banner colours have STP set.
- Plates use the stock plate palette.

### Konami LZ

- A control byte gives 8 flags, LSB first.
- Long references: distance ≤ 1023, length 3–34.
- Short references: distance 1–16, length 2–5.
- Literal runs: 8–70 bytes.
- 0xFF ends the stream.

### Free disc space

- READ_DT padding at LBA 28573..31999 (0xFF bytes).
- DUMMY.BIN at LBA 91836..105335 (zeros).

The builder checks both regions before using them.

## The plugin

It installs on the first VBlank where all its `STOCK_CHECK` words hold
their stock values. Install does the following:

- reads `data/extrasongs.txt`;
- copies the song, stream, graphics and XA tables into enhancement memory
  (0x1000 bytes) and appends the new records;
- fills a blank master-table slot with the title and artist;
- applies the TOC overrides;
- rewrites the code constants that hold each table's base and the count 20;
- raises the music id limit.

**Workout results banners (fixed).** The loader loop (0x8008A984, in
0x8008A828) places banner i at x = 0x280 + 64·(i/4), y = 0x100 + 55·(i%4)
with its palette at (0x280, 0x1dc + i), inline at 0x8008A9A0..0x8008A9F0;
the slot function 0x8008CE50 (one caller, 0x800903C0) computes the same for
drawing. Index 20 lands at (960, 256), on the font page, so the extra
song's banner overwrote the lettering. Both now call a plugin guest
function (0x8F000010) that keeps the stock formula for i < 20 and gives
extras one of 8 free VRAM slots (EXTRA_WORKOUT_SLOTS). **Limit:** a 9th
extra song falls back to the stock formula and would need more free VRAM
found first.

The disc data itself arrives through the package's `disc_user` overlays.

**Install race (fixed).** The EXE is read from the disc over several frames,
in address order. The first version checked only code words below
0x80076000. In the Windows build it installed at frame 104, which was before
the TOC (0x80092B50), the master table and the song table had loaded. The
load then overwrote the TOC and master-table writes, and the song-table copy
was garbage, so the music select showed wrong graphics. The checks now also
require three stock words:

- 0x800932A4: the last TOC entry;
- 0x800B017C: the last song record;
- 0x800B4C68: the image's last non-zero word.

It now installs at frame 146.

## Making a package

```
port/tools/extrasong/build_extrasong.py SONG_DIR DISC.cue \
    mods/installed/dsf.extrasongs/1.0.0 [--title T] [--artist A]
```

SONG_DIR holds these files:

- NAME.sm: one BPM, no stops; holds become taps;
- NAME.ogg, .mp3 or .wav;
- NAME.png: a 256×80 banner;
- NAME-bg.png: a 320×240 background.

The title plate is set in the game's own lettering (plate_font.py). The
letters are cut from the 20 stock plates on the player's disc at build
time:

- fill (palette index ≥ 11) gives one area per letter; dots join their letter;
- lines are told apart by their top row;
- the outline and shading go to the nearest fill;
- the labels are the master-table titles and the songs' credited artists
  (the executable has no artist strings);
- per-character side bearings are fitted to the stock letter gaps (mean
  error 0.2 px).

"Move Your Feet / Junior Senior" set this way matches the stock plate
almost exactly. Some stock titles (321STARS) are stretched wider, so they
come out narrower. 61 characters are available; the stock plates have no G, Q, Z, q, 4, 5
or 6. A missing character stops the build with a message. Instead,
`--plate` takes a PNG (title and artist on transparent; the Remotion still
`SongPlate` in port/graphics makes one).

`--background BG.png` replaces NAME-bg.png. The Remotion still
`SongBackground` (port/graphics) makes a PS1-version-style background from
the player's photo; Tough Enough's is rendered with:

```
npx remotion still src/index.ts SongBackgroundLogos out.png --public-dir=DIR
```

DIR holds photo.png and the band's logos, cut from the player's own art
with `port/tools/extrasong/logo_matte.py`. For Tough Enough's jacket:

- **Stamp:** `--box 255 240 430 345 --key red --min-part 0.002`
- **Script:** `--box 100 316 398 385 --key white --on-dark 7 --rotate 4 --min-part 0.008`,
  plus `--exclude` boxes for the brand logos on the rope and `--drop-at` for
  three specks.

The game runs its zoom, split and tile effects on it, as on the stock
backgrounds (checked on the Windows build).

The builder also needs ffmpeg and ffprobe. The package targets the disc by
SHA-256.

## Song pack received (2026-10-18)

The player's pack is in `user-assets/songpack/pack/Dancing Stage Fusion PC/`
(their files, never in git; checksums matched on rejoin). One folder per
song: .sm, .ogg, banner 256×80, -bg 320×240, -jacket 512×512, some .avi.

- Missing from the agreed list: It's Raining Men, Superstar.
- Not on the list: Like A Virgin (tama_happytone), Move Your Feet (stock
  song): ask what they are for.
- Several BPMs: PARANOIA survivor 8, Rappers Delight 10 (drifting tempo),
  Kids in America 6, Go West 3, Good Luck 2 (×2). No stops. So the builder
  needs multi-BPM tempo maps before most of these.
- No double charts: Chihuahua 2002, Danger! High Voltage, Familiar Feeling,
  Good Luck, HEAVEN AND EARTH, Rappers Delight.
- Player's requests: new PS1-style graphics without real people for Kids In
  America, Go West and the stock song Mickey; each song its own background
  animation script (to discuss).

## 12 songs (2026-10-18, test build 7)

Builder now takes a song list (user-assets/songs.toml) and writes one
verified media file (data/media.bin, 2336-byte Mode-2 sectors) that the
plugin appends after the disc in its activation callback; a preparer
(dsf.extrasongs.media) hands the runtime that package file. The "data"
folder resource must not be `required` (a preparer must supply every
required resource of its feature).

- Music ids: stock streams 1-23, stock XA 25-49; extras: previews 50-61,
  streams 62-73. 0x8002C398 (stream vs XA: id < 25) replaced by KIND_GUEST.
  The id limit at 0x800296E0 applies to every play command: raise it to the
  largest stream id too (that was why songs ended at once).
- Every menu picture goes through the LZ decoder 0x8002E6F0 (replaced by a
  host decoder, LZ_GUEST); extra pictures are 16-byte "DSFX" markers in the
  packs, served from gfx.bin.
- Music select list filter 0x80050C40 (DOUBLE mode: only songs with a
  double Light rating) replaced by AVAIL_GUEST; unlock hook in place but
  `unlocked()` still returns true.
- Workout history ring: 0x800DE668 head, +4 count (max 50), rows of 16
  bytes from 0x800DE67C, song number at +8.
- Timing of all 12 songs checked against the simfile library: within
  0.007 s (a sync-check script).

Save records (results code 0x80089880..0x80089B10): [0x800B1AFC] + 0x90 +
0x50*idx, idx < 32.  +0x20 plays, +0x24 song id; per chart d (single
Beginner, Light, Standard, Heavy = 0..3; double +4, from the player's
difficulty byte +0x74 and style byte +0x76 via 0x8004A6BC): score +4d, two
u16 at +0x26+2d and +0x36+2d, grade +0x46+d, flag bits d in +0x4E / +0x4F.
Written only after a cleared play (a failed one leaves the record alone; no
record when +0x76 is 0 or 7), so a grade means "cleared".  The unlock
(unlocked() in dsfcore_extrasong.cpp) lists a song once its unlocking song
has a grade on one of the named charts; the music select list is rebuilt
between stages.  Tested: hidden after a STANDARD clear only, listed after a
DIFFICULT clear.

RECORDS (overlay e228 at 0x800E8000): the mini-banner loop 0x800E8F78 fills
20-byte slots at [0x800F2140] + 0x7C (count +0x2FC) and stores each next id
before its end test: with 32 songs the end mark overwrote the count (no
banner or title on any row).  The plugin moves that store after the test
(0x800E8FA4 / 0x800E8FAC) and puts banners 27-31, which ran into the
palettes at y = 496 and off VRAM, on the free page (960, 0).

Workout results: the banners load before the song just played joins the
history (0x8008FC5C), so slots go to that song (0x800BA32A) first.

Background animation script (song entry slot 1), verified layout for all 20
songs: u32 size (= 12 + 36n), u16 5, u16 k, u32 n, n x s32 cue positions in
1/4096 measure (the first -99999, the second 0), then n x 8 words (4 pairs)
for what the stage does from that cue: the dancer (PARANOIA MAX's script
brings its dancer), the effect backgrounds and the song's own picture, the
camera.  No time values in the records, only the positions.  The builder
gives each extra song a stock song's script (songs.toml script = g) fitted
to its own shape: piecewise-linear between the first / last step (and
breaks paired in order), snapped to whole measures or beats.  Seen in play:
PARANOIA survivor with PARANOIA MAX's, Danger! High Voltage with I Believe
In A Thing Called Love's.  The meaning of each of the 8 words is not known.

Stage sets: script header k picks the stage background set through the
table at 0x800B0510 (k = 1..17 -> e51..e57, e65..e67, e58..e64; loaded at
0x8007E910 from [0x800BA312]).  Stock songs use k = 2,3,4,5,7,8,12..17; k =
1, 6, 9, 10 and 11 (e51, e56, e66, e67, e58; e58 has 4 pictures) are used
by none.  Interpreter: 0x8006839C finds the cue for the position (script
copied to 0x800D81C0, max 5120 bytes = 141 cues, pointer [0x800D95C0]) and
0x8006B11C runs its record: two 8-byte layer programs (rec+0, rec+8) whose
2-bit fields select among the four parameter words at rec+0x10..0x1C
([type, picture A, picture B, extra]; pictures 0..0x0E index the 16-slot
table at 0x800D6180, 0xFF = the song's own picture).  Records from
different scripts mix under any 6-picture stage set (seen in play:
Chihuahua 2002 on k = 10, Danger! High Voltage on k = 9).

## Step list and pattern table (autoplay)

- During play (state 0x800BA310 == 4) the chart is a list of 16-byte
  entries at 0x800BCA44: {u32 position (1/4096 measure), s16 pattern
  (negative = control, -2 = end), s16, s16 time (1/75 s), s16, u32}.
  Song clock: s32 at 0x800BB4AC (1/75 s), 1.5 ticks a frame (PAL).
- The pattern is a row of the table at 0x800BC894 (8 bytes per row, one
  per panel slot: 0 Left, 1 Down, 2 Up, 3 Right, 4-7 the double side;
  ended by a row of 5s), built with the step list by 0x8008082C (called
  from 0x8007FEB8).  Rows 0-7 are the single arrows; jumps get 8, 9, ...
  in the order they first appear (PARANOIA survivor EXPERT: 8 D+U, 9 L+R,
  10 L+D, 11 U+R, 12 D+R, 13 L+U).  The buffer is reused after the song.
- Autoplay (dsfcore_hooks.cpp, DSF_AUTOPLAY=1; slots 4-7 go to player 2's pad,
  for double): pressing each step on the
  frame nearest time - 3 ticks judges every step PERFECT (at offset 0 most
  are GREAT).  PARANOIA survivor EXPERT: 509 PERFECT, max combo 509,
  10,000,000, AA, on Linux and on test8 + new core on the Windows build.
- Recording on the build machine (no graphics card): at PS1InternalResolution
  1080p or 720p the game runs below full speed on llvmpipe while x11grab
  encodes (0.89 at 720p with veryfast, 0.96 with ultrafast); native with
  ultrafast is 0.997 (measured by aligning the recording's spectrogram
  with the song's .ogg).

## Song clock and drift (fixed in the runtime, test build 11)
- Song clock 0x800BB4AC = [0x800B5C10] / 10000 (0x8002BE7C); the counter gains
  30144 every 2 VBlanks (0x8002C964), i.e. 15072 per VBlank = 20.096 ms: the
  game assumes real PAL progressive timing, 49.761 Hz -> exactly 75 ticks/s.
- The runtime ran PAL at 33.8688 MHz / 50 (the interlaced field rate), so the
  clock ran 75.36 ticks per second of the game's own audio: steps crept 0.48%
  ahead of the music (PARANOIA survivor: -21 ms at the start, -360 ms at the
  end).  psxrecomp e1d7019b: hardware periods by standard and interlace
  (mednafen psx/gpu.cpp).  After: 75.001 ticks/s, judged-vs-music offset
  +31 ms constant (spread 3-5 ms) from the first note to the last.
- Measuring: a drift-capture script (clock/audio pairs + audio_wav of the song;
  keep the dump under the tap ring's 4194304 frames) and drift/ana.py (game
  audio aligned to the .ogg by spectrogram windows; steps in order = simfile rows).
