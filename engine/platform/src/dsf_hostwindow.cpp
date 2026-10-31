/* Host-owned window for input-only builds (DSF_INPUT_ONLY).
 *
 * StepMania's DirectInput driver asks GraphicsWindow::GetHwnd() for the window
 * it attaches to (InputHandler_DirectInputHelper.cpp, SetCooperativeLevel) and
 * for its foreground check (InputHandler_DirectInput.cpp).  In the game the
 * window belongs to the runtime, so this replaces archutils/Win32/GraphicsWindow.cpp
 * with just that accessor, returning the window the host hands in. */
#include "global.h"
#include "dsfplatform.h"

#if defined(_WINDOWS)
#include <windows.h>
#include "archutils/Win32/GraphicsWindow.h"

static HWND g_hHostWnd = NULL;

HWND GraphicsWindow::GetHwnd() { return g_hHostWnd; }

namespace dsf
{
	void SetHostWindow( void *pNativeWindow ) { g_hHostWnd = (HWND) pNativeWindow; }
}
#else
namespace dsf
{
	void SetHostWindow( void * ) { }
}
#endif
