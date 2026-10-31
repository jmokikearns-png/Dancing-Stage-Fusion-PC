/* dsfplatform-test: exercises the StepMania 2005 platform layer.
 *
 * Opens a window (RageDisplay_OGL + LowLevelWindow), reads input through
 * RageInput -> InputFilter -> InputMapper, maps it to the two PS1 digital pads,
 * and logs every mapped press/release with its timestamp.  The window shows the
 * 2 x 14 pad buttons as quads (lit while held), drawn with RageDisplay.
 *
 *   dsfplatform-test [--config DIR] [--log FILE] [--seconds S] [--no-window]
 *                    [--drivers LIST] [--set P1:Cross=Key_k ...] [--clear P1:Cross:2]
 *                    [--reset] [--save] [--dump-map] [--screenshot NAME.bmp]
 *                    [--fullscreen] [--size WxH] [--no-vsync] [--unthreaded]
 *
 * Exits on Esc (unmapped by default), window close, or after --seconds.
 * Log line format (one per mapped edge):
 *   PAD t=<seconds> pad=<1|2> button=<Name> <down|up> dev=<DeviceInput> word=0x<hex>
 * Unmapped physical edges are logged as
 *   RAW t=<seconds> <down|up> dev=<DeviceInput>
 */
#include "dsfplatform.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
static void SleepMs( int ms ) { Sleep( ms ); }
#else
#include <unistd.h>
static void SleepMs( int ms ) { usleep( ms * 1000 ); }
#endif

/* RageDisplay drawing (the only engine header the harness uses, for the overlay demo). */
#include "global.h"
#include "RageDisplay.h"
#include "RageTypes.h"
#include "RageTimer.h"

static FILE *g_pLog = NULL;

static void Log( const char *fmt, ... )
{
	char buf[1024];
	va_list va;
	va_start( va, fmt );
	vsnprintf( buf, sizeof(buf), fmt, va );
	va_end( va );
	fputs( buf, stdout );
	fputc( '\n', stdout );
	fflush( stdout );
	if( g_pLog )
	{
		fputs( buf, g_pLog );
		fputc( '\n', g_pLog );
		fflush( g_pLog );
	}
}

static bool ParseButtonSpec( const std::string &s, int &iPad, dsf::Ps1Button &b, std::string &sRest )
{
	/* "P1:Cross=..." or "P2:L1:3" */
	if( s.size() < 4 || (s[0] != 'P' && s[0] != 'p') || (s[1] != '1' && s[1] != '2') || s[2] != ':' )
		return false;
	iPad = s[1] - '1';
	std::string t = s.substr( 3 );
	size_t sep = t.find_first_of( "=:" );
	std::string sName = t.substr( 0, sep );
	sRest = sep == std::string::npos? "": t.substr( sep+1 );
	for( int i = 0; i < dsf::NUM_PS1_BUTTONS; ++i )
		if( !strcasecmp( sName.c_str(), dsf::Ps1ButtonName( (dsf::Ps1Button) i ) ) )
		{
			b = (dsf::Ps1Button) i;
			return true;
		}
	return false;
}

static void DumpMap()
{
	for( int p = 0; p < dsf::NUM_PADS; ++p )
		for( int b = 0; b < dsf::NUM_PS1_BUTTONS; ++b )
		{
			std::string s;
			for( int slot = 0; slot < dsf::NUM_SLOTS; ++slot )
			{
				std::string m = dsf::GetMapping( p, (dsf::Ps1Button) b, slot );
				if( m.empty() )
					continue;
				char tmp[64];
				snprintf( tmp, sizeof(tmp), " [%d]%s", slot, m.c_str() );
				s += tmp;
			}
			Log( "MAP pad=%d button=%s%s", p+1, dsf::Ps1ButtonName( (dsf::Ps1Button) b ), s.c_str() );
		}
}

static void DrawPads( const char *szScreenshot = NULL )
{
	RageDisplay *d = dsf::Display();
	if( d == NULL || !d->BeginFrame() )
		return;
	d->SetTexture( 0, NULL );
	d->SetBlendMode( BLEND_NORMAL );
	d->SetZTestMode( ZTEST_OFF );
	d->SetZWrite( false );
	d->SetCullMode( CULL_NONE );
	d->SetLighting( false );

	const float fW = 36, fH = 36, fGap = 6;
	for( int p = 0; p < dsf::NUM_PADS; ++p )
		for( int b = 0; b < dsf::NUM_PS1_BUTTONS; ++b )
		{
			const bool bOn = dsf::IsPressed( p, (dsf::Ps1Button) b );
			RageColor c = bOn? RageColor( 1, 0.85f, 0.1f, 1 ): RageColor( 0.25f, 0.25f, 0.3f, 1 );
			if( p == 1 && !bOn ) c = RageColor( 0.2f, 0.3f, 0.25f, 1 );
			const float x = 20 + (b % 7) * (fW + fGap) + p * 320;
			const float y = 180 + (b / 7) * (fH + fGap);
			RageSpriteVertex v[4];
			memset( v, 0, sizeof(v) );
			v[0].p = RageVector3( x,    y,    0 );
			v[1].p = RageVector3( x,    y+fH, 0 );
			v[2].p = RageVector3( x+fW, y+fH, 0 );
			v[3].p = RageVector3( x+fW, y,    0 );
			for( int i = 0; i < 4; ++i )
				v[i].c = c;
			d->DrawQuads( v, 4 );
		}
	d->EndFrame();
	/* RageDisplay_OGL::CreateScreenshot() reads GL_FRONT (2005 code).  On Mesa
	 * llvmpipe under Xvfb that readback is black although the window shows the
	 * frame (checked with an X root-window capture); kept as-is for the record. */
	if( szScreenshot )
		Log( "SCREENSHOT %s ok=%d", szScreenshot, d->SaveScreenshot( szScreenshot, RageDisplay::SAVE_LOSSLESS )? 1: 0 );
}

int main( int argc, char **argv )
{
	dsf::Config cfg;
	std::string sLogPath, sScreenshot;
	double fSeconds = 0;
	std::vector<std::string> vSets, vClears;
	bool bReset = false, bSave = false, bDump = false;

	for( int i = 1; i < argc; ++i )
	{
		std::string a = argv[i];
		std::string next = i+1 < argc? argv[i+1]: "";
		if( a == "--config" )		{ cfg.sConfigDir = next; ++i; }
		else if( a == "--log" )		{ sLogPath = next; ++i; }
		else if( a == "--seconds" )	{ fSeconds = atof( next.c_str() ); ++i; }
		else if( a == "--drivers" )	{ cfg.sInputDrivers = next; ++i; }
		else if( a == "--set" )		{ vSets.push_back( next ); ++i; }
		else if( a == "--clear" )	{ vClears.push_back( next ); ++i; }
		else if( a == "--screenshot" )	{ sScreenshot = next; ++i; }
		else if( a == "--size" )	{ sscanf( next.c_str(), "%dx%d", &cfg.iWidth, &cfg.iHeight ); ++i; }
		else if( a == "--no-window" )	cfg.bOpenWindow = false;
		else if( a == "--fullscreen" )	cfg.bWindowed = false;
		else if( a == "--no-vsync" )	cfg.bVsync = false;
		else if( a == "--unthreaded" )	cfg.bThreadedInput = false;
		else if( a == "--reset" )	bReset = true;
		else if( a == "--save" )	bSave = true;
		else if( a == "--dump-map" )	bDump = true;
		else if( a == "--verbose" )	cfg.bLogToStdout = true;
		else { fprintf( stderr, "unknown argument %s\n", a.c_str() ); return 2; }
	}
	if( !sLogPath.empty() )
		g_pLog = fopen( sLogPath.c_str(), "w" );	/* opened before Init() chdir()s */

	std::string sError;
	if( !dsf::Init( argc, argv, cfg, sError ) )
	{
		Log( "INIT FAILED: %s", sError.c_str() );
		return 1;
	}
	Log( "READY keymaps=%s window=%s", dsf::KeymapsPath().c_str(), dsf::Display()? "yes": "no" );
	{
		std::vector<std::string> vDev;
		dsf::GetDevices( vDev );
		for( unsigned i = 0; i < vDev.size(); ++i )
			Log( "DEVICE %s", vDev[i].c_str() );
	}

	if( bReset )
		dsf::ResetMappingToDefaults();
	for( unsigned i = 0; i < vSets.size(); ++i )
	{
		int iPad; dsf::Ps1Button b; std::string sDev;
		int iSlot = 0;
		if( !ParseButtonSpec( vSets[i], iPad, b, sDev ) )
		{ Log( "BAD --set %s", vSets[i].c_str() ); continue; }
		/* optional "@slot" suffix */
		size_t at = sDev.find( '@' );
		if( at != std::string::npos ) { iSlot = atoi( sDev.c_str() + at + 1 ); sDev = sDev.substr( 0, at ); }
		Log( "SET pad=%d button=%s slot=%d dev=%s ok=%d", iPad+1, dsf::Ps1ButtonName( b ), iSlot, sDev.c_str(),
			dsf::SetMapping( iPad, b, iSlot, sDev )? 1: 0 );
	}
	for( unsigned i = 0; i < vClears.size(); ++i )
	{
		int iPad; dsf::Ps1Button b; std::string sSlot;
		if( !ParseButtonSpec( vClears[i], iPad, b, sSlot ) )
		{ Log( "BAD --clear %s", vClears[i].c_str() ); continue; }
		dsf::ClearMapping( iPad, b, atoi( sSlot.c_str() ) );
	}
	if( bSave )
	{
		dsf::SaveMapping();
		Log( "SAVED %s", dsf::KeymapsPath().c_str() );
	}
	if( bDump )
		DumpMap();

	RageTimer start, frame;
	bool bQuit = false;
	int iFrames = 0;
	while( !bQuit )
	{
		const float fDelta = frame.GetDeltaTime();
		dsf::Update( fDelta );

		std::vector<dsf::PadEvent> vPad;
		std::vector<dsf::RawEvent> vRaw;
		dsf::GetEvents( vPad, vRaw );
		for( unsigned i = 0; i < vPad.size(); ++i )
		{
			const dsf::PadEvent &e = vPad[i];
			Log( "PAD t=%.4f pad=%d button=%s %s dev=%s word=0x%04X",
				e.fTimestamp, e.iPad+1, dsf::Ps1ButtonName( e.button ), e.bPressed? "down": "up",
				e.sDeviceInput.c_str(), dsf::GetPadWord( e.iPad ) );
		}
		for( unsigned i = 0; i < vRaw.size(); ++i )
		{
			const dsf::RawEvent &e = vRaw[i];
			bool bMapped = false;
			for( unsigned j = 0; j < vPad.size(); ++j )
				if( vPad[j].sDeviceInput == e.sDeviceInput && vPad[j].fTimestamp == e.fTimestamp )
					bMapped = true;
			if( !bMapped )
				Log( "RAW t=%.4f %s dev=%s", e.fTimestamp, e.bPressed? "down": "up", e.sDeviceInput.c_str() );
			if( e.bPressed && e.sDeviceInput == "Key_escape" )
				bQuit = true;
		}

		DrawPads();
		++iFrames;
		if( dsf::QuitRequested() )
			bQuit = true;
		if( fSeconds > 0 && start.Ago() >= fSeconds )
			bQuit = true;
		if( !dsf::Display() || !cfg.bVsync )
			SleepMs( 4 );
	}

	if( !sScreenshot.empty() && dsf::Display() )
		DrawPads( sScreenshot.c_str() );
	Log( "EXIT frames=%d seconds=%.2f", iFrames, start.Ago() );
	dsf::Shutdown();
	if( g_pLog )
		fclose( g_pLog );
	return 0;
}
