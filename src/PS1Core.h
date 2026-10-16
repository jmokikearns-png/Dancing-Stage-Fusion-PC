/* PS1Core - the original Dancing Stage Fusion, run inside StepMania.
 *
 * The game is the player's own PS1 program, recompiled into dsfcore.dll (see
 * port/core/dsfcore_api.h).  This class loads that DLL, boots it on its own
 * thread and connects it to StepMania:
 *
 *  - Sound: the PlayStation's SPU output goes straight into the sound
 *    device's mix (RageSound_Generic_Software direct stream), with no
 *    decode-ahead buffering.
 *  - Pacing: the guest only runs as fast as the sound device plays its audio
 *    ("audio master"), so game time can never drift away from the music.
 *    Without a working sound device it is paced by the clock at 50 Hz.
 *  - Picture: each guest frame is queued with the position of its audio.  By
 *    default the frame shown is the one whose sound is being heard now, so
 *    arrows and music stay in sync whatever the audio latency; the player
 *    can add a fine offset.
 *  - Input: pad words built from StepMania's InputMapper ("ps1pad" game).
 *
 * The core is started once per program run.  Leaving the game screen parks
 * the guest at its next VBlank; coming back resumes it.
 */

#ifndef PS1_CORE_H
#define PS1_CORE_H

#include "RageThreads.h"
#include <deque>

class RageTexture;
class PS1FrameTexture;

class PS1Core
{
public:
	PS1Core();
	~PS1Core();

	/* Load dsfcore.dll and boot the PlayStation (only the first call does
	 * anything).  Returns false, with a message for the player, when the core
	 * or the disc image is missing or the boot failed. */
	bool Start( CString &sErrorOut );
	bool IsStarted() const { return m_bStarted; }
	bool HasExited() const;
	int GetExitCode() const;

	/* Let the guest run, or park it at its next VBlank (menus open). */
	void SetRunning( bool bRunning );
	bool IsRunning() const { return m_bRunning; }

	/* Main thread, every StepMania update while the game screen is up. */
	void SetPads( uint16_t iPad1, uint16_t iPad2 );
	void Update( float fDeltaTime );

	/* Main thread: the frame to show, already uploaded.  NULL until the
	 * first frame.  fU/fV: the image's extent in texture coordinates. */
	RageTexture *GetFrameTexture( float &fUOut, float &fVOut );

	/* Diagnostics for the screen's status line and the log. */
	CString GetStats() const;

	/* Sound device mixer thread (RageSound_Generic_Software). */
	int MixAudio( int16_t *pBuf, int iFrames );

private:
	/* Core thread callbacks (see dsfcore_api.h). */
	static void CoreVideo( void *pUser, const uint32_t *pARGB, int iWidth, int iHeight, int iPitch, int iScale, int iFlags );
	static void CoreAudio( void *pUser, const int16_t *pStereo, int iFrames );
	static void CoreFrameDone( void *pUser, uint16_t aPadsOut[2] );

	struct Frame
	{
		vector<uint32_t> pixels;	/* in the display's FMT_RGBA8 layout */
		int iWidth, iHeight;
		bool bBlank;
		int64_t iAudioEnd;	/* audio frames produced when this frame was finished */
		int64_t iSerial;
	};
	Frame *GetFreeFrame();		/* m_FrameLock held */
	const Frame *PickFrameToShow();	/* m_FrameLock held */
	void Upload( const Frame &f );
	int64_t HeardAudioPosition() const;
	void Grant( int iFrames );

	bool m_bStarted;
	bool m_bRunning;
	void *m_hModule;
	void *m_pfnState, *m_pfnExitCode;

	/* Frame hand-over: the core thread fills, the main thread shows. */
	RageMutex m_FrameLock;
	std::deque<Frame*> m_Ready;	/* oldest first */
	vector<Frame*> m_Free;
	int64_t m_iFramesProduced;
	int64_t m_iShownSerial;
	uint32_t m_uRShift, m_uGShift, m_uBShift, m_uAShift;

	PS1FrameTexture *m_pTexture;
	float m_fU, m_fV;

	/* Pacing: one semaphore count lets the guest run one frame. */
	RageSemaphore m_Grants;
	float m_fClockFrames;		/* clock-paced fallback accumulator */
	float m_fSecondsSinceAudioRead;
	int64_t m_iLastAudioRead;

	/* SPU output ring: written by the core thread, read by the mixer. */
	vector<int16_t> m_Ring;		/* interleaved stereo */
	int m_iRingFrames;
	volatile int64_t m_iAudioWritten;	/* frames ever written */
	volatile int64_t m_iAudioRead;		/* frames ever read */
	int64_t m_iUnderruns, m_iOverruns;
	float m_fStatsSeconds;		/* log a pacing summary every few seconds */
	int64_t m_iStatsFrames, m_iStatsAudio;

	volatile uint32_t m_iPads;	/* port 1 in the low half, port 2 in the high half */
};

extern PS1Core *PS1CORE;	/* created on first use by ScreenPS1Game */

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
