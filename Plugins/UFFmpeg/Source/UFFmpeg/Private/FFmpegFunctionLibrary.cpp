// Fill out your copyright notice in the Description page of Project Settings.

#include "FFmpegFunctionLibrary.h"
#include "FFmpegDirector.h"

UFFmpegDirector* UFFmpegFunctionLibrary::CreateFFmpegDirector(UWorld* World, int32 VideoLength, FString OutFileName, bool UseGPU, FString VideoFilter, int VideoFPS, int VideoBitRate, int AudioBitRate, int AudioSampleRate, float AudioDelay, float SoundVolume)
{
	UFFmpegDirector* FFmpegDirector = NewObject<UFFmpegDirector>();
    FFmpegDirector->AddToRoot();
    FFmpegDirector->Initialize_Director(World, VideoLength, OutFileName, UseGPU, VideoFilter, VideoFPS, VideoBitRate, AudioBitRate, AudioSampleRate, AudioDelay, SoundVolume);
	return FFmpegDirector;
}

UWorld* UFFmpegFunctionLibrary::GetWorldContext(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject,EGetWorldErrorMode::LogAndReturnNull);
	return World;
}
