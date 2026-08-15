// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunLabModule.h"
#include "LightgunTypes.h"

DEFINE_LOG_CATEGORY(LogLightgunLab);

void FLightgunLabModule::StartupModule()
{
	UE_LOG(LogLightgunLab, Log, TEXT("LightgunLab module started"));
}

void FLightgunLabModule::ShutdownModule()
{
	UE_LOG(LogLightgunLab, Log, TEXT("LightgunLab module shut down"));
}

IMPLEMENT_MODULE(FLightgunLabModule, LightgunLab)
