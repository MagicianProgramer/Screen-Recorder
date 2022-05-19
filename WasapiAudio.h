/*
 * Copyright (c) 2013-2014 Microsoft Mobile. All rights reserved.
 * See the license text file delivered with this project for more information.
 */

#pragma once

#include <windows.h>

#include <synchapi.h>
#include <audioclient.h>
#include <phoneaudioclient.h>

#include <array>
#include <WinNT.h>
using namespace std;



class WasapiAudio sealed
{
public:
	WasapiAudio();
	virtual ~WasapiAudio();

	bool StartAudioCapture();
	bool StopAudioCapture();
	int ReadBytes(Array<byte>* a);

	bool StartAudioRender();
	bool StopAudioRender();
	void SetAudioBytes(const Array<byte>* a);
	bool Update();
	void SkipFiveSecs();

private:
	HRESULT InitCapture();
	HRESULT InitRender();

	bool started;
	int m_sourceFrameSizeInBytes;

	WAVEFORMATEX* m_waveFormatEx;	

	// Actual capture object
	IAudioCaptureClient* m_pCaptureClient;
	IAudioRenderClient* m_pRenderClient;

	// TEST AUDIO
	BYTE* audioBytes;
	long audioIndex;
	long audioByteCount;
};