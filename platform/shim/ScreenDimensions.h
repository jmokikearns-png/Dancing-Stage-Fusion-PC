/* dsfplatform shim for ScreenDimensions.h.
 * The real header takes the logical screen size from the theme (ThemeMetric
 * Common::ScreenWidth/Height).  Here it is the logical size the host renderer
 * draws in, set with dsf::Config::iLogicalWidth/Height (default 640x480). */
#ifndef SCREEN_DIMENSIONS_H
#define SCREEN_DIMENSIONS_H

/* The real header includes ThemeManager.h/ThemeMetric.h, which bring in these;
 * RageDisplay_OGL.cpp relies on that. */
#include <map>
#include <set>
#include <deque>

#define SCREEN_WIDTH ScreenWidth()
#define SCREEN_HEIGHT ScreenHeight()
float ScreenWidth();
float ScreenHeight();

#define		SCREEN_LEFT		(0)
#define		SCREEN_RIGHT	(SCREEN_WIDTH)
#define		SCREEN_TOP		(0)
#define		SCREEN_BOTTOM	(SCREEN_HEIGHT)
#define		SCREEN_CENTER_X		(SCREEN_LEFT + (SCREEN_RIGHT - SCREEN_LEFT)/2.0f)
#define		SCREEN_CENTER_Y		(SCREEN_TOP + (SCREEN_BOTTOM - SCREEN_TOP)/2.0f)
#define	SCREEN_NEAR		(-1000)
#define	SCREEN_FAR		(1000)

#endif
