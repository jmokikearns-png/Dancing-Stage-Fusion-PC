/* dsfcore_smooth.cpp -- SMOOTH ARROWS: the step field at the PAL frame rate.
 *
 * The game draws a frame every second VBlank (frame divisor 0x800BA309 = 2,
 * the main loop's VSync(2) at 0x800263B8), so the arrows move 24.88 times a
 * second.  With SMOOTH ARROWS each song frame gets one render pass
 * (docs/RENDER_PASSES.md; this host presents passes itself,
 * render_pass_embed.h).  The loop builds a frame's ordering table, waits
 * in that VSync, shows the frame before (PutDispEnv) and only then has the
 * GPU draw the new one (DrawOTagEnv 0x8003A848, with DRAWENV
 * 0x800B4CD0 + 0x5C db, db = [0x800B4DB0]).  At the VSync's entry the pass
 * replays the loop from RAM -- its top through DrawSync, 0x80026294 ..
 * 0x800263B8, then PutDispEnv and DrawOTagEnv, 0x800263C0 .. 0x80026430
 * (the pass drops the display change) -- with the song clock one frame on
 * and each player's scroll position halfway: the next frame, drawn into the
 * buffer this one is about to be drawn in.  It is shown on the VBlank after
 * this frame's own.  The pass is sandboxed and rolled back, so the game
 * itself runs as it always does: judgements, life, the dancer and every
 * effect keep their timing (the in-between shows them a frame on).
 *
 * The clock.  The system update after the pad read (0x8002BC5C ->
 * 0x8002C928) adds the VBlanks since its last call to the song counters
 * 0x800B5C10 / 0x800B5C14; the frame then turns the counter into the clock
 * (0x8002BE7C, 1/75 s ticks) and the clock into each player's scroll
 * position (stored at 0x800488D4, `sw v0, 4(s0)`, s0 = the player's record
 * at 0x800BB4AC + ...; position by the tempo table).  A pass adds the last
 * frame's advance to both counters.  The clock has whole ticks, three a
 * frame, so a half-frame clock would scroll by one and two ticks in turn;
 * instead the pass works out the next frame's position and stores the point
 * `alpha` of the way to it from the shown frame's (an instruction plugin,
 * game.toml mod_instruction_sites).
 *
 * The pads.  The last pad read's pressed edges ([0x80092A28] + 0x54 + 0x10p)
 * were judged by the shown frame; the pass clears them (a held panel stays
 * held), as the next read would without a new press.
 *
 * The rect: the clip rect of that DRAWENV.  The display shows a buffer's new
 * frame not at the flip right after this VSync (that one still shows the
 * buffer's frame from two loops ago) but at the next flip to it, so the
 * images wait 2 * period VBlanks (shown_after_vblanks).
 *
 * GRAPHIC OPTIONS > DISPLAY SETTINGS > SMOOTH ARROWS (DSFCORE_SETTING_SMOOTH_ARROWS,
 * default ON; DSF_SMOOTH=0 without a host).
 */
#include "dsfcore_host.h"
#include "mod_plugins.h"
#include "cpu_state.h"
#include "psx_cycle_freeze.h"
#include "render_pass.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

const char *const PLUGIN = "dsf.stepmania.smooth";

const uint32_t VSYNC = 0x80037ED8u, LOOP_VSYNC_RETURN = 0x800263C0u;
const uint32_t LOOP_TOP = 0x80026294u, LOOP_VSYNC_CALL = 0x800263B8u;
const uint32_t LOOP_DRAW_END = 0x80026430u;	/* after DrawOTagEnv returns */
const uint32_t SITE_SCROLL = 0x800488D4u, SCROLL_STOCK = 0xAE020004u;	/* sw v0, 4(s0) */

const uint32_t GAME_STATE = 0x800BA310u;
const int16_t STATE_PLAYING = 4;
const uint32_t SONG_COUNTER = 0x800B5C10u, SONG_COUNTER2 = 0x800B5C14u;
const uint32_t PADS_PTR = 0x80092A28u, PAD_PRESSED = 0x54u, PAD_STRIDE = 0x10u;
const uint32_t DRAWENV = 0x800B4CD0u, DRAWENV_SIZE = 0x5Cu, DB_INDEX = 0x800B4DB0u;

const int32_t MAX_STEP = 8 * 15072;	/* counter advance per frame beyond which it is no frame of a song */

bool g_enabled = true;
unsigned g_song_passes = 0, g_song_refused = 0;
int32_t g_last_counter = 0, g_last_counter2 = 0;
int32_t g_step = 0, g_step2 = 0;
bool g_have_last = false;
uint32_t g_pass_alpha = 0;		/* Q16 while a pass replays the frame */

bool smooth_on()
{
	DsfCoreSetting s;
	if( dsfcore_host_get_setting(DSFCORE_SETTING_SMOOTH_ARROWS, &s) )
		return s.choice == 1;
	return g_enabled;
}

/* A song's passes, at its end. */
void song_summary()
{
	if( !g_song_passes && !g_song_refused )
		return;
	RenderPassStats st;
	render_pass_get_stats( &st );
	std::fprintf( stdout, "[dsf.smooth] song: %u in-between frames, %u refused (status %u), %.2f ms each\n",
		g_song_passes, g_song_refused, psx_mod_render_pass_status(), st.avg_pass_ms );
	std::fflush( stdout );
	g_song_passes = g_song_refused = 0;
}

int draw_next_frame( CPUState *cpu, void *, uint32_t alpha_q16 )
{
	psx_mod_write_word( SONG_COUNTER, uint32_t(int32_t(psx_mod_read_word(SONG_COUNTER)) + g_step) );
	psx_mod_write_word( SONG_COUNTER2, uint32_t(int32_t(psx_mod_read_word(SONG_COUNTER2)) + g_step2) );
	const uint32_t pads = psx_mod_read_word( PADS_PTR );
	for( uint32_t p = 0; p < 2; ++p )
		psx_mod_write_word( pads + PAD_PRESSED + PAD_STRIDE * p, 0 );
	g_pass_alpha = alpha_q16;
	const int ok = psx_mod_run_guest_span( cpu, LOOP_TOP, LOOP_VSYNC_CALL ) &&
		psx_mod_run_guest_span( cpu, LOOP_VSYNC_RETURN, LOOP_DRAW_END );
	g_pass_alpha = 0;
	return ok;
}

void on_vsync( CPUState *cpu, uint32_t )
{
	if( cpu->gpr[31] != LOOP_VSYNC_RETURN || g_psx_render_pass_active )
		return;
	const bool playing = int16_t( psx_mod_read_half(GAME_STATE) ) == STATE_PLAYING;
	const int32_t counter = int32_t( psx_mod_read_word(SONG_COUNTER) );
	const int32_t counter2 = int32_t( psx_mod_read_word(SONG_COUNTER2) );
	if( !playing )
	{
		if( g_have_last )
			song_summary();
		g_have_last = false;
		return;
	}
	if( g_have_last )
	{
		g_step = counter - g_last_counter;
		g_step2 = counter2 - g_last_counter2;
	}
	const bool steady = g_have_last && g_step > 0 && g_step <= MAX_STEP;
	g_last_counter = counter;
	g_last_counter2 = counter2;
	g_have_last = true;
	const uint32_t period = cpu->gpr[4];	/* VSync(n): the VBlanks this frame stays */
	if( !steady || period < 2 || period > 4 || !smooth_on() )
		return;

	const uint32_t env = DRAWENV + DRAWENV_SIZE * (psx_mod_read_word(DB_INDEX) & 1u);
	PSXModRenderPassFrame frame = {};
	frame.struct_size = sizeof frame;
	frame.period_vblanks = period;
	frame.shown_after_vblanks = 2 * period;	/* the flip after next to the rect (see the top) */
	frame.x = psx_mod_read_half( env + 0 );
	frame.y = psx_mod_read_half( env + 2 );
	frame.w = psx_mod_read_half( env + 4 );
	frame.h = psx_mod_read_half( env + 6 );
	const uint32_t kept = psx_mod_render_pass_frame( cpu, &frame, draw_next_frame, nullptr );
	if( kept )
		g_song_passes += kept;
	else
		g_song_refused++;
	psx_mod_counter_add( kept ? "dsf.smooth.passes" : "dsf.smooth.refused", kept ? kept : 1 );
}

/* sw v0, 4(s0): the player's scroll position, `alpha` of the way from the
 * shown frame's to the next one's while a pass replays the frame. */
void on_scroll( CPUState *cpu, uint32_t )
{
	if( !g_pass_alpha )
		return;
	const int32_t shown = int32_t( psx_mod_read_word(cpu->gpr[16] + 4) );
	const int32_t next = int32_t( cpu->gpr[2] );
	cpu->gpr[2] = uint32_t( shown + int32_t((int64_t(next - shown) * g_pass_alpha) >> 16) );
}

} // namespace

PSX_MOD_CONSTRUCTOR( dsf_register_smooth )
{
	if( const char *e = std::getenv("DSF_SMOOTH") )
		g_enabled = e[0] != '0';
	(void) psx_mod_register_function_entry_plugin( PLUGIN, VSYNC, on_vsync );
	(void) psx_mod_register_instruction_plugin( PLUGIN, SITE_SCROLL, SCROLL_STOCK, on_scroll );
}
