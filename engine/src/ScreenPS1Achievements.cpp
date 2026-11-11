#include "global.h"
#include "ScreenPS1Achievements.h"
#include "DsfAchievements.h"
#include "ScreenManager.h"
#include "ScreenDimensions.h"
#include "ThemeManager.h"
#include "RageUtil.h"

REGISTER_SCREEN_CLASS( ScreenPS1Achievements );

/* Layout, in the theme's 640x480: inside the options page's panel (x 42-600,
 * y 52-452), the list in a column on the left, the chosen one beside it. */
#define LIST_X			THEME->GetMetricF(m_sName,"ListX")
#define LIST_Y			THEME->GetMetricF(m_sName,"ListY")
#define LIST_SPACING_Y		THEME->GetMetricF(m_sName,"ListSpacingY")
#define LIST_ROWS		THEME->GetMetricI(m_sName,"ListRows")
#define LIST_ZOOM		THEME->GetMetricF(m_sName,"ListZoom")
#define LIST_WIDTH		THEME->GetMetricF(m_sName,"ListWidth")
#define BODY_X			THEME->GetMetricF(m_sName,"BodyX")
#define BODY_Y			THEME->GetMetricF(m_sName,"BodyY")
#define BODY_WIDTH		THEME->GetMetricF(m_sName,"BodyWidth")
#define BODY_ZOOM		THEME->GetMetricF(m_sName,"BodyZoom")
#define NAME_ZOOM		THEME->GetMetricF(m_sName,"NameZoom")
static const RageColor EARNED_COLOR( 1, 0.85f, 0.3f, 1 );	/* the headers' gold */
static const RageColor LOCKED_COLOR( 1, 1, 1, 0.45f );
static const RageColor SELECTED_EARNED_COLOR( 1, 1, 1, 1 );	/* on the orange cursor */
static const RageColor SELECTED_LOCKED_COLOR( 1, 1, 1, 0.8f );

ScreenPS1Achievements::ScreenPS1Achievements( CString sName ) : ScreenWithMenuElements( sName )
{
	m_iSelected = 0;
	m_iTop = 0;
}

ScreenPS1Achievements::~ScreenPS1Achievements()
{
	for( unsigned i = 0; i < m_Rows.size(); ++i )
	{
		this->RemoveChild( m_Rows[i] );
		delete m_Rows[i];
	}
}

void ScreenPS1Achievements::Init()
{
	ScreenWithMenuElements::Init();

	m_sprPage.Load( THEME->GetPathG("ScreenOptions","page") );
	m_sprPage.SetXY( SCREEN_CENTER_X, SCREEN_CENTER_Y );
	this->AddChild( &m_sprPage );

	m_sprCursor.Load( THEME->GetPathG("ScreenPS1Info","cursor") );
	this->AddChild( &m_sprCursor );

	const int iRows = min( LIST_ROWS, DsfAchievements::GetCount() );
	for( int i = 0; i < iRows; ++i )
	{
		BitmapText *t = new BitmapText;
		t->LoadFromFont( THEME->GetPathF("Common","normal") );
		t->SetXY( LIST_X, LIST_Y + i*LIST_SPACING_Y );
		t->SetZoom( LIST_ZOOM );
		t->SetMaxWidth( LIST_WIDTH / LIST_ZOOM );
		t->SetShadowLength( 0 );
		m_Rows.push_back( t );
		this->AddChild( t );
	}

	m_textName.LoadFromFont( THEME->GetPathF("Common","normal") );
	m_textName.SetHorizAlign( Actor::align_left );
	m_textName.SetVertAlign( Actor::align_top );
	m_textName.SetXY( BODY_X, BODY_Y );
	m_textName.SetZoom( NAME_ZOOM );
	m_textName.SetMaxWidth( BODY_WIDTH / NAME_ZOOM );
	m_textName.SetShadowLength( 0 );
	this->AddChild( &m_textName );

	m_textBody.LoadFromFont( THEME->GetPathF("Common","normal") );
	m_textBody.SetHorizAlign( Actor::align_left );
	m_textBody.SetVertAlign( Actor::align_top );
	m_textBody.SetXY( BODY_X, BODY_Y + 36 );
	m_textBody.SetZoom( BODY_ZOOM );
	m_textBody.SetShadowLength( 0 );
	this->AddChild( &m_textBody );

	m_textStatus.LoadFromFont( THEME->GetPathF("Common","normal") );
	m_textStatus.SetHorizAlign( Actor::align_left );
	m_textStatus.SetVertAlign( Actor::align_top );
	m_textStatus.SetXY( BODY_X, BODY_Y + 120 );
	m_textStatus.SetZoom( BODY_ZOOM );
	m_textStatus.SetShadowLength( 0 );
	this->AddChild( &m_textStatus );

	m_textTotal.LoadFromFont( THEME->GetPathF("Common","normal") );
	m_textTotal.SetHorizAlign( Actor::align_left );
	m_textTotal.SetVertAlign( Actor::align_top );
	m_textTotal.SetXY( BODY_X, LIST_Y + (iRows-1)*LIST_SPACING_Y - 8 );
	m_textTotal.SetZoom( BODY_ZOOM );
	m_textTotal.SetShadowLength( 0 );
	m_textTotal.SetDiffuse( RageColor(1,1,1,0.75f) );
	m_textTotal.SetText( ssprintf("%d of %d earned", DsfAchievements::GetEarnedCount(), DsfAchievements::GetCount()) );
	this->AddChild( &m_textTotal );

	m_soundChange.Load( THEME->GetPathS("ScreenOptions","change") );
	Select( 0 );
	this->SortByDrawOrder();
}

void ScreenPS1Achievements::Select( int iChoice )
{
	const int iCount = DsfAchievements::GetCount();
	const int iRows = (int) m_Rows.size();
	if( iCount == 0 || iRows == 0 )
		return;
	m_iSelected = clamp( iChoice, 0, iCount-1 );
	/* keep the choice inside the visible rows, one row from the edge while the list goes on */
	if( m_iSelected < m_iTop + 1 )
		m_iTop = max( 0, m_iSelected - 1 );
	if( m_iSelected > m_iTop + iRows - 2 )
		m_iTop = min( iCount - iRows, m_iSelected - iRows + 2 );
	m_iTop = clamp( m_iTop, 0, max(0, iCount - iRows) );

	for( int r = 0; r < iRows; ++r )
	{
		const int i = m_iTop + r;
		DsfAchievements::Info info;
		DsfAchievements::GetInfo( i, info );
		CString sText = info.sName;
		if( r == 0 && m_iTop > 0 )
			sText = "...";
		else if( r == iRows-1 && i < iCount-1 )
			sText = "...";
		m_Rows[r]->SetText( sText );
		const bool bSelected = i == m_iSelected;
		m_Rows[r]->SetDiffuse( info.bEarned? (bSelected? SELECTED_EARNED_COLOR: EARNED_COLOR):
			(bSelected? SELECTED_LOCKED_COLOR: LOCKED_COLOR) );
	}

	m_sprCursor.StopTweening();
	m_sprCursor.BeginTweening( 0.1f, TWEEN_DECELERATE );
	m_sprCursor.SetXY( LIST_X, LIST_Y + (m_iSelected - m_iTop)*LIST_SPACING_Y );

	DsfAchievements::Info info;
	DsfAchievements::GetInfo( m_iSelected, info );
	m_textName.SetText( info.sName );
	m_textName.SetDiffuse( info.bEarned? EARNED_COLOR: RageColor(1,1,1,1) );
	m_textBody.SetText( info.sDescription, "", (int) (BODY_WIDTH / BODY_ZOOM) );
	CString sStatus;
	if( info.bEarned )
		sStatus = "Earned " + info.sDate;
	else if( info.iTarget > 0 )
		sStatus = ssprintf( "Not earned yet: %d of %d", info.iProgress, info.iTarget );
	else
		sStatus = "Not earned yet";
	m_textStatus.SetText( sStatus );
	m_textStatus.SetDiffuse( info.bEarned? EARNED_COLOR: RageColor(1,1,1,0.75f) );
}

void ScreenPS1Achievements::Input( const DeviceInput& DeviceI, const InputEventType type, const GameInput &GameI, const MenuInput &MenuI, const StyleInput &StyleI )
{
	if( IsTransitioning() )
		return;
	Screen::Input( DeviceI, type, GameI, MenuI, StyleI );
}

void ScreenPS1Achievements::MenuUp( PlayerNumber pn, const InputEventType type )
{
	if( type == IET_RELEASE || m_iSelected == 0 )
		return;
	Select( m_iSelected-1 );
	m_soundChange.Play();
}

void ScreenPS1Achievements::MenuDown( PlayerNumber pn, const InputEventType type )
{
	if( type == IET_RELEASE || m_iSelected+1 >= DsfAchievements::GetCount() )
		return;
	Select( m_iSelected+1 );
	m_soundChange.Play();
}

void ScreenPS1Achievements::MenuStart( PlayerNumber pn )
{
	MenuBack( pn );
}

void ScreenPS1Achievements::MenuBack( PlayerNumber pn )
{
	if( IsTransitioning() )
		return;
	SCREENMAN->PlayStartSound();
	StartTransitioning( SM_GoToPrevScreen );
}

void ScreenPS1Achievements::HandleScreenMessage( const ScreenMessage SM )
{
	switch( SM )
	{
	case SM_GoToNextScreen:
	case SM_GoToPrevScreen:
		SCREENMAN->SetNewScreen( THEME->GetMetric(m_sName,"PrevScreen") );
		break;
	}
}

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
