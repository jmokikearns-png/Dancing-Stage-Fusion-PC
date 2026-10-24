/* dsfcore_timing.cpp -- AUTO TIMING: keep the game's TIMING right on this PC.
 *
 * The game's TIMING (SOUND OPTIONS; system data [0x800B1AFC] + 0x52, signed,
 * -8..8) is added to every step's time when the step list is built
 * (0x8007FC90): one unit is one tick of the song clock, 1/75 s.  On a PS1 the
 * sound and picture reach the player at once; on a PC the sound comes out of
 * the sound device's queue later, and the picture after the screen's own
 * delay.  AUTO TIMING (a PC row of SOUND OPTIONS) does two things:
 *
 *  - Turned ON, it asks the host for the PC's delays (DsfCoreHost.
 *    measure_latency) and sets PICTURE SYNC ON (each picture shown with its
 *    sound), PICTURE DELAY to minus the picture's delay (so the picture is
 *    on screen when its sound is heard), and TIMING to the sound's delay in
 *    ticks (the player hears and sees each step that much late).
 *  - While a song plays it compares each step the player presses with the
 *    step's time and, when the song ends and AUTO TIMING is on, moves TIMING
 *    by the average error (middle half of at least 24 presses, at most 3 a
 *    song).  That covers what no scan sees: a TV's or headphones' delay, and
 *    the player.
 *
 * A press is read from the game's own pad state ([0x80092A28] + 0x54 + 0x10p:
 * buttons pressed at the last pad read) at the next pad read, against the
 * clock of that last read.  Autoplay at fixed offsets from the step times
 * finds the PERFECT window from about -7.25 to -0.7 ticks (all PERFECT from
 * -6 to -3, mostly GREAT at -7.5 and +1.5), middle -4; this measure reads
 * autoplay at -4 as +1.1 with a lead of 4 (a press shows up at a later read
 * than autoplay's offset alone says), so a press counts as dead on at clock
 * t - 3 and the error is clock - (t - 3).
 * Steps and their panels: the step list and pattern table, as autoplay reads
 * them (dsfcore_hooks.cpp); slots 4-7 are player 2's pad (double).
 */
#include "dsfcore_timing.h"
#include "dsfcore_host.h"
#include "mod_plugins.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

const uint32_t SYSTEM_DATA_PTR = 0x800B1AFCu, TIMING = 0x52u;
const int TIMING_MIN = -8, TIMING_MAX = 8;
const double MS_PER_TICK = 1000.0 / 75.0;

const uint32_t GAME_STATE = 0x800BA310u, SONG_CLOCK = 0x800BB4ACu;
const uint32_t STEP_LIST = 0x800BCA44u, PATTERNS = 0x800BC894u;
const int STEP_SIZE = 16, MAX_STEPS = 4096, MAX_PATTERNS = 64;
const uint32_t PADS_PTR = 0x80092A28u, PAD_PRESSED = 0x54u, PAD_STRIDE = 0x10u;
/* Game pad bits, by panel slot: Left, Down, Up, Right. */
const uint32_t PANEL_BIT[4] = { 0x8000u, 0x4000u, 0x1000u, 0x2000u };

const int PERFECT_LEAD = 3;		/* ticks before a step's time a press counts as dead on */
const int MATCH_WINDOW = 10;		/* ticks either side counted as aimed at that step */
const int MIN_PRESSES = 24, MAX_CHANGE = 3;
const int MAX_ERRORS = 2048;

int timing_now()
{
	return int8_t( psx_mod_read_byte(psx_mod_read_word(SYSTEM_DATA_PTR) + TIMING) );
}

void set_timing( int v )
{
	v = std::max( TIMING_MIN, std::min(TIMING_MAX, v) );
	psx_mod_write_byte( psx_mod_read_word(SYSTEM_DATA_PTR) + TIMING, uint8_t(int8_t(v)) );
}

bool auto_timing_on()
{
	DsfCoreSetting s;
	return dsfcore_host_get_setting( DSFCORE_SETTING_AUTO_TIMING, &s ) && s.choice == 1;
}

/* ---- learning ---- */

bool g_playing = false;
int32_t g_last_clock = 0;
int g_first = 0;			/* first step still in reach */
uint8_t g_slots[MAX_PATTERNS];		/* pattern -> slot bits 0-7 */
int g_patterns = 0;
uint8_t g_used[MAX_STEPS];		/* slots of each step already matched */
int g_errors[MAX_ERRORS];
int g_nerrors = 0;

void begin_song( int32_t clock )
{
	g_playing = true;
	g_last_clock = clock;
	g_first = 0;
	g_nerrors = 0;
	std::fill( g_used, g_used + MAX_STEPS, 0 );
	for( g_patterns = 0; g_patterns < MAX_PATTERNS; ++g_patterns )
	{
		const uint32_t row = PATTERNS + 8 * uint32_t(g_patterns);
		if( psx_mod_read_byte(row) == 5 )
			break;
		uint8_t bits = 0;
		for( int k = 0; k < 8; ++k )
			if( psx_mod_read_byte(row + uint32_t(k)) )
				bits |= uint8_t( 1u << k );
		g_slots[g_patterns] = bits;
	}
}

/* A press of panel slot `slot` read at `clock`: the nearest step on that
 * slot within the window, not matched yet. */
void press( int slot, int32_t clock )
{
	int best = -1, best_err = 0;
	for( int i = g_first; i < MAX_STEPS; ++i )
	{
		const uint32_t e = STEP_LIST + STEP_SIZE * uint32_t(i);
		const int16_t pattern = int16_t( psx_mod_read_half(e + 4) );
		if( pattern == -2 )
			break;
		const int t = int16_t( psx_mod_read_half(e + 8) );
		const int err = clock - (t - PERFECT_LEAD);
		if( err > MATCH_WINDOW )
		{
			if( i == g_first )
				++g_first;	/* gone by for good */
			continue;
		}
		if( err < -MATCH_WINDOW )
			break;
		if( pattern < 0 || pattern >= g_patterns || !(g_slots[pattern] & (1u << slot)) || (g_used[i] & (1u << slot)) )
			continue;
		if( best < 0 || std::abs(err) < std::abs(best_err) )
		{
			best = i;
			best_err = err;
		}
	}
	if( best < 0 )
		return;
	g_used[best] |= uint8_t( 1u << slot );
	if( g_nerrors < MAX_ERRORS )
		g_errors[g_nerrors++] = best_err;
}

void end_song()
{
	g_playing = false;
	psx_mod_counter_add( "dsf.timing.songs", 1 );
	if( g_nerrors < MIN_PRESSES )
		return;
	std::sort( g_errors, g_errors + g_nerrors );
	const int lo = g_nerrors / 4, hi = g_nerrors - g_nerrors / 4;
	double sum = 0;
	for( int i = lo; i < hi; ++i )
		sum += g_errors[i];
	const double mean = sum / double(hi - lo);
	const int was = timing_now();
	int change = int( std::lround(mean) );
	change = std::max( -MAX_CHANGE, std::min(MAX_CHANGE, change) );
	if( auto_timing_on() && std::fabs(mean) >= 0.75 && change != 0 )
	{
		set_timing( was + change );
		psx_mod_counter_add( "dsf.timing.adjusted", 1 );
	}
	std::fprintf( stdout, "[dsf.timing] %d presses, average %+.2f ticks (%+.0f ms): TIMING %+d -> %+d\n",
		g_nerrors, mean, mean * MS_PER_TICK, was, timing_now() );
	std::fflush( stdout );
}

} // namespace

bool dsf_auto_timing_scan( AutoTimingScan &out )
{
	DsfCoreLatency l;
	if( !dsfcore_host_measure_latency(&l) )
		return false;
	out.sound_ms = l.sound_ms;
	out.picture_ms = l.picture_ms;
	/* The picture with its sound, shown early by the screen's own delay. */
	dsfcore_host_set_setting( DSFCORE_SETTING_SYNC_PICTURE, 1 );
	const int delay = -int( std::lround(l.picture_ms / 5.0) ) * 5;
	DsfCoreSetting s;
	if( dsfcore_host_get_setting(DSFCORE_SETTING_PICTURE_OFFSET, &s) )
	{
		const int choice = std::max( 0, std::min(s.num_choices - 1, (delay + 150) / 5) );	/* -150 ms in 5 ms steps */
		dsfcore_host_set_setting( DSFCORE_SETTING_PICTURE_OFFSET, choice );
		out.picture_delay_ms = -150 + 5 * choice;
	}
	else
		out.picture_delay_ms = 0;
	/* Both reach the player the sound's delay late. */
	set_timing( int(std::lround(l.sound_ms / MS_PER_TICK)) );
	out.timing = timing_now();
	psx_mod_counter_add( "dsf.timing.scans", 1 );
	std::fprintf( stdout, "[dsf.timing] scan: sound %d ms, picture %d ms (%d Hz): PICTURE SYNC ON, PICTURE DELAY %+d ms, TIMING %+d\n",
		l.sound_ms, l.picture_ms, l.refresh_hz, out.picture_delay_ms, out.timing );
	std::fflush( stdout );
	return true;
}

void dsf_auto_timing_frame()
{
	const bool playing = int16_t( psx_mod_read_half(GAME_STATE) ) == 4;
	const int32_t clock = int32_t( psx_mod_read_word(SONG_CLOCK) );
	if( !playing )
	{
		if( g_playing )
			end_song();
		return;
	}
	if( !g_playing || clock < g_last_clock )
	{
		begin_song( clock );
		return;
	}
	const uint32_t pads = psx_mod_read_word( PADS_PTR );
	for( int pad = 0; pad < 2; ++pad )
	{
		const uint32_t pressed = psx_mod_read_word( pads + PAD_PRESSED + PAD_STRIDE * uint32_t(pad) );
		for( int k = 0; k < 4; ++k )
			if( pressed & PANEL_BIT[k] )
				press( 4 * pad + k, g_last_clock );
	}
	g_last_clock = clock;
}
