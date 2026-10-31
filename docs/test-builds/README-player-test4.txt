DANCING STAGE FUSION PC - TEST BUILD 4 (Windows x64)
===================================================

An unofficial, free, non-commercial PC port of the PS1 game Dancing Stage Fusion
(Europe, SLES-04163). The program is StepMania (the 2005 engine, heavily
modified) with the ORIGINAL game running inside it: the game's menus, songs,
step charts, timing, scoring and sound are the original's.

NEW IN TEST BUILD 4
  - The PC settings are now inside the game's own OPTIONS menu, in its own
    style and in all five languages:
      SOUND OPTIONS: volume, audio buffer, picture sync, picture delay
      GRAPHIC OPTIONS > DISPLAY SETTINGS: screen mode, resolution, refresh
        rate, VSync, game resolution, renderer
      CONTROLLER SETTINGS > KEY CONFIG 1P / 2P: key and pad mapping
  - Title menu: PLAY / INFO / EXIT. INFO explains how the port works, its
    settings and controls, and has the credits and licences.
  - The PC menu (Esc while playing) is now just Resume and Quit.
  - New title screen with the Dancing Stage Fusion logo plus "PC", and new
    theme graphics and font throughout.

PERSONAL USE ONLY
  dsfcore.dll was built from the game program on YOUR disc. It contains code
  derived from Konami's copyrighted software, so do not share or upload it.
  No Konami data (music, graphics, charts) is included: the game reads them
  from your own disc image.

SETUP
  1. Unzip this folder anywhere.
  2. Put your disc image in the "disc" folder:
       disc\Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).bin
       disc\Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).cue
  3. Optional: copy the "saves" folder from your previous test build into
     this folder to keep your memory card (saves\card1.mcd).
  4. Plug in your dance pad(s), then run DancingStageFusionPC.exe.
  No PlayStation BIOS is needed (the free OpenBIOS is included).
  The first time, the game asks to create system data on the memory card:
  choose YES.

CONTROLS
  StepMania menus: arrow keys and Enter, Esc goes back. On a dance pad: the
  arrows, Start (or Cross) to choose, Triangle to go back.

  In the game (defaults; change them in the game's OPTIONS > CONTROLLER
  SETTINGS > KEY CONFIG 1P / 2P):
    Player 1: arrow keys = Up/Down/Left/Right, X = Cross, C = Circle,
              Z = Square, S = Triangle, Q / 1 / E / 3 = L1 / L2 / R1 / R2,
              Enter = Start, Backspace = Select, Esc = Back (PC menu)
    Player 2: number pad 8/2/4/6 = arrows, 0 = Cross, . = Circle,
              1 = Square, 3 = Triangle, 7 / / / 9 / * = L1 / L2 / R1 / R2,
              number pad Enter = Start, number pad + = Select
  KEY CONFIG: choose a button, press Cross (X), then press the key or pad
  button for it. Each button keeps two of your own keys/buttons besides the
  default, so a keyboard and a dance pad can both work. Square (Z) clears a
  button, Esc cancels. Map "PC MENU" on your pad if you want the PC menu there.
  Alt+Enter = full screen / window.

SYNC
  The game's speed follows the sound card, and the picture is shown as soon as
  it is ready (least delay, best for timing). In the game's OPTIONS >
  SOUND OPTIONS:
    AUDIO BUFFER: smaller = less sound delay. AUTO = 23 ms (WASAPI). Raise
                  it if the sound crackles. Takes effect at the next start.
    PICTURE DELAY: delays the picture. If the arrows still look early against
                  the music, try +10 to +40 ms.
    PICTURE SYNC: ON shows each frame exactly when its sound is
                  heard. Arrows then match the music, but every step is judged
                  later by the sound delay, so OFF (the default) is better for
                  scoring.

GRAPHICS
  The game's OPTIONS > GRAPHIC OPTIONS > DISPLAY SETTINGS:
    GAME RESOLUTION: Native / 720p / 1080p (default) / 1440p. Sharpens the 3D
                  dancers and polygons; the 2D artwork keeps its original pixels.
    RENDERER:     OpenGL (default, uses your graphics card) or Software.
    Both take effect the next time you start. Screen mode, resolution,
    refresh rate and VSync apply when you leave the page.

NOT YET TESTED - PLEASE REPORT
  This build was only tested on a computer without
  a graphics card or sound card. Tested there:
    - the PC settings in the game's OPTIONS (values change, are saved, a new
      resolution applies), KEY CONFIG (a new key works in the game, clearing
      and cancelling), all five languages, the PC menu, INFO;
    - the game booting, its language and memory-card screens, keyboard input;
    - pacing by the sound output: steady 50 frames per second;
    - the OpenGL renderer at 1080p (correct picture).
  NOT tested: real Windows; your graphics card's speed at 1080p (here it was
  software-only, about 20 fps; native resolution ran at full speed); real
  sound cards; dance pads and joysticks; a full song inside this build (the
  game itself is the same as in test build 2). If 1080p is slow, set GAME
  RESOLUTION to Native or 720p and tell me.

FILES
  Data\StepMania.ini   settings          Data\Keymaps.ini   key/pad mappings
  saves\card1.mcd      memory card       log.txt, dsfcore.log, crashinfo.txt
  If something goes wrong, please send log.txt, dsfcore.log and, after a crash,
  crashinfo.txt.

LICENCES
  StepMania 2005 (MIT; libresample LGPL, Lua MIT, PCRE BSD, Crypto++ public
  domain/Boost-style); PSXRecomp runtime (PolyForm Noncommercial 1.0.0);
  OpenBIOS (MIT); SDL3 (zlib); zlib, libpng, libjpeg-turbo, Ogg/Vorbis
  (BSD-style). See the "licenses" folder. MP3 support (libmad, GPL) is not
  included.
  Fonts: Exo 2, Inter, Lobster (SIL Open Font License 1.1).
  Dancing Stage Fusion (c) 1998 2004 Konami. Not affiliated with Konami.
  The logo on the title screen is Konami's: it is in your copy only.

  Port by CrashReporter.
