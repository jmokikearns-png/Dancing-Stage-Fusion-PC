/* dsfcore_hooks.cpp -- StepMania inside the game's own code.
 *
 * Runtime function hooks (mod_plugins.h) that replace parts of Dancing Stage
 * Fusion's code paths with calls into the host StepMania.  Each hook needs the
 * function listed in game.toml [recompiler] mod_function_entry_funcs (the
 * recompiler then emits the hook call at the function's entry) and the
 * dsf.stepmania mod package (mods/), which enables it.
 *
 * Pads (dsf.stepmania.pads)
 *   0x800243FC is the game's once-per-frame pad read.  It decodes the two
 *   LIBPAD receive buffers the game registered with PadInitDirect
 *   (0x800B4DB8 player 1, 0x800B4DEC player 2; at 0x80025924):
 *     buf[0] == 0x00 (ok), buf[1] >> 4 == 4 (digital) or 7 (analog),
 *     buttons = ~(buf[2] << 8 | buf[3]), then pressed/released edges.
 *   On entry we ask StepMania for both players' buttons (its InputMapper,
 *   "ps1pad" mappings) and write them into those buffers as a digital pad
 *   reply, so the game's own decode runs on StepMania's input instead of
 *   on what LIBPAD received over the emulated controller port.
 *
 * Autoplay (DSF_AUTOPLAY=1 in the environment, for demo videos)
 *   While a song plays (state 0x800BA310 == 4), player 1's arrows come from
 *   the game's own step list instead: 16-byte entries at 0x800BCA44 {u32
 *   position, s16 pattern (negative = control, -2 = end), s16, s16 time in
 *   1/75 s, ...}, against the song clock at 0x800BB4AC (1/75 s).  A pattern
 *   is a row of the chart's pattern table at 0x800BC894 (built with the step
 *   list by 0x8008082C, ended by a row of 5s): 8 bytes, one per panel slot
 *   (0 Left, 1 Down, 2 Up, 3 Right, 4-7 the same on the double side), so
 *   rows 0-7 are single arrows and jumps get 8, 9, ... as they first
 *   appear.  The table is reused after the song, so it is copied when play
 *   starts.  Single only: slots 4-7 are not pressed.
 *   Each step is pressed for one frame, on the frame whose clock is nearest
 *   its time + DSF_AUTOPLAY_OFFSET ticks (the clock moves 1.5 ticks a
 *   frame).  The default -3 is what judges every step PERFECT: the game
 *   judges a press against the clock a little later than the frame it was
 *   read on.
 */
#include "dsfcore_host.h"
#include "mod_plugins.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

const uint32_t GAME_PAD_READ = 0x800243FCu;
const uint32_t PAD_RECV_BUFFER[2] = { 0x800B4DB8u, 0x800B4DECu };

bool g_bLoggedPads = false;

/* ---- autoplay ---- */
const uint32_t GAME_STATE = 0x800BA310, SONG_CLOCK = 0x800BB4AC, STEP_LIST = 0x800BCA44,
	PATTERNS = 0x800BC894;
const int STEP_SIZE = 16, MAX_STEPS = 4096, MAX_PATTERNS = 64;
const uint16_t PANEL_BIT[4] = { 1u << 7, 1u << 6, 1u << 4, 1u << 5 };	/* Left, Down, Up, Right */
int g_autoplay = -1;		/* -1: not read yet */
double g_autoplay_offset = -3.0;
bool g_playing = false;
int g_next_step = 0;
int32_t g_last_clock = 0;
uint16_t g_pattern_bits[MAX_PATTERNS];	/* pattern -> player 1 pad bits to clear */
int g_patterns = 0;

void read_patterns()
{
	for( g_patterns = 0; g_patterns < MAX_PATTERNS; ++g_patterns )
	{
		const uint32_t row = PATTERNS + 8 * uint32_t(g_patterns);
		if( psx_mod_read_byte(row) == 5 )
			break;
		uint16_t bits = 0;
		for( int k = 0; k < 4; ++k )
			if( psx_mod_read_byte(row + uint32_t(k)) )
				bits |= PANEL_BIT[k];
		g_pattern_bits[g_patterns] = bits;
	}
}

bool autoplay_pad( uint16_t &pad )
{
	if( g_autoplay < 0 )
	{
		const char *e = std::getenv( "DSF_AUTOPLAY" );
		g_autoplay = e && *e && *e != '0';
		if( const char *o = std::getenv("DSF_AUTOPLAY_OFFSET") )
			g_autoplay_offset = std::atof( o );
	}
	if( !g_autoplay || int16_t(psx_mod_read_half(GAME_STATE)) != 4 )
	{
		g_playing = false;
		return false;
	}
	const int32_t clock = int32_t( psx_mod_read_word(SONG_CLOCK) );
	if( !g_playing || clock < g_last_clock )
	{
		g_playing = true;
		g_next_step = 0;
		read_patterns();
	}
	g_last_clock = clock;
	pad = 0xFFFF;
	for( ; g_next_step < MAX_STEPS; ++g_next_step )
	{
		const uint32_t e = STEP_LIST + STEP_SIZE * uint32_t(g_next_step);
		const int16_t pattern = int16_t( psx_mod_read_half(e + 4) );
		if( pattern == -2 )
			break;
		if( pattern < 0 || pattern >= g_patterns )
			continue;
		const double t = int16_t( psx_mod_read_half(e + 8) ) + g_autoplay_offset;
		if( t > clock + 0.75 )	/* nearer the next frame's clock (+1.5) */
			break;
		pad &= uint16_t( ~g_pattern_bits[pattern] );
	}
	return true;
}

void on_game_pad_read( struct CPUState *, uint32_t )
{
	uint16_t pads[2] = { 0xFFFF, 0xFFFF };	/* active-low: nothing pressed */
	const bool host = dsfcore_host_read_pads( pads ) != 0;
	const bool autoplay = autoplay_pad( pads[0] );
	if( !host && !autoplay )
		return;
	if( host && !g_bLoggedPads )
	{
		g_bLoggedPads = true;
		std::fprintf( stdout, "[dsf.stepmania] pad read hook active: the game reads StepMania's input\n" );
	}
	for( int i = 0; i < 2; ++i )
	{
		/* A digital pad's reply: status ok, ID 0x41, button bytes as sent. */
		const uint8_t reply[4] = { 0x00, 0x41, uint8_t(pads[i] & 0xFF), uint8_t(pads[i] >> 8) };
		psx_mod_host_write_ram( PAD_RECV_BUFFER[i], reply, sizeof(reply) );
	}
}

} // namespace

PSX_MOD_CONSTRUCTOR( dsf_register_stepmania_hooks )
{
	(void) psx_mod_register_function_entry_plugin( "dsf.stepmania.pads", GAME_PAD_READ, on_game_pad_read );
}
