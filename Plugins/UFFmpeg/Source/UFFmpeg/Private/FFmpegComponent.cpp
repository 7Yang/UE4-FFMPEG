#include "FFmpegComponent.h"

UFFmpegComponent::UFFmpegComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    FFmpegSettings.UseNvidaGPU  =   true;
    FFmpegSettings.AudioBitRate = 192000;
    FFmpegSettings.VideoBitRate =   6000;
    FFmpegSettings.Resolution   = TEXT("3840x2160");
}

void UFFmpegComponent::BeginPlay()
{
    Super::BeginPlay();

    StreamingStart();
}

void UFFmpegComponent::StreamingStart() 
{
    if (IsConnecting || !IsResolutionValidate(FFmpegSettings.Resolution)) return;

    if (!FFmpegDirector)
    {
        FFmpegDirector = NewObject<UFFmpegDirector>();
    }

    Async(EAsyncExecution::TaskGraphMainThread, [this]()
    {
        UE_LOG(LogTemp, Log, TEXT("============[rtmp connecting server!]============="));

        struct FRecommended
        {
            float      SoundVolume;
            uint32     AudioBitRate;
            uint32     AudioSampleRate;
            uint32     VideoBitRate;
            float      SoundDelayTime;
        } Recommended = { 100.f, this->FFmpegSettings.AudioBitRate, 48 * ONE_K, this->FFmpegSettings.VideoBitRate, 0.f};

        if (UFFmpegDirector* CapturedDirector = this->FFmpegDirector)
        {
            this->IsConnecting   = true;
            //FString Resolution = ResolutionToString(this->FFmpegSettings.Resolution);

            CapturedDirector->ConnectedDelegate.AddDynamic(this, &UFFmpegComponent::OnConnectedServerCallback);
            CapturedDirector->Initialize_Director(this->GetWorld(), 100, this->FFmpegSettings.ServerRtmpUrl, this->FFmpegSettings.UseNvidaGPU, this->FFmpegSettings.Resolution, this->FFmpegSettings.Fps, Recommended.VideoBitRate, Recommended.AudioBitRate, Recommended.AudioSampleRate, Recommended.SoundDelayTime, Recommended.SoundVolume);
        }
    });
}

void UFFmpegComponent::StreamingStop() 
{
    if (FFmpegDirector)
    {
        FFmpegDirector->FinishDirector();
        FFmpegDirector = nullptr;
        IsConnecting   = false;
    }
}

void UFFmpegComponent::OnConnectedServerCallback(bool Success)
{
    const FString Connected = Success ? TEXT("success") : TEXT("failed");
    if (OnServerConnected.IsBound()) OnServerConnected.Broadcast(Success);

    //UE_LOG(LogTemp, Warning, TEXT("======================================================"));
    //UE_LOG(LogTemp, Warning, TEXT("==     Connecting rtmp server [%s]!     =="), *Connected);
    //UE_LOG(LogTemp, Warning, TEXT("======================================================"));

    if (!Success) IsConnecting = false;
    FFmpegDirector->ConnectedDelegate.RemoveDynamic(this, &UFFmpegComponent::OnConnectedServerCallback);
}

void UFFmpegComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StreamingStop();

    Super::EndPlay(EndPlayReason);
}

bool UFFmpegComponent::IsResolutionValidate(const FString& Resolution)
{
    FString StrWidth, StrHeight;

    if (!Resolution.Split("x", &StrWidth, &StrHeight)) 
    {
        UE_LOG(LogTemp, Log, TEXT("FFmpeg resulotion[%s] format string error!"), *Resolution);
        return false;
    }

    return true;
}

FString UFFmpegComponent::ResolutionToString(EFFmpegResolution::Type InResolution) const
{
    switch (InResolution) 
    {
        case EFFmpegResolution::EResolution_720 : return TEXT( "720x480" );
        case EFFmpegResolution::EResolution_1080: return TEXT("1920x1080");
        case EFFmpegResolution::EResolution_2K  : return TEXT("2048x1080");
        case EFFmpegResolution::EResolution_4K  : return TEXT("3840x2160");
    }

    checkf(false, TEXT("Conevert enum resolution to string error!"));
    return TEXT("");
}