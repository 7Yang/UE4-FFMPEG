// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "EncodeData.h"

template<typename DataType>
class UCircleQueue;

DECLARE_DELEGATE_OneParam(FEncodeDelegate,      uint8*)
DECLARE_DELEGATE_OneParam(FTimeEncodeDelegate, double*)

class UFFMPEG_API FEncoderThread: public FRunnable 
{
public:
	FEncoderThread();
	~FEncoderThread();

	virtual bool   Init() override; 
	virtual uint32 Run () override; 
	virtual void   Stop() override;  
	virtual void   Exit() override; 

    void PauseThread(bool value);
    void Resize(int video_data_size, int audio_data_size);
	void CreateQueue(int video_data_size, int auido_data_size, int video_data_num, int auido_data_num);

    FEncodeDelegate& GetAudioProcessDelegate();
    FTimeEncodeDelegate& GetAudioTimeProcessDelegate();

	bool IsQueneEmpty();
	bool InsertVideo(uint8* Src);
	bool InsertAudio(uint8* Src, double* time);
	void GetBufferData(uint8* data);

	bool IsDone = false;
	FCriticalSection     AudioMutex;
	FCriticalSection     VideoBufferMutex;
    FTimeEncodeDelegate  TimeEncodeDeletate;
    FEncodeDelegate      VideoEncodeDelegate;

private:
	void RunEncode();
	void EncodeVideo();
	void EncodeAudio();

private:
	int     Counter       = 0;
    bool    Pause         = false;
	uint8*  VideoData     = nullptr;

	UCircleQueue<uint8>*  AudioQueue;
	UCircleQueue<double>* AudioTimeQueue;
	UCircleQueue<uint8>*  VideoBufferQueue;
};
