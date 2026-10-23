/* dsfcore_host_stub.cpp -- dsfcore_host.h without a host, for the stand-alone
 * Linux test build (the debug server drives it): no buttons from a host, PC
 * settings kept in memory with StepMania's choice lists, and key mappings
 * with the port's default keys.  A key capture can't see real keys here: it
 * completes by itself after about a second with a made-up pad button.
 * QUIT on the main menu ends the program. */
#include "dsfcore_host.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct StubSetting
{
	const char *choices[64];
	int count, choice, def, later;
};

StubSetting g_settings[DSFCORE_SETTING_COUNT];

void init()
{
	static bool done = false;
	if( done )
		return;
	done = true;
	static char offsets[61][12];
	const struct { int id; const char *c[16]; int def, later; } table[] = {
		{ DSFCORE_SETTING_VOLUME, { "SILENT","10%","20%","30%","40%","50%","60%","70%","80%","90%","100%" }, 10, 0 },
		{ DSFCORE_SETTING_AUDIO_BUFFER, { "AUTO","12 ms","17 ms","23 ms","35 ms","46 ms","70 ms","93 ms" }, 0, 1 },
		{ DSFCORE_SETTING_SYNC_PICTURE, { "OFF","ON" }, 0, 0 },
		{ DSFCORE_SETTING_SCREEN_MODE, { "FULLSCREEN","WINDOWED" }, 1, 0 },
		{ DSFCORE_SETTING_RESOLUTION, { "640x480","800x600","960x720","1280x960","1280x720","1920x1080" }, 2, 0 },
		{ DSFCORE_SETTING_REFRESH_RATE, { "DEFAULT","50","60","75","120" }, 0, 0 },
		{ DSFCORE_SETTING_VSYNC, { "NO","YES" }, 1, 0 },
		{ DSFCORE_SETTING_RENDER_QUALITY, { "NATIVE","720p","1080p","1440p" }, 2, 1 },
		{ DSFCORE_SETTING_RENDERER, { "SOFTWARE","OPENGL" }, 1, 1 },
	};
	for( unsigned i = 0; i < sizeof(table)/sizeof(table[0]); ++i )
	{
		StubSetting &s = g_settings[table[i].id];
		for( int k = 0; k < 16 && table[i].c[k]; ++k )
			s.choices[s.count++] = table[i].c[k];
		s.choice = s.def = table[i].def;
		s.later = table[i].later;
	}
	StubSetting &o = g_settings[DSFCORE_SETTING_PICTURE_OFFSET];
	for( int k = 0; k < 61; ++k )
	{
		std::snprintf( offsets[k], sizeof(offsets[k]), "%+d ms", -150 + 5*k );
		o.choices[o.count++] = offsets[k];
	}
	std::strcpy( offsets[30], "0 ms" );
	o.choice = o.def = 30;
}

} // namespace

extern "C" int dsfcore_host_read_pads( uint16_t pads_out[2] )
{
	(void) pads_out;
	return 0;	/* the debug server's input arrives through the controller port */
}

extern "C" int dsfcore_host_get_setting( int id, DsfCoreSetting *out )
{
	init();
	if( id < 0 || id >= DSFCORE_SETTING_COUNT || g_settings[id].count == 0 )
		return 0;
	const StubSetting &s = g_settings[id];
	out->choice = s.choice;
	out->num_choices = s.count;
	out->default_choice = s.def;
	out->applies_later = s.later;
	std::snprintf( out->text, sizeof(out->text), "%s", s.choices[s.choice] );
	return 1;
}

extern "C" void dsfcore_host_set_setting( int id, int choice )
{
	init();
	if( id >= 0 && id < DSFCORE_SETTING_COUNT && choice >= 0 && choice < g_settings[id].count )
		g_settings[id].choice = choice;
}

extern "C" void dsfcore_host_commit_settings( void )
{
}

namespace {

DsfCoreMapping g_mappings[2][DSFCORE_BUTTON_COUNT];
int g_capture_state = DSFCORE_CAPTURE_IDLE, g_capture_player, g_capture_button, g_capture_polls;

void init_mappings()
{
	static bool done = false;
	if( done )
		return;
	done = true;
	const char *keys[2][DSFCORE_BUTTON_COUNT] = {
		{ "KEY UP", "KEY DOWN", "KEY LEFT", "KEY RIGHT", "KEY X", "KEY C", "KEY Z", "KEY S",
		  "KEY Q", "KEY 1", "KEY E", "KEY 3", "KEY ENTER", "KEY BACKSPACE", "KEY ESCAPE" },
		{ "KEY KP 8", "KEY KP 2", "KEY KP 4", "KEY KP 6", "KEY KP 0", "KEY KP .", "KEY KP 1", "KEY KP 3",
		  "KEY KP 7", "KEY KP /", "KEY KP 9", "KEY KP *", "KEY KP ENTER", "KEY KP +", "" } };
	for( int p = 0; p < 2; ++p )
		for( int b = 0; b < DSFCORE_BUTTON_COUNT; ++b )
			std::snprintf( g_mappings[p][b].slot[2], sizeof(g_mappings[p][b].slot[2]), "%s", keys[p][b] );
}

} // namespace

extern "C" int dsfcore_host_get_mapping( int player, int button, DsfCoreMapping *out )
{
	init_mappings();
	if( player < 0 || player > 1 || button < 0 || button >= DSFCORE_BUTTON_COUNT )
		return 0;
	*out = g_mappings[player][button];
	return 1;
}

extern "C" void dsfcore_host_begin_mapping( int player, int button )
{
	init_mappings();
	g_capture_state = DSFCORE_CAPTURE_WAITING;
	g_capture_player = player;
	g_capture_button = button;
	g_capture_polls = 0;
}

extern "C" int dsfcore_host_mapping_state( void )
{
	if( g_capture_state == DSFCORE_CAPTURE_WAITING && ++g_capture_polls >= 50 )
	{
		DsfCoreMapping &m = g_mappings[g_capture_player][g_capture_button];
		const int slot = m.slot[0][0] == 0? 0: 1;
		char t[32];
		std::snprintf( t, sizeof(t), "JOY%d B%d", g_capture_player + 1, g_capture_button + 1 );
		std::snprintf( m.slot[slot], sizeof(m.slot[slot]), "%s", t );
		g_capture_state = DSFCORE_CAPTURE_DONE;
	}
	return g_capture_state;
}

extern "C" void dsfcore_host_clear_mapping( int player, int button )
{
	init_mappings();
	if( player < 0 || player > 1 || button < 0 || button >= DSFCORE_BUTTON_COUNT )
		return;
	g_mappings[player][button].slot[0][0] = 0;
	g_mappings[player][button].slot[1][0] = 0;
}

/* No host to return to: QUIT ends the test program. */
extern "C" void dsfcore_host_quit( void )
{
	std::fflush( stdout );
	std::exit( 0 );
}
