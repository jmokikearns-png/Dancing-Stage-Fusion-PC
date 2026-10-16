/* Definitions that replace StepMania subsystems the platform layer does not
 * include.  Each block names what it replaces.  None of this changes how the
 * included input/window code behaves; it only stops it from reaching into the
 * rest of the engine. */
#include "global.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageTimer.h"
#include "PrefsManager.h"
#include "GameState.h"
#include "GameManager.h"
#include "RageTextureManager.h"
#include "ScreenDimensions.h"
#include "StepMania.h"
#include "Style.h"
#include "arch/Dialog/Dialog.h"
#include "arch/ArchHooks/ArchHooks.h"
#include "RageSurface_Save_JPEG.h"

#include <cstdio>
#include <cstdlib>

/* ---- singletons the original sources reference ---- */
PrefsManager	*PREFSMAN = NULL;
GameState	*GAMESTATE = NULL;
GameManager	*GAMEMAN = NULL;
RageTextureManager *TEXTUREMAN = NULL;

/* ---- RageTextureManager (see shim header) ---- */
namespace dsf { void NotifyContextLost(); }
void RageTextureManager::InvalidateTextures() { dsf::NotifyContextLost(); }

/* ---- ScreenDimensions.cpp (theme metrics) ---- */
namespace dsf { extern float g_fLogicalWidth, g_fLogicalHeight; }
float ScreenWidth() { return dsf::g_fLogicalWidth; }
float ScreenHeight() { return dsf::g_fLogicalHeight; }

/* ---- StepMania.cpp (main loop hooks used by GraphicsWindow / X11) ---- */
namespace dsf { extern bool g_bQuitRequested, g_bHasFocus; }
int g_argc = 0;
char **g_argv = NULL;
void ExitGame() { dsf::g_bQuitRequested = true; }
void FocusChanged( bool bHasFocus ) { dsf::g_bHasFocus = bHasFocus; }
/* RageException::Throw() ends here (e.g. no X display, DirectInputCreate failed).
 * StepMania shuts the game down, shows Dialog::Error and exits; same here. */
void NORETURN HandleException( CString error )
{
	Dialog::Error( error );
	exit( 1 );
}

/* ---- arch/Dialog (message boxes): log instead of blocking on a dialog ---- */
namespace Dialog
{
	void Init() { }
	void Shutdown() { }
	void SetWindowed( bool ) { }
	bool IsShowingDialog() { return false; }
	void Error( CString sError, CString sID ) { fprintf( stderr, "dsfplatform error: %s\n", sError.c_str() ); }
	void OK( CString sMessage, CString sID ) { fprintf( stderr, "dsfplatform: %s\n", sMessage.c_str() ); }
	Result AbortRetryIgnore( CString sMessage, CString sID ) { OK( sMessage, sID ); return ignore; }
	Result AbortRetry( CString sMessage, CString sID ) { OK( sMessage, sID ); return abort; }
	void IgnoreMessage( CString ) { }
}

/* ---- crash handler (archutils/Unix/CrashHandler, archutils/Win32/Crash) ----
 * The engine's crash handler forks a backtrace child (Unix) or shows a crash
 * dialog (Win32).  The port will have its own; here a failed assertion prints
 * the reason and aborts. */
#if defined(_WINDOWS)
#include "archutils/Win32/Crash.h"
void ForceCrashHandler( const char *reason ) { fprintf( stderr, "dsfplatform: fatal: %s\n", reason ); fflush( stderr ); abort(); }
void ForceCrashHandlerDeadlock( CString reason, uint64_t iID ) { ForceCrashHandler( reason ); }
#else
#include "archutils/Unix/CrashHandler.h"
void ForceCrashHandler( const char *reason ) { fprintf( stderr, "dsfplatform: fatal: %s\n", reason ); fflush( stderr ); abort(); }
void ForceCrashHandlerDeadlock( CString reason, uint64_t iID ) { ForceCrashHandler( reason ); }
#endif

/* ---- ArchHooks_Unix / ArchHooks_Win32: only the timer is used (RageTimer) ----
 * Same clocks as the 2005 code: gettimeofday() on Unix, timeGetTime() with
 * timeBeginPeriod(1) on Win32, with the original wrap/backwards fixups in
 * arch/ArchHooks/ArchHooks.cpp (reproduced below because that file also
 * instantiates the full ArchHooks driver). */
uint64_t ArchHooks::FixupTimeIfLooped( uint64_t usecs )
{
	static uint64_t last = 0;
	static uint64_t offset_us = 0;
	const uint64_t i32BitMaxMs = uint64_t(1) << 32;
	const uint64_t i32BitMaxUs = i32BitMaxMs*1000;
	const uint64_t one_day = uint64_t(24*60*60)*1000000;
	if( last > (i32BitMaxUs-one_day) && usecs < one_day )
		offset_us += i32BitMaxUs;
	last = usecs;
	return usecs + offset_us;
}

uint64_t ArchHooks::FixupTimeIfBackwards( uint64_t usecs )
{
	static uint64_t last = 0;
	static uint64_t offset_us = 0;
	if( usecs < last )
		offset_us += last-usecs;
	last = usecs;
	return usecs + offset_us;
}

#if defined(_WINDOWS)
#include <windows.h>
#include <mmsystem.h>
static bool g_bTimerInitialized;
static DWORD g_iStartTime;
int64_t ArchHooks::GetMicrosecondsSinceStart( bool bAccurate )
{
	if( !g_bTimerInitialized )
	{
		g_bTimerInitialized = true;
		timeBeginPeriod( 1 );
		g_iStartTime = timeGetTime();
	}
	int64_t ret = (timeGetTime() - g_iStartTime) * int64_t(1000);
	if( bAccurate )
	{
		ret = FixupTimeIfLooped( ret );
		ret = FixupTimeIfBackwards( ret );
	}
	return ret;
}
#else
#include <sys/time.h>
#include "arch/ArchHooks/ArchHooks_Unix.h"
/* Threads_Pthreads.cpp converts RageTimer timeouts to absolute time with this. */
int64_t ArchHooks_Unix::m_iStartTime = 0;
static int64_t GetMicrosecondsSinceEpoch()
{
	struct timeval tv;
	gettimeofday( &tv, NULL );
	return int64_t(tv.tv_sec) * 1000000 + int64_t(tv.tv_usec);
}
int64_t ArchHooks::GetMicrosecondsSinceStart( bool bAccurate )
{
	if( ArchHooks_Unix::m_iStartTime == 0 )
		ArchHooks_Unix::m_iStartTime = GetMicrosecondsSinceEpoch();
	int64_t ret = GetMicrosecondsSinceEpoch() - ArchHooks_Unix::m_iStartTime;
	if( bAccurate )
		ret = FixupTimeIfBackwards( ret );
	return ret;
}
#endif

/* ---- Style.cpp (needs NoteData etc.).  GAMESTATE->m_pCurStyle is always NULL in
 * dsfplatform, so InputMapper's/Game's style paths are never taken; these exist
 * only to satisfy the linker. ---- */
GameInput Style::StyleInputToGameInput( const StyleInput& ) const { FAIL_M( "dsfplatform: no styles" ); }
StyleInput Style::GameInputToStyleInput( const GameInput& ) const { FAIL_M( "dsfplatform: no styles" ); }
PlayerNumber Style::ControllerToPlayerNumber( GameController ) const { FAIL_M( "dsfplatform: no styles" ); }

/* ---- RageSurface_Save_JPEG.cpp (libjpeg): screenshots are BMP-only here ---- */
bool RageSurfaceUtils::SaveJPEG( RageSurface *, RageFile &, bool )
{
	LOG->Warn( "dsfplatform: JPEG screenshots are not built in" );
	return false;
}
