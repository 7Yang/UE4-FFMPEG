// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AudioMixerDevice.h"
#include "RHIResources.h"
#include "RHICommandList.h"
extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"
#include "libswscale/swscale.h"
#include "libavutil/file.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/error.h"
#include "libswresample/swresample.h"
}
#include "FFmpegDirector.generated.h"

class FEncoderThread;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectedDelegate, bool, Connected);

UCLASS(BlueprintType)
class UFFMPEG_API UFFmpegDirector: public UObject, public ISubmixBufferListener
{
	GENERATED_BODY()

public:
	UFFmpegDirector();
	virtual ~UFFmpegDirector();

    bool Initialize_Director(UWorld* World, int32 VideoLength, FString OutFileName, bool UseGPU, FString VideoFilter, int VideoFps, int VideoBitRate, int AudioBitRate, int AudioSampleRate, float AudioDelay, float SoundVolume);
	void RegisterAudioListener(UWorld* World);
	void RegisterVideoDelegate();

	void SetAudioEncodeCurrentTime(double* Time);
	void EncodeAudioFrame(uint8_t* AudioFrameData);
	void EncodeVideoFrame(uint8_t* VideoFrameData);
	void EncodeFinish();
	
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;
	void OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer);
    void OnPostResizeBackBuffer(void* Data);
	bool AddTickTime(float time);
	bool CheckThreadJobDone(float time);

	void EndWindowReader(const bool i);
	void EndWindowReaderStandardGame(void* i);
	void FinishDirector();

public:
    UFUNCTION()
    void OnConnectedServerCallback(bool Connected);

    FOnConnectedDelegate ConnectedDelegate;

private:
    bool CreateVideoEncoder(bool UseNGPU, const char* FileName, int BitRate);
    void CreateAudioEncoder(const char* EncoderName, int BitRate, int SampleRate);
    void CreateAudioSwresample(int SampleRate);
	void CreateVideoGraphFilter();
    void CreateStateTexture();

	void AddTickFunction();
	void CreateEncodeThread();
    void ResizeEncodeThread();
	void AddEndFunction();

	void GrabSlateFrameData();
	void ScaleVideoFrame(uint8_t *RGB);
	void SetAudioChannelData(AVFrame *Frame);

	void StopCapture();
	void Stop(UWorld* InWorld);
	uint32 FormatSize_X(uint32 x);

private:
    UWorld*            World;
	EWorldType::Type   GameMode;

	FString            FileAddress;
    int                IsUseRTMP;
    bool               IsClosing;
    bool               IsConnected;
    bool               IsMarkReceive;
    bool               IsDestory;

	uint32             Width;
	uint32             Height;
	uint32             OutWidth;
	uint32             OutHeight;
	int                VideoFps;
	uint64             TotalFrame;
	uint64             FrameCount;
	uint64             VideoCounter;
	uint32             FrameDuration;
	float              VideoTickTime;

    bool               IsTextured;
	SWindow*           SlateWindow;
	TArray<FColor>     TexturePixel;
	FTexture2DRHIRef   GameTexture;
	FTexture2DRHIRef   CopyedTexture;
    AVFormatContext*   FormatContext;
    AVCodecContext*    VideoCodecContext;
	AVStream*          VideoStream;
	int32_t            VideoStreamIndex;
	AVFrame*           VideoFrame;
    int64              VideoLastSendPts;
	uint8_t*           VideoFrameBufferPtr;
	uint32             LolStride;

	double             TickedTime;
	double             CurrentTime;
	float              AudioDelay;
	float              AudioVolume;
	uint64             AudioCounter;
	FAudioDevice*      AudioDevice;
    AVCodecContext*    AudioCodecContext;
	AVStream*          AudioStream;
	AVFrame*           AudioFrame;
    int64              AudioLastSendPts;
	int32_t            AudioStreamIndex;

	SwrContext*        SWResample;
	SwsContext*        SWSResampleContext;
	uint8_t*           SWSResampleOuts[2];

    FString            FilterDescription;
    AVFilterGraph*     FilterGraph;
    AVFilterInOut*     FilterInputs;
    AVFilterInOut*     FilterOutputs;
    AVFilterContext*   BufferSource;
    AVFilterContext*   BufferSink;

    FEncoderThread*    Runnable;
    FRunnableThread*   RunnableThread;
	FDelegateHandle    EndPIEDelegateHandle;
	FDelegateHandle    BackBufferReadyHandle;
    FDelegateHandle    ResizeBackBufferHandle;
	FTSTicker::FDelegateHandle    TickDelegateHandle;
	FTSTicker::FDelegateHandle    CheckDelegateHandle;
};
