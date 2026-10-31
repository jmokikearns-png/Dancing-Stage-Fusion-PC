/* dsfplatform facade: owns the StepMania singletons the platform layer uses and
 * converts StepMania input events into PS1 pad state.  Init order follows
 * src/StepMania.cpp (FILEMAN, LOG, PREFSMAN, GAMEMAN/GAMESTATE, INPUTFILTER,
 * INPUTMAPPER, DISPLAY, INPUTMAN). */
#include "global.h"
#include "dsfplatform.h"

#include "RageLog.h"
#include "RageUtil.h"
#include "RageTimer.h"
#include "RageFileManager.h"
#include "RageInput.h"
#include "RageInputDevice.h"
#include "InputFilter.h"
#include "InputMapper.h"
#include "PrefsManager.h"
#include "GameState.h"
#include "GameManager.h"
#include "Game.h"
#if !defined(DSF_INPUT_ONLY)
#include "RageDisplay.h"
#include "RageDisplay_OGL.h"
#endif
#include "StepMania.h"

#if defined(_WINDOWS)
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

extern Game g_Ps1PadGame;

namespace dsf
{
	bool g_bQuitRequested = false;
	bool g_bHasFocus = true;
	float g_fLogicalWidth = 640, g_fLogicalHeight = 480;
	static CString g_sConfigDirReal;
	static vector<PadEvent> g_PadEvents;
	static vector<RawEvent> g_RawEvents;

	void NotifyContextLost() { LOG->Trace( "dsfplatform: GL context recreated" ); }

	/* SCPH-1080 digital pad, reply bytes 3/4 (psx-spx "Controllers"):
	 * byte3: Select L3 R3 Start Up Right Down Left (bit0..7)
	 * byte4: L2 R2 L1 R1 Triangle Circle Cross Square (bit0..7) */
	static const int g_iBit[NUM_PS1_BUTTONS] =
	{
		4, 6, 7, 5,		/* Up Down Left Right */
		14, 13, 15, 12,		/* Cross Circle Square Triangle */
		10, 8, 11, 9,		/* L1 L2 R1 R2 */
		3, 0,			/* Start Select */
	};
	int Ps1ButtonBit( Ps1Button b ) { return g_iBit[b]; }
	const char *Ps1ButtonName( Ps1Button b ) { return g_Ps1PadGame.m_szButtonNames[b]; }

	Config::Config():
		sConfigDir( "." ), sInputDrivers( "" ), bOpenWindow( true ), bWindowed( true ),
		iWidth( 640 ), iHeight( 480 ), iRefreshRate( 0 ), bVsync( true ),
		iLogicalWidth( 640 ), iLogicalHeight( 480 ),
		sWindowTitle( "Dancing Stage Fusion" ), bThreadedInput( true ), bLogToStdout( false )
	{ }

	static CString MakeAbsolute( const CString &s )
	{
		if( s.size() && (s[0] == '/' || s[0] == '\\' || (s.size() > 1 && s[1] == ':')) )
			return s;
		char buf[4096];
		if( getcwd( buf, sizeof(buf) ) == NULL )
			return s;
		CString ret = CString(buf) + "/" + s;
		ret.Replace( '\\', '/' );
		return ret;
	}

	bool Init( int argc, char **argv, const Config &cfg, std::string &sErrorOut )
	{
		::g_argc = argc; ::g_argv = argv;

		/* RageFileManager chdir()s to the executable's directory; resolve first. */
		g_sConfigDirReal = MakeAbsolute( cfg.sConfigDir.c_str() );

		FILEMAN = new RageFileManager( argv[0] );
		FILEMAN->Mount( "dir", g_sConfigDirReal, "/" );

		LOG = new RageLog();
		LOG->SetShowLogOutput( cfg.bLogToStdout );
		LOG->SetLogToDisk( true );	/* <config dir>/log.txt, info.txt */
		LOG->SetInfoToDisk( true );
		LOG->SetFlushing( true );

		PREFSMAN = new PrefsManager;
		PREFSMAN->m_bWindowed.Set( cfg.bWindowed );
		PREFSMAN->m_bThreadedInput.Set( cfg.bThreadedInput );
		PREFSMAN->m_fDisplayAspectRatio.Set( float(cfg.iLogicalWidth) / cfg.iLogicalHeight );
		g_fLogicalWidth = (float) cfg.iLogicalWidth;
		g_fLogicalHeight = (float) cfg.iLogicalHeight;

		GAMEMAN = new GameManager;
		GAMESTATE = new GameState;
		GAMESTATE->m_pCurGame = &g_Ps1PadGame;

		INPUTFILTER = new InputFilter;
		INPUTMAPPER = new InputMapper;	/* reads Data/Keymaps.ini, adds keyboard defaults */

#if !defined(DSF_INPUT_ONLY)
		if( cfg.bOpenWindow )
		{
			RageDisplay::VideoModeParams params(
				cfg.bWindowed, cfg.iWidth, cfg.iHeight, 32, cfg.iRefreshRate, cfg.bVsync,
				false /*interlaced*/, false /*smooth lines*/, false /*trilinear*/,
				false /*anisotropic*/, cfg.sWindowTitle.c_str(), "" /*icon*/, false /*PAL*/,
				float(cfg.iWidth) / cfg.iHeight );
			RageDisplay_OGL *pDisplay = new RageDisplay_OGL;
			CString sError = pDisplay->Init( params, true /*allow unaccelerated*/ );
			if( sError != "" )
			{
				delete pDisplay;
				sErrorOut = "OpenGL init failed: " + std::string( sError.c_str() );
				return false;
			}
			DISPLAY = pDisplay;
		}
#endif

		CString sDrivers = cfg.sInputDrivers.c_str();
		if( sDrivers == "" )
		{
#if defined(_WINDOWS)
			sDrivers = "DirectInput";
#else
			sDrivers = cfg.bOpenWindow? "X11,Joystick": "Joystick";
#endif
		}
		INPUTMAN = new RageInput( sDrivers );

		/* Joysticks: apply StepMania's built-in per-adapter tables only to
		 * buttons the user has not mapped (same as StepMania on a device change). */
		INPUTMAPPER->AutoMapJoysticksForCurrentGame();
		return true;
	}

	void Shutdown()
	{
		SAFE_DELETE( INPUTMAN );
		SAFE_DELETE( INPUTMAPPER );	/* note: ~InputMapper() saves Keymaps.ini (StepMania behaviour) */
		SAFE_DELETE( INPUTFILTER );
#if !defined(DSF_INPUT_ONLY)
		SAFE_DELETE( DISPLAY );
#endif
		SAFE_DELETE( GAMESTATE );
		SAFE_DELETE( GAMEMAN );
		SAFE_DELETE( PREFSMAN );
		SAFE_DELETE( LOG );
		SAFE_DELETE( FILEMAN );
	}

	static double TimerToSeconds( const RageTimer &t )
	{
		return t.m_secs + t.m_us / 1000000.0;
	}

	void Update( float fDeltaSeconds )
	{
#if !defined(DSF_INPUT_ONLY)
		if( DISPLAY )
			DISPLAY->Update( fDeltaSeconds );	/* window messages (Win32), screensaver */
#endif	/* window messages (Win32), screensaver */
		INPUTFILTER->Update( fDeltaSeconds );	/* polls INPUTMAN, generates repeats */

		InputEventArray events;
		INPUTFILTER->GetInputEvents( events );
		for( unsigned i = 0; i < events.size(); ++i )
		{
			const InputEvent &ev = events[i];
			if( ev.type != IET_FIRST_PRESS && ev.type != IET_RELEASE )
				continue;	/* key repeat / analog level: not PS1 pad semantics */
			DeviceInput di = ev;
			const bool bPressed = ev.type == IET_FIRST_PRESS;

			RawEvent raw;
			raw.bPressed = bPressed;
			raw.fTimestamp = TimerToSeconds( di.ts );
			raw.sDeviceInput = di.toString().c_str();
			g_RawEvents.push_back( raw );

			GameInput gi;
			if( !INPUTMAPPER->DeviceToGame( di, gi ) )
				continue;
			if( gi.controller >= NUM_PADS || gi.button >= NUM_PS1_BUTTONS )
				continue;
			PadEvent pe;
			pe.iPad = gi.controller;
			pe.button = (Ps1Button) gi.button;
			pe.bPressed = bPressed;
			pe.fTimestamp = raw.fTimestamp;
			pe.sDeviceInput = raw.sDeviceInput;
			g_PadEvents.push_back( pe );
		}
	}

	void GetEvents( vector<PadEvent> &vPadOut, vector<RawEvent> &vRawOut )
	{
		vPadOut.swap( g_PadEvents ); g_PadEvents.clear();
		vRawOut.swap( g_RawEvents ); g_RawEvents.clear();
	}

	bool IsPressed( int iPad, Ps1Button b )
	{
		/* true if any of the (up to 5) physical inputs mapped to it is held */
		return INPUTMAPPER->IsButtonDown( GameInput( (GameController) iPad, (GameButton) b ) );
	}

	uint16_t GetPadWord( int iPad )
	{
		uint16_t w = 0xFFFF;
		for( int b = 0; b < NUM_PS1_BUTTONS; ++b )
			if( IsPressed( iPad, (Ps1Button) b ) )
				w &= ~(1 << g_iBit[b]);
		return w;
	}

	bool SetMapping( int iPad, Ps1Button b, int iSlot, const std::string &sDeviceInput )
	{
		DeviceInput di;
		if( !di.fromString( sDeviceInput.c_str() ) || !di.IsValid() )
			return false;
		INPUTMAPPER->SetInputMap( di, GameInput( (GameController) iPad, (GameButton) b ), iSlot );
		return true;
	}

	void ClearMapping( int iPad, Ps1Button b, int iSlot )
	{
		INPUTMAPPER->ClearFromInputMap( GameInput( (GameController) iPad, (GameButton) b ), iSlot );
	}

	std::string GetMapping( int iPad, Ps1Button b, int iSlot )
	{
		DeviceInput di;
		if( !INPUTMAPPER->GameToDevice( GameInput( (GameController) iPad, (GameButton) b ), iSlot, di ) )
			return "";
		return di.toString().c_str();
	}

	void ResetMappingToDefaults()
	{
		INPUTMAPPER->ClearAllMappings();
		INPUTMAPPER->AddDefaultMappingsForCurrentGameIfUnmapped();
		INPUTMAPPER->AutoMapJoysticksForCurrentGame();
	}

	void LoadMapping() { INPUTMAPPER->ReadMappingsFromDisk(); }
	void SaveMapping() { INPUTMAPPER->SaveMappingsToDisk(); }
	std::string KeymapsPath() { return std::string( g_sConfigDirReal.c_str() ) + "/Data/Keymaps.ini"; }

	void GetDevices( vector<std::string> &vOut )
	{
		vector<InputDevice> vDevices;
		vector<CString> vDescriptions;
		INPUTMAN->GetDevicesAndDescriptions( vDevices, vDescriptions );
		for( unsigned i = 0; i < vDevices.size(); ++i )
			vOut.push_back( std::string( InputDeviceToString( vDevices[i] ).c_str() ) + ": " + vDescriptions[i].c_str() );
	}

#if !defined(DSF_INPUT_ONLY)
	RageDisplay *Display() { return DISPLAY; }
#else
	RageDisplay *Display() { return NULL; }
#endif
	bool QuitRequested() { return g_bQuitRequested; }
	bool HasFocus() { return g_bHasFocus; }
}
