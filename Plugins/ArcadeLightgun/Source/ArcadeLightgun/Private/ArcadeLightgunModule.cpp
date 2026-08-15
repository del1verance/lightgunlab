#include "ArcadeLightgunModule.h"
#include "LightgunTypes.h"

DEFINE_LOG_CATEGORY(LogArcadeLightgun);

void FArcadeLightgunModule::StartupModule()
{
	UE_LOG(LogArcadeLightgun, Log, TEXT("ArcadeLightgun module started"));
}

void FArcadeLightgunModule::ShutdownModule()
{
	UE_LOG(LogArcadeLightgun, Log, TEXT("ArcadeLightgun module shut down"));
}

IMPLEMENT_MODULE(FArcadeLightgunModule, ArcadeLightgun)
