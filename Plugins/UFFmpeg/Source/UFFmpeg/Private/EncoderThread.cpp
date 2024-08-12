// Fill out your copyright notice in the Description page of Project Settings.

#include "EncoderThread.h"

#define SAFE_FREE(Obj) \
{                      \
    delete Obj;        \
    Obj = nullptr;     \
}

FEncoderThread::FEncoderThread()
{
}

FEncoderThread::~FEncoderThread()
{
    if (AudioQueue) SAFE_FREE(AudioQueue);
    if (AudioTimeQueue) SAFE_FREE(AudioTimeQueue);
    if (VideoBufferQueue) SAFE_FREE(VideoBufferQueue);
}

bool FEncoderThread::Init()
{
	Counter = 0;
	return true;
}

uint32 FEncoderThread::Run()
{
	while (!IsDone)
	{
		RunEncode();
	}
	return 1;
}

void FEncoderThread::Stop()
{
	IsDone = true;
}

void FEncoderThread::Exit()
{
}

void FEncoderThread::PauseThread(bool value)
{
    Pause = value;
}

void FEncoderThread::Resize(int video_data_size, int audio_data_size)
{
	AudioQueue->Resize(audio_data_size);
    VideoBufferQueue->Resize(video_data_size);
}

void FEncoderThread::CreateQueue(int video_data_size, int auido_data_size, int video_data_num, int auido_data_num)
{
	VideoBufferQueue = new UCircleQueue<uint8>();
    VideoBufferQueue->Init(video_data_num, video_data_size);

	AudioQueue       = new UCircleQueue<uint8>();
	AudioQueue->Init(auido_data_num, auido_data_size);

	AudioTimeQueue   = new UCircleQueue<double>();
	AudioTimeQueue->Init(auido_data_num, auido_data_num * sizeof(double));

    VideoBufferQueue->EncodeDelegateUint8.BindRaw(this, &FEncoderThread::GetBufferData);
}

FEncodeDelegate& FEncoderThread::GetAudioProcessDelegate()
{
	return AudioQueue->EncodeDelegateUint8;
}

FTimeEncodeDelegate& FEncoderThread::GetAudioTimeProcessDelegate()
{
	return AudioTimeQueue->EncodeDelegateDouble;
}

bool FEncoderThread::InsertVideo(uint8* Src)
{
	if (!VideoBufferQueue) return false;

	{
		FScopeLock ScopeLock(&VideoBufferMutex);		

		while (!VideoBufferQueue->InsertEncodeData(Src))
		{
            VideoBufferQueue->PrcessEncodeData();
			EncodeVideo();
		}
	}		

	return true;
}

bool FEncoderThread::InsertAudio(uint8* Src, double* time)
{
	if (!AudioQueue) return false;

	{
		FScopeLock ScopeLock(&AudioMutex);

		while (!AudioQueue->InsertEncodeData(Src) || !AudioTimeQueue->InsertEncodeData(time))
		{
			EncodeAudio();
		}
	}

	return true;
}

void FEncoderThread::GetBufferData(uint8* data)
{
	VideoData = data;	
}

bool FEncoderThread::IsQueneEmpty()
{
	return VideoBufferQueue->IsEmpty() && AudioQueue->IsEmpty() && AudioTimeQueue->IsEmpty();
}

void FEncoderThread::RunEncode()
{
	bool IsNeedEncode = false;

	{
		FScopeLock ScopeLock(&AudioMutex);
		EncodeAudio();
	}

	{
		FScopeLock ScopeLock1(&VideoBufferMutex);
        if (!Pause) 
        {
		    IsNeedEncode = VideoBufferQueue->PrcessEncodeData();

		    if (IsNeedEncode) EncodeVideo();
        }
	}
}

void FEncoderThread::EncodeVideo()
{
	if (VideoData)
	{
		Counter++;
		VideoEncodeDelegate.ExecuteIfBound(VideoData);
		VideoData = nullptr;
	}
}

void FEncoderThread::EncodeAudio()
{
	if (AudioQueue)
	{
		AudioTimeQueue->PrcessEncodeData();
		AudioQueue->PrcessEncodeData();
	}
}
