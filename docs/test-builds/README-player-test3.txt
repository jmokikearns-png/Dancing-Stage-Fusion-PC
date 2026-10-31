DANCING STAGE FUSION PC - TEST BUILD 3 (Windows x64)
===================================================

An unofficial, free, non-commercial PC port of the PS1 game Dancing Stage Fusion
(Europe, SLES-04163). The program is StepMania (the 2005 engine, heavily
modified) with the ORIGINAL game running inside it: the game's menus, songs,
step charts, timing, scoring and sound are the original's.

NEW IN TEST BUILD 3
  - The program is now StepMania. Title menu: PLAY / OPTIONS / EXIT.
  - While playing, press Esc (the "Back" button) for the PC menu: Resume,
    Key/Joy Mappings, Sound/Sync, Graphics, Quit. The game pauses meanwhile
    and continues exactly where it was.
  - Controls are set with StepMania's own mapping screen (Options > Key / Joy
    Mappings, or from the PC menu in the game). The separate controller setup
    tool is gone.
  - Sync: the game now runs at the speed the sound card plays its music, so the
    music and the game cannot drift apart during a song. New low-latency
    sound output (WASAPI). See SYNC below.
  - Graphics options in the menus (game resolution, renderer, window size,
    full screen).

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
  3. Optional: copy the "saves" folder from test build 2 into this folder to
     keep your memory card (saves\card1.mcd).
  4. Plug in your dance pad(s), then run DancingStageFusionPC.exe.
  No PlayStation BIOS is needed (the free OpenBIOS is included).
  The first time, the game asks to create system data on the memory card:
  choose YES.

CONTROLS
  StepMania menus: arrow keys and Enter, Esc goes back. On a dance pad: the
  arrows, Start (or Cross) to choose, Triangle to go back.

  In the game (defaults, change them in Key / Joy Mappings):
    Player 1: arrow keys = Up/Down/Left/Right, X = Cross, C = Circle,
              Z = Square, S = Triangle, Q / 1 / E / 3 = L1 / L2 / R1 / R2,
              Enter = Start, Backspace = Select, Esc = Back (PC menu)
    Player 2: number pad 8/2/4/6 = arrows, 0 = Cross, . = Circle,
              1 = Square, 3 = Triangle, 7 / / / 9 / * = L1 / L2 / R1 / R2,
              number pad Enter = Start, number pad + = Select
  Key / Joy Mappings: choose a box with the arrows, press Enter, then press the
  key or pad button. Each button has several boxes, so a keyboard and a dance
  pad can both work. Map "Back" on your pad if you want the PC menu there.
  Alt+Enter = full screen / window.

SYNC
  The game's speed follows the sound card, and the picture is shown as soon as
  it is ready (least delay, best for timing). In Options > Sound Options (or
  the PC menu > Sound / Sync):
    Audio Buffer: smaller = less sound delay. AUTO = 23 ms (WASAPI). Raise
                  it if the sound crackles. Takes effect at the next start.
    Picture Offset: delays the picture. If the arrows still look early against
                  the music, try +10 to +40 ms.
    Match Picture to Sound: ON shows each frame exactly when its sound is
                  heard. Arrows then match the music, but every step is judged
                  later by the sound delay, so OFF (the default) is better for
                  scoring.

GRAPHICS
  Options > Graphic Options (or the PC menu > Graphics):
    Game Resolution: Native / 720p / 1080p (default) / 1440p. Sharpens the 3D
                  dancers and polygons; the 2D artwork keeps its original pixels.
    Game Renderer: OpenGL (default, uses your graphics card) or Software.
    Both take effect the next time you start. Window size and full screen
    apply immediately.

NOT YET TESTED - PLEASE REPORT
  This build was only tested on a computer without
  a graphics card or sound card. Tested there:
    - menus, key mapping screen, the PC menu (pause/resume);
    - the game booting, its language and memory-card screens, keyboard input;
    - pacing by the sound output: steady 50 frames per second;
    - the OpenGL renderer at 1080p (correct picture).
  NOT tested: real Windows; your graphics card's speed at 1080p (here it was
  software-only, about 20 fps; native resolution ran at full speed); real
  sound cards; dance pads and joysticks; a full song inside this build (the
  game itself is the same as in test build 2). If 1080p is slow, set Game
  Resolution to Native or 720p and tell me.

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
  Dancing Stage Fusion (c) 1998 2004 Konami. Not affiliated with Konami.
