// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

/**
 * MAME "-output windows" compatible emitter for classic MAMEHooker 5.1, which
 * predates network outputs. Protocol per MAME src/osd/modules/output/win32_output.cpp
 * and the MAME Interop SDK: a hidden "MAMEOutput" window, registered broadcast
 * messages (MAMEOutputStart/Stop/UpdateState/Register/Unregister/GetIDString),
 * and WM_COPYDATA id->name resolution (dwData = 1, payload {uint32 id; char name[]}).
 * Windows-only; all no-ops elsewhere.
 */
class FMameWindowBroadcaster
{
public:
	explicit FMameWindowBroadcaster(const FString& InGameName);
	~FMameWindowBroadcaster();

	bool Start();
	void Stop();
	bool IsRunning() const;

	void SendOutput(const FString& Name, int32 Value);

private:
	FString GameName;

#if PLATFORM_WINDOWS
	struct FImpl;
	FImpl* Impl = nullptr;
#endif
};
