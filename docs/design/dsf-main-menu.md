# Dancing Stage Fusion (SLES-04163): the main menu

Verified 2026-10-23 on the Linux debug build (write traces of the cursor,
disassembly, VRAM dumps).  Used by `port/core/dsfcore_quitmenu.cpp`.

- Main-executable code.  State at 0x800D95F0: +0x00 mask of the entries that
  can be chosen (bit i; init 0x80073140 asks 0x8008A004 for the six ids at
  0x800AFC10), +0x04 idle timer (sound 0x337 at 500, back to the title at
  750), +0x08 fade, +0x10 cursor, +0x11 the chosen action (0x10 = back).
- Input 0x80073258, for both players: Up / Down step the cursor modulo 6
  through the mask (inline loops at 0x80073344 / 0x800733F4, then the
  cursor sound 0x31D at 0x80073438); decide (0x860) jumps through the table
  at 0x80020580 by cursor < 6, every case ending at the decide sound
  (0x30C) at 0x80073508; back (0x110) sets +0x11 = 0x10.
- Draw 0x80075320 -> 0x80073B44 (entries), 0x800738D0, 0x80073648.  Entries:
  6 sprites from the table 0x800AFC1C {u, v, w, h}: 256-texel 4-bit
  pictures stacked in the VRAM page at (960, 0) (v 0..186, heights 34, 34,
  34, 28, 28, 28); palettes (960, 240) normal and (976, 240) for the
  cursor's entry (0x80027C84: clut = y << 6 | x >> 4); a shadow copy 4 px
  down-right at brightness 0; y from -0x96, h + 8 apart.  Rows 186..239 of
  that page are free on this screen.
- The pictures stay in VRAM (they are not reloaded when the menu opens).
- Palette indices: 0 clear, 1 shadow, 2..7 outline (blue), 8..15 fill (grey
  to white; yellow in the cursor palette).
