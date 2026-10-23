/* dsfcore_quitmenu.cpp -- QUIT on Dancing Stage Fusion's main menu.
 *
 * The main menu (GAME MODE .. OPTIONS) is main-executable code with its state
 * at 0x800D95F0: +0x00 a mask of the entries that can be chosen (bit i,
 * built by the init function 0x80073140), +0x10 the cursor, +0x11 the
 * chosen action.  Its parts, and what changes here:
 *
 *  - Pictures: each entry is a 256-texel-wide 4-bit picture in the VRAM
 *    page at (960, 0), drawn by the loop at 0x80073B44 from the table
 *    0x800AFC1C {u, v, w, h} (6 entries; y from -0x96, h + 8 apart) with the
 *    palettes at (960, 240), or (976, 240) for the cursor's entry.  The loop
 *    now reads a 7-entry copy of the table, starts 10 higher, and the
 *    seventh entry is a QUIT picture at v = 186 (free rows of that page).
 *  - The QUIT picture is put together from the menu's own lettering, which
 *    is in VRAM by then (the game keeps it there): I and T from OPTIONS, U
 *    from DOUBLE MODE with six of its identical straight rows taken out (the
 *    small entries' height), Q from OPTIONS' O with a short tail.  QUIT_RUNS
 *    says, for each run of pixels, where it comes from in that page or
 *    which palette index it is; the page is checked first (QUIT_SOURCE_HASH)
 *    so a different picture never gets cut up.  Put in place at the end of
 *    the menu's init, after its own VRAM work (DrawSync): the init's call of
 *    the empty function 0x80075358 goes to QM_INIT.
 *  - Cursor: Up / Down step the cursor modulo 6 through the mask (two inline
 *    loops at 0x80073344 and 0x800733F4); both call QM_STEP instead, which
 *    does the same modulo 7 with QUIT always available, then go on to the
 *    loops' common tail (cursor sound) at 0x80073438.
 *  - Choice: every choice plays the decide sound at 0x80073508 (QUIT, past
 *    the jump table's 6 cases, only that); that call goes to QM_DECIDE,
 *    which plays it and, on QUIT, asks the host to leave the program.
 */
#include "dsfcore_host.h"
#include "mod_plugins.h"
#include "cpu_state.h"

#include <cstdint>
#include <cstring>

extern "C" void gr_vram_transfer_out( int x, int y, int w, int h, uint16_t *data );

namespace {

uint32_t rd32( uint32_t a ) { return psx_mod_read_word( a ); }
uint8_t rd8( uint32_t a ) { return psx_mod_read_byte( a ); }
void wr32( uint32_t a, uint32_t v ) { psx_mod_write_word( a, v ); }
void wr8( uint32_t a, uint8_t v ) { psx_mod_write_byte( a, v ); }
inline uint32_t jal( uint32_t target ) { return 0x0C000000u | ((target >> 2) & 0x03FFFFFFu); }
inline uint32_t j( uint32_t target ) { return 0x08000000u | ((target >> 2) & 0x03FFFFFFu); }
inline uint32_t hi16( uint32_t a ) { return ((a + 0x8000u) >> 16) & 0xFFFFu; }
inline uint32_t lo16( uint32_t a ) { return a & 0xFFFFu; }

const uint32_t MENU = 0x800D95F0, MENU_MASK = MENU + 0x00, MENU_CURSOR = MENU + 0x10;
const int ENTRIES = 6, QUIT = 6;
const uint32_t PICTURES = 0x800AFC1C;	/* 6 x {u, v, w, h} */
const uint32_t SOUND = 0x8002A6D0;	/* sound(id) */

const uint32_t QM_INIT = 0x8F000100, QM_DECIDE = 0x8F000110, QM_STEP = 0x8F000120;
const uint32_t INIT_CALL = 0x80073234;		/* jal 0x80075358 (an empty function) */
const uint32_t DRAW_Y = 0x80073B4C;		/* addiu s6, zero, -0x96 */
const uint32_t DRAW_TABLE_HI = 0x80073B6C, DRAW_TABLE_LO = 0x80073B74;	/* lui v0, 0x800B; addiu s4, v0, -0x3E4 */
const uint32_t DRAW_COUNT = 0x80073C7C;	/* slti v0, s5, 6 */
const uint32_t STEP_UP = 0x80073344, STEP_DOWN = 0x800733F4;	/* the two step loops */
const uint32_t STEP_TAIL = 0x80073438;	/* cursor sound, a0 = 0x31D */
const uint32_t DECIDE_CALL = 0x80073508;	/* jal 0x8002A6D0, a0 = 0x30C in its delay slot */

const uint32_t STOCK[][2] = {
	{ INIT_CALL, 0x0C01D4D6 }, { 0x80075358, 0x03E00008 },
	{ DRAW_Y, 0x2416FF6A }, { DRAW_TABLE_HI, 0x3C02800B }, { DRAW_TABLE_LO, 0x2454FC1C }, { DRAW_COUNT, 0x2AA20006 },
	{ STEP_UP, 0x92040010 }, { STEP_UP + 4, 0x00000000 }, { STEP_UP + 8, 0x24840005 }, { STEP_UP + 12, 0x00930018 },
	{ STEP_DOWN, 0x92040010 }, { STEP_DOWN + 4, 0x00000000 }, { STEP_DOWN + 8, 0x24840001 }, { STEP_DOWN + 12, 0x00930018 },
	{ 0x80073384, 0x0801CD0E }, { 0x80073388, 0x2404031D }, { 0x80073434, 0x2404031D },
	{ DECIDE_CALL, 0x0C00A9B4 }, { DECIDE_CALL + 4, 0x2404030C },
};

/* The QUIT picture: 256 x 28 texels at v = 186 of the page at (960, 0). */
const int QUIT_W = 256, QUIT_H = 28, QUIT_V = 186;
const int SOURCE_ROWS = 186;			/* the six pictures */
const uint32_t QUIT_SOURCE_HASH = 0x279D1541u;	/* FNV-1a of those rows' halfwords */
struct Run { int16_t y, x, n, src_v, src_u; };	/* src_v < 0: palette index src_u */
const Run QUIT_RUNS[] = {
	{ 2, 98, 9, 163, 77 }, { 2, 117, 6, 40, 64 }, { 2, 127, 7, 40, 74 }, { 2, 138, 6, 163, 123 },
	{ 2, 147, 14, 163, 108 }, { 3, 96, 14, 164, 75 }, { 3, 116, 8, 41, 63 }, { 3, 126, 9, 41, 73 },
	{ 3, 137, 8, 164, 122 }, { 3, 146, 16, 164, 107 }, { 4, 94, 17, 165, 73 }, { 4, 116, 9, 42, 63 },
	{ 4, 126, 9, 42, 73 }, { 4, 135, 1, -1, 1 }, { 4, 136, 1, -1, 6 }, { 4, 137, 8, 165, 122 },
	{ 4, 145, 16, 165, 106 }, { 4, 161, 1, -1, 6 }, { 4, 162, 1, -1, 1 }, { 5, 93, 19, 166, 72 },
	{ 5, 116, 9, 43, 63 }, { 5, 125, 1, -1, 1 }, { 5, 126, 9, 43, 73 }, { 5, 135, 1, -1, 1 },
	{ 5, 136, 1, -1, 6 }, { 5, 137, 8, 166, 122 }, { 5, 145, 1, -1, 6 }, { 5, 146, 15, 166, 107 },
	{ 5, 161, 1, -1, 6 }, { 5, 162, 2, -1, 1 }, { 6, 92, 21, 167, 71 }, { 6, 116, 9, 44, 63 },
	{ 6, 125, 1, -1, 1 }, { 6, 126, 9, 44, 73 }, { 6, 135, 1, -1, 1 }, { 6, 136, 1, -1, 6 },
	{ 6, 137, 8, 167, 122 }, { 6, 145, 1, -1, 6 }, { 6, 146, 15, 167, 107 }, { 6, 161, 1, -1, 6 },
	{ 6, 162, 2, -1, 1 }, { 7, 92, 21, 168, 71 }, { 7, 113, 1, -1, 6 }, { 7, 116, 19, 45, 63 },
	{ 7, 135, 1, -1, 1 }, { 7, 136, 8, 168, 121 }, { 7, 144, 2, -1, 1 }, { 7, 146, 1, -1, 6 },
	{ 7, 147, 13, 168, 108 }, { 7, 160, 1, -1, 6 }, { 7, 161, 3, -1, 1 }, { 8, 91, 23, 169, 70 },
	{ 8, 114, 1, -1, 1 }, { 8, 115, 20, 52, 62 }, { 8, 135, 9, 169, 120 }, { 8, 144, 3, -1, 1 },
	{ 8, 147, 13, 169, 108 }, { 8, 160, 4, -1, 1 }, { 9, 91, 10, 170, 70 }, { 9, 101, 2, -1, 1 },
	{ 9, 103, 11, 170, 82 }, { 9, 114, 1, -1, 1 }, { 9, 115, 20, 53, 62 }, { 9, 135, 1, -1, 1 },
	{ 9, 136, 8, 170, 121 }, { 9, 144, 2, -1, 1 }, { 9, 148, 10, 170, 109 }, { 9, 158, 5, -1, 1 },
	{ 10, 91, 9, 171, 70 }, { 10, 100, 5, -1, 1 }, { 10, 105, 9, 171, 84 }, { 10, 114, 2, -1, 1 },
	{ 10, 116, 19, 54, 63 }, { 10, 135, 1, -1, 1 }, { 10, 136, 8, 171, 121 }, { 10, 144, 2, -1, 1 },
	{ 10, 148, 10, 171, 109 }, { 10, 158, 4, -1, 1 }, { 11, 91, 9, 172, 70 }, { 11, 100, 3, -1, 1 },
	{ 11, 105, 9, 172, 84 }, { 11, 114, 2, -1, 1 }, { 11, 116, 19, 55, 63 }, { 11, 135, 1, -1, 1 },
	{ 11, 136, 8, 172, 121 }, { 11, 144, 2, -1, 1 }, { 11, 148, 10, 172, 109 }, { 11, 158, 2, -1, 1 },
	{ 12, 91, 9, 173, 70 }, { 12, 100, 2, -1, 1 }, { 12, 106, 7, 173, 85 }, { 12, 113, 1, -1, 6 },
	{ 12, 114, 2, -1, 1 }, { 12, 116, 19, 56, 63 }, { 12, 135, 1, -1, 1 }, { 12, 136, 8, 173, 121 },
	{ 12, 144, 2, -1, 1 }, { 12, 148, 10, 173, 109 }, { 12, 158, 2, -1, 1 }, { 13, 91, 10, 174, 70 },
	{ 13, 105, 8, 174, 84 }, { 13, 113, 3, -1, 1 }, { 13, 116, 19, 57, 63 }, { 13, 135, 1, -1, 1 },
	{ 13, 136, 8, 174, 121 }, { 13, 144, 2, -1, 1 }, { 13, 148, 10, 174, 109 }, { 13, 158, 2, -1, 1 },
	{ 14, 91, 11, 175, 70 }, { 14, 104, 9, 175, 83 }, { 14, 113, 3, -1, 1 }, { 14, 116, 19, 58, 63 },
	{ 14, 135, 1, -1, 1 }, { 14, 136, 8, 175, 121 }, { 14, 144, 2, -1, 1 }, { 14, 149, 9, 175, 110 },
	{ 14, 158, 2, -1, 1 }, { 15, 91, 19, 176, 70 }, { 15, 110, 1, -1, 4 }, { 15, 111, 2, 176, 90 },
	{ 15, 113, 2, -1, 1 }, { 15, 115, 19, 59, 62 }, { 15, 134, 2, -1, 1 }, { 15, 136, 8, 176, 121 },
	{ 15, 144, 2, -1, 1 }, { 15, 149, 9, 176, 110 }, { 15, 158, 2, -1, 1 }, { 16, 92, 17, 177, 71 },
	{ 16, 109, 1, -1, 6 }, { 16, 110, 2, 177, 89 }, { 16, 112, 1, -1, 4 }, { 16, 113, 2, -1, 3 },
	{ 16, 115, 18, 60, 62 }, { 16, 133, 3, -1, 1 }, { 16, 136, 8, 177, 121 }, { 16, 144, 2, -1, 1 },
	{ 16, 149, 9, 177, 110 }, { 16, 158, 2, -1, 1 }, { 17, 93, 15, 178, 72 }, { 17, 108, 2, -1, 11 },
	{ 17, 110, 2, 178, 89 }, { 17, 112, 2, -1, 6 }, { 17, 114, 1, -1, 3 }, { 17, 115, 1, -1, 1 },
	{ 17, 117, 15, 61, 64 }, { 17, 132, 4, -1, 1 }, { 17, 136, 9, 178, 121 }, { 17, 145, 1, -1, 1 },
	{ 17, 149, 9, 178, 110 }, { 17, 158, 2, -1, 1 }, { 18, 93, 13, 179, 72 }, { 18, 106, 1, -1, 6 },
	{ 18, 107, 1, -1, 9 }, { 18, 108, 2, -1, 15 }, { 18, 110, 1, -1, 11 }, { 18, 111, 2, -1, 5 },
	{ 18, 113, 1, -1, 6 }, { 18, 114, 1, -1, 3 }, { 18, 115, 2, -1, 1 }, { 18, 118, 13, 62, 65 },
	{ 18, 131, 4, -1, 1 }, { 18, 136, 8, 179, 121 }, { 18, 144, 2, -1, 1 }, { 18, 149, 8, 179, 110 },
	{ 18, 157, 3, -1, 1 }, { 19, 94, 13, 180, 73 }, { 19, 107, 1, -1, 9 }, { 19, 108, 3, -1, 15 },
	{ 19, 111, 1, -1, 11 }, { 19, 112, 1, -1, 5 }, { 19, 113, 1, -1, 6 }, { 19, 114, 1, -1, 3 },
	{ 19, 115, 2, -1, 1 }, { 19, 119, 11, 63, 66 }, { 19, 130, 4, -1, 1 }, { 19, 136, 8, 180, 121 },
	{ 19, 144, 3, -1, 1 }, { 19, 149, 8, 180, 110 }, { 19, 157, 3, -1, 1 }, { 20, 96, 12, 181, 75 },
	{ 20, 108, 1, -1, 9 }, { 20, 109, 2, -1, 15 }, { 20, 111, 1, -1, 8 }, { 20, 112, 1, -1, 5 },
	{ 20, 113, 1, -1, 6 }, { 20, 114, 1, -1, 3 }, { 20, 115, 2, -1, 1 }, { 20, 120, 8, 64, 67 },
	{ 20, 128, 5, -1, 1 }, { 20, 137, 6, 181, 122 }, { 20, 143, 3, -1, 1 }, { 20, 150, 6, 181, 111 },
	{ 20, 156, 3, -1, 1 }, { 21, 97, 1, -1, 1 }, { 21, 98, 8, 182, 77 }, { 21, 106, 1, -1, 4 },
	{ 21, 107, 1, -1, 6 }, { 21, 108, 1, -1, 5 }, { 21, 109, 1, -1, 9 }, { 21, 110, 2, -1, 8 },
	{ 21, 112, 1, -1, 5 }, { 21, 113, 1, -1, 6 }, { 21, 114, 1, -1, 3 }, { 21, 115, 2, -1, 1 },
	{ 21, 121, 11, -1, 1 }, { 21, 138, 4, 182, 123 }, { 21, 142, 4, -1, 1 }, { 21, 151, 4, 182, 112 },
	{ 21, 155, 4, -1, 1 }, { 22, 98, 8, -1, 1 }, { 22, 106, 1, -1, 4 }, { 22, 107, 1, -1, 6 },
	{ 22, 108, 5, -1, 5 }, { 22, 113, 1, -1, 6 }, { 22, 114, 1, -1, 3 }, { 22, 115, 2, -1, 1 },
	{ 22, 123, 7, -1, 1 }, { 22, 139, 6, -1, 1 }, { 22, 152, 6, -1, 1 }, { 23, 100, 6, -1, 1 },
	{ 23, 106, 1, -1, 3 }, { 23, 107, 7, -1, 6 }, { 23, 114, 1, -1, 3 }, { 23, 115, 2, -1, 1 },
	{ 23, 140, 4, -1, 1 }, { 23, 153, 4, -1, 1 }, { 24, 106, 9, -1, 3 }, { 24, 115, 2, -1, 1 },
	{ 25, 107, 10, -1, 1 }, { 26, 108, 9, -1, 1 },
};

uint32_t g_table = 0;		/* the 7-entry picture table (enhancement memory) */
bool g_have_picture = false;	/* QUIT_RUNS applied to the right page this time */

bool stock_code_present()
{
	for( const auto &c : STOCK )
		if( rd32( c[0] ) != c[1] )
			return false;
	return true;
}

void install()
{
	if( g_table == 0 )
		g_table = psx_mod_alloc_guest_memory( 8 * (ENTRIES + 1), 4 );
	if( g_table == 0 )
		return;
	psx_mod_write_code_word( INIT_CALL, jal(QM_INIT) );
	psx_mod_write_code_word( DRAW_Y, 0x2416FF60 );				/* addiu s6, zero, -0xA0 */
	psx_mod_write_code_word( DRAW_TABLE_HI, 0x3C020000 | hi16(g_table) );	/* lui v0, hi */
	psx_mod_write_code_word( DRAW_TABLE_LO, 0x24540000 | lo16(g_table) );	/* addiu s4, v0, lo */
	psx_mod_write_code_word( DRAW_COUNT, 0x2AA20000 | (ENTRIES + 1) );	/* slti v0, s5, 7 */
	const uint32_t step[2][2] = { { STEP_UP, 0xFFFF }, { STEP_DOWN, 1 } };
	for( const auto &s : step )
	{
		psx_mod_write_code_word( s[0], jal(QM_STEP) );
		psx_mod_write_code_word( s[0] + 4, 0x24040000 | s[1] );	/* addiu a0, zero, dir */
		psx_mod_write_code_word( s[0] + 8, j(STEP_TAIL) );
		psx_mod_write_code_word( s[0] + 12, 0x2404031D );		/* addiu a0, zero, 0x31D */
	}
	psx_mod_write_code_word( DECIDE_CALL, jal(QM_DECIDE) );
	psx_mod_counter_add( "dsf.quitmenu.installed", 1 );
}

void on_vblank()
{
	if( stock_code_present() )
		install();
}

/* ---- QM_INIT: the QUIT picture, after the menu's own VRAM work ---- */
void on_init( CPUState *cpu, uint32_t )
{
	/* The table is filled here, not at install: the executable's data is
	 * still arriving from the disc when its code first matches. */
	for( int i = 0; i < 2 * ENTRIES; ++i )
		wr32( g_table + 4 * i, rd32(PICTURES + 4 * i) );
	wr32( g_table + 8 * QUIT, uint32_t(QUIT_V) << 16 );			/* u 0, v 186 */
	static uint16_t page[SOURCE_ROWS][64];
	gr_vram_transfer_out( 960, 0, 64, SOURCE_ROWS, &page[0][0] );
	uint32_t h = 0x811C9DC5u;
	for( int y = 0; y < SOURCE_ROWS; ++y )
		for( int x = 0; x < 64; ++x )
			for( int b = 0; b < 2; ++b )
				h = (h ^ ((page[y][x] >> (8 * b)) & 0xFF)) * 0x01000193u;
	g_have_picture = h == QUIT_SOURCE_HASH;
	if( !g_have_picture )
	{
		wr32( g_table + 8 * QUIT + 4, uint32_t(QUIT_W) );	/* h 0: nothing drawn */
		psx_mod_counter_add( "dsf.quitmenu.other_page", 1 );
		return;
	}
	wr32( g_table + 8 * QUIT + 4, uint32_t(QUIT_H) << 16 | QUIT_W );
	auto texel = [&]( int v, int u ) { return (page[v][u >> 2] >> (4 * (u & 3))) & 15; };
	static uint8_t pic[QUIT_H][QUIT_W];
	std::memset( pic, 0, sizeof(pic) );
	for( const Run &r : QUIT_RUNS )
		for( int i = 0; i < r.n; ++i )
			pic[r.y][r.x + i] = uint8_t( r.src_v < 0? r.src_u: texel(r.src_v, r.src_u + i) );
	/* Staged below the guest's stack pointer: nothing runs between this
	 * write and the upload. */
	const uint32_t bytes = QUIT_W / 2 * QUIT_H;
	const uint32_t buf = (cpu->gpr[29] - 0x400 - bytes) & ~3u;
	static uint16_t words[QUIT_H][QUIT_W / 4];
	for( int y = 0; y < QUIT_H; ++y )
		for( int x = 0; x < QUIT_W / 4; ++x )
			words[y][x] = uint16_t( pic[y][4*x] | pic[y][4*x + 1] << 4 | pic[y][4*x + 2] << 8 | pic[y][4*x + 3] << 12 );
	psx_mod_host_write_ram( buf, words, bytes );
	psx_mod_psyq_load_image( 960, QUIT_V, QUIT_W / 4, QUIT_H, buf );
	wr32( MENU_MASK, rd32(MENU_MASK) | 1u << QUIT );
	psx_mod_counter_add( "dsf.quitmenu.picture", 1 );
}

/* ---- QM_STEP(dir): the cursor to the next entry that can be chosen ---- */
void on_step( CPUState *cpu, uint32_t )
{
	const int dir = int32_t( cpu->gpr[4] ) < 0? ENTRIES: 1;	/* -1 == +6 modulo 7 */
	uint32_t mask = rd32( MENU_MASK );
	if( g_have_picture )
		mask |= 1u << QUIT;
	else
		mask &= ~(1u << QUIT);
	int c = rd8( MENU_CURSOR ) % (ENTRIES + 1);
	for( int i = 0; i <= ENTRIES; ++i )
	{
		c = (c + dir) % (ENTRIES + 1);
		if( mask >> c & 1 )
			break;
	}
	wr8( MENU_CURSOR, uint8_t(c) );
}

/* ---- QM_DECIDE(sound): the decide sound; on QUIT, leave ---- */
void on_decide( CPUState *cpu, uint32_t )
{
	psx_mod_counter_add( "dsf.quitmenu.decide", 1 );
	psx_mod_call_guest( cpu, SOUND, cpu->gpr[31], cpu->gpr[4], cpu->gpr[5], cpu->gpr[6], cpu->gpr[7] );
	if( g_have_picture && rd8(MENU_CURSOR) == QUIT )
	{
		psx_mod_counter_add( "dsf.quitmenu.quit", 1 );
		dsfcore_host_quit();
	}
}

} // namespace

PSX_MOD_CONSTRUCTOR( dsf_register_quitmenu )
{
	const char *id = "dsf.stepmania.quit";
	(void) psx_mod_register_vblank_plugin( id, on_vblank );
	(void) psx_mod_register_guest_function_plugin( id, QM_INIT, on_init );
	(void) psx_mod_register_guest_function_plugin( id, QM_STEP, on_step );
	(void) psx_mod_register_guest_function_plugin( id, QM_DECIDE, on_decide );
}
