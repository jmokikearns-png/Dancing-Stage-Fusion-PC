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

/* ---- Extra songs (port/core/dsfcore_extrasong.cpp, the PS1 side) ----------
 * The builder (dlxbuild.c) fills dlx_num_extra, dlx_extra[] and dlx_tables
 * (the song, stream, graphics, group, entry and XA tables with the extra
 * songs after the stock entries, and their names) and points the game's
 * constants at them; these routines replace the few game functions whose
 * answers change. */

#define MAX_EXTRA 12
struct DlxExtra
{
	unsigned record_id;	/* master-table id (song record word 0) */
	unsigned number;	/* song number (record word 1) */
	unsigned stream_id;	/* music id of its stream (record word 2) */
	unsigned unlock_id;	/* listed once this song is cleared (0: always) */
	unsigned unlock_mask;	/* on these charts (bit = Beginner, Light, Standard, Heavy) */
};
unsigned dlx_num_extra;
struct DlxExtra dlx_extra[MAX_EXTRA];
unsigned dlx_tables[0x1580 / 4];

/* 0x8002C398: music id -> 2 (STR stream) or 1 (XA).  Ids below 25 are the
 * stock streams; the extra songs' streams follow their preview ids. */
unsigned dlx_kind( unsigned id )
{
	unsigned k;
	id &= 0xFFFF;
	if( id < 25 )
		return 2;
	for( k = 0; k < dlx_num_extra; ++k )
		if( dlx_extra[k].stream_id == id )
			return 2;
	return 1;
}

/* Results keep one save record per song: +0x24 the song id, the grade of
 * chart d (single 0..3, double 4..7) at +0x46 + d, written only after a
 * cleared play. */
#define SAVE_PTR	(*(unsigned char *volatile *)0x800B1AFCu)
static int unlocked( const struct DlxExtra *e )
{
	const unsigned char *save = SAVE_PTR;
	int i, d;
	if( ((unsigned)save & 0xFFE00000u) != 0x80000000u )
		return 0;
	for( i = 0; i < 32; ++i )
	{
		const unsigned char *rec = save + 0x90 + 0x50 * i;
		if( (*(const unsigned *)(rec + 0x24) & 0xFFFF) != e->unlock_id )
			continue;
		for( d = 0; d < 8; ++d )
			if( ((e->unlock_mask >> (d & 3)) & 1) && rec[0x46 + d] )
				return 1;
	}
	return 0;
}

/* 0x80050C40: is this song record listed (DOUBLE: only with double charts)?
 * Plus the unlocks. */
#define PLAY_STYLE	(*(volatile signed char *)0x800BA328u)
unsigned dlx_avail( const unsigned *rec )
{
	unsigned listed = 1, k;
	if( PLAY_STYLE == 2 )
		listed = (rec[5] & 0xF) != 0;
	for( k = 0; listed && k < dlx_num_extra; ++k )
		if( dlx_extra[k].record_id == rec[0] && dlx_extra[k].unlock_id && !unlocked(&dlx_extra[k]) )
			listed = 0;
	return listed;
}

/* Workout results: song idx's 176x55 banner at x = 640 + 64 (idx / 4),
 * y = 256 + 55 (idx % 4), CLUT (640, 476 + idx) -- 0x8008CE50 for drawing,
 * the loader at 0x8008A9A0 (patched to call this) for loading.  The extra
 * songs get the 8 free places on that screen, the most recently played
 * first; any other one loads a 2x1 stand-in (dlx_workout_lz) to a corner. */
#define NUM_SLOTS 8
static const unsigned short SLOT_XY[NUM_SLOTS][2] = {
	{ 768, 0 }, { 768, 55 }, { 768, 110 }, { 768, 165 },
	{ 847, 156 }, { 896, 50 }, { 896, 105 }, { 896, 160 },
};
#define LOADER_RA	0x8008A9BCu	/* after the loader's jal to us and its delay slot */
#define HISTORY		((volatile unsigned *)0x800DE668u)	/* head, count */
#define HISTORY_ROWS	((volatile unsigned char *)0x800DE67Cu)	/* 50 x 16, song number at +8 */
#define CURRENT_SONG	(*(volatile unsigned char *)0x800BA32Au)
static int g_slot_song[NUM_SLOTS];
static int g_stand_in;

static int extra_by_number( unsigned number )
{
	unsigned k;
	for( k = 0; k < dlx_num_extra; ++k )
		if( dlx_extra[k].number == number )
			return (int)k;
	return -1;
}

static void assign_slots( void )
{
	unsigned count = HISTORY[1], head = HISTORY[0], r;
	int used = 0, s;
	for( s = 0; s < NUM_SLOTS; ++s )
		g_slot_song[s] = -1;
	if( count > 50 ) count = 50;
	if( head >= 50 ) head = 0;
	for( r = 0; r <= count && used < NUM_SLOTS; ++r )
	{
		const unsigned row = (head + 50 - r) % 50;	/* newest first; r = 0: the song just played */
		const int k = extra_by_number( r == 0 ? CURRENT_SONG : HISTORY_ROWS[16 * row + 8] );
		int have = k < 0;
		for( s = 0; s < used && !have; ++s )
			have = g_slot_song[s] == k;
		if( !have )
			g_slot_song[used++] = k;
	}
}

void dlx_workout_slot( int i, unsigned *x, unsigned *y, unsigned *clut_x, unsigned *clut_y )
{
	const int loading = __builtin_return_address(0) == (void *)LOADER_RA;
	unsigned px = 640 + 64 * (unsigned)(i / 4), py = 256 + 55 * (unsigned)(i % 4);
	if( i >= 20 && i - 20 < (int)dlx_num_extra )
	{
		const int k = i - 20;
		int s, slot = 0;
		if( loading && k == 0 )
			assign_slots();
		px = 1000; py = 511;	/* the stand-in's corner */
		for( s = 0; s < NUM_SLOTS; ++s )
			if( g_slot_song[s] == k )
			{
				px = SLOT_XY[s][0];
				py = SLOT_XY[s][1];
				slot = 1;
			}
		if( loading && !slot )
			g_stand_in = 1;
	}
	*x = px;
	*y = py;
	*clut_x = 640;
	*clut_y = 476 + (unsigned)i;
}

/* The loader's picture decode (0x8008AA00): a song without a place gets its
 * TIM cut to a 2x1 image (its CLUT kept), uploaded to the corner. */
typedef unsigned (*LzFn)( const unsigned char *src, unsigned char *dst );
#define GAME_LZ	((LzFn)0x8002E6F0u)
unsigned dlx_workout_lz( const unsigned char *src, unsigned char *dst )
{
	unsigned n = GAME_LZ( src, dst ), clut, i;
	unsigned char *im;
	static const unsigned char image[16] = { 16, 0, 0, 0,  0, 0, 0, 0,  2, 0, 1, 0,  0, 0, 0, 0 };
	if( !g_stand_in )
		return n;
	g_stand_in = 0;
	clut = dst[8] | dst[9] << 8 | dst[10] << 16 | (unsigned)dst[11] << 24;
	im = dst + 8 + clut;
	for( i = 0; i < 16; ++i )
		im[i] = image[i];
	return 8 + clut + 16;
}

/* RECORDS' mini-banner loop (overlay 0x800E8F78): banner n goes to
 * x = 832 + 21 (n % 3), y = 256 + 26 (n / 3), which past n = 26 runs into
 * the palettes and off VRAM: those go to the free page at x = 960. */
#define REC_OBJ	(*(unsigned char *volatile *)0x800F2140u)
void dlx_records_slot( unsigned char *slot )
{
	unsigned char *obj = REC_OBJ;
	const unsigned n = *(volatile unsigned *)(obj + 0x2FC) - 1;	/* counted before the decode */
	unsigned k, x, y;
	if( n < 27 || n >= 32 || slot != obj + 0x7C + 20 * n )
		return;
	k = n - 27;
	x = 960 + 21 * (k % 3);
	y = 26 * (k / 3);
	slot[0xC] = (unsigned char)x; slot[0xD] = (unsigned char)(x >> 8);
	slot[0xE] = (unsigned char)y; slot[0xF] = (unsigned char)(y >> 8);
}

/* ---- NEW!! beside an unlocked song not yet played ----------------------
 * The music select's sprite sheet (pack 201.. item 4, the arrows' texture
 * page) holds a "New!!" sprite at u 8, v 96, 64 x 25 that the game never
 * draws.  The select draws its two arrows with the sprite routine
 * 0x80027C84 (x, y, w, h; u, v, clut x, clut y, colour, OT slot on the
 * stack) from 0x80050364: that call comes here, and after the right arrow
 * (u 52) the song under the cursor gets NEW!! when it is an extra song with
 * an unlock condition, unlocked, and has no save record yet (one is written
 * when a play is cleared).  The song under the cursor: the list
 * [0x800A9E10], number at +0x18 + 4 * [+0x14]. */
typedef void (*SprtFn)( int x, int y, int w, int h, int u, int v, int cx, int cy, int colour, int ot );
#define GAME_SPRT	((SprtFn)0x80027C84u)
#define SELECT_LIST	(*(unsigned char *volatile *)0x800A9E10u)

static int played( const struct DlxExtra *e )
{
	const unsigned char *save = SAVE_PTR;
	int i;
	if( ((unsigned)save & 0xFFE00000u) != 0x80000000u )
		return 1;
	for( i = 0; i < 32; ++i )
		if( (*(const unsigned *)(save + 0x90 + 0x50 * i + 0x24) & 0xFFFF) == (e->record_id & 0xFFFF) )
			return 1;
	return 0;
}

static int cursor_is_new( void )
{
	const unsigned char *list = SELECT_LIST;
	unsigned number, k;
	if( ((unsigned)list & 0xFFE00000u) != 0x80000000u )
		return 0;
	number = *(const unsigned *)(list + 0x18 + 4 * *(const unsigned *)(list + 0x14));
	for( k = 0; k < dlx_num_extra; ++k )
		if( dlx_extra[k].number == number )
			return dlx_extra[k].unlock_id && unlocked( &dlx_extra[k] ) && !played( &dlx_extra[k] );
	return 0;
}

void dlx_arrow_sprite( int x, int y, int w, int h, int u, int v, int cx, int cy, int colour, int ot )
{
	GAME_SPRT( x, y, w, h, u, v, cx, cy, colour, ot );
	if( u == 52 && cursor_is_new() )
		GAME_SPRT( -136, y - 34, 64, 25, 8, 96, cx, cy, colour, ot );
}
