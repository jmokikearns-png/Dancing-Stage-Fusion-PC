/* dsfplatform shim for GameManager.h: the only game is the PS1 digital pad
 * ("ps1pad", defined in platform/src/dsf_ps1game.cpp). */
#ifndef GAMEMANAGER_H
#define GAMEMANAGER_H

class Game;

class GameManager
{
public:
	const Game* StringToGameType( CString sGameType );
};

extern GameManager* GAMEMAN;

#endif
