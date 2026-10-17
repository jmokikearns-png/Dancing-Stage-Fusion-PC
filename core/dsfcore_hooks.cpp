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
 */
#include "mod_plugins.h"

#include <cstdint>
#include <cstdio>

extern "C" void dsfcore_host_read_pads( uint16_t pads_out[2] );	/* dsfcore_embed.cpp */

namespace {

const uint32_t GAME_PAD_READ = 0x800243FCu;
const uint32_t PAD_RECV_BUFFER[2] = { 0x800B4DB8u, 0x800B4DECu };

bool g_bLoggedPads = false;

void on_game_pad_read( struct CPUState *, uint32_t )
{
	if( !g_bLoggedPads )
	{
		g_bLoggedPads = true;
		std::fprintf( stdout, "[dsf.stepmania] pad read hook active: the game reads StepMania's input\n" );
	}
	uint16_t pads[2] = { 0xFFFF, 0xFFFF };	/* active-low: nothing pressed */
	dsfcore_host_read_pads( pads );
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
