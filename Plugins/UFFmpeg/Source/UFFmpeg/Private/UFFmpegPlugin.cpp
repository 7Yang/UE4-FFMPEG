// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UFFmpegPlugin.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FUffmpegModule"

DEFINE_LOG_CATEGORY(LogFFmpeg);

void FUFFmpegModule::StartupModule()
{
    TArray<FString> Libraries =
    {
        TEXT("avutil"), TEXT("swscale"), TEXT("postproc"), TEXT("swresample"), TEXT("avcodec"), TEXT("avformat"), TEXT("avfilter"), TEXT("avdevice")
    };

    for (FString Library : Libraries)
    {
        LoadedLibraryArray.Add(LoadLibrary(Library));
    }

    Initialized = true;
}

void FUFFmpegModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  
    // For modules that support dynamic reloading, we call this function before unloading the module.
	if (!Initialized)
	{
		return;
	}

    for (auto Library: LoadedLibraryArray)
    {
        FPlatformProcess::FreeDllHandle(Library);
    }

    LoadedLibraryArray.Reset();
	Initialized = false;
}

void* FUFFmpegModule::LoadLibrary(const FString& name)
{
	FString          LibDir;
	FString          extension;
	FString          prefix;
	FString          separator;
#if PLATFORM_MAC
	FString  BaseDir = IPluginManager::Get().FindPlugin("Uffmpeg")->GetBaseDir();
	LibDir           = FPaths::Combine(*BaseDir, TEXT("ThirdParty/ffmpeg/lib/osx"));
	extension        = TEXT(".dylib");
	prefix           = "lib";
	separator        = ".";
#elif PLATFORM_WINDOWS
	extension        = TEXT(".dll");
	prefix           = "";
	separator        = "-";
	FString  BaseDir = IPluginManager::Get().FindPlugin("Uffmpeg")->GetBaseDir();
    LibDir           = FPaths::Combine(*BaseDir, TEXT("ThirdParty/ffmpeg-6.2.r113110/bin"));
#endif

	if (!LibDir.IsEmpty()) 
    {
		FString LibraryPath = FPaths::Combine(*LibDir, prefix + name + extension);
		return FPlatformProcess::GetDllHandle(*LibraryPath);
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUFFmpegModule, UFFmpeg)