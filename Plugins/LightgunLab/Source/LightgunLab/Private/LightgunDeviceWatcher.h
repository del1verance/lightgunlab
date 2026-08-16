// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"

/**
 * Always-on USB topology watcher: fires a callback (on the game thread) whenever
 * Windows announces device arrival/removal (WM_DEVICECHANGE / DBT_DEVNODES_CHANGED,
 * which is broadcast to every top-level window with no registration needed).
 * One physical plug event arrives as a burst - the subsystem debounces before
 * rescanning. Windows-only; Create() returns null elsewhere.
 */
class FLightgunDeviceWatcher
{
public:
	static TSharedPtr<FLightgunDeviceWatcher> Create();

	virtual ~FLightgunDeviceWatcher() {}

	/** Registers the window-message hook. Callback runs on the game thread. */
	virtual bool Start(TFunction<void()> InOnDevicesChanged) = 0;
	virtual void Stop() = 0;
};
