// Fill out your copyright notice in the Description page of Project Settings.

#include "FFmpegDirector.h"
#include "EncodeData.h"
#include "EncoderThread.h"

#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "Logging/LogVerbosity.h"

#include "Misc/DateTime.h"
#include "Misc/CoreDelegates.h"
#include "Misc/MessageDialog.h"

#include "RenderingThread.h"
#include "RenderGraphBuilder.h"
#include "Framework/Application/SlateApplication.h"

#define SAFE_FREE(Obj) \
{                      \
    delete Obj;        \
    Obj = nullptr;     \
}

static int64 GetUtcNowMissisecond() 
{
    return FDateTime::UtcNow().GetTicks() / ETimespan::TicksPerMillisecond;
}

UFFmpegDirector::UFFmpegDirector() : 
    IsClosing(false), IsConnected(false), IsMarkReceive(false), IsDestory(false),
    SlateWindow(nullptr), FormatContext(nullptr), VideoCodecContext(nullptr), VideoStream(nullptr), VideoFrame(nullptr), VideoFrameBufferPtr(nullptr), 
    AudioDevice(nullptr), AudioCodecContext(nullptr), AudioStream(nullptr), AudioFrame(nullptr), 
    SWResample(nullptr),  SWSResampleContext(nullptr), 
    FilterGraph(nullptr), FilterInputs(nullptr), FilterOutputs(nullptr), BufferSource(nullptr), BufferSink(nullptr), 
    Runnable(nullptr), RunnableThread(nullptr)
{
	VideoFps         = 0;
    AudioLastSendPts = 0;
    VideoLastSendPts = 0;
    CurrentTime      = 0.f;
    FMemory::Memzero(SWSResampleOuts, sizeof(uint8) * 2);
}

UFFmpegDirector::~UFFmpegDirector()
{

}

void UFFmpegDirector::FinishDirector()
{
	if (!IsDestory)
	{
        if (IsConnected) 
        {
            Runnable->Stop();
            RunnableThread->Kill(true);
            SAFE_FREE(Runnable);
        }

        if (IsConnected)
        {
            //FEditorDelegates::PrePIEEnded.Remove(EndPIEDelegateHandle);
            FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

            if (AudioDevice)
            {
                AudioDevice->UnregisterSubmixBufferListener(this);
            }

            if (FSlateApplication::IsInitialized())
            {
                FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().RemoveAll(this);
                FSlateApplication::Get().GetRenderer()->OnPostResizeWindowBackBuffer().RemoveAll(this);
            }
        }

        avformat_network_deinit();
        EncodeFinish();

		FMemory::Free(SWSResampleOuts[0]);
		FMemory::Free(SWSResampleOuts[1]);
		FMemory::Free(VideoFrameBufferPtr);

 		IsDestory             = true;
        IsConnected           = false;
        VideoFrameBufferPtr   = nullptr;
	}
}

void UFFmpegDirector::EndWindowReader(const bool i)
{
    FinishDirector();
	RemoveFromRoot();
	ConditionalBeginDestroy();
	BeginDestroy();
}

void UFFmpegDirector::EndWindowReaderStandardGame(void* data)
{
    FinishDirector();
	RemoveFromRoot();
	ConditionalBeginDestroy();
	BeginDestroy();
}

void UFFmpegDirector::RegisterAudioListener(UWorld* InWorld)
{
	GameMode    = InWorld->WorldType;
	AudioDevice = InWorld->GetAudioDevice().GetAudioDevice();
	if (AudioDevice)
	{
		AudioDevice->RegisterSubmixBufferListener(this);
	}
}

bool UFFmpegDirector::Initialize_Director(UWorld* InWorld, int32 InVideoLength, FString InFileName, bool InUseGPU, FString InVideoFilter, int InVideoFps, int InVideoBitRate, int InAudioBitRate, int InAudioSampleRate, float InAudioDelay, float InSoundVolume)
{
	World               = InWorld;
	FileAddress         = InFileName;

    VideoCounter        = 0;
    AudioCounter        = 0;
	FrameCount          = 0;
	TotalFrame          = InVideoLength;

    TickedTime          = 0;
	AudioDelay          = InAudioDelay;
	AudioVolume         = InSoundVolume;
	VideoFps            = InVideoFps; 
	VideoTickTime       = 1.f / VideoFps;

    IsTextured          = false;
	SlateWindow         = GEngine->GameViewport->GetWindow().Get();
	OutWidth            = Width  = FormatSize_X(SlateWindow->GetViewportSize().X);
	OutHeight           = Height = SlateWindow->GetViewportSize().Y;
    
	VideoFrameBufferPtr = (uint8_t *)FMemory::Realloc(VideoFrameBufferPtr, 3 * Width * Height);
    SWSResampleOuts[0]  = (uint8_t *)FMemory::Realloc(SWSResampleOuts[0], 4096);
    SWSResampleOuts[1]  = (uint8_t *)FMemory::Realloc(SWSResampleOuts[1], 4096);
    
    if (avformat_network_init() < 0) 
    {
        UE_LOG(LogTemp, Log, TEXT("FFmpeg initialized network error!")); 
        return false;
    }

	if (InVideoFilter.Len() > 0)
	{
	    FString StrWidth, StrHeight;
        InVideoFilter.Split("x", &StrWidth, &StrHeight);
		OutWidth       = FCString::Atoi(*StrWidth);
		OutHeight      = FCString::Atoi(*StrHeight);
	}
	FilterDescription.Append("[in]");
	FilterDescription.Append("scale=");
	FilterDescription.Append(FString::FromInt(OutWidth));
	FilterDescription.Append(":");
	FilterDescription.Append(FString::FromInt(OutHeight));
	FilterDescription.Append("[out]");

	IsUseRTMP = InFileName.Find("rtmp");
	if (IsUseRTMP == 0)
	{
		if (avformat_alloc_output_context2(&FormatContext, NULL, "flv", TCHAR_TO_ANSI(*InFileName)) < 0)
		{
			check(false);
		}
	}
	else
	{
		if (avformat_alloc_output_context2(&FormatContext, NULL, NULL, TCHAR_TO_ANSI(*InFileName)) < 0)
		{
			check(false);
		}
	}

	//create audio encoder
    CreateAudioSwresample(InAudioSampleRate);
    CreateAudioEncoder("aac", InAudioBitRate, InAudioSampleRate);

	//create video encoder
    CreateVideoEncoder(InUseGPU, TCHAR_TO_ANSI(*InFileName), InVideoBitRate);

    return true;
}

void UFFmpegDirector::RegisterVideoDelegate()
{
    ResizeBackBufferHandle = FSlateApplication::Get().GetRenderer()->OnPostResizeWindowBackBuffer().AddUObject(this, &UFFmpegDirector::OnPostResizeBackBuffer);
	BackBufferReadyHandle  = FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddUObject(this, &UFFmpegDirector::OnBackBufferReady_RenderThread);
}

void UFFmpegDirector::OnBackBufferReady_RenderThread(SWindow& InSlateWindow, const FTexture2DRHIRef& BackBuffer)
{
    if (!IsClosing && (SlateWindow == &InSlateWindow))
    {
        if (TickedTime >= VideoTickTime)
        {
            GameTexture = BackBuffer;
            if (!IsTextured)
            {
                CreateStateTexture();
            }

            if (FrameCount++ > TotalFrame)
            {
                if (IsUseRTMP != 0)
                {
                    IsClosing = true;
                    StopCapture();
                }
            }

            TickedTime -= VideoTickTime;
            GrabSlateFrameData();
        }
    }
}

void UFFmpegDirector::OnPostResizeBackBuffer(void* Data)
{
    if (IsTextured) 
    {
        ResizeEncodeThread();
    }
}

void UFFmpegDirector::StopCapture()
{
	//Remove the frame catch ticker, cause we don't need it now.
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	//Bind a new ticker to check if the other thread finish the job;
	CheckDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UFFmpegDirector::CheckThreadJobDone));
}

void UFFmpegDirector::Stop(UWorld* InWorld)
{
	FTSTicker::GetCoreTicker().RemoveTicker(CheckDelegateHandle);

    Runnable->Stop();
    RunnableThread->Kill(true);
    SAFE_FREE(Runnable);

	if (AudioDevice)
	{
		AudioDevice->UnregisterSubmixBufferListener(this);
	}

    if (FSlateApplication::IsInitialized()) 
    {
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(BackBufferReadyHandle);
        FSlateApplication::Get().GetRenderer()->OnPostResizeWindowBackBuffer().Remove(ResizeBackBufferHandle);
    }

    EncodeFinish();

	FMemory::Free(SWSResampleOuts[0]);
	FMemory::Free(SWSResampleOuts[1]);
	FMemory::Free(VideoFrameBufferPtr);
}

bool UFFmpegDirector::AddTickTime(float DeltaTime)
{
    if (IsMarkReceive) 
    {
        IsMarkReceive = false;
        RegisterVideoDelegate();
        RegisterAudioListener(World);
    }

	TickedTime += DeltaTime;
	return true;
}

bool UFFmpegDirector::CheckThreadJobDone(float time)
{
	if (Runnable->IsQueneEmpty())
	{
		if (AudioCounter >= TotalFrame && VideoCounter >= TotalFrame)
		{
			Stop(World);
		}
	}
	return true;
}

void UFFmpegDirector::AddEndFunction()
{
	// Default End Function Logic, Stop when game thread end.
	//if(GameMode== EWorldType::Game)
		FSlateApplication::Get().GetRenderer()->OnSlateWindowDestroyed().AddUObject(this, &UFFmpegDirector::EndWindowReaderStandardGame);
    // 		
    // 	if(GameMode == EWorldType::PIE)
    // 		FEditorDelegates::EndPIE.AddUObject(this, &UFFmpegDirector::EndWindowReader);
    //
	// Stop Recorder when get enough frame.
}

void UFFmpegDirector::AddTickFunction()
{
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UFFmpegDirector::AddTickTime));
}

void UFFmpegDirector::CreateStateTexture()
{
    const FRHITextureCreateDesc CopyedTextureDesc =
        FRHITextureCreateDesc::Create2D(TEXT("CopyedTexture"))
        .SetFormat(GameTexture->GetFormat())
        .SetInitialState(ERHIAccess::CopyDest)
        .SetExtent(SlateWindow->GetViewportSize().X, SlateWindow->GetViewportSize().Y);

    CopyedTexture = RHICreateTexture(CopyedTextureDesc);
    IsTextured    = true;
}

void UFFmpegDirector::GrabSlateFrameData()
{
	FString RHIName = GDynamicRHI->GetName();
    
	// DirectX 11
	// FRHICommandListImmediate::LockTexture2D() will cause crash in DirectX 12, use DirectX 11.
	if (RHIName == TEXT("D3D11"))
	{
	    FRHICommandListImmediate& list = GRHICommandList.GetImmediateCommandList();
		uint8* TextureData = (uint8*)list.LockTexture2D(GameTexture->GetTexture2D(), 0, EResourceLockMode::RLM_ReadOnly, LolStride, false);
		if (Runnable) Runnable->InsertVideo(TextureData);
		list.UnlockTexture2D(GameTexture, 0, false);
	}
	// DirectX 11 end

	// DirectX 12 & Vulkan
	if (RHIName == TEXT("D3D12") || RHIName == TEXT("Vulkan"))
	{
        ENQUEUE_RENDER_COMMAND(CopyRHITextureData)([this](FRHICommandListImmediate& RHICmdList)
        {
            RHICmdList.Transition(FRHITransitionInfo(this->GameTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
            RHICmdList.Transition(FRHITransitionInfo(this->CopyedTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
            RHICmdList.CopyTexture(this->GameTexture, this->CopyedTexture, FRHICopyTextureInfo());

            // Send video data to ffmpeg
            uint8* TextureData = (uint8*)RHICmdList.LockTexture2D(this->CopyedTexture, 0, EResourceLockMode::RLM_ReadOnly, this->LolStride, false);
            if (this->Runnable) { this->Runnable->InsertVideo(TextureData); }
            RHICmdList.UnlockTexture2D(this->CopyedTexture, 0, false);

            RHICmdList.Transition(FRHITransitionInfo(this->GameTexture, ERHIAccess::CopySrc, ERHIAccess::Present));
            RHICmdList.Transition(FRHITransitionInfo(this->CopyedTexture, ERHIAccess::CopyDest, ERHIAccess::CopySrc));
         });
	}
	// DirectX 12 & Vulkan end
}

void UFFmpegDirector::CreateEncodeThread()
{
	Runnable = new FEncoderThread();
	Runnable->CreateQueue(4 * Width * Height, 2048 * sizeof(float), 30, 40);
	Runnable->GetAudioProcessDelegate().BindUObject(this, &UFFmpegDirector::EncodeAudioFrame);
	Runnable->VideoEncodeDelegate.BindUObject(this, &UFFmpegDirector::EncodeVideoFrame);
	Runnable->GetAudioTimeProcessDelegate().BindUObject(this, &UFFmpegDirector::SetAudioEncodeCurrentTime);
	RunnableThread = FRunnableThread::Create(Runnable, TEXT("EncoderThread"));
}

void UFFmpegDirector::ResizeEncodeThread()
{
    if (IsClosing || !IsConnected) return;

    //TODO: 窗口尺寸变化!!!
    CreateStateTexture();

    OutWidth  = Width   = FormatSize_X(SlateWindow->GetViewportSize().X);
    OutHeight = Height  = SlateWindow->GetViewportSize().Y;
    VideoFrameBufferPtr = (uint8_t*)FMemory::Realloc(VideoFrameBufferPtr, 3 * Width * Height);
    SWSResampleOuts[0]  = (uint8_t*)FMemory::Realloc(SWSResampleOuts[0], 4096);
    SWSResampleOuts[1]  = (uint8_t*)FMemory::Realloc(SWSResampleOuts[1], 4096);

    checkf(Runnable, TEXT("Thread error!"));
    Runnable->PauseThread(true);
    Runnable->Resize(4 * Width * Height, 2048 * sizeof(float));

    int Result = av_image_alloc(VideoFrame->data, VideoFrame->linesize, OutWidth, OutHeight, VideoCodecContext->pix_fmt, 32);
    if (Result < 0)
    {
        check(false);
    }

    {
        SWSResampleContext = sws_getCachedContext(SWSResampleContext, Width, Height, AV_PIX_FMT_BGR24, OutWidth, OutHeight, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
    }

    //CreateVideoGraphFilter();

    Runnable->PauseThread(false);
}

void UFFmpegDirector::CreateAudioEncoder(const char* EncoderName, int BitRate, int SampleRate)
{
	const AVCodec* AudioCodec;

    AudioCodec        = avcodec_find_encoder_by_name(EncoderName);
	AudioStream       = avformat_new_stream(FormatContext, AudioCodec);
	AudioStreamIndex  = AudioStream->index;
	AudioCodecContext = avcodec_alloc_context3(AudioCodec);
	if (!AudioStream) { check(false); }

	AudioCodecContext->codec_id    = AV_CODEC_ID_AAC;
	AudioCodecContext->bit_rate    = BitRate;
	AudioCodecContext->codec_type  = AVMEDIA_TYPE_AUDIO;
	AudioCodecContext->sample_rate = SampleRate;
	AudioCodecContext->sample_fmt  = AV_SAMPLE_FMT_FLTP;
	AudioCodecContext->channels    = 2;
	AudioCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;

	if (FormatContext->oformat->flags & AVFMT_GLOBALHEADER)
		AudioCodecContext->flags    |= AV_CODEC_FLAG_GLOBAL_HEADER;

	AudioCodecContext->codec_tag     = 0;
	AudioStream->codecpar->codec_tag = 0;
	if (avcodec_open2(AudioCodecContext, AudioCodec, NULL) < 0)
	{
		check(false);
	}
	avcodec_parameters_from_context(AudioStream->codecpar, AudioCodecContext);

	AudioFrame                 = av_frame_alloc();
	AudioFrame->format         = AudioCodecContext->sample_fmt;
	AudioFrame->nb_samples     = AudioCodecContext->frame_size;
    AudioFrame->channels       = AudioCodecContext->channels;
    AudioFrame->channel_layout = AudioCodecContext->channel_layout;
}

bool UFFmpegDirector::CreateVideoEncoder(bool UseNvidiaGPU, const char* FileName, int BitRate)
{
	const AVCodec* VideoCodec;

    if (!UseNvidiaGPU)
    {
        VideoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    else
    {
        //H265编解码器压缩率很高，但测试了RTMP协议VLCPlayer播放器不支持，所以放弃了H265编解码器
        //H266技术刚出来，更不行! AV_CODEC_ID_H265: hevc_nvenc, AV_CODEC_ID_H266
        VideoCodec = avcodec_find_encoder_by_name("h264_nvenc");
        if (!VideoCodec && UseNvidiaGPU)
        {
            UE_LOG(LogTemp, Log, TEXT("FFmpeg create encoder/decoder error with nvidia driver! please install version[560.70-desktop-win10-win11-64bit-international-nsd-dch-whql]"));
            return false;
        }
    }

	VideoStream       = avformat_new_stream(FormatContext, VideoCodec);
	VideoStreamIndex  = VideoStream->index;

	VideoCodecContext = avcodec_alloc_context3(VideoCodec);
	if (!VideoCodecContext)
	{
		check(false);
	}

	VideoCodecContext->bit_rate      = BitRate;
    //VideoCodecContext->rc_min_rate = bit_rate;
	//VideoCodecContext->rc_max_rate = bit_rate;
	//VideoCodecContext->bit_rate_tolerance = bit_rate;
	//VideoCodecContext->rc_buffer_size     = bit_rate;
	//VideoCodecContext->rc_initial_buffer_occupancy = bit_rate * 3 / 4;
	VideoCodecContext->width         = OutWidth;
	VideoCodecContext->height        = OutHeight;
	VideoCodecContext->max_b_frames  = 2;
	VideoCodecContext->time_base.num = 1;
	VideoCodecContext->time_base.den = VideoFps;
	VideoCodecContext->pix_fmt       = AV_PIX_FMT_YUV420P;
	VideoCodecContext->me_range      = 16;
	VideoCodecContext->codec_type    = AVMEDIA_TYPE_VIDEO;
	VideoCodecContext->profile       = FF_PROFILE_H264_HIGH;
	VideoCodecContext->frame_num     = 1;
	VideoCodecContext->qcompress     = 1;
	VideoCodecContext->max_qdiff     = 4;
	VideoCodecContext->level         = 30;
	VideoCodecContext->gop_size      = 25;
	VideoCodecContext->qmin          = 18;
	VideoCodecContext->qmax          = 28;
	VideoCodecContext->me_range      = 16;
	VideoCodecContext->framerate     = { VideoFps, 1 };

	//ultrafast,superfast, veryfast, faster, fast, medium, slow, slower, veryslow, placebo.
    if (VideoCodec)
    {
		av_opt_set(VideoCodecContext->priv_data, "preset", "fast", 0);
    }

    if (FormatContext->oformat->flags & AVFMT_GLOBALHEADER) 
    {
		VideoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

	int Result = avcodec_open2(VideoCodecContext, VideoCodec, NULL);
	if (Result < 0)
	{
		check(false);
	}

	Result = avcodec_parameters_from_context(VideoStream->codecpar, VideoCodecContext);
	if (Result < 0)
	{
		check(false);
	}

	VideoFrame = av_frame_alloc();
	if (!VideoFrame)
	{
		check(false);
	}

	Result = av_image_alloc(VideoFrame->data, VideoFrame->linesize, OutWidth, OutHeight, VideoCodecContext->pix_fmt, 32);
	if (Result < 0)
	{
		check(false);
	}

    Async(EAsyncExecution::ThreadIfForkSafe, [this]()
    {
        ConnectedDelegate.AddDynamic(this, &UFFmpegDirector::OnConnectedServerCallback);

        int Result = avio_open(&this->FormatContext->pb, TCHAR_TO_ANSI(*this->FileAddress), AVIO_FLAG_WRITE);
        if (ConnectedDelegate.IsBound()) ConnectedDelegate.Broadcast(Result >= 0);
    });

    return true;
}

void UFFmpegDirector::OnConnectedServerCallback(bool Connected)
{
    this->IsConnected = !IsUseRTMP ? Connected : false;
    ConnectedDelegate.RemoveDynamic(this, &UFFmpegDirector::OnConnectedServerCallback);

    if (!Connected) 
    {
        FinishDirector();
    }
    else
    {
        if (avformat_write_header(FormatContext, NULL) < 0)
        {
            check(false);
        }

        IsMarkReceive      = true;
        FrameDuration      = VideoStream->time_base.den / VideoFps;
        SWSResampleContext = sws_getCachedContext(SWSResampleContext, Width, Height, AV_PIX_FMT_BGR24, OutWidth, OutHeight, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);

        // allocate graph filter 
        CreateVideoGraphFilter();
 
        //create encode thread
        CreateEncodeThread();

        //add tick functions
        AddTickFunction();
    }
}

void UFFmpegDirector::ScaleVideoFrame(uint8_t* RGB)
{
	const int LineSize[1] = { 3 * (int)Width };
	sws_scale(SWSResampleContext, (const uint8_t * const *)&RGB, LineSize, 0, Height, VideoFrame->data, VideoFrame->linesize);

	VideoFrame->width  = OutWidth;
	VideoFrame->height = OutHeight;
	VideoFrame->format = AV_PIX_FMT_YUV420P;
}

void UFFmpegDirector::CreateAudioSwresample(int sample_rate)
{
	SWResample = swr_alloc();
	av_opt_set_int(SWResample, "in_channel_layout",   AV_CH_LAYOUT_STEREO,  0);
	av_opt_set_int(SWResample, "out_channel_layout",  AV_CH_LAYOUT_STEREO,  0);
	av_opt_set_int(SWResample, "in_sample_rate",     /*48000*/sample_rate,  0);
	av_opt_set_int(SWResample, "out_sample_rate",    /*48000*/sample_rate,  0);
	av_opt_set_sample_fmt(SWResample, "in_sample_fmt",  AV_SAMPLE_FMT_FLT,  0);
	av_opt_set_sample_fmt(SWResample, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
	swr_init(SWResample);
}

void UFFmpegDirector::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)
{
    if (Runnable) 
    {
        //AudioCodecContext->bit_rate  = bit_rate;
        AudioCodecContext->sample_rate = SampleRate;
        AudioCodecContext->channels    = NumChannels;

        av_opt_set_int(SWResample,  "in_sample_rate", SampleRate, 0);
        av_opt_set_int(SWResample, "out_sample_rate", SampleRate, 0);

		Runnable->InsertAudio((uint8_t*)AudioData, &AudioClock);
    }
}

void UFFmpegDirector::EncodeAudioFrame(uint8_t* InAudioData)
{
    static int64 FirstSendPts = 0;

    if (!AudioLastSendPts) 
    {
        AudioLastSendPts = GetUtcNowMissisecond();
        if (!FirstSendPts) 
        {
            FirstSendPts = GetUtcNowMissisecond();
        }

        return;
    }

	const uint8_t* AudioData = InAudioData;
	AVPacket* AudioPkt  = av_packet_alloc();

	av_init_packet(AudioPkt);
	swr_convert(SWResample, SWSResampleOuts, 4096, &AudioData, 1024);

	AudioFrame->data[0] = (uint8_t*)SWSResampleOuts[0];
	AudioFrame->data[1] = (uint8_t*)SWSResampleOuts[1];
    SetAudioChannelData(AudioFrame);

	if (avcodec_send_frame(AudioCodecContext, AudioFrame) < 0)
	{
		check(false);
	}

	int Result = avcodec_receive_packet(AudioCodecContext, AudioPkt);
	if (Result < 0 && Result != -11)
	{
		check(false);
	}

	if (AudioPkt->size != 0)
	{
        //const int AudioBitRate = (int)AudioCodecContext->bit_rate;
        //AudioPkt->duration     = av_rescale_q(AudioPkt->duration, { 1, AudioBitRate }, AudioStream->time_base);
		//AudioPkt->pts          = AudioPkt->dts = av_rescale_q((/*pass_time*/CurrentTime + AudioDelay) / av_q2d({1, AudioBitRate}), {1, AudioBitRate}, AudioStream->time_base);

        AudioPkt->stream_index   = AudioStreamIndex;
        AudioPkt->duration       = GetUtcNowMissisecond() - AudioLastSendPts;
        AudioPkt->pts            = AudioPkt->dts = GetUtcNowMissisecond() - FirstSendPts;

		av_write_frame(FormatContext, AudioPkt);
		av_packet_unref(AudioPkt);

        AudioLastSendPts         = GetUtcNowMissisecond();
	}

    AudioCounter++;
}

void UFFmpegDirector::EncodeVideoFrame(uint8_t *VideoFrameData)
{
    static int64 FirstSendPts = 0;

    if (!VideoLastSendPts)
    {
        VideoLastSendPts = GetUtcNowMissisecond();
        if (!FirstSendPts)
        {
            FirstSendPts = GetUtcNowMissisecond();
        }

        return;
    }

	AVPacket* VideoPkt = av_packet_alloc();
	if (VideoPkt) av_init_packet(VideoPkt);

    //TODO: 可以使用parall处理!!!
    uint32 Row = 0,   Col  = 0;
	uint8* TextureDataPtr  = VideoFrameData;
	uint8_t* BufferHeadPtr = VideoFrameBufferPtr;
	for (Row = 0; Row < Height; ++Row)
	{
	    uint32* PixelPtr   = (uint32*)TextureDataPtr;
		for (Col = 0; Col < Width; ++Col)
		{
			// AV_PIX_FMT_BGR24	这里暂时转换为BGR
			// AV_PIX_FMT_RGB24	掉帧严重 暂时不知道为什么!
            uint32 EncodedPixel            = *PixelPtr;
			*(VideoFrameBufferPtr + 2)     = (EncodedPixel >> 2 ) & 0xFF;
			*(VideoFrameBufferPtr + 1)     = (EncodedPixel >> 12) & 0xFF;
			*(VideoFrameBufferPtr + 0)     = (EncodedPixel >> 22) & 0xFF;
			VideoFrameBufferPtr           += 3;
			++PixelPtr;		
		}
		TextureDataPtr += LolStride;
	}

	VideoFrameBufferPtr = BufferHeadPtr;
    ScaleVideoFrame(VideoFrameBufferPtr);

    AVFrame* FilterFrame = av_frame_alloc();
	if (av_buffersrc_add_frame_flags(BufferSource, VideoFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
	{
        check(false);
	}

	while (true)
	{
        int Result = av_buffersink_get_frame(BufferSink, FilterFrame);
        if (Result == AVERROR(EAGAIN) || Result == AVERROR_EOF || Result < 0) 
        {
            break;
        }

        int SndResult = avcodec_send_frame(VideoCodecContext, FilterFrame);
        if (SndResult < 0)
        {
            //UE_LOG(LogTemp, Log, TEXT("======avcodec_send_frame() error!")); 
        }

	    while (Result >= 0)
		{
            Result = avcodec_receive_packet(VideoCodecContext, VideoPkt);
            if (Result == AVERROR(EAGAIN) || Result == AVERROR_EOF || Result < 0)
            {
                av_packet_unref(VideoPkt);
                break;
			}

            const int64 NowMissisecond = GetUtcNowMissisecond();
            VideoPkt->duration         = NowMissisecond - VideoLastSendPts;
            VideoPkt->pts              = VideoPkt->dts  = NowMissisecond - FirstSendPts;
            VideoPkt->stream_index     = VideoStreamIndex;

            if (av_write_frame(FormatContext, VideoPkt) < 0)
            {
                UE_LOG(LogTemp, Log, TEXT("av_write_frame() error!"));
            }

            VideoLastSendPts           = GetUtcNowMissisecond();
		}
		av_packet_unref(VideoPkt);
	}

    VideoCounter++;
	av_frame_unref(FilterFrame);
}

void UFFmpegDirector::SetAudioEncodeCurrentTime(double* Time)
{
    CurrentTime = *Time;
}

void UFFmpegDirector::SetAudioChannelData(AVFrame *Frame)
{
	float *LeftChannel   = (float *)Frame->data[0];
	float *RightChannel  = (float *)Frame->data[1];
	for (int i = 0; i < Frame->nb_samples; i++)
	{
		LeftChannel [i] *= AudioVolume;
		RightChannel[i] *= AudioVolume;
	}
}

void UFFmpegDirector::CreateVideoGraphFilter()
{
	FilterGraph   = avfilter_graph_alloc();
	FilterInputs  = avfilter_inout_alloc();
	FilterOutputs = avfilter_inout_alloc();
	if (!FilterGraph || !FilterInputs || !FilterOutputs) 
    {
        check(false); 
    }

	char  FilterArgs[100];
    const AVRational NewTimeBase  = { 1, 1000000 };
    const AVFilter*  FilterSource = avfilter_get_by_name("buffer");
    const AVFilter*  FilterSink   = avfilter_get_by_name("buffersink");
    enum  AVPixelFormat PixelFormat[] = { VideoCodecContext->pix_fmt, AV_PIX_FMT_NONE };

	snprintf(FilterArgs, sizeof(FilterArgs), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d", VideoCodecContext->width, VideoCodecContext->height, VideoCodecContext->pix_fmt, NewTimeBase.num, NewTimeBase.den, VideoCodecContext->sample_aspect_ratio.num, VideoCodecContext->sample_aspect_ratio.den);
	int Result = avfilter_graph_create_filter(&BufferSource, FilterSource, "in", FilterArgs, NULL, FilterGraph);
	if (Result < 0)
	{
		check(false);
	}

	Result = avfilter_graph_create_filter(&BufferSink, FilterSink, "out", NULL, NULL, FilterGraph);
	if (Result < 0)
	{
		check(false);
	}

	Result = av_opt_set_int_list(BufferSink, "pix_fmts", PixelFormat, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	if (Result < 0)
	{
		check(false);
	}

	FilterOutputs->name       = av_strdup("in");
	FilterOutputs->filter_ctx = BufferSource;
	FilterOutputs->pad_idx    = 0;
	FilterOutputs->next       = NULL;

	FilterInputs->name        = av_strdup("out");
	FilterInputs->filter_ctx  = BufferSink;
	FilterInputs->pad_idx     = 0;
	FilterInputs->next        = NULL;

	if ((Result = avfilter_graph_parse_ptr(FilterGraph, TCHAR_TO_ANSI(*FilterDescription), &FilterInputs, &FilterOutputs, NULL)) < 0)
	{
		check(false);
	}

	if ((Result = avfilter_graph_config(FilterGraph, NULL)) < 0)
	{
		check(false);
	}
}

uint32 UFFmpegDirector::FormatSize_X(uint32 x)
{
	while ((x % 32) != 0)
	{
		++x;
	}
	return x;
}

void UFFmpegDirector::EncodeFinish()
{
	if (FormatContext)
	{
		if (IsConnected) av_write_trailer(FormatContext);
		if (IsConnected) avio_close(FormatContext->pb);
		avformat_free_context(FormatContext);
	}

	if (VideoCodecContext)
	{
		avcodec_free_context(&VideoCodecContext);
		if (IsConnected) avcodec_close(VideoCodecContext);
		av_free(VideoCodecContext);
	}

	if (AudioCodecContext)
	{
		avcodec_free_context(&AudioCodecContext);
		if (IsConnected) avcodec_close(AudioCodecContext);
		av_free(AudioCodecContext);
	}
	if (SWResample)
	{
		swr_close(SWResample);
		swr_free(&SWResample);
		sws_freeContext(SWSResampleContext);
	}

    if (IsConnected) 
    {
	    avfilter_graph_free(&FilterGraph);
	    avfilter_inout_free(&FilterInputs);
	    avfilter_inout_free(&FilterOutputs);
    }

	av_frame_free(&VideoFrame);
	av_frame_free(&AudioFrame);
}