/* dsfcore_stepcolors.cpp -- STEP MARK COLORS TYPE3: DDR Solo's note colours.
 *
 * GRAPHIC OPTIONS' STEP MARK COLORS is system data [0x800B1AFC] + 0x77:
 * 0 TYPE1 (the colours cycle), 1 TYPE2 (by note: 4th red, 8th blue, 16th
 * yellow, anything else green).  TYPE3 = 2 is TYPE2 with Solo's colours:
 * 4th orange, 8th cyan, 12th magenta, 16th and finer purple.  The menu side
 * (the value, its text) is in dsfcore_menus.cpp.
 *
 * The game tests the setting `== 1` in three places, each `lbu v1, 0x77(..);
 * addiu v0, zero, 1; b?? v1, v0`:
 *   0x80051950  the note's palette: TYPE2 by its place in the beat (sp+0x970,
 *               set at 0x80051984: 0 a 4th, 2 an 8th, 1 a 16th, else 3)
 *   0x80054D24, 0x800556FC  the two per-frame palette builders: four
 *               palettes, red, yellow, blue, green, a sine-animated gradient
 *               plus beat-flash entries, in the buffer [0x800CC448 + 0x84]:
 *               builder 1 at the halfword indices in 0x800CE360[0..3] (16
 *               entries each), builder 2 at 0, 64, 128, 192 (64 each).
 * Before each `addiu` a value of 2 is presented as 1, so TYPE3 runs TYPE2's
 * code.  Then, for TYPE3 only:
 *   0x800519F0  (after the note's palette is chosen) the palette-3 notes that
 *               are not 12ths (320 / 672 in the beat) move to palette 1, so
 *               palette 3 is 12ths only;
 *   0x8005517C, 0x80055A7C  (each builder's exit) the four palettes are
 *               re-tinted: each colour keeps its brightness (top channel)
 *               and whiteness (bottom / top channel) and takes Solo's hue.
 * 0x80084BC0 is the save check (`sltiu v0, v0, 2`, else the default): 2 is
 * let through.  Every site is an instruction plugin, listed in game.toml
 * [recompiler] mod_instruction_sites.
 */
#include "mod_plugins.h"
#include "cpu_state.h"

#include <cstdint>

namespace {

const uint32_t SYSTEM_DATA_PTR = 0x800B1AFCu;
const uint32_t STEP_MARK_COLORS = 0x77u;
const uint8_t TYPE3 = 2;

const uint32_t SITE_NOTE_TEST = 0x80051950u, SITE_BUILD1_TEST = 0x80054D24u, SITE_BUILD2_TEST = 0x800556FCu;
const uint32_t ADDIU_V0_1 = 0x24020001u;
const uint32_t SITE_NOTE_PALETTE = 0x800519F0u, NOTE_PALETTE_STOCK = 0x3C0B800Cu;	/* lui t3, 0x800c */
const uint32_t SITE_BUILD1_EXIT = 0x8005517Cu, BUILD1_EXIT_STOCK = 0x0801551Cu;	/* j 0x80055470 */
const uint32_t SITE_BUILD2_EXIT = 0x80055A7Cu, BUILD2_EXIT_STOCK = 0x08015723u;	/* j 0x80055c8c */
const uint32_t SITE_SAVE_CHECK = 0x80084BC0u, SAVE_CHECK_STOCK = 0x2C420002u;	/* sltiu v0, v0, 2 */

const uint32_t SP_NOTE_PALETTE = 0x970u, SP_NOTE = 0x934u;	/* s16 palette; -> step entry (u32 position) */
const uint32_t PALETTE_STATE = 0x800CC448u, PALETTE_BUFFER = 0x84u;
const uint32_t BUILD1_BASES = 0x800CE360u;

/* Solo's hues (the top channel is 1), by palette: 0 4th, 1 16th, 2 8th, 3 12th. */
const float HUE[4][3] = {
	{ 1.00f, 0.61f, 0.00f },	/* orange */
	{ 0.87f, 0.03f, 1.00f },	/* purple */
	{ 0.03f, 0.73f, 1.00f },	/* cyan */
	{ 1.00f, 0.00f, 0.64f },	/* magenta */
};

bool type3()
{
	const uint32_t sys = psx_mod_read_word( SYSTEM_DATA_PTR );
	return sys != 0 && psx_mod_read_byte( sys + STEP_MARK_COLORS ) == TYPE3;
}

/* v1 = 2 -> 1 just before `addiu v0, zero, 1; b?? v1, v0`. */
void on_type_test( CPUState *cpu, uint32_t )
{
	if( cpu->gpr[3] == TYPE3 )
		cpu->gpr[3] = 1;
}

/* The save check: let 2 through. */
void on_save_check( CPUState *cpu, uint32_t )
{
	if( cpu->gpr[2] == TYPE3 )
		cpu->gpr[2] = 1;
}

/* A 12th (1024 per beat): the step-list builder places triplet notes at
 * (3 - n) * 1024 / 3 rounded down to 32 (0x8007FDDC), i.e. 320 and 672. */
bool is_twelfth( uint32_t position )
{
	const uint32_t x = position & 0x3FFu;
	return x == 320 || x == 672;
}

void on_note_palette( CPUState *cpu, uint32_t )
{
	if( !type3() )
		return;
	const uint32_t sp = cpu->gpr[29];
	if( psx_mod_read_half(sp + SP_NOTE_PALETTE) != 3 )
		return;
	const uint32_t note = psx_mod_read_word( sp + SP_NOTE );
	if( !is_twelfth(psx_mod_read_word(note)) )
		psx_mod_write_half( sp + SP_NOTE_PALETTE, 1 );
}

uint16_t tint( uint16_t c, const float *hue )
{
	const int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
	const int top = r > g? (r > b? r: b): (g > b? g: b);
	const int bottom = r < g? (r < b? r: b): (g < b? g: b);
	if( top - bottom < 2 )
		return c;	/* transparent, black, greys */
	const float white = float(bottom) / float(top);
	int out[3];
	for( int i = 0; i < 3; ++i )
	{
		const int v = int( float(top) * (hue[i] + (1.0f - hue[i]) * white) + 0.5f );
		out[i] = v > 31? 31: v;
	}
	return uint16_t( (c & 0x8000u) | (out[2] << 10) | (out[1] << 5) | out[0] );
}

void tint_palette( uint32_t entries, int n, int palette )
{
	for( int i = 0; i < n; ++i )
	{
		const uint32_t a = entries + 2u * uint32_t(i);
		psx_mod_write_half( a, tint(psx_mod_read_half(a), HUE[palette]) );
	}
}

void on_build1_exit( CPUState *, uint32_t )
{
	if( !type3() )
		return;
	const uint32_t buf = psx_mod_read_word( PALETTE_STATE + PALETTE_BUFFER );
	for( int p = 0; p < 4; ++p )
		tint_palette( buf + 2u * uint32_t(int16_t(psx_mod_read_half(BUILD1_BASES + 2u * uint32_t(p)))), 16, p );
	psx_mod_counter_add( "dsf.stepcolors.build1", 1 );
}

void on_build2_exit( CPUState *, uint32_t )
{
	if( !type3() )
		return;
	const uint32_t buf = psx_mod_read_word( PALETTE_STATE + PALETTE_BUFFER );
	for( int p = 0; p < 4; ++p )
		tint_palette( buf + 0x80u * uint32_t(p), 64, p );
	psx_mod_counter_add( "dsf.stepcolors.build2", 1 );
}

} // namespace

PSX_MOD_CONSTRUCTOR( dsf_register_stepcolors )
{
	const char *id = "dsf.stepmania.stepcolors";
	(void) psx_mod_register_instruction_plugin( id, SITE_NOTE_TEST, ADDIU_V0_1, on_type_test );
	(void) psx_mod_register_instruction_plugin( id, SITE_BUILD1_TEST, ADDIU_V0_1, on_type_test );
	(void) psx_mod_register_instruction_plugin( id, SITE_BUILD2_TEST, ADDIU_V0_1, on_type_test );
	(void) psx_mod_register_instruction_plugin( id, SITE_NOTE_PALETTE, NOTE_PALETTE_STOCK, on_note_palette );
	(void) psx_mod_register_instruction_plugin( id, SITE_BUILD1_EXIT, BUILD1_EXIT_STOCK, on_build1_exit );
	(void) psx_mod_register_instruction_plugin( id, SITE_BUILD2_EXIT, BUILD2_EXIT_STOCK, on_build2_exit );
	(void) psx_mod_register_instruction_plugin( id, SITE_SAVE_CHECK, SAVE_CHECK_STOCK, on_save_check );
}
