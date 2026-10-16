/* Driver factory for dsfplatform: the input-handler and window parts of
 * src/arch/arch.cpp, restricted to the drivers the platform layer ships.
 * (arch.cpp itself also instantiates sound, lights, memory-card, movie and
 * loading-window drivers, which would pull most of the engine back in.)
 * Differences from arch.cpp, deliberately:
 *   - InputHandler_MonkeyKeyboard (random key presses for soak tests) is not added;
 *   - only DirectInput / X11 / Linux joystick are known driver names. */
#include "global.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "arch/arch.h"
#include "arch/arch_platform.h"

#if defined(HAVE_WIN32)
#include "arch/InputHandler/InputHandler_DirectInput.h"
#include "arch/LowLevelWindow/LowLevelWindow_Win32.h"
#define DSF_LOW_LEVEL_WINDOW LowLevelWindow_Win32
#else
#include "arch/InputHandler/InputHandler_X11.h"
#include "arch/InputHandler/InputHandler_Linux_Joystick.h"
#include "arch/LowLevelWindow/LowLevelWindow_X11.h"
#define DSF_LOW_LEVEL_WINDOW LowLevelWindow_X11
#endif

void MakeInputHandlers( CString drivers, vector<InputHandler *> &Add )
{
	CStringArray DriversToTry;
	split( drivers, ",", DriversToTry, true );

	for( unsigned i = 0; i < DriversToTry.size(); ++i )
	{
		const CString &s = DriversToTry[i];
		InputHandler *ret = NULL;
#if defined(HAVE_WIN32)
		if( !s.CompareNoCase("DirectInput") )	ret = new InputHandler_DInput;
#else
		if( !s.CompareNoCase("X11") )		ret = new InputHandler_X11;
		if( !s.CompareNoCase("Joystick") )	ret = new InputHandler_Linux_Joystick;
#endif
		if( ret == NULL )
			LOG->Warn( "Unknown input driver name: %s", s.c_str() );
		else
			Add.push_back( ret );
	}
}

LowLevelWindow *MakeLowLevelWindow()
{
	return new DSF_LOW_LEVEL_WINDOW;
}
