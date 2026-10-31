DANCING STAGE FUSION PC - TEST BUILD 8 (Windows x64)
===================================================

An unofficial, free, non-commercial PC port of the PS1 game Dancing Stage Fusion
(Europe, SLES-04163). The program is StepMania (the 2005 engine, heavily
modified) with the ORIGINAL game running inside it: the game's menus, songs,
step charts, timing, scoring and sound are the original's.

NEW IN TEST BUILD 8
  - QUIT on the game's own main menu, under OPTIONS, in the menu's own
    lettering: it closes Dancing Stage Fusion PC.
  - The PC menu (Esc while playing) is gone: the PC settings are in the
    game's OPTIONS (SOUND / GRAPHIC / CONTROLLER SETTINGS), and QUIT closes
    the game.
  - Extra songs (with the 12-song pack v5): the RECORDS high scores show their
    banners and titles again, the workout results show the banner of the song
    you just played, and Heaven and Earth stays hidden until you clear
    PARANOIA survivor on DIFFICULT or EXPERT.

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
  Tested only on a computer without a graphics
  card or sound card, and on the Linux test build of the same game core:
    - QUIT on the main menu (closes the program), the cursor wrapping round
      the seven entries, the other entries still opening their screens;
    - Esc while playing no longer opens a PC menu;
    - RECORDS high scores and workout results with the extra songs, and the
      Heaven and Earth unlock (Linux test build).
  NOT tested: real Windows, real graphics and sound cards, dance pads and
  joysticks.

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
