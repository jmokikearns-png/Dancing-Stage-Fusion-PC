DANCING STAGE FUSION DELUXE - TEST BUILD 2 (PlayStation disc builder, Windows)
==========================================================================

Dancing Stage Fusion Deluxe is the PlayStation version of the Dancing Stage
Fusion PC changes: the same game on a PS1 disc image, with the PC port's
additions built in one at a time.  It plays on DuckStation (or any accurate
PS1 emulator), and should on a real PlayStation too.

This is not a disc image.  It is a small program that makes one from YOUR
copy of the game, on your computer.  Nothing of Konami's is included.

NEW IN TEST BUILD 2
  - The 12 extra songs, from the same song pack the PC version uses: in the
    music select (GAME, DOUBLE, WORKOUT), with their banners, title plates,
    previews, music, charts and backgrounds; the results, WORKOUT results and
    RECORDS show them too.  Heaven and Earth stays hidden until you clear
    PARANOIA survivor on DIFFICULT or EXPERT, as on PC.
  - The image is bigger: about 320 MB (the songs go after the end of the
    disc).

IN TEST BUILD 1
  - STEP MARK COLORS TYPE3 (OPTIONS > GRAPHIC OPTIONS > STEP MARK COLORS):
    the arrows coloured by rhythm like TYPE2, in DDR Solo's colours -
    quarter notes orange, eighths cyan, twelfths (triplets) magenta,
    sixteenths and finer purple.  Left / Right goes TYPE1, TYPE2, TYPE3.
    It is saved with your system data like the other options.

WHAT YOU NEED
  Your disc image of Dancing Stage Fusion (Europe), SLES-04163, as a raw
  .bin + .cue (the usual Redump format, "MODE2/2352"), e.g.
    Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).bin
    Dancing Stage Fusion (Europe) (En,Fr,De,Es,It).cue

  For the extra songs: the song pack you use with the PC version (its three
  files extrasongs.txt, gfx.bin and media.bin, in the PC version's folder
  mods\installed\dsf.extrasongs\1.0.0\data).  Without it the builder
  makes a disc with TYPE3 colours only.

MAKING THE DISC
  1. Put your .bin and .cue in the "disc" folder (or next to the program),
     and the song pack's three files in the "songs" folder.
  2. Double-click "Build Deluxe Disc.exe".  (Or drag your .cue onto it.)
  3. It checks your image first: it must be the PAL game, unpatched, and
     undamaged.  If anything is wrong it says what, and writes nothing.
  4. It writes "Dancing Stage Fusion Deluxe.bin" and ".cue" beside your
     image (about 250 MB; your original is not changed).
  5. Open "Dancing Stage Fusion Deluxe.cue" in DuckStation.

PLEASE TEST
  - STEP MARK COLORS: TYPE3 shows, Left / Right cycles through all three.
  - Songs with TYPE3: the colours above, and look for palette corruption -
    wrong colours on the arrows, the receptors (the grey arrows at the
    top), or anything else on screen, in GAME, DOUBLE,
    WORKOUT and LESSON modes.  TYPE1 and TYPE2 should look exactly as before.
  - The setting staying TYPE3 after saving and restarting.
  - The extra songs: each one's banner, preview, music, steps and
    background; two or more songs in a row (stage 2 and 3); the results,
    WORKOUT results and RECORDS > HIGH SCORES; DOUBLE MODE; the Heaven and
    Earth unlock.  Loading between songs may take a moment longer than
    before: the dancers now load again after the menus.
  If you see a problem, a screenshot and the song / difficulty help most.

GOOD TO KNOW
  - DuckStation gives each game its own memory card by its serial; the
    Deluxe disc has the same serial as the original, so they share a card.
    That is fine: the original game just shows TYPE1 where Deluxe saved
    TYPE3.
  - Tested here (Beetle PSX with the free OpenBIOS, no sound output): the
    32-song list, extra songs played through to the results (music
    playing), two stages in a row in GAME and WORKOUT mode, the WORKOUT
    results with an extra song's banner, RECORDS > HIGH SCORES, DOUBLE MODE
    starting an extra song, the Heaven and Earth unlock.
  - Tested only in an emulator here,
    not yet in DuckStation with a real PlayStation BIOS or on a real
    PlayStation.  The image keeps the original's error-correction data
    valid, so it should also burn and play on hardware - untested.

PERSONAL USE ONLY
  The disc image you make contains Konami's game: keep it to yourself, do
  not share or upload it.  This program contains only this project's own
  code.

Dancing Stage Fusion (c) 1998 2004 Konami.  Not affiliated with Konami.
Port by CrashReporter.
