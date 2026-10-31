# Dancing Stage Fusion PC

**Dancing Stage Fusion** (PlayStation, Europe, 2004) on modern Windows PCs.

This is not a remake or an emulator front end. The original game program is
statically recompiled to native code and runs inside a modified
[StepMania](https://github.com/stepmania/stepmania) 3.95, which supplies the
window, sound, controllers and settings. The menus, songs, step charts,
timing and scoring are the original game's own; the project adds high
resolution graphics, dance-pad and keyboard support, timing tools and extra
songs on top.

It also includes **Dancing Stage Fusion Deluxe**, a builder that applies the
same game changes to a PlayStation disc image for DuckStation or real hardware.

> **No game data is included.** You need your own copy of
> *Dancing Stage Fusion* (Europe, SLES-04163) as a disc image, and simfile
> packs for both the PlayStation 2 and the arcade versions of *Dancing Stage
> Fusion*. Everything is built on your own computer from your own files.

## Status

The first release, 1.0 beta 1, is built and has been tested on Windows;
downloads will be on the Releases page.

## Features

**PC version**

- The original game, recompiled to native x86-64 code and running at the PAL
  PlayStation's real speed (49.76 frames per second), in sync with its music.
- Internal resolution Native, 720p, 1080p or 1440p (OpenGL), or software
  rendering; window or full screen.
- **SMOOTH ARROWS**: an extra frame between each two of the game's own, so the
  arrows move every frame instead of every other one.
- Dance pads, joysticks and keyboards through StepMania's input, configured in
  the game's own **KEY CONFIG** (several keys per button).
- Input is latched between the game's pad reads, so quick steps aren't lost.
- **AUTO TIMING**: a tap test that measures your setup's sound and picture
  delay, then keeps fine-tuning the timing from your steps.
- Audio buffer, picture delay and picture sync settings for low latency.
- All PC settings live in the game's own OPTIONS menus, in all five of its
  languages; **QUIT** is on the main menu.
- **STEP MARK COLORS TYPE3**: arrows coloured by rhythm (quarter, eighth,
  triplet, sixteenth).
- **New songs**: songs the PlayStation version never had, built from
  your simfile packs into the game's own song list. They play like the
  original 20: in the music select, results, WORKOUT and RECORDS, with their
  own banners, previews and backgrounds, and unlock conditions (Heaven and
  Earth stays hidden until you clear PARANOIA survivor on DIFFICULT or
  EXPERT).

**Deluxe (PlayStation disc)**

- `Build Deluxe Disc.exe` turns your disc image into a Deluxe image: TYPE3
  colours, the new songs from the same song packs, unlocks, and a NEW!!
  marker on newly unlocked songs.
- Checks your image first and changes nothing if it isn't the expected,
  undamaged PAL disc; patched sectors keep valid error-correction data.

## What you need

- *Dancing Stage Fusion* (Europe, SLES-04163) as a raw `.bin` + `.cue` image
  (MODE2/2352).
- **Simfile packs for both the PlayStation 2 and the arcade versions of
  *Dancing Stage Fusion*,** plus simfiles of **Go West**, **It's Raining Men**
  and **PARANOIA survivor**, which are in neither pack. The new songs are the
  heart of this project, so these are required, not optional. Each song is a
  StepMania simfile folder: the `.sm` step chart, the audio (`.ogg`, `.mp3`
  or `.wav`), a 256x80 banner and a 320x240 background. This project does not
  host or link to them, because they contain the songs' music: you supply
  your own.
- A 64-bit Windows PC. High resolution rendering uses OpenGL; there is a
  software renderer for machines without it.

No PlayStation BIOS is needed: the free [OpenBIOS](https://github.com/grumpycoders/pcsx-redux)
is used.

## Repository layout

| Path | What |
|---|---|
| `core/` | The game-specific code built into the game core: input, menus, settings, smooth arrows, step colours, extra songs, QUIT |
| `game-project/` | Recompiler project for the game (`game.toml`, CMake) |
| `engine/` | The modified StepMania 3.95 (its own licence: MIT) |
| `graphics/` | Theme graphics and fonts, rendered from design tokens |
| `mods/` | Mod packages loaded by the game core |
| `tools/extrasong/` | Song pack builder: your simfile packs in, a song pack out |
| `tools/ps1disc/` | The Deluxe disc builder and its PlayStation code |
| `tools/setup/` | `Setup.exe`: builds the game core from the player's disc |
| `patches/psxrecomp/` | Changes to PSXRecomp this project needs, applied at build time |
| `scripts/` | Build and packaging scripts |

## Building

The game core is built with [PSXRecomp](https://github.com/mstan/psxrecomp):
the recompiler reads the game program from your disc and generates C code,
which is compiled together with the PSXRecomp runtime and `core/` into
`dsfcore.dll`. The generated code is derived from the game, so it is never
committed or distributed; it exists only on the machine that built it.

### With the release (Windows)

The release does that part on your PC, with no tools to install:

1. Unzip it anywhere you can write to.
2. Put your disc image (`.bin` + `.cue`) in its `disc` folder.
3. Run `Setup.exe`. It checks the image against the Redump dump, reads the
   game program off it, recompiles it (`setup\psxrecomp-game.exe`), compiles
   that with the included GCC (`setup\compiler`) and links it with the
   prebuilt rest of the game core (`setup\kit`) into `dsfcore.dll`; it also
   gives the program the game's memory card icon. About a minute, and the
   same `dsfcore.dll`, byte for byte, on every PC. The recompiled code is
   deleted afterwards.
4. Run `DancingStageFusionPC.exe`.

### From source

Cross-compiled from Linux. The compiler is conda-forge's GCC 16 for
`x86_64-w64-mingw32` (UCRT, mingw-w64 12, posix threads): its Linux-hosted
build (`gcc_impl_win-64`, `gxx_impl_win-64`, `binutils_impl_win-64` and the
`mingw-w64-ucrt-x86_64-*` sysroot packages from the `linux-64` and `noarch`
channels) builds everything here, and its Windows-hosted build of the same
version is the one the release ships for `Setup.exe`, so the prebuilt kit and
the player's compiled game code always match.

1. Check out PSXRecomp at the version in `patches/psxrecomp/` and apply the
   patches there (`git am --keep-cr`).
2. Extract the game program from your disc and generate the game's C code
   with the recompiler and `game-project/game.toml`.
3. Build `dsfcore.dll` with CMake and Ninja, using
   `scripts/win/toolchain-conda-ucrt.cmake` (`DSF_TC` = the toolchain's
   folder) and `-DDSF_EMBED=ON -DPSX_EMBED=ON -DCMAKE_C_STANDARD=17
   -DCMAKE_CXX_STANDARD=17 -DPSX_ENABLE_VULKAN=OFF`.
4. Build the engine in `engine/` (MinGW-w64 x86-64).
5. `scripts/dsfpc/package.sh` packages a personal build (with your
   `dsfcore.dll`); `scripts/dsfpc/make_release.sh VERSION` makes the release
   instead: no `dsfcore.dll`, plus `Setup.exe` (`tools/setup/`), the kit
   (`make_kit.sh`: the game core minus the game's code), the trimmed Windows
   compiler (`make_compiler.sh`) and the recompiler. It refuses to package
   anything from the game.

Tools: CMake, Ninja, the conda-forge compilers above, Python 3 with Pillow.

## Building the new songs

The new songs are built once, from your simfile packs, into a song pack: one
pack works for both the PC version and the Deluxe disc.

**With the release:** put your simfile packs in the `simfiles` folder, whole
and unzipped, add simfiles of Go West, It's Raining Men and PARANOIA survivor
(in neither pack) anywhere in it, and run `Build New Songs.bat`. It finds each song in them by its
title (and artist), with the music, banner and background its `.sm` names,
runs the song builder with the tools and song list in `songtools` (Python
with numpy and Pillow, FFmpeg, and the PlayStation audio encoder), and
installs the pack in the game. It also gives five songs this project's own
gameplay backgrounds (Go West, Kids in America, Heaven and Earth, Tough
Enough and the stock song Mickey), finished with the lettering or photo from
the player's own banners and jacket picture. Nothing to install.

**From source:**

You need Python 3.11 or newer with Pillow and numpy (`pip install pillow numpy`),
ffmpeg on your `PATH`, and a C compiler available as `cc` (it builds the small
PlayStation audio encoder in `tools/extrasong/psxaudio.c`).

1. Make a folder, e.g. `my-songs`, and copy
   [`tools/extrasong/songs.toml`](tools/extrasong/songs.toml) into it. It lists
   the 12 new songs in order, with their song IDs, background animations and
   Heaven and Earth's unlock.
2. Put your PlayStation 2 and arcade simfile packs in that folder (or any
   folder, given with `--simfiles DIR`), whole,
   and simfiles of Go West, It's Raining Men and PARANOIA survivor (in
   neither pack).
   Each song is found in them by its title and artist; to use a particular
   folder instead, give the song a `dir` line in `songs.toml`.
3. Build the pack against your disc image:

   ```
   python tools/extrasong/build_extrasong.py my-songs/songs.toml "path/to/Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).cue" dsf.extrasongs
   ```

   It converts the charts, music, previews and pictures to the game's own
   formats. The result, `dsf.extrasongs/`, holds `manifest.toml` and
   `data/` (`extrasongs.txt`, `gfx.bin`, `media.bin`).
4. Use it:
   - **PC version:** copy the `dsf.extrasongs` folder's contents into the game
     folder's `mods\installed\dsf.extrasongs\1.0.0\`.
   - **Deluxe:** copy the three files in `data/` into the Deluxe builder's
     `songs` folder, then run `Build Deluxe Disc.exe`.

The song pack contains your music and charts: it is for your own use, don't
share it. To use your own pictures for a song, add `background = ...` or
`banner = ...` to its entry in `songs.toml` (examples are commented in there).

## Legal

This is a free, non-commercial fan project for preservation and personal use.
It is not affiliated with, endorsed by or connected to Konami.
*Dancing Stage* and *Dancing Stage Fusion* are trademarks of Konami; the game,
its music, artwork and data are Konami's and are not part of this repository.
Please own the game you play.

## Licences

- `engine/`: StepMania, MIT licence
- PSXRecomp (used to build the game core): PolyForm Noncommercial 1.0.0, so
  builds of the PC version are for non-commercial use
- OpenBIOS: MIT
- SDL3: zlib; libchdr, libwebp: BSD-3-Clause; Lua 5.0: MIT; zlib, libpng,
  libjpeg-turbo, Ogg Vorbis, PCRE, Crypto++, libresample: their own licences
- Fonts: Exo 2, Inter, Lobster: SIL Open Font License 1.1

## Credits

- Port by CrashReporter
- PSXRecomp: Matthew Stanley (mstan) and contributors
- StepMania: the StepMania team
- OpenBIOS: the PCSX-Redux authors
- Fonts: Exo 2 by Natanael Gama, Inter by Rasmus Andersson, Lobster by Pablo
  Impallari
