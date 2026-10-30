/* payload.c -- Dancing Stage Fusion Deluxe: the resident code (PS1, MIPS I).
 *
 * Built into the executable's zero block 0x80098AA8..0x8009DC04, from 0x80098AB0
 * (notes/dsf-ps1-version.md) by make_patch.py; reached from patched game
 * code through the trampolines in entry.S.
 *
 * STEP MARK COLORS TYPE3 (the PS1 side of port/core/dsfcore_stepcolors.cpp
 * and the menu part of dsfcore_menus.cpp): system data [0x800B1AFC] + 0x77
 * = 2 runs TYPE2's code (the three `== 1` tests become `!= 0`, a patch of
 * the branches), then the 12ths keep palette 3 and the four palettes take
 * DDR Solo's hues. */

#define SYS_DATA	(*(unsigned char *volatile *)0x800B1AFCu)
#define STEP_MARK_COLORS 0x77
#define TYPE3 2

static int type3( void )
{
	const unsigned char *s = SYS_DATA;
	return s != 0 && s[STEP_MARK_COLORS] == TYPE3;
}

/* After the note's palette is chosen (0x800519F0): the palette-3 notes that
 * are not 12ths (320 / 672 in the 1024 of a beat) move to palette 1.
 * gsp: the game function's sp (s16 palette at +0x970, the note at +0x934). */
void dlx_note_palette( unsigned char *gsp )
{
	short *palette;
	unsigned pos;
	if( !type3() )
		return;
	palette = (short *)(gsp + 0x970);
	if( *palette != 3 )
		return;
	pos = **(unsigned **)(gsp + 0x934) & 0x3FFu;
	if( pos != 320 && pos != 672 )
		*palette = 1;
}

/* Solo's hues, Q16 (top channel 1), by palette: 0 4th orange, 1 16th purple,
 * 2 8th cyan, 3 12th magenta. */
static const unsigned HUE[4][3] = {
	{ 65536, 39977, 0 },
	{ 57016, 1966, 65536 },
	{ 1966, 47841, 65536 },
	{ 65536, 0, 41943 },
};

/* Each colour keeps its brightness (top channel) and whiteness (bottom /
 * top) and takes the hue: top * (h + (1 - h) * bottom / top). */
static void tint( volatile unsigned short *p, int n, int palette )
{
	const unsigned *h = HUE[palette];
	int i, k;
	for( i = 0; i < n; ++i )
	{
		const unsigned c = p[i];
		int ch[3], top, bottom;
		unsigned out = c & 0x8000u;
		ch[0] = c & 31; ch[1] = (c >> 5) & 31; ch[2] = (c >> 10) & 31;
		top = ch[0]; bottom = ch[0];
		for( k = 1; k < 3; ++k )
		{
			if( ch[k] > top ) top = ch[k];
			if( ch[k] < bottom ) bottom = ch[k];
		}
		if( top - bottom < 2 )
			continue;	/* transparent, black, greys */
		for( k = 0; k < 3; ++k )
		{
			unsigned v = ((unsigned)top * h[k] + (unsigned)bottom * (65536u - h[k]) + 32768u) >> 16;
			out |= (v > 31? 31: v) << (5 * k);
		}
		p[i] = (unsigned short)out;
	}
}

#define PALETTE_BUFFER	(*(unsigned short *volatile *)(0x800CC448u + 0x84u))
#define BUILD1_BASES	((volatile short *)0x800CE360u)

/* The per-frame palette builders' exits (0x8005517C, 0x80055A7C). */
void dlx_build1( void )
{
	int p;
	if( !type3() )
		return;
	for( p = 0; p < 4; ++p )
		tint( PALETTE_BUFFER + BUILD1_BASES[p], 16, p );
}

void dlx_build2( void )
{
	int p;
	if( !type3() )
		return;
	for( p = 0; p < 4; ++p )
		tint( PALETTE_BUFFER + 64 * p, 64, p );
}

/* GRAPHIC OPTIONS draws STEP MARK COLORS' value with the overlay's string
 * lookup 0x800E8EF4 on its 2-entry table 0x800F130C (TYPE1, TYPE2): value 2
 * shows the TYPE2 text with its last 2 made a 3, in the game's language. */
typedef const char *(*StringFn)( unsigned id );
#define GAME_STRING	((StringFn)0x800E8EF4u)
#define STEP_MARK_TABLE	((volatile unsigned *)0x800F130Cu)

static char g_type3_text[32];

const char *dlx_step_mark_text( unsigned id, unsigned value )
{
	const char *t;
	int i, two = -1;
	if( value != TYPE3 )
		return GAME_STRING( id );
	t = GAME_STRING( STEP_MARK_TABLE[1] );
	for( i = 0; i < 31 && t[i]; ++i )
	{
		g_type3_text[i] = t[i];
		if( t[i] == '2' )
			two = i;
	}
	g_type3_text[i] = 0;
	if( two >= 0 )
		g_type3_text[two] = '3';
	return g_type3_text;
}
