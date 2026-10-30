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
 *   starts.  Slots 0-3 are pressed on player 1's pad, 4-7 (double play's
 *   right-hand mat) on player 2's; player 2's pad is only taken over when
 *   the chart uses those slots.
 *   Each step is pressed for one frame, on the frame whose clock is nearest
 *   its time + DSF_AUTOPLAY_OFFSET ticks (the clock moves 1.5 ticks a
 *   frame).  The default -4 is the middle of the PERFECT window (about
 *   -7.25 to -0.7 ticks; see dsfcore_timing.cpp).
 *   For tests that need a player's sloppiness: DSF_AUTOPLAY_JITTER=j moves
 *   each step by a fixed pseudo-random -j .. +j ticks, DSF_AUTOPLAY_HOLD=n
 *   keeps each panel held for n pad reads.
 */
#include "dsfcore_host.h"
#include "dsfcore_timing.h"
#include "mod_plugins.h"
#include "psx_cycle_freeze.h"

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
double g_autoplay_offset = -4.0;
double g_autoplay_jitter = 0.0;
int g_autoplay_hold = 1;
uint8_t g_held[2][16];			/* pad reads each bit stays pressed */
bool g_autoplay_raw = false;	/* DSF_AUTOPLAY_RAW=1: ignore TIMING, like a player with a fixed delay (tests) */
bool g_playing = false;
int g_next_step = 0;
int32_t g_last_clock = 0;
uint16_t g_pattern_bits[MAX_PATTERNS][2];	/* pattern -> each pad's bits to clear */
int g_patterns = 0;
bool g_double = false;			/* the chart has slots 4-7 */

void read_patterns()
{
	g_double = false;
	for( g_patterns = 0; g_patterns < MAX_PATTERNS; ++g_patterns )
	{
		const uint32_t row = PATTERNS + 8 * uint32_t(g_patterns);
		if( psx_mod_read_byte(row) == 5 )
			break;
		for( int pad = 0; pad < 2; ++pad )
		{
			uint16_t bits = 0;
			for( int k = 0; k < 4; ++k )
				if( psx_mod_read_byte(row + uint32_t(4 * pad + k)) )
					bits |= PANEL_BIT[k];
			g_pattern_bits[g_patterns][pad] = bits;
		}
	}
}

bool autoplay_pads( uint16_t pads[2] )
{
	if( g_autoplay < 0 )
	{
		const char *e = std::getenv( "DSF_AUTOPLAY" );
		g_autoplay = e && *e && *e != '0';
		if( const char *o = std::getenv("DSF_AUTOPLAY_OFFSET") )
			g_autoplay_offset = std::atof( o );
		if( const char *j = std::getenv("DSF_AUTOPLAY_JITTER") )
			g_autoplay_jitter = std::atof( j );
		if( const char *h = std::getenv("DSF_AUTOPLAY_HOLD") )
			g_autoplay_hold = std::atoi( h ) > 0 ? std::atoi( h ) : 1;
		const char *r = std::getenv( "DSF_AUTOPLAY_RAW" );
		g_autoplay_raw = r && *r && *r != '0';
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
	pads[0] = 0xFFFF;
	if( g_double )
		pads[1] = 0xFFFF;
	for( int pad = 0; pad < 2; ++pad )
		for( int b = 0; b < 16; ++b )
			if( g_held[pad][b] )
			{
				--g_held[pad][b];
				pads[pad] &= uint16_t( ~(1u << b) );
			}
	for( ; g_next_step < MAX_STEPS; ++g_next_step )
	{
		const uint32_t e = STEP_LIST + STEP_SIZE * uint32_t(g_next_step);
		const int16_t pattern = int16_t( psx_mod_read_half(e + 4) );
		if( pattern == -2 )
			break;
		if( pattern < 0 || pattern >= g_patterns )
			continue;
		double t = int16_t( psx_mod_read_half(e + 8) ) + g_autoplay_offset;
		if( g_autoplay_jitter > 0 )
		{
			const uint32_t hsh = uint32_t(g_next_step) * 2654435761u;
			t += g_autoplay_jitter * ( double(hsh >> 16) / 32768.0 - 1.0 );
		}
		if( g_autoplay_raw )	/* the step's time without the game's TIMING (system data + 0x52) */
			t -= int8_t( psx_mod_read_byte(psx_mod_read_word(0x800B1AFCu) + 0x52u) );
		if( t > clock + 0.75 )	/* nearer the next frame's clock (+1.5) */
			break;
		pads[0] &= uint16_t( ~g_pattern_bits[pattern][0] );
		if( g_pattern_bits[pattern][1] && !g_double )
		{
			g_double = true;	/* from now on player 2's pad is ours too */
			pads[1] = 0xFFFF;
		}
		pads[1] &= uint16_t( ~g_pattern_bits[pattern][1] );
		for( int pad = 0; pad < 2; ++pad )
			for( int b = 0; b < 16; ++b )
				if( g_pattern_bits[pattern][pad] & (1u << b) )
					g_held[pad][b] = uint8_t( g_autoplay_hold - 1 );
	}
	return true;
}

uint16_t g_last_pads[2] = { 0xFFFF, 0xFFFF };
bool g_have_last_pads = false;

void write_pads( const uint16_t pads[2] )
{
	for( int i = 0; i < 2; ++i )
	{
		/* A digital pad's reply: status ok, ID 0x41, button bytes as sent. */
		const uint8_t reply[4] = { 0x00, 0x41, uint8_t(pads[i] & 0xFF), uint8_t(pads[i] >> 8) };
		psx_mod_host_write_ram( PAD_RECV_BUFFER[i], reply, sizeof(reply) );
	}
}

void on_game_pad_read( struct CPUState *, uint32_t )
{
	/* A render pass (SMOOTH ARROWS) replays the loop, this read with it: it
	 * gets the last real read's pads, so it takes no press from the host's
	 * latches (the pass is thrown away) and AUTO TIMING counts nothing. */
	if( g_psx_render_pass_active )
	{
		if( g_have_last_pads )
			write_pads( g_last_pads );
		return;
	}
	dsf_auto_timing_frame();	/* before this read: the last read's presses */
	uint16_t pads[2] = { 0xFFFF, 0xFFFF };	/* active-low: nothing pressed */
	const bool host = dsfcore_host_read_pads( pads ) != 0;
	const bool autoplay = autoplay_pads( pads );
	if( !host && !autoplay )
		return;
	if( host && !g_bLoggedPads )
	{
		g_bLoggedPads = true;
		std::fprintf( stdout, "[dsf.stepmania] pad read hook active: the game reads StepMania's input\n" );
	}
	write_pads( pads );
	g_last_pads[0] = pads[0];
	g_last_pads[1] = pads[1];
	g_have_last_pads = true;
}

} // namespace

PSX_MOD_CONSTRUCTOR( dsf_register_stepmania_hooks )
{
	(void) psx_mod_register_function_entry_plugin( "dsf.stepmania.pads", GAME_PAD_READ, on_game_pad_read );
}
