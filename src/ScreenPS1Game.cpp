#include "global.h"
#include "ScreenPS1Game.h"
#include "PS1Core.h"
#include "ScreenManager.h"
#include "ScreenDimensions.h"
#include "ThemeManager.h"
#include "InputMapper.h"
#include "GameInput.h"
#include "RageDisplay.h"
#include "RageTexture.h"
#include "RageLog.h"
#include "RageUtil.h"

REGISTER_SCREEN_CLASS( ScreenPS1Game );

/* ps1pad game buttons -> bits of the PlayStation digital pad word (which is
 * active-low: a pressed button reads 0). */
static const int PS1_PAD_BIT[NUM_PS1PAD_PS1_BUTTONS] =
{
	4,	/* Up */
	6,	/* Down */
	7,	/* Left */
	5,	/* Right */
	14,	/* Cross */
	13,	/* Circle */
	15,	/* Square */
	12,	/* Triangle */
	10,	/* L1 */
	8,	/* L2 */
	11,	/* R1 */
	9,	/* R2 */
	3,	/* Start */
	0,	/* Select */
};

ScreenPS1Game::ScreenPS1Game( CString sName ) : Screen( sName )
{
	m_bFailed = false;
}

void ScreenPS1Game::Init()
{
	Screen::Init();

	m_textMessage.LoadFromFont( THEME->GetPathF("Common","normal") );
	m_textMessage.SetXY( SCREEN_CENTER_X, SCREEN_CENTER_Y );
	m_textMessage.SetZoom( 0.7f );
	m_textMessage.SetHidden( true );
	this->AddChild( &m_textMessage );

	if( PS1CORE == NULL )
		PS1CORE = new PS1Core;

	CString sError;
	if( !PS1CORE->Start(sError) )
	{
		ShowError( sError );
		return;
	}
	SendPads();
	PS1CORE->SetRunning( true );
}

ScreenPS1Game::~ScreenPS1Game()
{
	/* Park the guest; it resumes where it was when this screen comes back. */
	if( PS1CORE )
	{
		PS1CORE->SetPads( 0xFFFF, 0xFFFF );
		PS1CORE->SetRunning( false );
	}
}

void ScreenPS1Game::ShowError( const CString &sError )
{
	LOG->Warn( "ScreenPS1Game: %s", sError.c_str() );
	m_bFailed = true;
	m_textMessage.SetText( sError + "\n\nPress Back (Esc) to return." );
	m_textMessage.SetHidden( false );
	if( PS1CORE )
		PS1CORE->SetRunning( false );
}

void ScreenPS1Game::SendPads()
{
	uint16_t aPads[2] = { 0xFFFF, 0xFFFF };
	if( PS1CORE->HoldGameInput() )	/* the game's KEY CONFIG is waiting for a key */
	{
		PS1CORE->SetPads( aPads[0], aPads[1] );
		return;
	}
	for( int c = 0; c < 2; ++c )
	{
		for( int b = 0; b < NUM_PS1PAD_PS1_BUTTONS; ++b )
		{
			if( INPUTMAPPER->IsButtonDown( GameInput((GameController) c, (GameButton) b) ) )
				aPads[c] &= ~(1 << PS1_PAD_BIT[b]);
		}
	}
	PS1CORE->SetPads( aPads[0], aPads[1] );
}

void ScreenPS1Game::Update( float fDeltaTime )
{
	Screen::Update( fDeltaTime );
	if( m_bFailed || PS1CORE == NULL )
		return;

	if( PS1CORE->TakeQuitRequest() )
	{
		SCREENMAN->SetNewScreen( "ScreenExit" );
		return;
	}
	if( PS1CORE->HasExited() )
	{
		ShowError( ssprintf("The game stopped (code %i).\nSee dsfcore.log in the game folder.", PS1CORE->GetExitCode()) );
		return;
	}

	/* AUTO TIMING's tap test runs while the game waits in its OPTIONS. */
	if( PS1CORE->CalibrationActive() )
	{
		m_textMessage.SetText( PS1CORE->CalibrationText() );
		m_textMessage.SetHidden( false );
	}
	else if( !m_textMessage.GetHidden() )
		m_textMessage.SetHidden( true );

	SendPads();
	PS1CORE->Update( fDeltaTime );
}

void ScreenPS1Game::DrawPrimitives()
{
	float fU = 1, fV = 1;
	RageTexture *pTex = (PS1CORE && !m_bFailed)? PS1CORE->GetFrameTexture( fU, fV ): NULL;
	if( pTex != NULL )
	{
		/* The PlayStation's picture is 4:3 whatever its pixel size; centre it,
		 * full height, in StepMania's screen. */
		const float fHeight = SCREEN_HEIGHT;
		const float fWidth = fHeight * 4.0f / 3.0f;
		const float fLeft = SCREEN_CENTER_X - fWidth/2, fRight = SCREEN_CENTER_X + fWidth/2;
		const float fTop = SCREEN_CENTER_Y - fHeight/2, fBottom = SCREEN_CENTER_Y + fHeight/2;

		RageSpriteVertex v[4];
		v[0].p = RageVector3( fLeft,  fTop,    0 );	v[0].t = RageVector2( 0,  0 );
		v[1].p = RageVector3( fLeft,  fBottom, 0 );	v[1].t = RageVector2( 0,  fV );
		v[2].p = RageVector3( fRight, fBottom, 0 );	v[2].t = RageVector2( fU, fV );
		v[3].p = RageVector3( fRight, fTop,    0 );	v[3].t = RageVector2( fU, 0 );
		for( int i = 0; i < 4; ++i )
			v[i].c = RageColor( 1, 1, 1, 1 );

		DISPLAY->SetTexture( 0, pTex );
		DISPLAY->SetTextureModeModulate();
		DISPLAY->SetTextureWrapping( false );
		DISPLAY->SetTextureFiltering( true );
		DISPLAY->SetBlendMode( BLEND_NORMAL );
		DISPLAY->SetCullMode( CULL_NONE );
		DISPLAY->SetZTestMode( ZTEST_OFF );
		DISPLAY->DrawQuad( v );
		DISPLAY->SetTexture( 0, NULL );
	}

	Screen::DrawPrimitives();
}

void ScreenPS1Game::Input( const DeviceInput& DeviceI, const InputEventType type, const GameInput &GameI, const MenuInput &MenuI, const StyleInput &StyleI )
{
	/* A key capture for the game's KEY CONFIG takes everything (Esc cancels). */
	if( PS1CORE && !m_bFailed && PS1CORE->CaptureInput(DeviceI, type) )
		return;

	if( PS1CORE && !m_bFailed && PS1CORE->CalibrationActive() )
	{
		if( type == IET_FIRST_PRESS && GameI.IsValid() )
			PS1CORE->CalibrationTap( DeviceI.ts.Ago() );
		return;
	}

	/* A pad button: held for the game's next read (sampled in Update too). */
	if( PS1CORE && !m_bFailed && GameI.IsValid() && GameI.controller < 2 && GameI.button < NUM_PS1PAD_PS1_BUTTONS &&
	    !PS1CORE->HoldGameInput() && (type == IET_FIRST_PRESS || type == IET_RELEASE) )
		PS1CORE->LatchPad( GameI.controller, PS1_PAD_BIT[GameI.button], type == IET_FIRST_PRESS );

	/* Everything belongs to the PlayStation (sampled in Update): the game
	 * has its own OPTIONS for the PC settings and QUIT on its main menu.  Back
	 * only leaves the error message when the game couldn't start. */
	if( type != IET_FIRST_PRESS || !GameI.IsValid() || GameI.button != PS1PAD_BUTTON_BACK )
		return;

	if( m_bFailed )
		SCREENMAN->SetNewScreen( "ScreenTitleMenu" );
}

void ScreenPS1Game::HandleScreenMessage( const ScreenMessage SM )
{
	Screen::HandleScreenMessage( SM );
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
