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
> packs for both the console and the arcade versions of *Dancing Stage
> Fusion*. Everything is built on your own computer from your own files.

## Status

Work in progress. Test builds are being played on Windows; the first public
release is being prepared. Until then there is no download: building it
yourself needs the steps under [Building](#building).

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
- **The arcade songs**: songs the PlayStation version never had, built from
  your simfile packs into the game's own song list. They play like the
  original 20: in the music select, results, WORKOUT and RECORDS, with their
  own banners, previews and backgrounds, and unlock conditions (Heaven and
  Earth stays hidden until you clear PARANOIA survivor on DIFFICULT or
  EXPERT).

**Deluxe (PlayStation disc)**

- `Build Deluxe Disc.exe` turns your disc image into a Deluxe image: TYPE3
  colours, the arcade songs from the same song packs, unlocks, and a NEW!!
  marker on newly unlocked songs.
- Checks your image first and changes nothing if it isn't the expected,
  undamaged PAL disc; patched sectors keep valid error-correction data.

## What you need

- *Dancing Stage Fusion* (Europe, SLES-04163) as a raw `.bin` + `.cue` image
  (MODE2/2352).
- **Simfile packs for both versions of *Dancing Stage Fusion*: the console
  (PlayStation) version and the arcade version.** The arcade songs are the
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
| `patches/psxrecomp/` | Changes to PSXRecomp this project needs, applied at build time |
| `scripts/` | Build and packaging scripts |

## Building

The game core is built with [PSXRecomp](https://github.com/mstan/psxrecomp):
the recompiler reads the game program from your disc and generates C code,
which is compiled together with the PSXRecomp runtime and `core/` into
`dsfcore.dll`. The generated code is derived from the game, so it is never
committed or distributed; it exists only on the machine that built it.

Outline (cross-compiling from Linux; a detailed guide comes with the first
release):

1. Check out PSXRecomp at the version in `patches/psxrecomp/` and apply the
   patches there.
2. Extract the game program from your disc and generate the game's C code
   with the recompiler and `game-project/game.toml`.
3. Build `dsfcore.dll` (CMake, Ninja, MinGW-w64 x86-64).
4. Build the engine in `engine/` the same way.
5. Package: the program, `dsfcore.dll`, the data archive and your disc image
   in its `disc` folder.

Tools: CMake, Ninja, MinGW-w64 (x86_64, posix threads), Python 3.

## Legal

This is a free, non-commercial fan project for preservation and personal use.
It is not affiliated with, endorsed by or connected to Konami.
*Dancing Stage* and *Dancing Stage Fusion* are trademarks of Konami; the game,
its music, artwork and data are Konami's and are not part of this repository.
Please own the game you play.

## Licences

- This project's own code: **(licence to be chosen before the first release)**
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
