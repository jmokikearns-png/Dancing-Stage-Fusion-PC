#include "global.h"
#include "RageSoundDriver_WASAPI.h"

#include "RageLog.h"
#include "RageUtil.h"
#include "PrefsManager.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

static const int channels = 2;
static const int bytes_per_frame = channels*2; /* 16-bit */
static const int samplerate = 44100;
static const int default_writeahead = 1024; /* frames, when SoundWriteAhead is 0 */

/* Defined here so no extra import library is needed. */
static const CLSID CLSID_MMDeviceEnumerator_ = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const IID IID_IMMDeviceEnumerator_ = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
static const IID IID_IAudioClient_ = { 0x1CB9AD4C, 0xDBFA, 0x4C32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
static const IID IID_IAudioRenderClient_ = { 0xF294ACFC, 0x3146, 0x4483, { 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2 } };

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

template<class T> static void SafeRelease( T *&p )
{
	if( p != NULL )
		p->Release();
	p = NULL;
}

RageSound_WASAPI::RageSound_WASAPI()
{
	m_bShutdown = false;
	m_pDevice = NULL;
	m_pClient = NULL;
	m_pRender = NULL;
	m_hEvent = NULL;
	m_iBufferFrames = 0;
	m_iTargetFrames = default_writeahead;
	m_fStreamLatency = 0;
	m_iFramesWritten = 0;
	m_iPadding = 0;
}

CString RageSound_WASAPI::Init()
{
	/* Like DSound::DSound; harmless if COM is already up on this thread. */
	CoInitialize( NULL );

	IMMDeviceEnumerator *pEnum = NULL;
	HRESULT hr = CoCreateInstance( CLSID_MMDeviceEnumerator_, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator_, (void **) &pEnum );
	if( FAILED(hr) )
		return hr_ssprintf( hr, "WASAPI unavailable (MMDeviceEnumerator)" );
	hr = pEnum->GetDefaultAudioEndpoint( eRender, eConsole, &m_pDevice );
	pEnum->Release();
	if( FAILED(hr) )
		return hr_ssprintf( hr, "No default sound output" );
	hr = m_pDevice->Activate( IID_IAudioClient_, CLSCTX_ALL, NULL, (void **) &m_pClient );
	if( FAILED(hr) )
		return hr_ssprintf( hr, "IMMDevice::Activate" );

	m_iTargetFrames = PREFSMAN->m_iSoundWriteAhead? PREFSMAN->m_iSoundWriteAhead.Get(): default_writeahead;
	m_iTargetFrames = clamp( m_iTargetFrames, 256, 8192 );

	WAVEFORMATEX wfx;
	memset( &wfx, 0, sizeof(wfx) );
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = channels;
	wfx.nSamplesPerSec = samplerate;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = bytes_per_frame;
	wfx.nAvgBytesPerSec = samplerate * bytes_per_frame;

	/* Ask for the target plus a period of slack; the engine may round up. */
	const REFERENCE_TIME hnsBuffer = REFERENCE_TIME(m_iTargetFrames) * 10000000 / samplerate + 100000;
	/* Windows converts our 44.1 kHz 16-bit stream to the device's mix
	 * format (usually 48 kHz float); retry without the flags for engines
	 * that only take the mix format as it is. */
	DWORD iFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
	hr = m_pClient->Initialize( AUDCLNT_SHAREMODE_SHARED, iFlags, hnsBuffer, 0, &wfx, NULL );
	if( FAILED(hr) )
	{
		LOG->Trace( "%s", hr_ssprintf(hr, "WASAPI: Initialize with format conversion").c_str() );
		/* A failed Initialize leaves the client unusable; get a new one. */
		SafeRelease( m_pClient );
		hr = m_pDevice->Activate( IID_IAudioClient_, CLSCTX_ALL, NULL, (void **) &m_pClient );
		if( FAILED(hr) )
			return hr_ssprintf( hr, "IMMDevice::Activate" );
		hr = m_pClient->Initialize( AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsBuffer, 0, &wfx, NULL );
		if( FAILED(hr) )
			return hr_ssprintf( hr, "IAudioClient::Initialize (44.1 kHz 16-bit stereo)" );
	}

	m_hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	hr = m_pClient->SetEventHandle( (HANDLE) m_hEvent );
	if( FAILED(hr) )
		return hr_ssprintf( hr, "IAudioClient::SetEventHandle" );

	UINT32 iFrames = 0;
	hr = m_pClient->GetBufferSize( &iFrames );
	if( FAILED(hr) )
		return hr_ssprintf( hr, "IAudioClient::GetBufferSize" );
	m_iBufferFrames = iFrames;
	m_iTargetFrames = min( m_iTargetFrames, int(m_iBufferFrames) );

	REFERENCE_TIME hnsLatency = 0;
	if( SUCCEEDED(m_pClient->GetStreamLatency(&hnsLatency)) )
		m_fStreamLatency = hnsLatency / 10000000.0f;

	hr = m_pClient->GetService( IID_IAudioRenderClient_, (void **) &m_pRender );
	if( FAILED(hr) )
		return hr_ssprintf( hr, "IAudioClient::GetService(IAudioRenderClient)" );

	LOG->Info( "WASAPI: device buffer %u frames, keeping %i queued (%.0f ms), engine latency %.0f ms",
		m_iBufferFrames, m_iTargetFrames, m_iTargetFrames * 1000.0f / samplerate, m_fStreamLatency * 1000 );

	StartDecodeThread();

	hr = m_pClient->Start();
	if( FAILED(hr) )
		return hr_ssprintf( hr, "IAudioClient::Start" );

	m_MixingThread.SetName( "Mixer thread" );
	m_MixingThread.Create( MixerThread_start, this );
	return "";
}

int RageSound_WASAPI::MixerThread_start( void *p )
{
	((RageSound_WASAPI *) p)->MixerThread();
	return 0;
}

void RageSound_WASAPI::MixerThread()
{
	CoInitializeEx( NULL, COINIT_MULTITHREADED );
	if( !SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) )
		SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL );

	while( !m_bShutdown )
	{
		/* The engine signals once per period (typically 10 ms). */
		WaitForSingleObject( (HANDLE) m_hEvent, 100 );
		if( m_bShutdown )
			break;

		UINT32 iPadding = 0;
		if( FAILED(m_pClient->GetCurrentPadding(&iPadding)) )
			continue;
		m_iPadding = iPadding;

		/* Keep about m_iTargetFrames queued: low latency, not a full buffer. */
		const int iFrames = m_iTargetFrames - int(iPadding);
		if( iFrames <= 0 )
			continue;

		BYTE *pData = NULL;
		if( FAILED(m_pRender->GetBuffer(iFrames, &pData)) )
			continue;
		const int64_t iFrameNo = m_iFramesWritten;
		this->Mix( (int16_t *) pData, iFrames, iFrameNo, iFrameNo - iPadding );
		m_pRender->ReleaseBuffer( iFrames, 0 );
		m_iFramesWritten = iFrameNo + iFrames;
	}

	m_pClient->Stop();
	CoUninitialize();
}

RageSound_WASAPI::~RageSound_WASAPI()
{
	if( m_MixingThread.IsCreated() )
	{
		m_bShutdown = true;
		SetEvent( (HANDLE) m_hEvent );
		LOG->Trace( "Shutting down mixer thread ..." );
		LOG->Flush();
		m_MixingThread.Wait();
		LOG->Trace( "Mixer thread shut down." );
		LOG->Flush();
	}
	SafeRelease( m_pRender );
	SafeRelease( m_pClient );
	SafeRelease( m_pDevice );
	if( m_hEvent )
		CloseHandle( (HANDLE) m_hEvent );
}

int64_t RageSound_WASAPI::GetPosition( const RageSoundBase *snd ) const
{
	/* Frames written minus those still queued: what is playing now. */
	return m_iFramesWritten - m_iPadding;
}

float RageSound_WASAPI::GetPlayLatency() const
{
	return float(m_iTargetFrames) / samplerate + m_fStreamLatency;
}

int RageSound_WASAPI::GetSampleRate( int rate ) const
{
	return samplerate;
}

void RageSound_WASAPI::SetupDecodingThread()
{
	if( !SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL) )
		LOG->Warn( werr_ssprintf(GetLastError(), "Failed to set decoding thread priority") );
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
