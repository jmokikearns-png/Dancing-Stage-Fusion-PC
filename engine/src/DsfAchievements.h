/* DsfAchievements - DSF PC's achievements.
 *
 * The game core reports each song's result and what the memory card's save
 * holds (dsfcore_api.h: DsfCorePlay, DsfCoreProgress; PS1Core hands them
 * over on the main thread).  What has been earned is kept in
 * Data/Achievements.ini, beside StepMania.ini: a new memory card doesn't
 * take achievements away, and records already on a card count once it is
 * loaded.  The game screen shows each new one (TakeEarned); the title menu's
 * ACHIEVEMENTS lists them all. */
#ifndef DSF_ACHIEVEMENTS_H
#define DSF_ACHIEVEMENTS_H

#include "StdString.h"
#include "dsfcore_api.h"

namespace DsfAchievements
{
	struct Info
	{
		CString sName, sDescription;
		bool bEarned;
		CString sDate;			/* when earned: YYYY-MM-DD */
		int iProgress, iTarget;		/* iTarget 0: no count to show */
	};

	/* Main thread. */
	void OnPlay( const DsfCorePlay &play );
	void OnProgress( const DsfCoreProgress &progress );

	int GetCount();
	int GetEarnedCount();
	void GetInfo( int i, Info &out );

	/* Once each: the names of achievements earned since the last call, oldest first. */
	bool TakeEarned( vector<CString> &asNamesOut );
}

#endif
