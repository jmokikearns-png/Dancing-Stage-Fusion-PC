/* dsfcore_embed.cpp -- host side of psx_embed.h, exported as dsfcore_api.h.
 *
 * Compiled into the PSXRecomp runtime when it is built as dsfcore.dll
 * (PSX_EMBED).  Starts the runtime's main on a 64 MiB thread (the size the
 * runtime's executable links with) and forwards the runtime's embed hooks to
 * the host's callbacks. */
#include "dsfcore_api.h"
#include "dsfcore_host.h"
#include "psx_embed.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" int psx_embed_runtime_main( int argc, char **argv );

namespace {

DsfCoreHost g_host;
std::vector<std::string> g_args;
std::vector<char *> g_argv;
volatile LONG g_state = DSFCORE_STATE_NOT_STARTED;
volatile LONG g_exit_code = 0;
uint16_t g_frame_pads[2] = { 0xFFFF, 0xFFFF };	/* from the last frame_done */

DWORD WINAPI core_thread( void * )
{
	const int ret = psx_embed_runtime_main( int(g_argv.size()) - 1, g_argv.data() );
	std::fflush( stdout );
	std::fflush( stderr );
	InterlockedExchange( &g_exit_code, ret );
	InterlockedExchange( &g_state, DSFCORE_STATE_EXITED );
	return 0;
}

} // namespace

extern "C" {

void psx_embed_video( const uint32_t *argb, int width, int height, int pitch_px, int scale, int flags )
{
	if( g_host.video )
		g_host.video( g_host.user, argb, width, height, pitch_px, scale, flags );
}

void psx_embed_audio( const int16_t *stereo, int frames )
{
	if( g_host.audio && frames > 0 )
		g_host.audio( g_host.user, stereo, frames );
}

void psx_embed_frame_done( uint16_t pads_out[2] )
{
	if( g_host.frame_done )
		g_host.frame_done( g_host.user, pads_out );
	g_frame_pads[0] = pads_out[0];
	g_frame_pads[1] = pads_out[1];
}

/* dsfcore_host.h; core thread. */
int dsfcore_host_read_pads( uint16_t pads_out[2] )
{
	if( g_host.read_pads )
	{
		g_host.read_pads( g_host.user, pads_out );
		return 1;
	}
	pads_out[0] = g_frame_pads[0];
	pads_out[1] = g_frame_pads[1];
	return 1;
}

int dsfcore_host_get_setting( int id, DsfCoreSetting *out )
{
	return g_host.get_setting? g_host.get_setting( g_host.user, id, out ): 0;
}

void dsfcore_host_set_setting( int id, int choice )
{
	if( g_host.set_setting )
		g_host.set_setting( g_host.user, id, choice );
}

void dsfcore_host_commit_settings( void )
{
	if( g_host.commit_settings )
		g_host.commit_settings( g_host.user );
}

int dsfcore_host_get_mapping( int player, int button, DsfCoreMapping *out )
{
	return g_host.get_mapping? g_host.get_mapping( g_host.user, player, button, out ): 0;
}

void dsfcore_host_begin_mapping( int player, int button )
{
	if( g_host.begin_mapping )
		g_host.begin_mapping( g_host.user, player, button );
}

int dsfcore_host_mapping_state( void )
{
	return g_host.mapping_state? g_host.mapping_state( g_host.user ): DSFCORE_CAPTURE_IDLE;
}

void dsfcore_host_clear_mapping( int player, int button )
{
	if( g_host.clear_mapping )
		g_host.clear_mapping( g_host.user, player, button );
}

__declspec(dllexport) int dsfcore_start( const DsfCoreHost *host, int argc, const char *const *argv, const char *log_path )
{
	if( g_state != DSFCORE_STATE_NOT_STARTED || host == NULL ||
	    host->struct_size < sizeof(DsfCoreHost) || host->api_version != DSFCORE_API_VERSION )
		return 1;
	g_host = *host;

	if( log_path && log_path[0] )
	{
		std::freopen( log_path, "w", stdout );
		std::freopen( log_path, "a", stderr );
		std::setvbuf( stdout, NULL, _IOLBF, BUFSIZ );
		std::setvbuf( stderr, NULL, _IOLBF, BUFSIZ );
	}

	g_args.clear();
	g_args.push_back( "dsfcore" );
	for( int i = 1; i < argc; ++i )
		g_args.push_back( argv[i] );
	/* Development: DSFCORE_DEBUG_PORT=4370 starts the runtime's TCP debug
	 * server (localhost only). */
	if( const char *port = std::getenv("DSFCORE_DEBUG_PORT") )
	{
		g_args.push_back( "--debug-port" );
		g_args.push_back( port );
	}
	g_argv.clear();
	for( size_t i = 0; i < g_args.size(); ++i )
		g_argv.push_back( &g_args[i][0] );
	g_argv.push_back( NULL );

	InterlockedExchange( &g_state, DSFCORE_STATE_RUNNING );
	HANDLE h = CreateThread( NULL, 64u << 20, core_thread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL );
	if( h == NULL )
	{
		InterlockedExchange( &g_state, DSFCORE_STATE_EXITED );
		return 2;
	}
	CloseHandle( h );
	return 0;
}

__declspec(dllexport) int dsfcore_state( void ) { return int(g_state); }
__declspec(dllexport) int dsfcore_exit_code( void ) { return int(g_exit_code); }

} // extern "C"
