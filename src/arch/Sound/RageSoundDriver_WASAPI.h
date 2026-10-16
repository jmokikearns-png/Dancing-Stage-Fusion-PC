#ifndef RAGE_SOUND_WASAPI_H
#define RAGE_SOUND_WASAPI_H

/* RageSound_WASAPI - shared-mode, event-driven WASAPI output (Windows Vista
 * and later).  DSF PC: the lowest-latency path for the PS1 game's sound;
 * DirectSound itself runs on top of WASAPI since Vista.  The amount kept
 * queued in the device is SoundWriteAhead (Sound Options > Audio Buffer). */

#include "RageThreads.h"
#include "RageSoundDriver_Generic_Software.h"

struct IMMDevice;
struct IAudioClient;
struct IAudioRenderClient;

class RageSound_WASAPI: public RageSound_Generic_Software
{
public:
	RageSound_WASAPI();
	virtual ~RageSound_WASAPI();
	CString Init();

	int64_t GetPosition( const RageSoundBase *snd ) const;
	float GetPlayLatency() const;
	int GetSampleRate( int rate ) const;

protected:
	void SetupDecodingThread();

private:
	static int MixerThread_start( void *p );
	void MixerThread();

	RageThread m_MixingThread;
	volatile bool m_bShutdown;

	IMMDevice *m_pDevice;
	IAudioClient *m_pClient;
	IAudioRenderClient *m_pRender;
	void *m_hEvent;
	unsigned m_iBufferFrames;	/* the device buffer */
	int m_iTargetFrames;		/* what we keep queued in it */
	float m_fStreamLatency;		/* the audio engine's own, seconds */
	volatile int64_t m_iFramesWritten;
	volatile int m_iPadding;	/* written but not yet played, at the last check */
};
#define USE_RAGE_SOUND_WASAPI

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
