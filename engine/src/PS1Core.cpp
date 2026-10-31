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
#include <math.h>
#include <algorithm>

PS1Core *PS1CORE = NULL;

extern CString DirOfExecutable;	/* RageFileManager.cpp */

namespace
{
	const int SAMPLE_RATE = 44100;
	/* A PAL progressive frame is 680624 CPU cycles (49.761 Hz; the runtime's
	 * psx_video_timing.h), and a sound frame 768 cycles (44100 Hz). */
	const int64_t PAL_FRAME_CPU_CYCLES = 680624, CPU_CLOCK_HZ = 33868800;
	const int PAL_AUDIO_FRAMES_PER_VIDEO_FRAME = 886;	/* 680624 / 768 = 886.2 */
	const int MAX_QUEUED_FRAMES = 16;	/* ~320 ms of picture, more than any sound latency we use */
	/* Sound kept queued ahead of the device: one video frame (the guest
	 * produces it a frame at a time) plus the device's chunk of ~12 ms. */
	const int64_t AUDIO_QUEUE_TARGET = PAL_AUDIO_FRAMES_PER_VIDEO_FRAME + 512;

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
		/* The install folder: the program's own (dsfcore.dll, disc, Data, ...). */
		char buf[MAX_PATH] = "";
		GetModuleFileNameA( NULL, buf, sizeof(buf) );
		if( char *slash = strrchr(buf, '\\') )
			*slash = 0;
		return buf;
	}

	/* A setting for the core, read from its environment when it starts.
	 * dsfcore.dll uses the Universal C runtime, which keeps its own copy of
	 * the environment (taken when it first loads, and not updated by
	 * SetEnvironmentVariable): set the value there too, as well as in the
	 * process and in this program's runtime. */
	void SetCoreEnv( const char *sName, const CString &sValue )
	{
		SetEnvironmentVariableA( sName, sValue.c_str() );
		_putenv_s( sName, sValue.c_str() );
		typedef int (__cdecl *PutEnvS)( const char *, const char * );
		if( HMODULE hUcrt = LoadLibraryA("ucrtbase.dll") )
			if( PutEnvS pPut = (PutEnvS) GetProcAddress(hUcrt, "_putenv_s") )
				pPut( sName, sValue.c_str() );
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
	m_FrameLock( "PS1Core frames" )
{
	m_bStarted = false;
	m_bRunning = false;
	m_bQuitRequested = false;
	m_hModule = NULL;
	m_pfnState = m_pfnExitCode = NULL;
	m_iFramesProduced = 0;
	m_iShownSerial = -1;
	m_iShownContent = -1;
	m_iLastContent = -1;
	m_uRShift = m_uGShift = m_uBShift = m_uAShift = 0;
	m_pTexture = NULL;
	m_fU = m_fV = 1;
	m_hRunEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_hAudioEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	LARGE_INTEGER f;
	QueryPerformanceFrequency( &f );
	m_iTicksPerSecond = f.QuadPart;
	m_iLastMixTicks = 0;
	m_iClockNextTicks = 0;
	m_bClockPaced = 0;
	m_iGuestStartTicks = 0;
	m_iGuestTicks = m_iCaptureTicks = m_iTimedFrames = 0;
	m_iStatsGuestTicks = m_iStatsCaptureTicks = m_iStatsTimedFrames = 0;
	m_iRingFrames = 16384;	/* 370 ms */
	m_Ring.resize( m_iRingFrames * 2 );
	m_iAudioWritten = m_iAudioRead = 0;
	m_fQueueAverage = 0;
	m_iCalib = 0;
	m_iTaps = 0;
	m_iClicksSent = 0;
	m_iUnderruns = m_iOverruns = 0;
	m_fStatsSeconds = 0;
	m_iStatsFrames = m_iStatsAudio = 0;
	m_iPads = 0xFFFFFFFF;
	m_iPressedSince = m_iReleasedSince = 0;
	m_iLastReported = m_iPendingPress = 0;
	for( int i = 0; i < MAX_QUEUED_FRAMES + 2; ++i )
		m_Free.push_back( new Frame );
}

PS1Core::~PS1Core()
{
	/* The core thread may be parked in CoreFrameDone; it ends with the process. */
	RageSound_Generic_Software::SetDirectStream( NULL, NULL );
	delete m_pTexture;
}

bool PS1Core::CheckInstalled( CString &sError )
{
	const CString sRoot = RootDir();
	if( FindDiscImage(sRoot + "\\disc").empty() )
	{
		sError = "No disc image found.\n\nPut your Dancing Stage Fusion (Europe) disc image\n(.bin and .cue) in the \"disc\" folder, then run Setup.exe.";
		return false;
	}
	if( !FileExists(sRoot + "\\dsfcore.dll") )
	{
		sError = "Dancing Stage Fusion PC isn't set up yet.\n\nRun Setup.exe first: it builds the game\nfrom your Dancing Stage Fusion disc.";
		return false;
	}
	return true;
}

bool PS1Core::Start( CString &sError )
{
	if( m_bStarted )
	{
		if( HasExited() )
		{
			sError = ssprintf( "The game stopped (code %i). See Data\\dsfcore.log in the game folder.", GetExitCode() );
			return false;
		}
		return true;
	}

	const CString sRoot = RootDir();
	const CString sDll = sRoot + "\\dsfcore.dll";
	if( !FileExists(sDll) )
	{
		sError = "dsfcore.dll is missing.\n\nRun Setup.exe first: it builds the game\nfrom your own Dancing Stage Fusion disc.";
		return false;
	}
	const CString sDisc = FindDiscImage( sRoot + "\\disc" );
	if( sDisc.empty() )
	{
		sError = "No disc image found.\n\nPut your Dancing Stage Fusion (Europe) disc image\n(.bin and .cue) in the \"disc\" folder.";
		return false;
	}

	/* The core's files go in Data\, not beside the program: its overlay
	 * capture store (read from the environment when the core starts). */
	if( getenv("PSX_OVERLAY_CAPTURES") == NULL )
		SetCoreEnv( "PSX_OVERLAY_CAPTURES", sRoot + "\\Data\\overlay_captures.json" );
	if( getenv("PSX_CACHE_DIR") == NULL )
		SetCoreEnv( "PSX_CACHE_DIR", sRoot + "\\Data\\cache" );

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
	/* Graphic Options: OpenGL renders on a hidden context of the core's own
	 * (StepMania's context is untouched) and is read back each frame. */
	if( PREFSMAN->m_bPS1OpenGL )
	{
		vArgs.push_back( "--renderer" );	vArgs.push_back( "opengl" );
		vArgs.push_back( "--headless-opengl" );
	}
	else
	{
		vArgs.push_back( "--renderer" );	vArgs.push_back( "software" );
	}
	SetCoreEnv( "PSX_INTERNAL_RESOLUTION", PREFSMAN->m_sPS1InternalResolution.Get() );
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
	host.read_pads = CoreReadPads;
	host.get_setting = CoreGetSetting;
	host.set_setting = CoreSetSetting;
	host.commit_settings = CoreCommitSettings;
	host.get_mapping = CoreGetMapping;
	host.begin_mapping = CoreBeginMapping;
	host.mapping_state = CoreMappingState;
	host.clear_mapping = CoreClearMapping;
	host.quit = CoreQuit;
	host.measure_latency = CoreMeasureLatency;

	RageSound_Generic_Software::SetDirectStream( MixThunk, this );
	if( !RageSound_Generic_Software::DirectStreamSupported() )
		LOG->Warn( "PS1Core: the sound driver doesn't mix direct streams; the game will be silent and clock-paced" );

	LOG->Trace( "PS1Core: starting %s with disc \"%s\"", sDll.c_str(), sDisc.c_str() );
	const int iRet = pfnStart( &host, (int) vArgv.size(), &vArgv[0], (sRoot + "\\Data\\dsfcore.log").c_str() );
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

int64_t PS1Core::Now() const
{
	LARGE_INTEGER t;
	QueryPerformanceCounter( &t );
	return t.QuadPart;
}

void PS1Core::SetRunning( bool bRunning )
{
	m_bRunning = bRunning;
	if( bRunning )
	{
		/* Assume the device is alive until it has had time to prove otherwise. */
		AtomicStore( m_iLastMixTicks, Now() );
		m_iClockNextTicks = 0;
		SetEvent( (HANDLE) m_hRunEvent );
	}
	else
		ResetEvent( (HANDLE) m_hRunEvent );
}

void PS1Core::SetPads( uint16_t iPad1, uint16_t iPad2 )
{
	__atomic_store_n( &m_iPads, uint32_t(iPad1) | (uint32_t(iPad2) << 16), __ATOMIC_RELEASE );
}

void PS1Core::LatchPad( int iController, int iBit, bool bPressed )
{
	const uint32_t iMask = 1u << (iBit + 16 * iController);
	__atomic_fetch_or( bPressed ? &m_iPressedSince : &m_iReleasedSince, iMask, __ATOMIC_ACQ_REL );
}

/* What the game's pad read sees (active-low words): the buttons held now,
 * plus any pressed since the last read even if already let go (a quick step
 * between two reads, 40 ms apart at the game's 25 frames a second).  A
 * button the last read saw down that was let go and pressed again since
 * reads up now and down at the next read, so the game sees both steps. */
uint32_t PS1Core::TakePads()
{
	const uint32_t iHeld = ~__atomic_load_n( &m_iPads, __ATOMIC_ACQUIRE );
	const uint32_t iPressed = __atomic_exchange_n( &m_iPressedSince, 0, __ATOMIC_ACQ_REL );
	const uint32_t iReleased = __atomic_exchange_n( &m_iReleasedSince, 0, __ATOMIC_ACQ_REL );
	uint32_t iDown = iHeld | iPressed | m_iPendingPress;
	const uint32_t iAgain = m_iLastReported & iReleased & iPressed & ~m_iPendingPress;
	iDown &= ~iAgain;
	m_iPendingPress = iAgain;
	m_iLastReported = iDown;
	return ~iDown;
}

void PS1Core::Update( float fDeltaTime )
{
	if( !m_bStarted || !m_bRunning )
		return;

	m_Settings.Update( fDeltaTime );

	/* Pacing happens on the core thread (PaceNextFrame); this only reports. */
	m_fStatsSeconds += fDeltaTime;
	if( m_fStatsSeconds >= 10 )
	{
		int64_t iFrames;
		{
			LockMut( m_FrameLock );
			iFrames = m_iFramesProduced;
		}
		const int64_t iRead = AtomicLoad( m_iAudioRead );
		const int64_t iTimed = AtomicLoad( m_iTimedFrames ) - m_iStatsTimedFrames;
		const double fMsPerTick = 1000.0 / m_iTicksPerSecond;
		const double fGuestMs = iTimed? (AtomicLoad(m_iGuestTicks) - m_iStatsGuestTicks) * fMsPerTick / iTimed: 0;
		const double fCaptureMs = iTimed? (AtomicLoad(m_iCaptureTicks) - m_iStatsCaptureTicks) * fMsPerTick / iTimed: 0;
		LOG->Trace( "PS1Core: %.2f guest fps, %.0f audio frames/s, paced by %s; per frame: guest %.1f ms, picture hand-over %.1f ms; %s",
			(iFrames - m_iStatsFrames) / m_fStatsSeconds, (iRead - m_iStatsAudio) / m_fStatsSeconds,
			m_bClockPaced? "clock": "sound device", fGuestMs, fCaptureMs, GetStats().c_str() );
		m_iStatsFrames = iFrames;
		m_iStatsAudio = iRead;
		m_iStatsGuestTicks = AtomicLoad( m_iGuestTicks );
		m_iStatsCaptureTicks = AtomicLoad( m_iCaptureTicks );
		m_iStatsTimedFrames = AtomicLoad( m_iTimedFrames );
		m_fStatsSeconds = 0;
	}
}

/* Main thread: the stream position, in audio frames, being heard now. */
int64_t PS1Core::HeardAudioPosition() const
{
	const int64_t iRead = AtomicLoad( m_iAudioRead );
	const int64_t iLatency = int64_t( SOUNDMAN->GetPlayLatency() * SAMPLE_RATE );
	return iRead - iLatency;
}

/* Drop the oldest queued frame.  A repeat behind it that shows its picture
 * takes the pixels over, so no repeat is left without them. */
void PS1Core::FreeOldest()
{
	Frame *f = m_Ready.front();
	m_Ready.pop_front();
	if( !f->bRepeat && !m_Ready.empty() && m_Ready.front()->bRepeat && m_Ready.front()->iContent == f->iContent &&
	    f->iContent != m_iShownContent )
	{
		Frame *n = m_Ready.front();
		n->pixels.swap( f->pixels );
		n->iWidth = f->iWidth;
		n->iHeight = f->iHeight;
		n->bBlank = f->bBlank;
		n->bRepeat = false;
	}
	m_Free.push_back( f );
}

PS1Core::Frame *PS1Core::GetFreeFrame()
{
	if( m_Free.empty() )
	{
		ASSERT( !m_Ready.empty() );
		FreeOldest();
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
		/* Lowest lag (default): the newest frame, or one Picture Offset
		 * older when the offset is positive. */
		const int64_t iLimit = m_Ready.back()->iAudioEnd -
			int64_t( max(0, PREFSMAN->m_iPS1SyncOffsetMs.Get()) * (SAMPLE_RATE / 1000) );
		while( m_Ready.size() > 1 && m_Ready[1]->iAudioEnd <= iLimit )
			FreeOldest();
		return m_Ready.front();
	}

	/* The newest frame whose sound has started playing (plus the player's
	 * offset: positive shows pictures later).  A frame's own audio is the
	 * PAL_AUDIO_FRAMES_PER_VIDEO_FRAME frames before its iAudioEnd. */
	const int64_t iHeard = HeardAudioPosition() -
		int64_t( PREFSMAN->m_iPS1SyncOffsetMs * (SAMPLE_RATE / 1000) );
	while( m_Ready.size() > 1 &&
		   m_Ready[1]->iAudioEnd - PAL_AUDIO_FRAMES_PER_VIDEO_FRAME <= iHeard )
		FreeOldest();
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
			if( !f->bRepeat && f->iContent != m_iShownContent )
			{
				m_iShownContent = f->iContent;
				Upload( *f );
			}
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
	const int64_t iStart = p->Now();
	const bool bRepeat = (iFlags & DSFCORE_FRAME_REPEAT) != 0;
	const bool bBlank = !bRepeat && ((iFlags & DSFCORE_FRAME_BLANK) || pARGB == NULL || iWidth <= 0 || iHeight <= 0);

	/* Convert outside the lock into a buffer of our own, then queue it. */
	static vector<uint32_t> buf;
	if( !bBlank && !bRepeat )
	{
		buf.resize( size_t(iWidth) * iHeight );
		const uint32_t rs = p->m_uRShift, gs = p->m_uGShift, bs = p->m_uBShift, as = p->m_uAShift;
		const bool bRGBA = rs == 0 && gs == 8 && bs == 16 && as == 24;	/* the usual case: swap R and B */
		for( int y = 0; y < iHeight; ++y )
		{
			if( bRGBA )
			{
				const uint32_t *in = pARGB + size_t(y) * iPitch;
				uint32_t *out = &buf[size_t(y) * iWidth];
				for( int x = 0; x < iWidth; ++x )
				{
					const uint32_t c = in[x];
					out[x] = (c & 0x0000FF00) | ((c >> 16) & 0xFF) | ((c & 0xFF) << 16) | 0xFF000000;
				}
				continue;
			}
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
	f->iSerial = p->m_iFramesProduced++;
	f->bRepeat = bRepeat && p->m_iFramesProduced > 1;
	if( f->bRepeat )
		f->iContent = p->m_iLastContent;	/* no pixels: those of the frame it repeats */
	else
	{
		f->bBlank = bBlank;
		f->iWidth = bBlank ? 0 : iWidth;
		f->iHeight = bBlank ? 0 : iHeight;
		if( !bBlank )
			f->pixels.swap( buf );
		f->iContent = f->iSerial;
		p->m_iLastContent = f->iSerial;
	}
	f->iAudioEnd = AtomicLoad( p->m_iAudioWritten );
	p->m_Ready.push_back( f );
	(void) iScale;

	/* Timing: guest (incl. the runtime's own rendering) since the last frame
	 * was released, and this hand-over. */
	if( p->m_iGuestStartTicks != 0 )
	{
		AtomicStore( p->m_iGuestTicks, AtomicLoad(p->m_iGuestTicks) + (iStart - p->m_iGuestStartTicks) );
		AtomicStore( p->m_iCaptureTicks, AtomicLoad(p->m_iCaptureTicks) + (p->Now() - iStart) );
		AtomicStore( p->m_iTimedFrames, AtomicLoad(p->m_iTimedFrames) + 1 );
	}
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
	p->PaceNextFrame();
	const uint32_t iPads = __atomic_load_n( &p->m_iPads, __ATOMIC_ACQUIRE );
	aPadsOut[0] = uint16_t( iPads & 0xFFFF );
	aPadsOut[1] = uint16_t( iPads >> 16 );
	p->m_iGuestStartTicks = p->Now();
}

/* Called from inside the game's own pad-read routine (dsfcore_hooks.cpp). */
void PS1Core::CoreReadPads( void *pUser, uint16_t aPadsOut[2] )
{
	PS1Core *p = (PS1Core *) pUser;
	const uint32_t iPads = p->TakePads();
	aPadsOut[0] = uint16_t( iPads & 0xFFFF );
	aPadsOut[1] = uint16_t( iPads >> 16 );
}

int PS1Core::CoreGetSetting( void *pUser, int id, DsfCoreSetting *pOut )
{
	return ((PS1Core *) pUser)->m_Settings.Get( id, *pOut );
}

void PS1Core::CoreSetSetting( void *pUser, int id, int iChoice )
{
	((PS1Core *) pUser)->m_Settings.Set( id, iChoice );
}

void PS1Core::CoreCommitSettings( void *pUser )
{
	((PS1Core *) pUser)->m_Settings.Commit();
}

int PS1Core::CoreGetMapping( void *pUser, int iPlayer, int iButton, DsfCoreMapping *pOut )
{
	return ((PS1Core *) pUser)->m_Settings.GetMapping( iPlayer, iButton, *pOut );
}

void PS1Core::CoreBeginMapping( void *pUser, int iPlayer, int iButton )
{
	((PS1Core *) pUser)->m_Settings.BeginMapping( iPlayer, iButton );
}

int PS1Core::CoreMappingState( void *pUser )
{
	return ((PS1Core *) pUser)->m_Settings.MappingState();
}

void PS1Core::CoreClearMapping( void *pUser, int iPlayer, int iButton )
{
	((PS1Core *) pUser)->m_Settings.ClearMapping( iPlayer, iButton );
}

void PS1Core::CoreQuit( void *pUser )
{
	((PS1Core *) pUser)->m_bQuitRequested = true;
}

/* For AUTO TIMING.  Sound: the guest's sound waits in our queue (its
 * average) and then the device's latency.  Picture: a guest frame waits on
 * average half a refresh for the next draw, is put on screen at the next
 * refresh with vsync, and its middle is scanned out half a refresh later:
 * about two refreshes (one without vsync).  A screen's own processing and
 * the driver's frame queue are not known here. */
int PS1Core::CoreMeasureLatency( void *pUser, DsfCoreLatency *pOut )
{
	PS1Core *p = (PS1Core *) pUser;
	if( p->m_bClockPaced )
		return 0;	/* no sound device */
	const RageDisplay::VideoModeParams vp = DISPLAY->GetVideoModeParams();
	const int iRate = vp.rate > 20 && vp.rate < 500? vp.rate: 60;
	const double fQueue = p->m_fQueueAverage / SAMPLE_RATE;
	int iTapMs = 0, iTaps = 0;
	const bool bTapped = p->RunCalibration( iTapMs, iTaps );
	/* Tapped: what the device and the player add after a sound leaves the
	 * mixer, measured.  Else the device's own figure (its buffer only). */
	const double fAfterMix = bTapped? iTapMs / 1000.0: SOUNDMAN->GetPlayLatency();
	pOut->sound_ms = int( (fQueue + fAfterMix) * 1000 + 0.5 );
	pOut->picture_ms = int( (vp.vsync? 2.0: 1.0) * 1000.0 / iRate + 0.5 );
	pOut->refresh_hz = vp.rate > 20 && vp.rate < 500? vp.rate: 0;
	pOut->taps = bTapped? iTaps: 0;
	LOG->Trace( "PS1Core: AUTO TIMING: sound %i ms (queue %.1f ms, %s %.1f ms), picture %i ms (%i Hz, vsync %s)",
		pOut->sound_ms, fQueue * 1000.0, bTapped? ssprintf("tap test, %i taps,", iTaps).c_str(): "device estimate",
		fAfterMix * 1000.0, pOut->picture_ms, iRate, vp.vsync? "on": "off" );
	return 1;
}

/* The tap test, on the core thread (the game waits in its OPTIONS meanwhile).
 * Clicks go through the very path the game's sound takes -- this ring, the
 * mixer, the device -- every half second: CALIB_LEAD to get the beat, then
 * the rest, whose taps are timed against the moment the mixer handed the
 * click to the device.  What is left is the device's delay (speakers, TV,
 * headphones) and the player's own.  Returns false with too few taps. */
bool PS1Core::RunCalibration( int &iOffsetMsOut, int &iTapsOut )
{
	const int INTERVAL = SAMPLE_RATE / 2;
	const int CLICK = SAMPLE_RATE / 60;	/* ~17 ms */
	const int64_t iStart = AtomicLoad( m_iAudioWritten ) + SAMPLE_RATE;	/* a second to read the text */
	const int64_t iEnd = iStart + int64_t(CALIB_CLICKS) * INTERVAL + SAMPLE_RATE / 2;
	for( int k = 0; k < CALIB_CLICKS; ++k )
	{
		m_aClickFrame[k] = iStart + int64_t(k) * INTERVAL;
		AtomicStore( m_aClickTicks[k], 0 );
	}
	m_iTaps = 0;
	m_iClicksSent = 0;
	m_iCalib = 1;

	vector<int16_t> buf( 512 * 2 );
	for( ;; )
	{
		const int64_t iWritten = AtomicLoad( m_iAudioWritten );
		if( iWritten >= iEnd )
			break;
		if( !m_bRunning || Now() - AtomicLoad(m_iLastMixTicks) > m_iTicksPerSecond / 2 )
		{
			m_iCalib = 0;	/* the screen went away or the device stopped */
			return false;
		}
		if( iWritten - AtomicLoad(m_iAudioRead) >= AUDIO_QUEUE_TARGET )
		{
			WaitForSingleObject( (HANDLE) m_hAudioEvent, 5 );
			continue;
		}
		for( int i = 0; i < 512; ++i )
		{
			const int64_t f = iWritten + i;
			double v = 0;
			if( f >= iStart )
			{
				const int64_t k = (f - iStart) / INTERVAL, t = (f - iStart) % INTERVAL;
				if( k < CALIB_CLICKS && t < CLICK )
				{
					const double hz = k < CALIB_LEAD? 1320.0: 880.0;
					v = 0.45 * sin( 2 * M_PI * hz * t / SAMPLE_RATE ) * exp( -double(t) / (CLICK / 4.0) );
				}
			}
			buf[i*2] = buf[i*2+1] = int16_t( v * 32767 );
		}
		CoreAudio( this, &buf[0], 512 );
	}
	/* Let the last click play and its tap arrive. */
	const int64_t iWaitUntil = Now() + m_iTicksPerSecond * 3 / 4 + int64_t( m_fQueueAverage / SAMPLE_RATE * m_iTicksPerSecond );
	while( Now() < iWaitUntil )
		Sleep( 10 );
	m_iCalib = 0;

	/* Each measured click's nearest tap within a quarter beat. */
	vector<double> vOffsets;
	const int iTaps = min( (int) m_iTaps, (int) CALIB_MAX_TAPS );
	for( int k = CALIB_LEAD; k < CALIB_CLICKS; ++k )
	{
		const int64_t iClick = AtomicLoad( m_aClickTicks[k] );
		if( iClick == 0 )
			continue;
		double fBest = 1e9;
		for( int t = 0; t < iTaps; ++t )
		{
			const double d = double( m_aTapTicks[t] - iClick ) / m_iTicksPerSecond;
			if( fabs(d) < fabs(fBest) )
				fBest = d;
		}
		if( fabs(fBest) < 0.25 )
			vOffsets.push_back( fBest );
	}
	iTapsOut = (int) vOffsets.size();
	if( iTapsOut < (CALIB_CLICKS - CALIB_LEAD) / 2 )
	{
		LOG->Trace( "PS1Core: AUTO TIMING tap test: %i of %i taps on the beat, not enough", iTapsOut, CALIB_CLICKS - CALIB_LEAD );
		return false;
	}
	sort( vOffsets.begin(), vOffsets.end() );
	iOffsetMsOut = int( vOffsets[vOffsets.size() / 2] * 1000 + (vOffsets[vOffsets.size() / 2] >= 0? 0.5: -0.5) );
	LOG->Trace( "PS1Core: AUTO TIMING tap test: %i taps, median %i ms after the click left the mixer (spread %.0f .. %.0f ms)",
		iTapsOut, iOffsetMsOut, vOffsets.front() * 1000, vOffsets.back() * 1000 );
	return true;
}

void PS1Core::CalibrationTap( float fAgo )
{
	const int i = m_iTaps;
	if( !m_iCalib || i >= CALIB_MAX_TAPS )
		return;
	m_aTapTicks[i] = Now() - int64_t( fAgo * m_iTicksPerSecond );
	m_iTaps = i + 1;
}

CString PS1Core::CalibrationText() const
{
	int iHeard = 0;
	for( int k = 0; k < CALIB_CLICKS; ++k )
		if( m_aClickTicks[k] != 0 )
			++iHeard;
	if( iHeard < CALIB_LEAD )
		return ssprintf( "AUTO TIMING\n\nListen: tap any arrow on every click.\nThe first %i high clicks are the count-in.", (int) CALIB_LEAD );
	return ssprintf( "AUTO TIMING\n\nTap on every click.\n\n%i / %i", max(0, iHeard - CALIB_LEAD), CALIB_CLICKS - CALIB_LEAD );
}

/* Return when the next guest frame should run. */
void PS1Core::PaceNextFrame()
{
	for(;;)
	{
		/* Parked here while StepMania's menus are open. */
		WaitForSingleObject( (HANDLE) m_hRunEvent, INFINITE );

		const int64_t iNow = Now();
		const bool bDeviceAlive = iNow - AtomicLoad(m_iLastMixTicks) < m_iTicksPerSecond / 4;
		if( bDeviceAlive )
		{
			/* Audio master: run as soon as the queued sound drops below target. */
			m_bClockPaced = 0;
			if( AtomicLoad(m_iAudioWritten) - AtomicLoad(m_iAudioRead) < AUDIO_QUEUE_TARGET )
				return;
			WaitForSingleObject( (HANDLE) m_hAudioEvent, 20 );
			continue;
		}

		/* No sound device asking for sound: the PAL frame rate by the clock, and drop the
		 * sound nobody plays. */
		m_bClockPaced = 1;
		AtomicStore( m_iAudioRead, AtomicLoad(m_iAudioWritten) );
		const int64_t iPeriod = m_iTicksPerSecond * PAL_FRAME_CPU_CYCLES / CPU_CLOCK_HZ;
		if( m_iClockNextTicks == 0 || iNow - m_iClockNextTicks > m_iTicksPerSecond / 5 )
			m_iClockNextTicks = iNow;	/* start, or resync after a stall */
		if( iNow >= m_iClockNextTicks )
		{
			m_iClockNextTicks += iPeriod;
			return;
		}
		const DWORD iMs = DWORD( (m_iClockNextTicks - iNow) * 1000 / m_iTicksPerSecond );
		WaitForSingleObject( (HANDLE) m_hAudioEvent, max(iMs, DWORD(1)) );
	}
}

/* ---- sound mixer thread ---- */

int PS1Core::MixAudio( int16_t *pBuf, int iFrames )
{
	if( !m_bStarted || !m_bRunning )
		return 0;
	AtomicStore( m_iLastMixTicks, Now() );
	const int64_t iWritten = AtomicLoad( m_iAudioWritten );
	const int64_t iRead = AtomicLoad( m_iAudioRead );
	int iGot = (int) min( int64_t(iFrames), iWritten - iRead );
	if( iGot < iFrames )
		++m_iUnderruns;
	/* A sample waits behind what is queued when it is written: on average
	 * about the middle of the queue before and after this read. */
	const double fQueued = double(iWritten - iRead) - iGot / 2.0;
	m_fQueueAverage = m_fQueueAverage == 0? fQueued: m_fQueueAverage * 0.98 + fQueued * 0.02;
	if( m_iCalib )
	{
		/* Stamp the clicks this read hands to the device. */
		const int64_t iNow = Now();
		for( int k = 0; k < CALIB_CLICKS; ++k )
			if( m_aClickFrame[k] >= iRead && m_aClickFrame[k] < iRead + iGot && AtomicLoad(m_aClickTicks[k]) == 0 )
				AtomicStore( m_aClickTicks[k], iNow + (m_aClickFrame[k] - iRead) * m_iTicksPerSecond / SAMPLE_RATE );
	}
	for( int i = 0; i < iGot; ++i )
	{
		const int iPos = int( (iRead + i) % m_iRingFrames );
		pBuf[i*2+0] = m_Ring[iPos*2+0];
		pBuf[i*2+1] = m_Ring[iPos*2+1];
	}
	AtomicStore( m_iAudioRead, iRead + iGot );
	SetEvent( (HANDLE) m_hAudioEvent );
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
