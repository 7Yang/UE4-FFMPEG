// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FFmpegDirector.h"
#include "Components/ActorComponent.h"

#include "FFmpegComponent.generated.h"

#define ONE_K 1000

UENUM(BlueprintType)
namespace EFFmpegResolution 
{
    enum Type : int
    {
        EResolution_720  UMETA(DisplayName="720"),
        EResolution_1080 UMETA(DisplayName="1080"),
        EResolution_2K   UMETA(DisplayName="2K"),
        EResolution_4K   UMETA(DisplayName="4K")
    };
}

USTRUCT(BlueprintType)
struct UFFMPEG_API FFFmpegSettings
{
    GENERATED_USTRUCT_BODY()

public:
    UPROPERTY(EditInstanceOnly, Category = FFmpeg)
    bool                        UseNvidaGPU;

    UPROPERTY(EditInstanceOnly, Category = FFmpeg)
    uint32                      Fps      = 40;

    UPROPERTY(EditInstanceOnly, Category = FFmpeg)
    FString                     ServerRtmpUrl;

    UPROPERTY(EditInstanceOnly, Category = FFmpeg)
    FString                     Resolution;

    UPROPERTY(EditInstanceOnly, Category = FFmpeg)
    uint32                      AudioBitRate;

    UPROPERTY(EditInstanceOnly, Category = FFmpeg)
    uint32                      VideoBitRate;

    //UPROPERTY(EditInstanceOnly, Category = FFmpeg)
    //TEnumAsByte<EFFmpegResolution::Type> Resolution;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnServerConnectedDelegate, bool, Connected);

UCLASS(ClassGroup = (FFmpeg), meta = (DisplayName = "FFmpeg Component", BlueprintSpawnableComponent))
class UFFMPEG_API UFFmpegComponent: public UActorComponent
{
    GENERATED_UCLASS_BODY()

public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = FFmpeg)
    void StreamingStart();

    UFUNCTION(BlueprintCallable, Category = FFmpeg)
    void StreamingStop();

    UFUNCTION() 
    void OnConnectedServerCallback(bool Connected);

public:
    UPROPERTY(EditInstanceOnly,    Category = FFmpeg)
    FFFmpegSettings                FFmpegSettings;

    UPROPERTY(BlueprintAssignable, Category = FFmpeg)
    FOnServerConnectedDelegate     OnServerConnected;

private:
    bool                           IsConnecting;

    UPROPERTY()
    UFFmpegDirector*               FFmpegDirector;

    bool    IsResolutionValidate(const FString& Resolution);
    FString ResolutionToString(EFFmpegResolution::Type Resolution) const;
};