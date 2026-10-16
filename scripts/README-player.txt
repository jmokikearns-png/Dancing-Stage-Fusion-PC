DANCING STAGE FUSION PC - TEST BUILD 2 (Windows x64)
===================================================

An unofficial, free, non-commercial PC port of the PS1 game Dancing Stage Fusion
(Europe, SLES-04163). The ORIGINAL game code runs natively on your PC: menus,
songs, step charts, timing, scoring and sound are the original game's.

NEW IN TEST BUILD 2
  - Audio/arrow sync: the sound buffer was cut from 180 ms to 50 ms. Build 1
    played the music about 0.2 s behind the arrows; that delay is gone.
  - Controls now go through StepMania's input code: keyboard, dance pads and
    joysticks (DirectInput), each fully remappable for Player 1 and Player 2,
    with a controller setup tool (press F1 in the game, or run
    DSF-ControllerSetup.exe).
  - Runtime hotkeys that could change the game (fast-forward, rewind, save
    states) are switched off.

PERSONAL USE ONLY
  This program was built from the game program on YOUR disc. It contains code
  derived from Konami's copyrighted software, so do not share or upload it.
  No Konami data (music, graphics, charts) is included: the game reads them
  from your own disc image.

SETUP
  1. Unzip this folder anywhere.
  2. Put your disc image in the "disc" folder:
       disc\Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).bin
       disc\Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).cue
     (Or just start the game: a file dialog opens, choose the .cue file, and the
      location is remembered.)
  3. Plug in your dance pad(s) BEFORE starting the game.
  4. Run DancingStageFusionPC.exe.
  No PlayStation BIOS is needed (the free OpenBIOS is included).
  On first start the game asks to create system data on the memory card:
  choose YES. Saves are stored in the "saves" folder (card1.mcd).

CONTROLS
  Press F1 in the game (or run DSF-ControllerSetup.exe) to open Controller
  Setup. Click a box, then press the key or dance-pad button you want for it.
  Each PlayStation button has 3 boxes per player, so a keyboard and a pad can
  both work at once. Delete or right-click clears a box. Rows light up while
  their button is held, which is a quick way to test a pad. Changes save
  immediately and the running game picks them up within a second.

  DANCE PADS: bind them once in Controller Setup (about 30 seconds): click
  "Up", step on the up arrow, and so on. In DDR menus Cross (X) confirms and
  Start starts.

  Default keyboard layout:
    Player 1: arrow keys = Up/Down/Left/Right, X = Cross, C = Circle,
              Z = Square, S = Triangle, Q / 1 / E / 3 = L1 / L2 / R1 / R2,
              Enter = Start, Backspace = Select
    Player 2: number pad 8/2/4/6 = arrows, 0 = Cross, . = Circle,
              1 = Square, 3 = Triangle, number pad Enter = Start, + = Select
  Alt+Enter or Ctrl+F = fullscreen.
  Keys are read only while the game window is in front.

  Bindings are stored in config\Data\Keymaps.ini (StepMania format).

SYNC / LATENCY
  If the arrows still feel early or late against the music, the sound buffer
  can be tuned in game.toml, section [audio]:
      buffer_ms = 50     (lower = less delay; raise to 60-80 only if the
                          sound crackles)
  For the smoothest arrow scrolling, set your monitor to 50 Hz if it supports
  it (the game runs at the PAL rate of 50 frames per second); a 60 Hz or
  higher monitor also works.

GRAPHICS
  The game renders at 1080p internal resolution by default (5x the PS1's).
  This sharpens 3D dancers, polygon edges and scaling effects. The game's 2D
  artwork is original pixel art and keeps its original detail.
  To change it, edit game.toml, section [video]:
      internal_resolution = "native"   (exact original pixels)
      internal_resolution = "1440p" / "4k" / "display" (match your monitor)

KNOWN ISSUES / NOT YET TESTED
  - Dance pads, joysticks and Windows audio were not testable on the build
    machine (no hardware); the keyboard path was tested.
  - Not yet compared against a real PlayStation; please report anything that
    looks, sounds or plays differently.
  - Some game code (loaded from the disc while playing) still runs in a slower
    compatibility mode; a reasonably fast PC is fine.

TROUBLESHOOTING
  config\dsf_input.log lists the input devices found and mapping reloads.
  If the StepMania input cannot start, the game falls back to its built-in
  keyboard controls (arrow keys, X, S, Z, A, Enter, Right Shift).

LICENCES
  Port runtime: PSXRecomp (PolyForm Noncommercial 1.0.0). Input code:
  StepMania 2005 (MIT). OpenBIOS (MIT), SDL3 (zlib) and other components: see
  the "licenses" folder.
  Dancing Stage Fusion (c) 1998 2004 Konami. Not affiliated with Konami.
