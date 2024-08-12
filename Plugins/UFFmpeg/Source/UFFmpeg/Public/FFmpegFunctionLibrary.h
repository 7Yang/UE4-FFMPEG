// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FFmpegFunctionLibrary.generated.h"

class UFFmpegDirector;

UCLASS()
class UFFMPEG_API UFFmpegFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "FFMpegEncoder", meta=(DeprecatedFunction, DeprecationMessage = "UFFmpegFunctionLibrary is deprecated, please use FFmpegComponent!"))
	static UFFmpegDirector* CreateFFmpegDirector(UWorld* World, int32 VideoLength, FString OutFileName, bool UseGPU, FString VideoFilter, int VideoFPS, int VideoBitRate, int AudioBitRate, int AudioSampleRate, float AudioDelay, float SoundVolume);

	UFUNCTION(BlueprintCallable, Category = "FFMpegEncoder", meta=(DeprecatedFunction, DeprecationMessage = "UFFmpegFunctionLibrary is deprecated, please use FFmpegComponent!"))
	static UWorld* GetWorldContext(UObject* WorldContextObject);
};
