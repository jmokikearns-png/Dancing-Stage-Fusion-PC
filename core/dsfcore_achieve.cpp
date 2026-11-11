/* dsfcore_achieve.cpp -- what the player did, for the host's achievements.
 *
 * Two reports go to the host (dsfcore_host_play_result / _save_progress):
 *
 * Each song's result.  The game's state (halfword 0x800BA310) is 4 while a
 * song plays, 5 when it has ended, then 6 for the results screen (cleared)
 * or 0x14 for GAME OVER (failed).  Each player has a block of 0x1264 bytes
 * from 0x800BA310 (the results code at 0x80089880 reads them so):
 *   +0x1A  byte   the song's number (the music select's)
 *   +0x38  word   WORKOUT: the session's calories (1/1000 kcal); +0x3C the
 *                 same before this song
 *   +0x74  byte   the chart: 7 BEGINNER, 0/1 STANDARD, 2 DIFFICULT, 3 EXPERT
 *                 (as 0x8004A6BC maps it)
 *   +0x76  byte   1 single, 2 DOUBLE, 0 not playing (7: the game saves nothing)
 *   +0x7C  byte   bit 2: WORKOUT MODE
 *   +0x80  words  PERFECT, GREAT, GOOD, ALMOST, BOO
 *   +0x98  half   the chart's steps; +0x9C the max combo; +0xA0 byte the grade
 * Read a moment after the state leaves 5, when the results are drawn.
 *
 * The save.  [0x800B1AFC] is the save the memory card is loaded into: 32
 * song records from +0x90, 0x50 bytes each (+0x20 cleared plays, +0x24 the
 * song's id, +0x26+2d the max combo and +0x46+d the grade per chart d, +0x4F
 * a bit per chart for a full combo); +0xA90.. a bit per finished LESSON MODE section; +0xAC0 and +0xB00
 * WORKOUT MODE's running total of calories.  Sent when it changes.
 */
#include "dsfcore_host.h"
#include "mod_plugins.h"
#include "psx_cycle_freeze.h"

#include <cstdint>
#include <cstring>

uint32_t dsf_song_id_by_number( uint32_t number, bool *new_song );	/* dsfcore_extrasong.cpp */
bool dsf_is_new_song( uint32_t id );
int dsf_new_song_count();

namespace {

const uint32_t GAME_STATE = 0x800BA310u, PLAYER = 0x800BA310u, PLAYER_SIZE = 0x1264u;
const uint16_t ST_PLAYING = 4, ST_ENDED = 5, ST_RESULTS = 6;
const uint32_t SAVE_PTR = 0x800B1AFCu;
const int SAVE_RECORDS = 32;
const int RESULT_DELAY = 25;	/* frames after the state leaves ENDED */
const int SAVE_PERIOD = 50;	/* frames between looks at the save */

uint32_t rd32( uint32_t a ) { return psx_mod_read_word( a ); }
uint16_t rd16( uint32_t a ) { return psx_mod_read_half( a ); }
uint8_t rd8( uint32_t a ) { return psx_mod_read_byte( a ); }

int g_last_state = -1;
bool g_ended = false;		/* a song ended; its result is coming */
int g_report_in = 0;		/* frames until the result is read */
bool g_cleared = false;
int g_frame = 0;
DsfCoreProgress g_sent;
bool g_have_sent = false;

int chart_of( uint8_t code )
{
	switch( code )
	{
	case 7: return 0;
	case 2: return 2;
	case 3: return 3;
	default: return 1;
	}
}

void report_players()
{
	for( uint32_t p = 0; p < 2; ++p )
	{
		const uint32_t b = PLAYER + p * PLAYER_SIZE;
		const uint8_t style = rd8( b + 0x76 );
		if( style != 1 && style != 2 )
			continue;
		DsfCorePlay r;
		std::memset( &r, 0, sizeof(r) );
		r.struct_size = sizeof(r);
		r.player = int32_t( p );
		r.workout = (rd8(b + 0x7C) & 4) != 0;
		r.double_style = style == 2;
		r.chart = chart_of( rd8(b + 0x74) );
		bool new_song = false;
		r.song_id = int32_t( dsf_song_id_by_number(rd8(b + 0x1A), &new_song) );
		r.new_song = new_song;
		r.grade = rd8( b + 0xA0 );
		r.cleared = g_cleared && r.grade != 0;
		r.perfect = int32_t( rd32(b + 0x80) );
		r.great = int32_t( rd32(b + 0x84) );
		r.good = int32_t( rd32(b + 0x88) );
		r.almost = int32_t( rd32(b + 0x8C) );
		r.boo = int32_t( rd32(b + 0x90) );
		r.steps = rd16( b + 0x98 );
		r.max_combo = rd16( b + 0x9C );
		if( r.workout )
		{
			const uint32_t now = rd32( b + 0x38 ), before = rd32( b + 0x3C );
			r.kcal_milli = now >= before? int32_t( now - before ): 0;
		}
		dsfcore_host_play_result( &r );
	}
}

void look_at_save()
{
	const uint32_t save = rd32( SAVE_PTR );
	if( (save & 0xFFE00000u) != 0x80000000u )
		return;
	DsfCoreProgress pr;
	std::memset( &pr, 0, sizeof(pr) );
	pr.struct_size = sizeof(pr);
	for( int i = 0; i < SAVE_RECORDS; ++i )
	{
		const uint32_t rec = save + 0x90u + 0x50u * uint32_t(i);
		const uint32_t id = rd32( rec + 0x24 ) & 0xFFFF;
		if( id == 0 )
			continue;
		DsfCoreSaveSong &s = pr.song[i];
		s.song_id = int32_t( id );
		s.new_song = dsf_is_new_song( id );
		s.plays = int32_t( rd32(rec + 0x20) );
		for( int d = 0; d < 8; ++d )
		{
			s.grade[d] = rd8( rec + 0x46 + uint32_t(d) );
			const int32_t combo = rd16( rec + 0x26 + 2 * uint32_t(d) );
			if( combo > s.best_combo )
				s.best_combo = combo;
		}
		s.full_combo = rd8( rec + 0x4F );
	}
	pr.new_songs = dsf_new_song_count();
	pr.list_songs = 20 + pr.new_songs;
	pr.lesson_sections = uint32_t(rd8(save + 0xA90)) | uint32_t(rd8(save + 0xA91)) << 8 | uint32_t(rd8(save + 0xA92)) << 16;
	const uint32_t k1 = rd32( save + 0xAC0 ), k2 = rd32( save + 0xB00 );
	pr.workout_kcal_milli = int32_t( k1 > k2? k1: k2 );
	if( g_have_sent && std::memcmp(&pr, &g_sent, sizeof(pr)) == 0 )
		return;
	g_sent = pr;
	g_have_sent = true;
	dsfcore_host_save_progress( &pr );
}

void on_vblank()
{
	if( g_psx_render_pass_active )	/* SMOOTH ARROWS' in-between pictures: thrown away */
		return;
	++g_frame;
	const int state = rd16( GAME_STATE );
	if( state != g_last_state )
	{
		if( g_last_state == ST_PLAYING && state == ST_ENDED )
			g_ended = true;
		else if( g_ended && g_last_state == ST_ENDED )
		{
			g_ended = false;
			g_cleared = state == ST_RESULTS;
			g_report_in = RESULT_DELAY;
		}
		else
			g_ended = false;
		g_last_state = state;
	}
	if( g_report_in > 0 && --g_report_in == 0 )
		report_players();
	if( g_frame % SAVE_PERIOD == 0 )
		look_at_save();
}

} // namespace

PSX_MOD_CONSTRUCTOR( dsf_register_achievements )
{
	(void) psx_mod_register_vblank_plugin( "dsf.stepmania.achievements", on_vblank );
}
