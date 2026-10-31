DANCING STAGE FUSION PC (Windows x64)
=====================================

An unofficial, free, non-commercial PC port of the PlayStation game Dancing
Stage Fusion (Europe, SLES-04163). The program is StepMania (the 2005 engine,
heavily modified) with the ORIGINAL game running inside it, recompiled to
native code: the game's menus, songs, step charts, timing, scoring and sound
are the original's.

NO GAME DATA IS INCLUDED. You need your own copy of the game as a disc image.
Setup.exe builds the game from it, on your PC.

WHAT YOU NEED
  - Dancing Stage Fusion (Europe) (En,Fr,De,Es,It), SLES-04163, as the
    Redump .bin + .cue (raw MODE2/2352). Setup checks it and stops if it is
    a different version or damaged.
  - 64-bit Windows 10 or 11.
  - About 200 MB of free space while Setup runs.

SETUP
  1. Unzip this folder anywhere you can write to (not Program Files).
  2. Put your disc image in the "disc" folder:
       disc\Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).bin
       disc\Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).cue
  3. Run Setup.exe. It checks the image, reads the game program off it,
     recompiles it (setup\psxrecomp-game.exe), compiles that with the
     included compiler (setup\compiler) and links dsfcore.dll, the game
     core. It also gives DancingStageFusionPC.exe the game's memory card
     icon, taken from your disc. This takes a few minutes; run it again any time to rebuild.
     Until Setup has run, DancingStageFusionPC.exe only tells you to run it.
  4. Plug in your dance pad(s), then run DancingStageFusionPC.exe.
  No PlayStation BIOS is needed (the free OpenBIOS is included).
  The first time, the game asks to create system data on the memory card:
  choose YES.

PERSONAL USE ONLY
  dsfcore.dll is built from the game program on YOUR disc. It contains code
  derived from Konami's copyrighted software, so do not share or upload it,
  or the game folder after Setup has run. The game reads its music, graphics
  and charts from your own disc image.

CONTROLS
  StepMania's title menu (PLAY / INFO / EXIT): arrow keys and Enter, Esc goes
  back. On a dance pad: the arrows, Start (or Cross) to choose, Triangle to go
  back. To close the game, choose QUIT on its main menu.

  In the game (defaults; change them in the game's OPTIONS > CONTROLLER
  SETTINGS > KEY CONFIG 1P / 2P):
    Player 1: arrow keys = Up/Down/Left/Right, X = Cross, C = Circle,
              Z = Square, S = Triangle, Q / 1 / E / 3 = L1 / L2 / R1 / R2,
              Enter = Start, Backspace = Select
    Player 2: number pad 8/2/4/6 = arrows, 0 = Cross, . = Circle,
              1 = Square, 3 = Triangle, 7 / / / 9 / * = L1 / L2 / R1 / R2,
              number pad Enter = Start, number pad + = Select
  KEY CONFIG: choose a button, press Cross (X), then press the key or pad
  button for it. Each button keeps two of your own keys/buttons besides the
  default, so a keyboard and a dance pad can both work. Square (Z) clears a
  button, Esc cancels.
  Alt+Enter = full screen / window.

SYNC
  In the game's OPTIONS > SOUND OPTIONS:
    AUTO TIMING:  ON plays clicks; tap any arrow on every click and it
                  measures your setup's sound and picture delay, then keeps
                  fine-tuning TIMING from your steps after each song.
    AUDIO BUFFER: smaller = less sound delay. AUTO = 23 ms (WASAPI). Raise
                  it if the sound crackles. Takes effect at the next start.
    PICTURE DELAY: delays the picture. If the arrows still look early against
                  the music, try +10 to +40 ms.
    PICTURE SYNC: ON shows each frame exactly when its sound is heard.
                  OFF (the default) is better for scoring.

GRAPHICS
  The game's OPTIONS > GRAPHIC OPTIONS > DISPLAY SETTINGS:
    GAME RESOLUTION: Native / 720p / 1080p (default) / 1440p.
    RENDERER:     OpenGL (default) or Software.
    SMOOTH ARROWS: an extra picture between each two of the game's own, so
                  the arrows move every frame (ON by default). Turn it OFF if
                  songs slow down.

THE NEW SONGS
  Songs the PlayStation version never had, built from your own simfile packs
  of the PlayStation 2 and arcade versions of Dancing Stage Fusion:
  1. Put each song's simfile folder in the "simfiles" folder, named as
     simfiles\PUT_YOUR_SIMFILES_HERE.txt lists (or change the "dir" lines in
     simfiles\songs.toml). A folder holds the .sm step chart, the audio
     (.ogg, .mp3 or .wav), a 256x80 banner and a 320x240 background.
  2. Run "Build New Songs.bat" (after Setup.exe). It converts the charts,
     music and pictures to the game's own formats with the tools in
     "songtools" (about two minutes) and puts the song pack in the game.
  The song pack holds your music and charts: for your own use only.

FILES
  Data\StepMania.ini   settings          Data\Keymaps.ini   key/pad mappings
  saves\card1.mcd      memory card
  Data\log.txt, Data\dsfcore.log, Data\crashinfo.txt: if something goes
  wrong, please send these. If Setup fails, it shows why; the details are in
  setup\work.

LICENCES
  StepMania 2005 (MIT; libresample LGPL, Lua MIT, PCRE BSD, Crypto++ public
  domain/Boost-style); PSXRecomp (PolyForm Noncommercial 1.0.0); OpenBIOS
  (MIT); SDL3 (zlib); zlib, libpng, libjpeg-turbo, Ogg/Vorbis (BSD-style);
  GCC and GNU Binutils (GPL 3, GCC Runtime Library Exception; see
  licenses\GCC-and-Binutils-README.txt for their source); the song tools:
  Python (PSF), numpy (BSD), Pillow (MIT-CMU), FFmpeg (GPL; see
  songtools\README.txt). See the
  "licenses" folder. Fonts: Exo 2, Inter, Lobster (SIL Open Font License 1.1).
  Dancing Stage Fusion (c) 1998 2004 Konami. Not affiliated with Konami.
