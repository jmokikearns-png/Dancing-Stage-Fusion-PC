/* DsfUpdate - DSF PC: is a newer release out on GitHub?  The asking is done
 * by Updater.exe (tools/updater/updater.c) beside the program:
 * "Updater.exe --check" in the background at start-up; the title menu offers
 * the update, and on yes Updater.exe installs it and starts the game again. */
#ifndef DSF_UPDATE_H
#define DSF_UPDATE_H

#include "StdString.h"

namespace DsfUpdate
{
	void StartCheck();			/* at start-up (PS1CheckForUpdates) */
	bool TakeOffer( CString &sVersion );	/* once: a newer version, if the check found one */
	bool Launch();				/* Updater.exe --yes --restart; the game then exits */
}

#endif
