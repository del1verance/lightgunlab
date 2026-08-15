// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "LightgunTypes.h"

/** SetupAPI-based scan for known lightgun hardware. Windows-only; returns empty elsewhere. */
struct FLightgunDetector
{
	/** Enumerates COM-port devices + Sinden HID/process presence into OutGuns. */
	static void Scan(TArray<FDetectedLightgun>& OutGuns);

	/** True if the Sinden desktop software (Lightgun.exe) is currently running. */
	static bool IsSindenSoftwareRunning();
};
