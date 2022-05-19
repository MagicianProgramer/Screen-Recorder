#include "stdafx.h"

#include <Windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <time.h>
#include <iostream>

using namespace std;

#pragma comment(lib, "Winmm.lib")

WCHAR fileName[] = L"loopback-capture.wav";
BOOL bDone = FALSE;
HMMIO hFile = NULL;

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  1000 * 10000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
	if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
	if ((punk) != NULL)  \
{ (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

class MyAudioSink
{
public:
	HRESULT CopyData(BYTE* pData, UINT32 NumFrames, BOOL* pDone, WAVEFORMATEX* pwfx, HMMIO hFile);
};

HRESULT WriteWaveHeader(HMMIO hFile, LPCWAVEFORMATEX pwfx, MMCKINFO* pckRIFF, MMCKINFO* pckData);
HRESULT FinishWaveFile(HMMIO hFile, MMCKINFO* pckRIFF, MMCKINFO* pckData);
HRESULT RecordAudioStream(MyAudioSink* pMySink);


///===================================================//
///=======================ffmpeg======================//
///===================================================//
extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avutil.lib")


AVOutputFormat *output_format;
AVFormatContext *outAVFormatContext;

//audio params//
AVStream* audio_st;
AVCodecContext* outAudioAVCodecContext;
AVCodec* outAudioAVCodec;
AVFrame* audioFrame;

int audioFrameCounter = 0;

BYTE tmpbuffer[100000];
int nFilled = 0;

char output_file[MAX_PATH] = "C:\\__temp\\test.mp4";
int openffmpegparams()
{
	int err;
	err = avformat_alloc_output_context2(&outAVFormatContext, NULL, NULL, output_file);
	output_format = (AVOutputFormat *)outAVFormatContext->oformat;

	//------------------------------//
	/* create empty video file */
	//------------------------------//
	if ( !(outAVFormatContext->flags & AVFMT_NOFILE) )
	{
		if( avio_open2(&outAVFormatContext->pb , output_file , AVIO_FLAG_WRITE ,NULL, NULL) < 0 )
			return -2;
	}

	//find codec//
	AVDictionary *opts = NULL;
	//int ret = av_dict_set(&opts, "b", "64K", 0);
	outAudioAVCodec = (AVCodec*)avcodec_find_encoder(AV_CODEC_ID_AAC);

	AVCodecContext *c;
	audio_st = avformat_new_stream(outAVFormatContext, outAudioAVCodec);////////-----------------------////////
	if (!audio_st) {
		fprintf(stderr, "Could not alloc stream\n");
		exit(1);
	}

	audio_st->id = (int)(outAVFormatContext->nb_streams - 1);//set index//	

	outAudioAVCodecContext = avcodec_alloc_context3(outAudioAVCodec);

	audio_st->codecpar->codec_id = AV_CODEC_ID_AAC;
	audio_st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;

	avcodec_parameters_to_context(outAudioAVCodecContext, audio_st->codecpar);

	outAudioAVCodecContext->sample_fmt  = outAudioAVCodec->sample_fmts ? outAudioAVCodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
	outAudioAVCodecContext->bit_rate    = 64000;
	outAudioAVCodecContext->sample_rate = 96000;
	if (outAudioAVCodec->supported_samplerates) {
		outAudioAVCodecContext->sample_rate = outAudioAVCodec->supported_samplerates[0];
		for (int i = 0; outAudioAVCodec->supported_samplerates[i]; i++) {
			if (outAudioAVCodec->supported_samplerates[i] == 96000)
				outAudioAVCodecContext->sample_rate = 96000;
		}
	}

	outAudioAVCodecContext->channels        = av_get_channel_layout_nb_channels(outAudioAVCodecContext->channel_layout);
	outAudioAVCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
	if (outAudioAVCodec->channel_layouts) {
		outAudioAVCodecContext->channel_layout = outAudioAVCodec->channel_layouts[0];
		for (int i = 0; outAudioAVCodec->channel_layouts[i]; i++) {
			if (outAudioAVCodec->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
				outAudioAVCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
		}
	}
	outAudioAVCodecContext->channels        = av_get_channel_layout_nb_channels(outAudioAVCodecContext->channel_layout);
	audio_st->time_base.num = 1;
	audio_st->time_base.den = outAudioAVCodecContext->sample_rate;


	/*//outAudioAVCodecContext->codec_id = codec_id;
	//outAudioAVCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
	outAudioAVCodecContext->bit_rate = 64000;
	//outAudioAVCodecContext->sample_fmt = AV_SAMPLE_FMT_S16;
	//outAudioAVCodecContext->sample_rate = samplespersec;
	outAudioAVCodecContext->sample_fmt = outAudioAVCodec->sample_fmts[0];	
	outAudioAVCodecContext->sample_rate = outAudioAVCodec->supported_samplerates[0];
	outAudioAVCodecContext->channels = 2;
	outAudioAVCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;

	/*outAudioAVCodecContext->bits_per_coded_sample = av_get_bits_per_sample(AV_CODEC_ID_AAC);
	outAudioAVCodecContext->block_align = outAudioAVCodecContext->channels * outAudioAVCodecContext->bits_per_coded_sample/8;*/
	
	outAudioAVCodecContext->frame_size = 1024;
	if(output_format->flags & AVFMT_GLOBALHEADER)
		outAudioAVCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	avcodec_parameters_from_context(audio_st->codecpar, outAudioAVCodecContext);

	
	if ((err = avcodec_open2(outAudioAVCodecContext, outAudioAVCodec, NULL)) < 0) {
		return -1;
	}

	audioFrame = av_frame_alloc();
	audioFrame->nb_samples= outAudioAVCodecContext->frame_size;
	audioFrame->format= outAudioAVCodecContext->sample_fmt;
	audioFrame->channel_layout = outAudioAVCodecContext->channel_layout;
	

	/* allocate the data buffers */
	int ret = av_frame_get_buffer(audioFrame, 0);
	if (ret < 0) {
		return -1;
	}

	//check frames//
	if(!outAVFormatContext->nb_streams)
		return -1;	

	//dump format context//
	av_dump_format(outAVFormatContext, 0, output_file, 1);


	/* imp: mp4 container or some advanced container file required header information*/
	err = avformat_write_header(outAVFormatContext , NULL);
	if(err < 0)		return -1;
}

bool FlushAudioPackets()
{
	int ret;
	do
	{
		AVPacket packet = { 0 };

		ret = avcodec_receive_packet(outAudioAVCodecContext, &packet);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;

		if (ret < 0)
		{
			return false;
		}

		/*int64_t dts = av_gettime();
		dts = av_rescale_q(dts, (AVRational){1, 1000000}, (AVRational){1, 90000});
		int duration = AUDIO_STREAM_DURATION; // 20
		if(m_prevAudioDts > 0LL) {
			duration = dts - m_prevAudioDts;
		}

		m_prevAudioDts = dts;
		pkt.pts = AV_NOPTS_VALUE;
		pkt.dts = m_currAudioDts;
		m_currAudioDts += duration;
		pkt.duration = duration;*/


		//av_packet_rescale_ts(&packet, outAudioAVCodecContext->time_base, audio_st->time_base);
		
		/*packet.stream_index = audio_st->index;

		if (packet.pts != AV_NOPTS_VALUE)
			packet.pts = av_rescale_q(packet.pts, outAudioAVCodecContext->time_base, audio_st->time_base);
		packet.flags |= AV_PKT_FLAG_KEY;*/

		ret = av_interleaved_write_frame(outAVFormatContext, &packet);

		av_packet_unref(&packet);
		if (ret < 0)	return false;
	} while (ret >= 0);

	return true;
}



int main()
{
	clock();

	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// Create file
	MMIOINFO mi = { 0 };
	hFile = mmioOpen(
		// some flags cause mmioOpen write to this buffer
		// but not any that we're using
		(LPWSTR)fileName,
		&mi,
		MMIO_WRITE | MMIO_CREATE
		);

	if (NULL == hFile) {
		wprintf(L"mmioOpen(\"%ls\", ...) failed. wErrorRet == %u", fileName, GetLastError());
		return E_FAIL;
	}

	MyAudioSink AudioSink;
	RecordAudioStream(&AudioSink);

	mmioClose(hFile, 0);

	CoUninitialize();
	

	return 0;
}


HRESULT MyAudioSink::CopyData(BYTE* pData, UINT32 NumFrames, BOOL* pDone, WAVEFORMATEX* pwfx, HMMIO hFile)
{
	HRESULT hr = S_OK;

	if (0 == NumFrames) {
		wprintf(L"IAudioCaptureClient::GetBuffer said to read 0 frames\n");
		return E_UNEXPECTED;
	}

	LONG lBytesToWrite = NumFrames * pwfx->nBlockAlign;
#pragma prefast(suppress: __WARNING_INCORRECT_ANNOTATION, "IAudioCaptureClient::GetBuffer SAL annotation implies a 1-byte buffer")
	LONG lBytesWritten = mmioWrite(hFile, reinterpret_cast<PCHAR>(pData), lBytesToWrite);
	if (lBytesToWrite != lBytesWritten) {
		wprintf(L"mmioWrite wrote %u bytes : expected %u bytes", lBytesWritten, lBytesToWrite);
		return E_UNEXPECTED;
	}

	//encode buffer//
	int ret;
	ret = av_frame_make_writable(audioFrame);
	memcpy(tmpbuffer + nFilled, pData, lBytesToWrite);
	nFilled = nFilled + lBytesToWrite;
	while(nFilled > 4096)
	{
		memcpy(audioFrame->data[0], tmpbuffer, 4096);
		memcpy(audioFrame->data[1], tmpbuffer, 4096);
		nFilled = nFilled - 4096;
		memcpy(tmpbuffer, tmpbuffer + 4096, nFilled);

		//audioFrame->pts = audioFrameCounter++;
		ret = avcodec_send_frame(outAudioAVCodecContext, audioFrame);
		FlushAudioPackets();
	}

	/*if (lBytesToWrite < 4096)
	{
		memcpy(audioFrame->data[0], pData, lBytesToWrite);
	}
	else
	{
		memcpy(audioFrame->data[0], pData, 4096);
	}*/
	
	

	/////////////////

	static int CallCount = 0;
	cout << "CallCount = " << CallCount++ << "NumFrames: " << NumFrames << endl ;	

	return S_OK;
}


void MyFillPcmFormat(WAVEFORMATEX& format, WORD channels, int sampleRate, WORD bits)
{
	format.wFormatTag        = WAVE_FORMAT_PCM;
	format.nChannels         = channels;
	format.nSamplesPerSec    = sampleRate;
	format.wBitsPerSample    = bits;
	format.nBlockAlign       = format.nChannels * (format.wBitsPerSample / 8);
	format.nAvgBytesPerSec   = format.nSamplesPerSec * format.nBlockAlign;
	format.cbSize            = 0;
}


HRESULT RecordAudioStream(MyAudioSink* pMySink)
{
	openffmpegparams();


	HRESULT hr;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	UINT32 bufferFrameCount;
	UINT32 numFramesAvailable;
	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDevice* pDevice = NULL;
	IAudioClient* pAudioClient = NULL;
	IAudioCaptureClient* pCaptureClient = NULL;
	WAVEFORMATEX* pwfx = NULL;
	UINT32 packetLength = 0;

	BYTE* pData;
	DWORD flags;

	MMCKINFO ckRIFF = { 0 };
	MMCKINFO ckData = { 0 };

	hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)& pEnumerator);
	EXIT_ON_ERROR(hr)

		hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	EXIT_ON_ERROR(hr)
		
		hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)& pAudioClient);
	EXIT_ON_ERROR(hr)
		
		hr = pAudioClient->GetMixFormat(&pwfx);

	//////testing///////
	/*WAVEFORMATEX temp;
	MyFillPcmFormat(temp, 2, 44100, 16); // stereo, 44100 Hz, 16 bit
	*pwfx = temp;*/
	int m_sourceFrameSizeInBytes = (pwfx->wBitsPerSample / 8) * pwfx->nChannels;
	///////////////////

	EXIT_ON_ERROR(hr)

		hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,	AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration,	0,	pwfx, NULL);
	EXIT_ON_ERROR(hr)

		// Get the size of the allocated buffer.
		hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	EXIT_ON_ERROR(hr)

		hr = pAudioClient->GetService(IID_IAudioCaptureClient,	(void**)& pCaptureClient);
	EXIT_ON_ERROR(hr)

		hr = WriteWaveHeader((HMMIO)hFile, pwfx, &ckRIFF, &ckData);
	if (FAILED(hr)) {
		// WriteWaveHeader does its own logging
		return hr;
	}

	// Calculate the actual duration of the allocated buffer.
	hnsActualDuration = (double)REFTIMES_PER_SEC * bufferFrameCount / pwfx->nSamplesPerSec;

	hr = pAudioClient->Start();  // Start recording.
	EXIT_ON_ERROR(hr)
		// Each loop fills about half of the shared buffer.
		while (bDone == FALSE)
		{
			// Sleep for half the buffer duration.
			int nSleepTime = hnsActualDuration / REFTIMES_PER_MILLISEC / 2;
			Sleep(nSleepTime);

			hr = pCaptureClient->GetNextPacketSize(&packetLength);
			EXIT_ON_ERROR(hr)

			
				while (packetLength != 0)
				{
					// Get the available data in the shared buffer.
					hr = pCaptureClient->GetBuffer(&pData,	&numFramesAvailable, &flags, NULL, NULL);
					EXIT_ON_ERROR(hr)

						if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
						{
							pData = NULL;  // Tell CopyData to write silence.
						}

						// Copy the available capture data to the audio sink.
						hr = pMySink->CopyData(pData, numFramesAvailable, &bDone, pwfx, (HMMIO)hFile);
						EXIT_ON_ERROR(hr)

							hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
						EXIT_ON_ERROR(hr)

							hr = pCaptureClient->GetNextPacketSize(&packetLength);
						EXIT_ON_ERROR(hr)
				}

			if (clock() > 10 * CLOCKS_PER_SEC) //Record 10 seconds. From the first time call clock() at the beginning of the main().
				break;
		}

		hr = pAudioClient->Stop();  // Stop recording.
		EXIT_ON_ERROR(hr)

			hr = FinishWaveFile((HMMIO)hFile, &ckData, &ckRIFF);
		if (FAILED(hr)) {
			// FinishWaveFile does it's own logging
			return hr;
		}

Exit:
		CoTaskMemFree(pwfx);
		SAFE_RELEASE(pEnumerator)
		SAFE_RELEASE(pDevice)
		SAFE_RELEASE(pAudioClient)
		SAFE_RELEASE(pCaptureClient)


		//////////////////////
		//write delayed frames//
		avcodec_send_frame(outAudioAVCodecContext, NULL);
		FlushAudioPackets();


		av_write_trailer(outAVFormatContext);

		int ret = avio_close(outAVFormatContext->pb);

		return hr;
}

HRESULT WriteWaveHeader(HMMIO hFile, LPCWAVEFORMATEX pwfx, MMCKINFO* pckRIFF, MMCKINFO* pckData) {
	MMRESULT result;

	// make a RIFF/WAVE chunk
	pckRIFF->ckid = MAKEFOURCC('R', 'I', 'F', 'F');
	pckRIFF->fccType = MAKEFOURCC('W', 'A', 'V', 'E');

	result = mmioCreateChunk(hFile, pckRIFF, MMIO_CREATERIFF);
	if (MMSYSERR_NOERROR != result) {
		wprintf(L"mmioCreateChunk(\"RIFF/WAVE\") failed: MMRESULT = 0x%08x", result);
		return E_FAIL;
	}

	// make a 'fmt ' chunk (within the RIFF/WAVE chunk)
	MMCKINFO chunk;
	chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
	result = mmioCreateChunk(hFile, &chunk, 0);
	if (MMSYSERR_NOERROR != result) {
		wprintf(L"mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x", result);
		return E_FAIL;
	}

	// write the WAVEFORMATEX data to it
	LONG lBytesInWfx = sizeof(WAVEFORMATEX) + pwfx->cbSize;
	LONG lBytesWritten =
		mmioWrite(
		hFile,
		reinterpret_cast<PCHAR>(const_cast<LPWAVEFORMATEX>(pwfx)),
		lBytesInWfx
		);
	if (lBytesWritten != lBytesInWfx) {
		wprintf(L"mmioWrite(fmt data) wrote %u bytes; expected %u bytes", lBytesWritten, lBytesInWfx);
		return E_FAIL;
	}

	// ascend from the 'fmt ' chunk
	result = mmioAscend(hFile, &chunk, 0);
	if (MMSYSERR_NOERROR != result) {
		wprintf(L"mmioAscend(\"fmt \" failed: MMRESULT = 0x%08x", result);
		return E_FAIL;
	}

	// make a 'fact' chunk whose data is (DWORD)0
	chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
	result = mmioCreateChunk(hFile, &chunk, 0);
	if (MMSYSERR_NOERROR != result) {
		wprintf(L"mmioCreateChunk(\"fmt \") failed: MMRESULT = 0x%08x", result);
		return E_FAIL;
	}

	// write (DWORD)0 to it
	// this is cleaned up later
	DWORD frames = 0;
	lBytesWritten = mmioWrite(hFile, reinterpret_cast<PCHAR>(&frames), sizeof(frames));
	if (lBytesWritten != sizeof(frames)) {
		wprintf(L"mmioWrite(fact data) wrote %u bytes; expected %u bytes", lBytesWritten, (UINT32)sizeof(frames));
		return E_FAIL;
	}

	// ascend from the 'fact' chunk
	result = mmioAscend(hFile, &chunk, 0);
	if (MMSYSERR_NOERROR != result) {
		wprintf(L"mmioAscend(\"fact\" failed: MMRESULT = 0x%08x", result);
		return E_FAIL;
	}

	// make a 'data' chunk and leave the data pointer there
	pckData->ckid = MAKEFOURCC('d', 'a', 't', 'a');
	result = mmioCreateChunk(hFile, pckData, 0);
	if (MMSYSERR_NOERROR != result) {
		wprintf(L"mmioCreateChunk(\"data\") failed: MMRESULT = 0x%08x", result);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT FinishWaveFile(HMMIO hFile, MMCKINFO* pckRIFF, MMCKINFO* pckData) {
	MMRESULT result;

	result = mmioAscend(hFile, pckData, 0);
	if (MMSYSERR_NOERROR != result) {
		wprintf(L"mmioAscend(\"data\" failed: MMRESULT = 0x%08x", result);
		return E_FAIL;
	}

	result = mmioAscend(hFile, pckRIFF, 0);
	if (MMSYSERR_NOERROR != result) {
		wprintf(L"mmioAscend(\"RIFF/WAVE\" failed: MMRESULT = 0x%08x", result);
		return E_FAIL;
	}

	return S_OK;
}