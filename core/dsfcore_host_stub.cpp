/* dsfcore_host_stub.cpp -- dsfcore_host.h without a host, for the stand-alone
 * Linux test build (the debug server drives it): no buttons from a host, and
 * PC settings kept in memory with StepMania's choice lists. */
#include "dsfcore_host.h"

#include <cstdio>
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
		{ DSFCORE_SETTING_REFRESH_RATE, { "DEFAULT","60","75","120" }, 0, 0 },
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
