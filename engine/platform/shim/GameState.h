/* dsfplatform shim for GameState.h.
 * The real GameState is StepMania's whole game/session state (songs, steps,
 * players, stages, Lua...).  The input stack only asks it for the current Game
 * (button set) and Style (column layout), so this keeps exactly those two. */
#ifndef GAMESTATE_H
#define GAMESTATE_H

class Game;
class Style;

class GameState
{
public:
	GameState(): m_pCurGame(0), m_pCurStyle(0) { }
	const Game*	m_pCurGame;
	const Style*	m_pCurStyle;	/* always NULL in dsfplatform: no StepMania styles */

	const Game*	GetCurrentGame() { return m_pCurGame; }
	const Style*	GetCurrentStyle() const { return m_pCurStyle; }
};

extern GameState* GAMESTATE;

#endif
