// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunDeviceWatcher.h"
#include "LightgunTypes.h"

#if PLATFORM_WINDOWS

#include "Windows/WindowsApplication.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	// From dbt.h (not pulled in by the minimal windows headers).
	constexpr uint32 DbtDevNodesChanged = 0x0007;
}

class FLightgunDeviceWatcherWindows final : public FLightgunDeviceWatcher, public IWindowsMessageHandler
{
public:
	virtual ~FLightgunDeviceWatcherWindows() override
	{
		Stop();
	}

	virtual bool Start(TFunction<void()> InOnDevicesChanged) override
	{
		if (bStarted)
		{
			return true;
		}
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}
		FWindowsApplication* WindowsApp = static_cast<FWindowsApplication*>(FSlateApplication::Get().GetPlatformApplication().Get());
		if (!WindowsApp)
		{
			return false;
		}
		OnDevicesChanged = MoveTemp(InOnDevicesChanged);
		WindowsApp->AddMessageHandler(*this);
		bStarted = true;
		return true;
	}

	virtual void Stop() override
	{
		if (bStarted && FSlateApplication::IsInitialized())
		{
			if (FWindowsApplication* WindowsApp = static_cast<FWindowsApplication*>(FSlateApplication::Get().GetPlatformApplication().Get()))
			{
				WindowsApp->RemoveMessageHandler(*this);
			}
		}
		bStarted = false;
		OnDevicesChanged = nullptr;
	}

	virtual bool ProcessMessage(HWND Hwnd, uint32 Msg, WPARAM WParam, LPARAM LParam, int32& OutResult) override
	{
		if (Msg == WM_DEVICECHANGE && WParam == DbtDevNodesChanged && OnDevicesChanged)
		{
			OnDevicesChanged();
		}
		return false; // observe only
	}

private:
	TFunction<void()> OnDevicesChanged;
	bool bStarted = false;
};

TSharedPtr<FLightgunDeviceWatcher> FLightgunDeviceWatcher::Create()
{
	return MakeShared<FLightgunDeviceWatcherWindows>();
}

#else // !PLATFORM_WINDOWS

TSharedPtr<FLightgunDeviceWatcher> FLightgunDeviceWatcher::Create()
{
	return nullptr;
}

#endif // PLATFORM_WINDOWS
