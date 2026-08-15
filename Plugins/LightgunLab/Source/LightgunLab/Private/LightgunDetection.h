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

	/**
	 * Walks a device interface path ("\\?\HID#VID_xxxx&PID_xxxx&MI_01#...#{guid}") up the devnode
	 * tree to the physical USB device's instance ID ("USB\VID_xxxx&PID_xxxx\<serial>"). This is
	 * what lets the raw-input router pair a HID mouse with the COM port on the same composite
	 * device when two identical guns are plugged in. Returns empty if unresolvable. Windows-only.
	 */
	static FString ResolveUsbCompositeParent(const FString& DeviceInterfacePath);
};
