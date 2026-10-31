# Dancing Stage Fusion (SLES-04163): the OPTIONS screens

Verified 2026-10-17 on the Linux debug build (disassembly of READ_DT e227 with
`analysis/disc-exe/scripts/ovl_dis.py`, live RAM over the debug port,
screenshots in `build-linux/runs/opt/`, disc-derived, not in git).  Used by
`port/core/dsfcore_menus.cpp`.

## Where it lives
- OPTIONS = game mode 1 = code overlay **e227** loaded at 0x800E8000 (38408 B),
  mode entry 0x800E8300 (mode table 0x800B324C, `scripts/mode_overlays.py`).
  While it runs, 0x801C4000 holds e233.
- `[0x800F0E00]` = pointer to the screen state (0x800F155C).  Fields used:
  - `+0x00` frame counter since the last accepted input; triggers need >= 8
  - `+0x04 + 4p` auto-repeat counter of player p (post handler increments it
    while Up/Down (0x5000) is held; > 0x20 repeats, then set to 0x1A)
  - `+0x0C` help-text animation counter (0 restarts it), `+0x24` help string id
  - `+0x10 + depth` cursor byte of the screen at that depth (top menu 0x10,
    submenus 0x11, sub-submenus 0x12)
  - `+0x15` current screen, `+0x16` next screen, `+0x17` 1 = running
  - `+0x20` base of the loaded string table (e027 leaf of the current language)
  - `+0x5C/+0x60` cursor highlight pulse (alpha 0x60..0x80)
- Screen records at **0x800F10A4**, 20 bytes each: `{init, input, draw, post, depth}`

| # | screen | init | input | draw | post | depth |
|---|---|---|---|---|---|---|
| 2 | OPTIONS top menu | 800EE9D4 | 800EEAB0 | 800EEE20 | 800EF020 | 0 |
| 3 | SOUND OPTIONS | 800ED04C | 800ED124 | 800ED5BC | 800EDC94 | 1 |
| 4 | CONTROLLER SETTINGS | 800EAC98 | 800EA4E4 | 800EA804 | 800EAC04 | 1 |
| 8 | GRAPHIC OPTIONS | 800EA420 | 800E9A48 | 800E9E90 | 800EA38C | 1 |

  (others: 5 800EC2B8.. DOUBLE MODE (depth 2), 6 SAVE/LOAD, 7 GAME, 9 WORKOUT,
  10 LANGUAGE, 11 CREDITS; 0/1 empty.)
- Change screen: `0x800E8E04(3, screen)`.  Strings: `0x800E8EF4(id)` -> pointer
  (`state+0x20` + offset table); help: `0x800E8F18(id)` stores `state+0x24`.

## Game services (main EXE)
- `0x80024CB4(x, y, str, flags)` menu text.  flags: low nibble = colour,
  0x2000 = centred on x, 0x8000 = fade by `[[0x80092A28]+0x17]`, 0x4000 = depth
  from `[[0x80092A2C]+8]` (the screens set it to 8 first).  Glyphs: table
  0x800921E4, 8 bytes per Latin-1 code.
- `0x800256C4(str)` -> width in pixels.
- Pads: `[0x80092A28] + 0x50 + 0x10p` held, `+0x54 + 0x10p` pressed this frame;
  bits (game order) Up 0x1000, Down 0x4000, Left 0x8000, Right 0x2000,
  Cross|Circle 0x60, Triangle|Select 0x110.
- Sounds: `0x80089DCC` cursor move, `0x80089E4C` value change, `0x80089DEC`
  decide, `0x80089E2C` back.
- System data `[0x800B1AFC]`; defaults table 0x800B1B08.  SOUND +0x50 (0..1),
  VOICE +0x51 (0..2), TIMING +0x52 (signed -8..8); GRAPHIC: DANCER DISPLAY
  +0x78 (0..2), BG FX +0x74 (0..1), BG BRIGHTNESS +0x75 (0..15), STEP MARK
  COLORS +0x77 (0..1; the port adds 2 = TYPE3, see below).

## List screen layout (draw handlers)
Rows from y = -0xA4, 0x22 apart; one blank row before EXIT.  Per row:
- label: `x = -width - 0x14`, flags `0xC000 | (row != cursor)`; EXIT: x = 0,
  flags `| 0x2000`.
- value: `x = 0x14`, flags `0xC000 | 2` when it equals the default, else `| 3`.
- cursor row: `0x80027BA4(-0x140, y-2, 0x280, 0x1E, 0xED, 0x9F, 0x1C,
  ((state[0x60] * [[0x800B3314]+1]) >> 7) & 0xFF, 0x80000009)` (bar), then
  `0x800EFBF4(0, y+0xD, 1, [[0x800B3314]+1], 8)` (star between label and
  value); on EXIT two stars at +-(width+0x28)/2 with third argument 0 and 1.
- after the rows: `0x800EFEDC()`; on EXIT `0x800F003C(0x10E, 0xCA,
  ([[0x80092A28]+0x90] * [[0x800B3314]+1]) >> 7, 8)`; then `0x800EF0B4()`
  (help panel; it starts at screen y 373, so 8 rows fit).

| screen | rows | gap after | gap instruction | help table |
|---|---|---|---|---|
| SOUND | SOUND, VOICE, TIMING, EXIT | row 2 | 0x800EDBFC `addiu s4,s4,0x22` | 0x800F144C |
| CONTROLLER | DOUBLE MODE SETTINGS, DANCE PLAY SETTINGS 1/2, EXIT | row 2 | 0x800EAB64 | (tbd) |
| GRAPHIC | DANCER DISPLAY, BG FX, BG BRIGHTNESS, STEP MARK COLORS, EXIT | row 3 | 0x800EA2F4 | 0x800F122C |

## Input handlers (SOUND / GRAPHIC; CONTROLLER to be confirmed)
For p = 0, 1 (first match wins), with `t` = pressed, `h` = held:
- `t & 0x110` and `state[0] >= 8`: back (sound 0x80089E2C, screen 2).
- `t & 0x60` and `state[0] >= 8`: only on EXIT, same as back.
- `h & Up` / `h & Down`: if `t` has it, accepted when `state[0] >= 8`; else
  when the repeat counter > 0x20 (set to 0x1A).  Accepted: `state[0] = 0`,
  cursor wraps (mod row count), sound 0x80089DCC.
- `t & Left` / `t & Right` with `state[0] >= 8`: `state[0] = 0`, value change
  (SOUND 0x800ED454, GRAPHIC 0x800E9D3C), sound 0x80089E4C when it changed.
- cursor changed: `state[0x0C] = 0`, help = table[cursor].

## STEP MARK COLORS (+0x77) and the port's TYPE3
- Change: 0x800E9D3C row 3 steps it `(v + dir + 2) mod 2`; text id from the
  2-entry table 0x800F130C (`lui v1,0x800f` 0x800EA150 / `addiu v1,v1,0x130c`
  0x800EA158); default byte 0x800B1B2F.  Save check 0x80084BB8: `>= 2` -> default.
- Gameplay tests `== 1` at 0x80051950 (note palette: pos & 0x3FF 0 -> 0, 0x200 -> 2,
  0x100/0x300 -> 1, else 3; stored at sp+0x970), 0x80054D24 (palette builder 1:
  four 16-colour palettes at the halfword indices 0x800CE360[0..3] of the
  buffer [0x800CC448+0x84]: red, yellow, blue, green; the one gameplay uses)
  and 0x800556FC (builder 2: 64-entry palettes at 0/0x80/0x100/0x180).
- TYPE3 (port/core/dsfcore_stepcolors.cpp, dsfcore_menus.cpp): instruction
  plugins present 2 as 1 at the three tests and the save check, move non-12th
  palette-3 notes to palette 1 (0x800519F0; the step-list builder puts triplets at
  (3 - n) * 1024 / 3 rounded down to 32 = 320 / 672 in the beat, never 341 / 683), and re-tint the palettes at the
  builders' exits (0x8005517C, 0x80055A7C) to DDR Solo's hues: 4th orange,
  8th cyan, 12th magenta, 16th and finer purple.
- Builder choice: 0x80052AEC `jal 0x800431E8` (= byte [[0x80092A28] + 0x102]):
  nonzero -> builder 2 (0x80055554), else builder 1 (0x80054928).
- Text: e027_0..4 (EN FR DE IT ES) say TYPE1/TYPE2, TYPE1/TYPE2, TYP1/TYP2,
  TIPO1/TIPO2, TIPO1/TIPO2; TYPE3's text is TYPE2's with its last 2 made 3.

## TIMING (+0x52) and the port's AUTO TIMING
- TIMING is signed -8..8 in song-clock ticks (1/75 s); the step-list builder adds
  it to every step's time (0x8007FC90), so + judges presses later.
- PERFECT window, measured with autoplay at fixed offsets from the step times:
  about -7.25 .. -0.7 ticks (all PERFECT -6..-3), middle -4.
- AUTO TIMING (port/core/dsfcore_timing.cpp; engine PS1Core::CoreMeasureLatency):
  scan = host's sound delay (queue average + device latency) and picture delay
  (2 refreshes with vsync) -> PICTURE SYNC ON, PICTURE DELAY -picture, TIMING
  round(sound / 13.3 ms).  Learning: presses from [0x80092A28]+0x54 (+0x10 per
  pad) at the next pad read, matched to steps within 10 ticks; error =
  clock - (t - 3); after the song TIMING += round(trimmed mean), capped 3.
