/* ScreenPS1Achievements - the title screen's ACHIEVEMENTS (DsfAchievements):
 * every achievement in a list on the left, the chosen one's description and
 * how far along it is on the right.  It looks like INFO. */

#ifndef SCREEN_PS1_ACHIEVEMENTS_H
#define SCREEN_PS1_ACHIEVEMENTS_H

#include "ScreenWithMenuElements.h"
#include "BitmapText.h"
#include "Sprite.h"
#include "RageSound.h"

class ScreenPS1Achievements : public ScreenWithMenuElements
{
public:
	ScreenPS1Achievements( CString sName );
	virtual ~ScreenPS1Achievements();
	virtual void Init();

	virtual void Input( const DeviceInput& DeviceI, const InputEventType type, const GameInput &GameI, const MenuInput &MenuI, const StyleInput &StyleI );
	virtual void HandleScreenMessage( const ScreenMessage SM );

	virtual void MenuUp( PlayerNumber pn, const InputEventType type );
	virtual void MenuDown( PlayerNumber pn, const InputEventType type );
	virtual void MenuStart( PlayerNumber pn );
	virtual void MenuBack( PlayerNumber pn );

private:
	void Select( int iChoice );

	vector<BitmapText*> m_Rows;	/* the visible part of the list */
	int m_iSelected, m_iTop;
	Sprite m_sprPage, m_sprCursor;
	BitmapText m_textName, m_textBody, m_textStatus, m_textTotal;
	RageSound m_soundChange;
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
