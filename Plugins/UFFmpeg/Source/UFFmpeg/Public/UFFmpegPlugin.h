// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFFmpeg, Log, All);

class FUFFmpegModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
    bool  Initialized = false;
	void* LoadLibrary(const FString& name);

    TArray<void*> LoadedLibraryArray;
};
