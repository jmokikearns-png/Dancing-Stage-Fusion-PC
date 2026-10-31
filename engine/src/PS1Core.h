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
 *    The core thread does this itself (it waits whenever enough sound is
 *    queued), so StepMania's own frame rate doesn't matter.  Without a
 *    working sound device it is paced by the clock at the PAL frame rate
 *    (49.761 Hz).
 *  - Picture: each guest frame is queued with the position of its audio.  By
 *    default the newest frame is shown (lowest lag: steps are judged by the
 *    game against what the player sees).  "Match Picture to Sound" instead
 *    shows the frame whose sound is being heard, which syncs arrows with the
 *    music but delays the picture by the sound latency; Picture Offset
 *    delays (or, when matching, shifts) the picture by a few milliseconds.
 *  - Input: pad words built from StepMania's InputMapper ("ps1pad" game).
 *    The game's own pad-read routine is hooked (port/core/dsfcore_hooks.cpp)
 *    and takes them from here when it runs, instead of from the emulated
 *    controller port.
 *  - Settings: the game's own OPTIONS screens get rows for StepMania's
 *    sound and display settings (port/core/dsfcore_menus.cpp), answered by
 *    PS1Settings with StepMania's option rows.
 *
 * The core is started once per program run.  Leaving the game screen parks
 * the guest at its next VBlank; coming back resumes it.
 */

#ifndef PS1_CORE_H
#define PS1_CORE_H

#include "RageThreads.h"
#include "PS1Settings.h"
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
	/* Before anything else starts: false, with a message for the player,
	 * when the game isn't set up (no disc image, or Setup.exe not run). */
	static bool CheckInstalled( CString &sErrorOut );
	bool IsStarted() const { return m_bStarted; }
	bool HasExited() const;
	/* Main thread: true once after the player chose QUIT on the game's main menu. */
	bool TakeQuitRequest() { bool b = m_bQuitRequested; m_bQuitRequested = false; return b; }
	int GetExitCode() const;

	/* Let the guest run, or park it at its next VBlank (menus open). */
	void SetRunning( bool bRunning );
	bool IsRunning() const { return m_bRunning; }

	/* Main thread, every StepMania update while the game screen is up. */
	void SetPads( uint16_t iPad1, uint16_t iPad2 );
	/* Main thread, from input events: a button pressed or released, kept
	 * until the game's next pad read so no step is lost between reads. */
	void LatchPad( int iController, int iBit, bool bPressed );
	/* Main thread: the game's KEY CONFIG capture (see PS1Settings). */
	bool CaptureInput( const DeviceInput &DeviceI, InputEventType type ) { return m_Settings.CaptureInput( DeviceI, type ); }
	bool HoldGameInput() { return m_Settings.HoldGameInput(); }
	void Update( float fDeltaTime );

	/* Main thread: the frame to show, already uploaded.  NULL until the
	 * first frame.  fU/fV: the image's extent in texture coordinates. */
	RageTexture *GetFrameTexture( float &fUOut, float &fVOut );

	/* AUTO TIMING's tap test (CoreMeasureLatency): the screen shows
	 * CalibrationText() while it runs and hands it the player's presses.
	 * fAgo: how long ago the press happened, in seconds. */
	bool CalibrationActive() const { return m_iCalib != 0; }
	void CalibrationTap( float fAgo );
	CString CalibrationText() const;

	/* Diagnostics for the screen's status line and the log. */
	CString GetStats() const;

	/* Sound device mixer thread (RageSound_Generic_Software). */
	int MixAudio( int16_t *pBuf, int iFrames );

private:
	/* Core thread callbacks (see dsfcore_api.h). */
	static void CoreVideo( void *pUser, const uint32_t *pARGB, int iWidth, int iHeight, int iPitch, int iScale, int iFlags );
	static void CoreAudio( void *pUser, const int16_t *pStereo, int iFrames );
	static void CoreFrameDone( void *pUser, uint16_t aPadsOut[2] );
	static void CoreReadPads( void *pUser, uint16_t aPadsOut[2] );
	static int CoreGetSetting( void *pUser, int id, DsfCoreSetting *pOut );
	static void CoreSetSetting( void *pUser, int id, int iChoice );
	static void CoreCommitSettings( void *pUser );
	static int CoreGetMapping( void *pUser, int iPlayer, int iButton, DsfCoreMapping *pOut );
	static void CoreBeginMapping( void *pUser, int iPlayer, int iButton );
	static int CoreMappingState( void *pUser );
	static void CoreClearMapping( void *pUser, int iPlayer, int iButton );
	static void CoreQuit( void *pUser );
	static int CoreMeasureLatency( void *pUser, DsfCoreLatency *pOut );

	struct Frame
	{
		vector<uint32_t> pixels;	/* in the display's FMT_RGBA8 layout */
		int iWidth, iHeight;
		bool bBlank;
		bool bRepeat;		/* shows the picture of frame iContent, pixels not here */
		int64_t iContent;	/* the frame whose picture this shows */
		int64_t iAudioEnd;	/* audio frames produced when this frame was finished */
		int64_t iSerial;
	};
	Frame *GetFreeFrame();		/* m_FrameLock held */
	void FreeOldest();		/* m_FrameLock held */
	const Frame *PickFrameToShow();	/* m_FrameLock held */
	void Upload( const Frame &f );
	int64_t HeardAudioPosition() const;
	void PaceNextFrame();		/* core thread */
	int64_t Now() const;		/* QueryPerformanceCounter ticks */

	bool m_bStarted;
	bool m_bRunning;
	volatile bool m_bQuitRequested;	/* set on the core thread */
	void *m_hModule;
	void *m_pfnState, *m_pfnExitCode;

	/* Frame hand-over: the core thread fills, the main thread shows. */
	RageMutex m_FrameLock;
	std::deque<Frame*> m_Ready;	/* oldest first */
	vector<Frame*> m_Free;
	int64_t m_iFramesProduced;
	int64_t m_iShownSerial;
	int64_t m_iShownContent;	/* the picture in the texture now */
	int64_t m_iLastContent;		/* core thread: the newest frame with pixels */
	uint32_t m_uRShift, m_uGShift, m_uBShift, m_uAShift;

	PS1FrameTexture *m_pTexture;
	float m_fU, m_fV;

	/* Pacing (core thread).  Run: set while the game screen is up. */
	void *m_hRunEvent;		/* manual-reset: the guest may run */
	void *m_hAudioEvent;		/* auto-reset: the mixer took some sound */
	int64_t m_iTicksPerSecond;
	volatile int64_t m_iLastMixTicks;	/* when the sound device last asked for sound */
	int64_t m_iClockNextTicks;	/* clock pacing: when the next frame is due */
	volatile int m_bClockPaced;
	/* Where the core thread's time goes (stats). */
	int64_t m_iGuestStartTicks;
	volatile int64_t m_iGuestTicks, m_iCaptureTicks, m_iTimedFrames;

	/* SPU output ring: written by the core thread, read by the mixer. */
	vector<int16_t> m_Ring;		/* interleaved stereo */
	int m_iRingFrames;
	volatile int64_t m_iAudioWritten;	/* frames ever written */
	volatile int64_t m_iAudioRead;		/* frames ever read */
	volatile double m_fQueueAverage;	/* sound queued ahead of the device, frames (mixer thread) */
	int64_t m_iUnderruns, m_iOverruns;
	float m_fStatsSeconds;		/* log a pacing summary every few seconds */
	int64_t m_iStatsFrames, m_iStatsAudio;
	int64_t m_iStatsGuestTicks, m_iStatsCaptureTicks, m_iStatsTimedFrames;

	volatile uint32_t m_iPads;	/* port 1 in the low half, port 2 in the high half */
	/* Pressed-bit masks (1 = pressed), same halves: presses and releases seen
	 * since the game's last read (main thread sets, core thread takes), and on
	 * the core thread what the last read reported and a press held over. */
	volatile uint32_t m_iPressedSince, m_iReleasedSince;
	uint32_t m_iLastReported, m_iPendingPress;
	uint32_t TakePads();		/* core thread, at the game's pad read */

	/* Tap test (core thread writes the clicks, the mixer stamps when each
	 * goes to the sound device, the main thread records the taps). */
	enum { CALIB_LEAD = 4, CALIB_CLICKS = 20, CALIB_MAX_TAPS = 64 };
	bool RunCalibration( int &iOffsetMsOut, int &iTapsOut );	/* core thread */
	volatile int m_iCalib;		/* 0 idle, 1 running */
	int64_t m_aClickFrame[CALIB_CLICKS];	/* audio frame each click starts at */
	volatile int64_t m_aClickTicks[CALIB_CLICKS];	/* when it went to the device (0: not yet) */
	int64_t m_aTapTicks[CALIB_MAX_TAPS];
	volatile int m_iTaps;
	volatile int m_iClicksSent;

	/* The PC settings rows in the game's OPTIONS screens. */
	PS1Settings m_Settings;
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
