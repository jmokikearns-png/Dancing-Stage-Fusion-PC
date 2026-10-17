/* ScreenPS1Info - the title screen's INFO: how the port works, its PC
 * settings and controls, credits and licences.  It looks like the options
 * screens (header, page, footer); topics on the left, the chosen topic's
 * text on the right.  The text is the theme's "Other/ScreenPS1Info text.txt"
 * ([TOPIC] lines start a topic). */

#ifndef SCREEN_PS1_INFO_H
#define SCREEN_PS1_INFO_H

#include "ScreenWithMenuElements.h"
#include "BitmapText.h"
#include "Sprite.h"
#include "RageSound.h"

class ScreenPS1Info : public ScreenWithMenuElements
{
public:
	ScreenPS1Info( CString sName );
	virtual ~ScreenPS1Info();
	virtual void Init();

	virtual void Input( const DeviceInput& DeviceI, const InputEventType type, const GameInput &GameI, const MenuInput &MenuI, const StyleInput &StyleI );
	virtual void HandleScreenMessage( const ScreenMessage SM );

	virtual void MenuUp( PlayerNumber pn, const InputEventType type );
	virtual void MenuDown( PlayerNumber pn, const InputEventType type );
	virtual void MenuStart( PlayerNumber pn );
	virtual void MenuBack( PlayerNumber pn );

private:
	void LoadText();
	void Select( int iTopic );

	struct Topic
	{
		CString sTitle, sBody;
		BitmapText text;
	};
	vector<Topic*> m_Topics;
	int m_iSelected;
	Sprite m_sprPage, m_sprCursor;
	BitmapText m_textBody;
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
