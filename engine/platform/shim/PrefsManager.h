/* dsfplatform shim for PrefsManager.h.
 * Only the preferences read by the compiled platform sources are kept, with the
 * StepMania 2005 defaults (src/PrefsManager.cpp).  dsf::Init() sets them from
 * dsf::Config. */
#ifndef PREFSMANAGER_H
#define PREFSMANAGER_H

#include "Preference.h"

class PrefsManager
{
public:
	PrefsManager():
		m_bWindowed( "Windowed", true ),
		m_bThreadedInput( "ThreadedInput", true ),
		m_bOnlyDedicatedMenuButtons( "OnlyDedicatedMenuButtons", false ),
		m_bDisableScreenSaver( "DisableScreenSaver", true ),
		m_fDisplayAspectRatio( "DisplayAspectRatio", 4/3.0f ),
		m_bLogToDisk( "LogToDisk", true ),
		m_bForceLogFlush( "ForceLogFlush", false ),
		m_bShowLogOutput( "ShowLogOutput", false )
	{ }

	Preference<bool>	m_bWindowed;
	Preference<bool>	m_bThreadedInput;		/* InputHandler_DInput thread */
	Preference<bool>	m_bOnlyDedicatedMenuButtons;	/* Game::GameInputToMenuInput */
	Preference<bool>	m_bDisableScreenSaver;		/* RageDisplay_OGL::Update */
	Preference<float>	m_fDisplayAspectRatio;
	Preference<bool>	m_bLogToDisk;
	Preference<bool>	m_bForceLogFlush;
	Preference<bool>	m_bShowLogOutput;
};

extern PrefsManager* PREFSMAN;

#endif
