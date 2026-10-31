/* The only StepMania "Game" in dsfplatform: the PS1 digital pad (SCPH-1080),
 * two controllers, 14 buttons.  Button names are what Keymaps.ini uses
 * ("Controller1_Cross=Key_x,..."), so they must stay stable. */
#include "global.h"
#include "Game.h"
#include "GameManager.h"
#include "RageInputDevice.h"
#include "dsfplatform.h"

Game g_Ps1PadGame =
{
	"ps1pad",					// m_szName
	"PlayStation digital pad",			// m_szDescription
	2,						// m_iNumControllers
	false,						// m_bCountNotesSeparately
	dsf::NUM_PS1_BUTTONS,				// m_iButtonsPerController
	{	// m_szButtonNames (order == dsf::Ps1Button)
		"Up", "Down", "Left", "Right",
		"Cross", "Circle", "Square", "Triangle",
		"L1", "L2", "R1", "R2",
		"Start", "Select",
	},
	{	// m_DedicatedMenuButton: none (menus are the PS1 game's own)
		GAME_BUTTON_INVALID, GAME_BUTTON_INVALID, GAME_BUTTON_INVALID,
		GAME_BUTTON_INVALID, GAME_BUTTON_INVALID, GAME_BUTTON_INVALID,
		GAME_BUTTON_INVALID, GAME_BUTTON_INVALID, GAME_BUTTON_INVALID,
	},
	{	// m_SecondaryMenuButton (only used by StepMania's MenuInput, unused here)
		dsf::PS1_LEFT,	// MENU_BUTTON_LEFT
		dsf::PS1_RIGHT,	// MENU_BUTTON_RIGHT
		dsf::PS1_UP,	// MENU_BUTTON_UP
		dsf::PS1_DOWN,	// MENU_BUTTON_DOWN
		dsf::PS1_START,	// MENU_BUTTON_START
		dsf::PS1_SELECT,	// MENU_BUTTON_SELECT
		dsf::PS1_TRIANGLE,	// MENU_BUTTON_BACK
		GAME_BUTTON_INVALID,	// MENU_BUTTON_COIN
		GAME_BUTTON_INVALID,	// MENU_BUTTON_OPERATOR
	},
	{	// m_iDefaultKeyboardKey (InputMapper puts these in slot 2 if the key is unused)
		{	// pad 1
			KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
			KEY_Cx, KEY_Cc, KEY_Cz, KEY_Cs,		// Cross Circle Square Triangle
			KEY_Cq, KEY_C1, KEY_Ce, KEY_C3,		// L1 L2 R1 R2
			KEY_ENTER, KEY_BACK,			// Start Select
		},
		{	// pad 2
			KEY_KP_C8, KEY_KP_C2, KEY_KP_C4, KEY_KP_C6,
			KEY_KP_C0, KEY_KP_PERIOD, KEY_KP_C1, KEY_KP_C3,
			KEY_KP_C7, KEY_KP_SLASH, KEY_KP_C9, KEY_KP_ASTERISK,
			KEY_KP_ENTER, KEY_KP_PLUS,
		},
	},
	TNS_MARVELOUS, TNS_PERFECT, TNS_GREAT, TNS_GOOD, TNS_BOO,	// unused score remaps
};

const Game* GameManager::StringToGameType( CString sGameType )
{
	if( !sGameType.CompareNoCase(g_Ps1PadGame.m_szName) )
		return &g_Ps1PadGame;
	return NULL;
}
