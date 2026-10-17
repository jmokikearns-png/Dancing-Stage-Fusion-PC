#include "global.h"
#include "PS1Settings.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageSoundManager.h"
#include "PrefsManager.h"
#include "ScreenOptionsMasterPrefs.h"
#include "StepMania.h"

namespace
{
	struct SettingDef
	{
		int id;
		const char *sConfOption;
		const char *sDefault;	/* the choice that is the default */
		bool bAppliesLater;	/* read once when the game starts */
	};
	const SettingDef g_Defs[] =
	{
		{ DSFCORE_SETTING_VOLUME,		"SoundVolume",			"100%",		false },
		{ DSFCORE_SETTING_AUDIO_BUFFER,		"SoundWriteAhead",		"AUTO",		true },
		{ DSFCORE_SETTING_SYNC_PICTURE,		"PS1MatchPictureToSound",	"OFF",		false },
		{ DSFCORE_SETTING_PICTURE_OFFSET,	"PS1SyncOffset",		"0 ms",		false },
		{ DSFCORE_SETTING_SCREEN_MODE,		"Windowed",			"WINDOWED",	false },
		{ DSFCORE_SETTING_RESOLUTION,		"DisplayResolution",		"960x720",	false },
		{ DSFCORE_SETTING_REFRESH_RATE,		"RefreshRate",			"DEFAULT",	false },
		{ DSFCORE_SETTING_VSYNC,		"Vsync",			"YES",		false },
		{ DSFCORE_SETTING_RENDER_QUALITY,	"PS1InternalResolution",	"1080p",	true },
		{ DSFCORE_SETTING_RENDERER,		"PS1OpenGL",			"OPENGL",	true },
	};

	/* DisplayResolution's first choices are named by width only. */
	CString ChoiceText( int id, const CString &s )
	{
		if( id != DSFCORE_SETTING_RESOLUTION )
			return s;
		const char *aOld[][2] = { {"320","320x240"}, {"400","400x300"}, {"512","512x384"},
			{"640","640x480"}, {"800","800x600"}, {"1024","1024x768"} };
		for( unsigned i = 0; i < ARRAYSIZE(aOld); ++i )
			if( s == aOld[i][0] )
				return aOld[i][1];
		return s;
	}

	/* The choice that matches the current preference. */
	int CurrentChoice( int id, const ConfOption *pOpt, int iNumChoices )
	{
		if( id == DSFCORE_SETTING_VOLUME && PREFSMAN->m_fSoundVolume.Get() == -1 )
			return iNumChoices - 1;	/* -1 = the default volume, 100% */
		return clamp( pOpt->Get(), 0, iNumChoices - 1 );
	}
}

PS1Settings::PS1Settings():
	m_Lock( "PS1Settings" )
{
	for( int i = 0; i < DSFCORE_SETTING_COUNT; ++i )
		m_Rows[i].bHave = false;
	m_bCommit = false;
	m_iPendingEffects = 0;
	m_fRefreshSeconds = 0;
}

int PS1Settings::Get( int id, DsfCoreSetting &out )
{
	LockMut( m_Lock );
	if( id < 0 || id >= DSFCORE_SETTING_COUNT || !m_Rows[id].bHave )
		return 0;
	out = m_Rows[id].info;
	return 1;
}

void PS1Settings::Set( int id, int choice )
{
	LockMut( m_Lock );
	if( id < 0 || id >= DSFCORE_SETTING_COUNT || !m_Rows[id].bHave )
		return;
	Row &r = m_Rows[id];
	if( choice < 0 || choice >= (int) r.choices.size() )
		return;
	m_Requests.push_back( make_pair(id, choice) );
	/* Show it at once; the main thread applies it on its next update. */
	r.info.choice = choice;
	snprintf( r.info.text, sizeof(r.info.text), "%s", r.choices[choice].c_str() );
}

void PS1Settings::Commit()
{
	LockMut( m_Lock );
	m_bCommit = true;
}

void PS1Settings::Update( float fDeltaTime )
{
	vector< pair<int, int> > vRequests;
	bool bCommit;
	{
		LockMut( m_Lock );
		vRequests.swap( m_Requests );
		bCommit = m_bCommit;
		m_bCommit = false;
	}

	for( unsigned i = 0; i < vRequests.size(); ++i )
	{
		const SettingDef *pDef = NULL;
		for( unsigned d = 0; d < ARRAYSIZE(g_Defs); ++d )
			if( g_Defs[d].id == vRequests[i].first )
				pDef = &g_Defs[d];
		ConfOption *pOpt = pDef? ConfOption::Find( pDef->sConfOption ): NULL;
		if( pOpt == NULL )
			continue;
		pOpt->UpdateAvailableOptions();
		pOpt->Put( vRequests[i].second );
		const int iEffects = pOpt->GetEffects();
		LOG->Trace( "PS1Settings: %s = choice %i (from the game's OPTIONS)", pDef->sConfOption, vRequests[i].second );
		if( iEffects & OPT_APPLY_SOUND )
			SOUNDMAN->SetPrefs( PREFSMAN->GetSoundVolume() );
		m_iPendingEffects |= iEffects & ~OPT_APPLY_SOUND;
	}

	if( bCommit && m_iPendingEffects )
	{
		if( m_iPendingEffects & OPT_APPLY_GRAPHICS )
			ApplyGraphicOptions( true );	/* no StepMania message over the game */
		if( m_iPendingEffects & OPT_SAVE_PREFERENCES )
			PREFSMAN->SaveGlobalPrefsToDisk();
		m_iPendingEffects = 0;
	}

	/* Keep the snapshot fresh (choices can change, e.g. after a display
	 * change), but not every frame. */
	m_fRefreshSeconds -= fDeltaTime;
	if( !vRequests.empty() || bCommit || m_fRefreshSeconds <= 0 )
	{
		LockMut( m_Lock );
		if( m_Requests.empty() )	/* don't undo a change queued meanwhile */
			Refresh();
		m_fRefreshSeconds = 0.5f;
	}
}

void PS1Settings::Refresh()
{
	for( unsigned d = 0; d < ARRAYSIZE(g_Defs); ++d )
	{
		const SettingDef &def = g_Defs[d];
		Row &r = m_Rows[def.id];
		ConfOption *pOpt = ConfOption::Find( def.sConfOption );
		r.bHave = false;
		if( pOpt == NULL )
			continue;
		pOpt->UpdateAvailableOptions();
		CStringArray asChoices;
		pOpt->MakeOptionsList( asChoices );
		if( asChoices.empty() )
			continue;
		r.choices.clear();
		for( unsigned c = 0; c < asChoices.size(); ++c )
			r.choices.push_back( ChoiceText(def.id, asChoices[c]) );
		DsfCoreSetting &info = r.info;
		info.num_choices = (int) r.choices.size();
		info.choice = CurrentChoice( def.id, pOpt, info.num_choices );
		info.default_choice = -1;
		for( unsigned c = 0; c < asChoices.size(); ++c )
			if( asChoices[c] == def.sDefault || r.choices[c] == def.sDefault )
				info.default_choice = (int) c;
		info.applies_later = def.bAppliesLater? 1: 0;
		snprintf( info.text, sizeof(info.text), "%s", r.choices[info.choice].c_str() );
		r.bHave = true;
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
