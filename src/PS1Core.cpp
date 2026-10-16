#include "global.h"
#include "PS1Core.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageDisplay.h"
#include "RageSurface.h"
#include "RageTexture.h"
#include "RageSoundManager.h"
#include "PrefsManager.h"
#include "arch/Sound/RageSoundDriver_Generic_Software.h"

#include "dsfcore_api.h"	/* keep in sync with port/core/dsfcore_api.h */

#include <windows.h>

PS1Core *PS1CORE = NULL;

extern CString DirOfExecutable;	/* RageFileManager.cpp */

namespace
{
	const int SAMPLE_RATE = 44100;
	const int PAL_AUDIO_FRAMES_PER_VIDEO_FRAME = 882;	/* 44100 / 50: exact for the PS1's PAL timing */
	const int MAX_QUEUED_FRAMES = 16;	/* ~320 ms of picture, more than any sound latency we use */
	const int MAX_GRANTS_AHEAD = 2;

	int64_t AtomicLoad( const volatile int64_t &v ) { return __atomic_load_n( &v, __ATOMIC_ACQUIRE ); }
	void AtomicStore( volatile int64_t &v, int64_t x ) { __atomic_store_n( &v, x, __ATOMIC_RELEASE ); }

	int MixThunk( void *pUser, int16_t *pBuf, int iFrames )
	{
		return ((PS1Core *) pUser)->MixAudio( pBuf, iFrames );
	}

	CString FindDiscImage( const CString &sDir )
	{
		/* Prefer a .cue (it names the track files); a lone .bin works too. */
		const char *aPatterns[] = { "*.cue", "*.bin", "*.img", "*.iso" };
		for( unsigned i = 0; i < ARRAYSIZE(aPatterns); ++i )
		{
			WIN32_FIND_DATAA fd;
			HANDLE h = FindFirstFileA( (sDir + "\\" + aPatterns[i]).c_str(), &fd );
			if( h == INVALID_HANDLE_VALUE )
				continue;
			CString sFound = sDir + "\\" + fd.cFileName;
			FindClose( h );
			return sFound;
		}
		return "";
	}

	bool FileExists( const CString &sPath )
	{
		const DWORD a = GetFileAttributesA( sPath.c_str() );
		return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
	}

	CString RootDir()
	{
		/* StepMania's working directory is the install folder (Themes, Data, ...). */
		char buf[MAX_PATH] = "";
		GetCurrentDirectoryA( sizeof(buf), buf );
		return buf;
	}

	int ShiftOf( uint32_t mask )
	{
		int i = 0;
		while( mask && !(mask & 1) ) { mask >>= 1; ++i; }
		return i;
	}
}

/* The guest picture as a StepMania texture: a power-of-two OpenGL texture
 * whose top-left corner holds the current frame. */
class PS1FrameTexture: public RageTexture
{
public:
	PS1FrameTexture(): RageTexture( RageTextureID("PS1 frame") )
	{
		m_uHandle = 0;
		m_iSourceWidth = m_iSourceHeight = 0;
		m_iTextureWidth = m_iTextureHeight = 0;
		m_iImageWidth = m_iImageHeight = 0;
		m_iFramesWide = m_iFramesHigh = 1;
	}
	~PS1FrameTexture() { Destroy(); }
	unsigned GetTexHandle() const { return m_uHandle; }
	/* The display was reset (e.g. Alt+Enter): the handle is gone; the next
	 * upload creates a new texture. */
	void Invalidate() { m_uHandle = 0; m_iTextureWidth = m_iTextureHeight = 0; }

	void Upload( const uint32_t *pPixels, int iWidth, int iHeight )
	{
		if( m_uHandle == 0 || iWidth > m_iTextureWidth || iHeight > m_iTextureHeight )
		{
			Destroy();
			m_iTextureWidth = power_of_two( iWidth );
			m_iTextureHeight = power_of_two( iHeight );
			RageSurface *pBlank = MakeSurface( NULL, m_iTextureWidth, m_iTextureHeight );
			memset( pBlank->pixels, 0, pBlank->pitch * pBlank->h );
			m_uHandle = DISPLAY->CreateTexture( RageDisplay::FMT_RGBA8, pBlank, false );
			delete pBlank;
		}
		m_iSourceWidth = m_iImageWidth = iWidth;
		m_iSourceHeight = m_iImageHeight = iHeight;
		RageSurface *pImg = MakeSurface( pPixels, iWidth, iHeight );
		DISPLAY->UpdateTexture( m_uHandle, pImg, 0, 0, iWidth, iHeight );
		pImg->pixels = NULL;	/* not ours */
		delete pImg;
	}

private:
	static RageSurface *MakeSurface( const uint32_t *pPixels, int iWidth, int iHeight )
	{
		const RageDisplay::PixelFormatDesc *pf = DISPLAY->GetPixelFormatDesc( RageDisplay::FMT_RGBA8 );
		if( pPixels == NULL )
			return CreateSurface( iWidth, iHeight, 32, pf->masks[0], pf->masks[1], pf->masks[2], pf->masks[3] );
		return CreateSurfaceFrom( iWidth, iHeight, 32, pf->masks[0], pf->masks[1], pf->masks[2], pf->masks[3],
			(uint8_t *) pPixels, iWidth * 4 );
	}
	void Destroy()
	{
		if( m_uHandle )
			DISPLAY->DeleteTexture( m_uHandle );
		m_uHandle = 0;
	}
	unsigned m_uHandle;
};

PS1Core::PS1Core():
	m_FrameLock( "PS1Core frames" ),
	m_Grants( "PS1Core grants", 0 )
{
	m_bStarted = false;
	m_bRunning = false;
	m_hModule = NULL;
	m_pfnState = m_pfnExitCode = NULL;
	m_iFramesProduced = 0;
	m_iShownSerial = -1;
	m_uRShift = m_uGShift = m_uBShift = m_uAShift = 0;
	m_pTexture = NULL;
	m_fU = m_fV = 1;
	m_fClockFrames = 0;
	m_fSecondsSinceAudioRead = 0;
	m_iLastAudioRead = 0;
	m_iRingFrames = 16384;	/* 370 ms */
	m_Ring.resize( m_iRingFrames * 2 );
	m_iAudioWritten = m_iAudioRead = 0;
	m_iUnderruns = m_iOverruns = 0;
	m_fStatsSeconds = 0;
	m_iStatsFrames = m_iStatsAudio = 0;
	m_iPads = 0xFFFFFFFF;
	for( int i = 0; i < MAX_QUEUED_FRAMES + 2; ++i )
		m_Free.push_back( new Frame );
}

PS1Core::~PS1Core()
{
	/* The core thread may be parked in CoreFrameDone; it ends with the process. */
	RageSound_Generic_Software::SetDirectStream( NULL, NULL );
	delete m_pTexture;
}

bool PS1Core::Start( CString &sError )
{
	if( m_bStarted )
	{
		if( HasExited() )
		{
			sError = ssprintf( "The game stopped (code %i). See dsfcore.log in the game folder.", GetExitCode() );
			return false;
		}
		return true;
	}

	const CString sRoot = RootDir();
	const CString sDll = sRoot + "\\dsfcore.dll";
	if( !FileExists(sDll) )
	{
		sError = "dsfcore.dll is missing.\n\nIt is built from your own Dancing Stage Fusion disc;\nsee README.txt.";
		return false;
	}
	const CString sDisc = FindDiscImage( sRoot + "\\disc" );
	if( sDisc.empty() )
	{
		sError = "No disc image found.\n\nPut your Dancing Stage Fusion (Europe) disc image\n(.bin and .cue) in the \"disc\" folder.";
		return false;
	}

	HMODULE h = LoadLibraryA( sDll.c_str() );
	if( h == NULL )
	{
		sError = werr_ssprintf( GetLastError(), "Couldn't load dsfcore.dll" );
		return false;
	}
	dsfcore_start_fn pfnStart = (dsfcore_start_fn) GetProcAddress( h, "dsfcore_start" );
	m_pfnState = (void *) GetProcAddress( h, "dsfcore_state" );
	m_pfnExitCode = (void *) GetProcAddress( h, "dsfcore_exit_code" );
	if( pfnStart == NULL || m_pfnState == NULL || m_pfnExitCode == NULL )
	{
		sError = "dsfcore.dll is not a Dancing Stage Fusion PC core (wrong version?).";
		FreeLibrary( h );
		return false;
	}
	m_hModule = h;

	/* Pixel layout of the display's RGBA8 textures. */
	const RageDisplay::PixelFormatDesc *pf = DISPLAY->GetPixelFormatDesc( RageDisplay::FMT_RGBA8 );
	m_uRShift = ShiftOf( pf->masks[0] );
	m_uGShift = ShiftOf( pf->masks[1] );
	m_uBShift = ShiftOf( pf->masks[2] );
	m_uAShift = ShiftOf( pf->masks[3] );

	CreateDirectoryA( (sRoot + "\\saves").c_str(), NULL );
	vector<CString> vArgs;
	vArgs.push_back( "dsfcore" );
	vArgs.push_back( "--game" );		vArgs.push_back( sRoot + "\\game.toml" );
	vArgs.push_back( "--disc" );		vArgs.push_back( sDisc );
	vArgs.push_back( "--memcard-dir" );	vArgs.push_back( sRoot + "\\saves" );
	if( FileExists(sRoot + "\\bios\\openbios.bin") )
	{
		vArgs.push_back( "--bios" );	vArgs.push_back( sRoot + "\\bios\\openbios.bin" );
	}
	vector<const char *> vArgv;
	for( unsigned i = 0; i < vArgs.size(); ++i )
		vArgv.push_back( vArgs[i].c_str() );

	DsfCoreHost host;
	memset( &host, 0, sizeof(host) );
	host.struct_size = sizeof(host);
	host.api_version = DSFCORE_API_VERSION;
	host.user = this;
	host.video = CoreVideo;
	host.audio = CoreAudio;
	host.frame_done = CoreFrameDone;

	RageSound_Generic_Software::SetDirectStream( MixThunk, this );
	if( !RageSound_Generic_Software::DirectStreamSupported() )
		LOG->Warn( "PS1Core: the sound driver doesn't mix direct streams; the game will be silent and clock-paced" );

	LOG->Trace( "PS1Core: starting %s with disc \"%s\"", sDll.c_str(), sDisc.c_str() );
	const int iRet = pfnStart( &host, (int) vArgv.size(), &vArgv[0], (sRoot + "\\dsfcore.log").c_str() );
	if( iRet != 0 )
	{
		sError = ssprintf( "dsfcore.dll refused to start (code %i).", iRet );
		return false;
	}
	m_bStarted = true;
	return true;
}

bool PS1Core::HasExited() const
{
	return m_pfnState && ((dsfcore_state_fn) m_pfnState)() == DSFCORE_STATE_EXITED;
}

int PS1Core::GetExitCode() const
{
	return m_pfnExitCode ? ((dsfcore_exit_code_fn) m_pfnExitCode)() : 0;
}

void PS1Core::SetRunning( bool bRunning )
{
	m_bRunning = bRunning;
	m_fClockFrames = 0;
	m_fSecondsSinceAudioRead = 0;
}

void PS1Core::SetPads( uint16_t iPad1, uint16_t iPad2 )
{
	__atomic_store_n( &m_iPads, uint32_t(iPad1) | (uint32_t(iPad2) << 16), __ATOMIC_RELEASE );
}

void PS1Core::Grant( int iFrames )
{
	for( int i = 0; i < iFrames; ++i )
		m_Grants.Post();
}

void PS1Core::Update( float fDeltaTime )
{
	if( !m_bStarted || !m_bRunning )
		return;

	const int64_t iWritten = AtomicLoad( m_iAudioWritten );
	const int64_t iRead = AtomicLoad( m_iAudioRead );
	if( iRead != m_iLastAudioRead )
	{
		m_iLastAudioRead = iRead;
		m_fSecondsSinceAudioRead = 0;
	}
	else
		m_fSecondsSinceAudioRead += fDeltaTime;

	const int iPending = m_Grants.GetValue();
	const bool bAudioMaster = m_fSecondsSinceAudioRead < 0.25f;

	m_fStatsSeconds += fDeltaTime;
	if( m_fStatsSeconds >= 10 )
	{
		int64_t iFrames;
		{
			LockMut( m_FrameLock );
			iFrames = m_iFramesProduced;
		}
		LOG->Trace( "PS1Core: %.2f guest fps, %.0f audio frames/s, paced by %s; %s",
			(iFrames - m_iStatsFrames) / m_fStatsSeconds, (iRead - m_iStatsAudio) / m_fStatsSeconds,
			bAudioMaster? "sound device": "clock", GetStats().c_str() );
		m_iStatsFrames = iFrames;
		m_iStatsAudio = iRead;
		m_fStatsSeconds = 0;
	}

	if( bAudioMaster )
	{
		/* Audio master: keep about two video frames of sound queued ahead of
		 * the device (it pulls in chunks of up to ~12 ms). */
		const int64_t iTarget = PAL_AUDIO_FRAMES_PER_VIDEO_FRAME * 2;
		int64_t iQueued = (iWritten - iRead) + int64_t(iPending) * PAL_AUDIO_FRAMES_PER_VIDEO_FRAME;
		int iGrant = 0;
		while( iQueued < iTarget && iPending + iGrant < MAX_GRANTS_AHEAD )
		{
			++iGrant;
			iQueued += PAL_AUDIO_FRAMES_PER_VIDEO_FRAME;
		}
		Grant( iGrant );
		m_fClockFrames = 0;
	}
	else
	{
		/* No sound device consuming audio: run at the PAL rate by the clock. */
		m_fClockFrames += fDeltaTime * 50.0f;
		int iGrant = (int) m_fClockFrames;
		if( iPending + iGrant > MAX_GRANTS_AHEAD )
			iGrant = max( 0, MAX_GRANTS_AHEAD - iPending );
		m_fClockFrames -= (int) m_fClockFrames;
		Grant( iGrant );
		/* Discard audio nobody will play. */
		AtomicStore( m_iAudioRead, iWritten );
	}
}

/* Main thread: the stream position, in audio frames, being heard now. */
int64_t PS1Core::HeardAudioPosition() const
{
	const int64_t iRead = AtomicLoad( m_iAudioRead );
	const int64_t iLatency = int64_t( SOUNDMAN->GetPlayLatency() * SAMPLE_RATE );
	return iRead - iLatency;
}

PS1Core::Frame *PS1Core::GetFreeFrame()
{
	if( m_Free.empty() )
	{
		/* Drop the oldest queued frame. */
		ASSERT( !m_Ready.empty() );
		m_Free.push_back( m_Ready.front() );
		m_Ready.pop_front();
	}
	Frame *f = m_Free.back();
	m_Free.pop_back();
	return f;
}

const PS1Core::Frame *PS1Core::PickFrameToShow()
{
	if( m_Ready.empty() )
		return NULL;

	if( !PREFSMAN->m_bPS1MatchPictureToSound )
	{
		while( m_Ready.size() > 1 )
		{
			m_Free.push_back( m_Ready.front() );
			m_Ready.pop_front();
		}
		return m_Ready.back();
	}

	/* The newest frame whose sound has started playing (plus the player's
	 * offset: positive shows pictures later).  A frame's own audio is the
	 * PAL_AUDIO_FRAMES_PER_VIDEO_FRAME frames before its iAudioEnd. */
	const int64_t iHeard = HeardAudioPosition() -
		int64_t( PREFSMAN->m_iPS1SyncOffsetMs * (SAMPLE_RATE / 1000) );
	while( m_Ready.size() > 1 &&
		   m_Ready[1]->iAudioEnd - PAL_AUDIO_FRAMES_PER_VIDEO_FRAME <= iHeard )
	{
		m_Free.push_back( m_Ready.front() );
		m_Ready.pop_front();
	}
	return m_Ready.front();
}

RageTexture *PS1Core::GetFrameTexture( float &fU, float &fV )
{
	{
		LockMut( m_FrameLock );
		const Frame *f = PickFrameToShow();
		if( f != NULL && f->iSerial != m_iShownSerial )
		{
			m_iShownSerial = f->iSerial;
			Upload( *f );
		}
	}
	fU = m_fU;
	fV = m_fV;
	return m_pTexture != NULL && m_pTexture->GetTexHandle() != 0 ? m_pTexture : NULL;
}

void PS1Core::Upload( const Frame &f )
{
	if( m_pTexture == NULL )
		m_pTexture = new PS1FrameTexture;
	if( f.bBlank || f.iWidth <= 0 || f.iHeight <= 0 )
	{
		static const uint32_t black[4] = { 0, 0, 0, 0 };
		uint32_t px[4];
		for( int i = 0; i < 4; ++i )
			px[i] = black[i] | (0xFFu << m_uAShift);
		m_pTexture->Upload( px, 2, 2 );
	}
	else
		m_pTexture->Upload( &f.pixels[0], f.iWidth, f.iHeight );
	m_fU = float( m_pTexture->GetImageWidth() ) / m_pTexture->GetTextureWidth();
	m_fV = float( m_pTexture->GetImageHeight() ) / m_pTexture->GetTextureHeight();
}

CString PS1Core::GetStats() const
{
	return ssprintf( "audio queued %i, latency %.0f ms, underruns %i, overruns %i",
		int( AtomicLoad(m_iAudioWritten) - AtomicLoad(m_iAudioRead) ),
		SOUNDMAN->GetPlayLatency() * 1000.0f, int(m_iUnderruns), int(m_iOverruns) );
}

/* ---- core thread ---- */

void PS1Core::CoreVideo( void *pUser, const uint32_t *pARGB, int iWidth, int iHeight, int iPitch, int iScale, int iFlags )
{
	PS1Core *p = (PS1Core *) pUser;
	const bool bBlank = (iFlags & DSFCORE_FRAME_BLANK) || pARGB == NULL || iWidth <= 0 || iHeight <= 0;

	/* Convert outside the lock into a buffer of our own, then queue it. */
	static vector<uint32_t> buf;
	if( !bBlank )
	{
		buf.resize( size_t(iWidth) * iHeight );
		const uint32_t rs = p->m_uRShift, gs = p->m_uGShift, bs = p->m_uBShift, as = p->m_uAShift;
		for( int y = 0; y < iHeight; ++y )
		{
			const uint32_t *in = pARGB + size_t(y) * iPitch;
			uint32_t *out = &buf[size_t(y) * iWidth];
			for( int x = 0; x < iWidth; ++x )
			{
				const uint32_t c = in[x];
				out[x] = (((c >> 16) & 0xFF) << rs) | (((c >> 8) & 0xFF) << gs) |
					((c & 0xFF) << bs) | (0xFFu << as);
			}
		}
	}

	LockMut( p->m_FrameLock );
	Frame *f = p->GetFreeFrame();
	f->bBlank = bBlank;
	f->iWidth = bBlank ? 0 : iWidth;
	f->iHeight = bBlank ? 0 : iHeight;
	if( !bBlank )
		f->pixels.swap( buf );
	f->iAudioEnd = AtomicLoad( p->m_iAudioWritten );
	f->iSerial = p->m_iFramesProduced++;
	p->m_Ready.push_back( f );
	(void) iScale;
}

void PS1Core::CoreAudio( void *pUser, const int16_t *pStereo, int iFrames )
{
	PS1Core *p = (PS1Core *) pUser;
	const int64_t iWritten = AtomicLoad( p->m_iAudioWritten );
	const int64_t iRead = AtomicLoad( p->m_iAudioRead );
	const int iSpace = p->m_iRingFrames - int( iWritten - iRead );
	if( iFrames > iSpace )
	{
		++p->m_iOverruns;
		iFrames = max( iSpace, 0 );
	}
	for( int i = 0; i < iFrames; ++i )
	{
		const int iPos = int( (iWritten + i) % p->m_iRingFrames );
		p->m_Ring[iPos*2+0] = pStereo[i*2+0];
		p->m_Ring[iPos*2+1] = pStereo[i*2+1];
	}
	AtomicStore( p->m_iAudioWritten, iWritten + iFrames );
}

void PS1Core::CoreFrameDone( void *pUser, uint16_t aPadsOut[2] )
{
	PS1Core *p = (PS1Core *) pUser;
	p->m_Grants.Wait( false );	/* parked here while menus are open */
	const uint32_t iPads = __atomic_load_n( &p->m_iPads, __ATOMIC_ACQUIRE );
	aPadsOut[0] = uint16_t( iPads & 0xFFFF );
	aPadsOut[1] = uint16_t( iPads >> 16 );
}

/* ---- sound mixer thread ---- */

int PS1Core::MixAudio( int16_t *pBuf, int iFrames )
{
	if( !m_bStarted || !m_bRunning )
		return 0;
	const int64_t iWritten = AtomicLoad( m_iAudioWritten );
	const int64_t iRead = AtomicLoad( m_iAudioRead );
	int iGot = (int) min( int64_t(iFrames), iWritten - iRead );
	if( iGot < iFrames )
		++m_iUnderruns;
	for( int i = 0; i < iGot; ++i )
	{
		const int iPos = int( (iRead + i) % m_iRingFrames );
		pBuf[i*2+0] = m_Ring[iPos*2+0];
		pBuf[i*2+1] = m_Ring[iPos*2+1];
	}
	AtomicStore( m_iAudioRead, iRead + iGot );
	return iGot;
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
