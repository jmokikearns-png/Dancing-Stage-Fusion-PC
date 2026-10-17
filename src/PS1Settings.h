/* PS1Settings - StepMania's own option rows (ConfOption) behind the PC
 * settings rows inside the game's OPTIONS screens.
 *
 * The rows are drawn and driven by the game's code plus port/core/
 * dsfcore_menus.cpp in dsfcore.dll, on the core thread; this class answers
 * them.  The core thread only reads a snapshot and queues changes; the main
 * thread applies them through the same ConfOption the StepMania option
 * screens use (its choice list, Put() and effects), so every value means
 * exactly what it means there.  Volume and picture sync apply at once,
 * display changes when the player leaves the screen (commit), and changed
 * preferences are saved then.
 */

#ifndef PS1_SETTINGS_H
#define PS1_SETTINGS_H

#include "RageThreads.h"
#include "dsfcore_api.h"	/* keep in sync with port/core/dsfcore_api.h */

class PS1Settings
{
public:
	PS1Settings();

	/* Core thread (DsfCoreHost callbacks). */
	int Get( int id, DsfCoreSetting &out );
	void Set( int id, int choice );
	void Commit();

	/* Main thread, every update while the game runs. */
	void Update( float fDeltaTime );

private:
	void Refresh();		/* main thread; m_Lock held */

	struct Row
	{
		bool bHave;
		DsfCoreSetting info;
		CStringArray choices;
	};
	RageMutex m_Lock;
	Row m_Rows[DSFCORE_SETTING_COUNT];
	vector< pair<int, int> > m_Requests;
	bool m_bCommit;
	int m_iPendingEffects;	/* ConfOption effects waiting for the commit */
	float m_fRefreshSeconds;
};

#endif

/*
 * (c) 2026 Dancing Stage Fusion PC port
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
