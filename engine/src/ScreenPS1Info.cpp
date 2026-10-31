#include "global.h"
#include "ScreenPS1Info.h"
#include "ScreenManager.h"
#include "ScreenDimensions.h"
#include "ThemeManager.h"
#include "RageFile.h"
#include "RageLog.h"
#include "RageUtil.h"

REGISTER_SCREEN_CLASS( ScreenPS1Info );

/* Layout, in the theme's 640x480: inside the options page's panel (x 42-600,
 * y 52-452), topics in a column on the left, the text beside them. */
#define TOPICS_X		THEME->GetMetricF(m_sName,"TopicsX")
#define TOPICS_Y		THEME->GetMetricF(m_sName,"TopicsY")
#define TOPICS_SPACING_Y	THEME->GetMetricF(m_sName,"TopicsSpacingY")
#define TOPIC_ZOOM		THEME->GetMetricF(m_sName,"TopicZoom")
#define BODY_X			THEME->GetMetricF(m_sName,"BodyX")
#define BODY_Y			THEME->GetMetricF(m_sName,"BodyY")
#define BODY_WIDTH		THEME->GetMetricF(m_sName,"BodyWidth")
#define BODY_ZOOM		THEME->GetMetricF(m_sName,"BodyZoom")
static const RageColor TOPIC_COLOR( 1, 1, 1, 0.75f );
static const RageColor TOPIC_SELECTED_COLOR( 1, 1, 1, 1 );	/* on the orange cursor */

ScreenPS1Info::ScreenPS1Info( CString sName ) : ScreenWithMenuElements( sName )
{
	m_iSelected = 0;
	m_bOnce = m_bSkip = false;
}

ScreenPS1Info::~ScreenPS1Info()
{
	for( unsigned i = 0; i < m_Topics.size(); ++i )
	{
		this->RemoveChild( &m_Topics[i]->text );
		delete m_Topics[i];
	}
}

void ScreenPS1Info::LoadText()
{
	CString sPath = THEME->GetPathO( m_sName, "text" );
	RageFile f;
	if( !f.Open(sPath) )
	{
		LOG->Warn( "ScreenPS1Info: can't open %s: %s", sPath.c_str(), f.GetError().c_str() );
		return;
	}
	CString sLine;
	while( f.GetLine(sLine) > 0 )
	{
		TrimRight( sLine, "\r\n" );
		if( sLine.Left(1) == "#" )
			continue;
		if( sLine.Left(1) == "[" && sLine.Right(1) == "]" )
		{
			Topic *t = new Topic;
			t->sTitle = sLine.substr( 1, sLine.size()-2 );
			m_Topics.push_back( t );
			continue;
		}
		if( m_Topics.empty() )
			continue;
		CString &sBody = m_Topics.back()->sBody;
		if( !sBody.empty() )
			sBody += "\n";
		sBody += sLine;
	}
	for( unsigned i = 0; i < m_Topics.size(); ++i )
		TrimRight( m_Topics[i]->sBody, "\n" );
}

void ScreenPS1Info::Init()
{
	ScreenWithMenuElements::Init();

	m_bOnce = THEME->HasMetric( m_sName, "Once" ) && THEME->GetMetricB( m_sName, "Once" );
	m_bSkip = m_bOnce && DoesFileExist( SeenFlag() );

	m_sprPage.Load( THEME->GetPathG("ScreenOptions","page") );
	m_sprPage.SetXY( SCREEN_CENTER_X, SCREEN_CENTER_Y );
	this->AddChild( &m_sprPage );

	m_sprCursor.Load( THEME->GetPathG(m_sName,"cursor") );
	this->AddChild( &m_sprCursor );

	LoadText();
	for( unsigned i = 0; i < m_Topics.size(); ++i )
	{
		BitmapText &t = m_Topics[i]->text;
		t.LoadFromFont( THEME->GetPathF("Common","normal") );
		t.SetText( m_Topics[i]->sTitle );
		t.SetXY( TOPICS_X, TOPICS_Y + i*TOPICS_SPACING_Y );
		t.SetZoom( TOPIC_ZOOM );
		t.SetShadowLength( 0 );
		this->AddChild( &t );
	}

	m_textBody.LoadFromFont( THEME->GetPathF("Common","normal") );
	m_textBody.SetHorizAlign( Actor::align_left );
	m_textBody.SetVertAlign( Actor::align_top );
	m_textBody.SetXY( BODY_X, BODY_Y );
	m_textBody.SetZoom( BODY_ZOOM );
	m_textBody.SetShadowLength( 0 );
	this->AddChild( &m_textBody );

	m_soundChange.Load( THEME->GetPathS("ScreenOptions","change") );
	Select( 0 );
	this->SortByDrawOrder();
}

void ScreenPS1Info::Select( int iTopic )
{
	if( m_Topics.empty() )
		return;
	m_iSelected = clamp( iTopic, 0, (int) m_Topics.size()-1 );
	for( unsigned i = 0; i < m_Topics.size(); ++i )
		m_Topics[i]->text.SetDiffuse( (int) i == m_iSelected? TOPIC_SELECTED_COLOR: TOPIC_COLOR );
	m_sprCursor.StopTweening();
	m_sprCursor.BeginTweening( 0.1f, TWEEN_DECELERATE );
	m_sprCursor.SetXY( TOPICS_X, TOPICS_Y + m_iSelected*TOPICS_SPACING_Y );
	m_textBody.SetText( m_Topics[m_iSelected]->sBody, "", (int) (BODY_WIDTH / BODY_ZOOM) );
}

void ScreenPS1Info::Input( const DeviceInput& DeviceI, const InputEventType type, const GameInput &GameI, const MenuInput &MenuI, const StyleInput &StyleI )
{
	if( IsTransitioning() )
		return;
	Screen::Input( DeviceI, type, GameI, MenuI, StyleI );
}

void ScreenPS1Info::MenuUp( PlayerNumber pn, const InputEventType type )
{
	if( type == IET_RELEASE || m_iSelected == 0 )
		return;
	Select( m_iSelected-1 );
	m_soundChange.Play();
}

void ScreenPS1Info::MenuDown( PlayerNumber pn, const InputEventType type )
{
	if( type == IET_RELEASE || m_iSelected+1 >= (int) m_Topics.size() )
		return;
	Select( m_iSelected+1 );
	m_soundChange.Play();
}

void ScreenPS1Info::MenuStart( PlayerNumber pn )
{
	MenuBack( pn );
}

void ScreenPS1Info::Update( float fDeltaTime )
{
	ScreenWithMenuElements::Update( fDeltaTime );
	if( m_bSkip )
	{
		m_bSkip = false;
		HandleScreenMessage( SM_GoToNextScreen );
	}
}

void ScreenPS1Info::MenuBack( PlayerNumber pn )
{
	if( IsTransitioning() )
		return;
	if( m_bOnce )
	{
		RageFile f;
		if( f.Open(SeenFlag(), RageFile::WRITE) )
			f.PutLine( "The welcome screen has been shown; delete this file to see it again." );
	}
	SCREENMAN->PlayStartSound();
	StartTransitioning( SM_GoToPrevScreen );
}

void ScreenPS1Info::HandleScreenMessage( const ScreenMessage SM )
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
