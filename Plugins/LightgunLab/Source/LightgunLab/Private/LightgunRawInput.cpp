// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunRawInput.h"
#include "LightgunTypes.h"
#include "LightgunDetection.h"

#if PLATFORM_WINDOWS

#include "Windows/WindowsApplication.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
#include "Containers/Ticker.h"

namespace
{
	constexpr int32 NumPlayers = LightgunMaxPlayers;

	int32 ParseVidFromPath(const FString& UpperPath)
	{
		const int32 Idx = UpperPath.Find(TEXT("VID_"));
		return Idx != INDEX_NONE ? FParse::HexNumber(*UpperPath.Mid(Idx + 4, 4)) : 0;
	}

	int32 ParsePidFromPath(const FString& UpperPath)
	{
		const int32 Idx = UpperPath.Find(TEXT("PID_"));
		return Idx != INDEX_NONE ? FParse::HexNumber(*UpperPath.Mid(Idx + 4, 4)) : 0;
	}
}

class FLightgunRawInputRouterWindows final : public FLightgunRawInputRouter, public IWindowsMessageHandler
{
public:
	virtual ~FLightgunRawInputRouterWindows() override
	{
		Stop();
	}

	virtual bool Start() override
	{
		if (bStarted)
		{
			return true;
		}
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}
		TargetHwnd = FindGameWindow();
		if (!TargetHwnd)
		{
			UE_LOG(LogLightgunLab, Warning, TEXT("Raw input router: no game window to target"));
			return false;
		}

		FWindowsApplication* WindowsApp = static_cast<FWindowsApplication*>(FSlateApplication::Get().GetPlatformApplication().Get());
		if (!WindowsApp)
		{
			return false;
		}
		WindowsApp->AddMessageHandler(*this);
		bHandlerAdded = true;

		if (!RegisterDevices())
		{
			Stop();
			return false;
		}

		// The mouse usage registration is per-process: the engine's high-precision
		// mouse mode replaces it on capture and RIDEV_REMOVEs it on release - in
		// PIE that happens whenever a click lands on editor chrome (e.g. after a
		// gun wanders offscreen), killing our aim stream. Re-check aggressively,
		// and also on every window (re)activation below.
		ReassertHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FLightgunRawInputRouterWindows::ReassertRegistration), 0.25f);

		bStarted = true;
		RebuildDeviceMap();
		UE_LOG(LogLightgunLab, Log, TEXT("Raw input router started (hwnd %p)"), TargetHwnd);
		return true;
	}

	virtual void Stop() override
	{
		if (ReassertHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(ReassertHandle);
			ReassertHandle.Reset();
		}
		if (bDevicesRegistered)
		{
			RAWINPUTDEVICE Remove[2] = {};
			Remove[0].usUsagePage = 0x01; Remove[0].usUsage = 0x02; Remove[0].dwFlags = RIDEV_REMOVE; Remove[0].hwndTarget = nullptr;
			Remove[1].usUsagePage = 0x01; Remove[1].usUsage = 0x06; Remove[1].dwFlags = RIDEV_REMOVE; Remove[1].hwndTarget = nullptr;
			::RegisterRawInputDevices(Remove, 2, sizeof(RAWINPUTDEVICE));
			bDevicesRegistered = false;
		}
		if (bHandlerAdded && FSlateApplication::IsInitialized())
		{
			if (FWindowsApplication* WindowsApp = static_cast<FWindowsApplication*>(FSlateApplication::Get().GetPlatformApplication().Get()))
			{
				WindowsApp->RemoveMessageHandler(*this);
			}
		}
		bHandlerAdded = false;
		bStarted = false;
	}

	virtual void SetPlayerBinding(int32 PlayerIndex, const FPlayerBinding& Binding) override
	{
		if (PlayerIndex >= 0 && PlayerIndex < NumPlayers)
		{
			Bindings[PlayerIndex] = Binding;
		}
	}

	virtual void RebuildDeviceMap() override
	{
		MouseToPlayer.Reset();
		KeyboardToPlayer.Reset();
		PressedKeys.Reset();
		NullAbsolutePlayer = INDEX_NONE;
		for (FPlayerRuntime& RT : Runtime)
		{
			RT.bMappedDeviceProducedAim = false;
			RT.bAdoptedOrphan = false;
		}
		bMapDirty = false;

		UINT DeviceCount = 0;
		if (::GetRawInputDeviceList(nullptr, &DeviceCount, sizeof(RAWINPUTDEVICELIST)) != 0 || DeviceCount == 0)
		{
			return;
		}
		TArray<RAWINPUTDEVICELIST> Devices;
		Devices.SetNumZeroed(DeviceCount);
		const UINT Filled = ::GetRawInputDeviceList(Devices.GetData(), &DeviceCount, sizeof(RAWINPUTDEVICELIST));
		if (Filled == static_cast<UINT>(-1))
		{
			return;
		}

		struct FCandidate
		{
			HANDLE Handle = nullptr;
			FString Path;
			FString UpperPath;
			bool bKeyboard = false;
		};
		TArray<FCandidate> Candidates;
		for (UINT Index = 0; Index < Filled; ++Index)
		{
			if (Devices[Index].dwType != RIM_TYPEMOUSE && Devices[Index].dwType != RIM_TYPEKEYBOARD)
			{
				continue;
			}
			WCHAR NameBuffer[512] = {};
			UINT NameSize = UE_ARRAY_COUNT(NameBuffer);
			if (::GetRawInputDeviceInfoW(Devices[Index].hDevice, RIDI_DEVICENAME, NameBuffer, &NameSize) == static_cast<UINT>(-1))
			{
				continue;
			}
			FCandidate Candidate;
			Candidate.Handle = Devices[Index].hDevice;
			Candidate.Path = FString(NameBuffer);
			Candidate.UpperPath = Candidate.Path.ToUpper();
			Candidate.bKeyboard = Devices[Index].dwType == RIM_TYPEKEYBOARD;
			Candidates.Add(MoveTemp(Candidate));
		}
		// Stable order so the identical-PID fallback assigns deterministically.
		Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Path < B.Path; });

		// Confidence passes: exact path, then USB parent, then VID/PID in stable order.
		TSet<HANDLE> Claimed;
		auto Claim = [this, &Claimed](const FCandidate& Candidate, int32 Player, const TCHAR* How)
		{
			Claimed.Add(Candidate.Handle);
			if (Candidate.bKeyboard)
			{
				KeyboardToPlayer.Add(Candidate.Handle, Player);
			}
			else
			{
				MouseToPlayer.Add(Candidate.Handle, Player);
			}
			UE_LOG(LogLightgunLab, Log, TEXT("Raw input: %s %p -> P%d (%s) [%s]"),
				Candidate.bKeyboard ? TEXT("keyboard") : TEXT("mouse"), Candidate.Handle, Player + 1, How, *Candidate.Path);
		};

		for (const FCandidate& Candidate : Candidates)
		{
			for (int32 Player = 0; Player < NumPlayers; ++Player)
			{
				const FPlayerBinding& B = Bindings[Player];
				if (B.bActive && !B.bDesktopMouse && !Candidate.bKeyboard &&
					!B.RawInputMousePath.IsEmpty() && B.RawInputMousePath == Candidate.Path)
				{
					Claim(Candidate, Player, TEXT("exact path"));
					break;
				}
			}
		}
		for (const FCandidate& Candidate : Candidates)
		{
			if (Claimed.Contains(Candidate.Handle))
			{
				continue;
			}
			// Resolving the parent costs a couple of registry hops; only bother for
			// devices whose VID/PID matches some bound gun at all.
			const int32 Vid = ParseVidFromPath(Candidate.UpperPath);
			const int32 Pid = ParsePidFromPath(Candidate.UpperPath);
			bool bVidPidMatchesAnyone = false;
			for (int32 Player = 0; Player < NumPlayers; ++Player)
			{
				const FPlayerBinding& B = Bindings[Player];
				if (B.bActive && !B.bDesktopMouse && B.Vid == Vid && B.Pid == Pid && Vid != 0)
				{
					bVidPidMatchesAnyone = true;
					break;
				}
			}
			if (!bVidPidMatchesAnyone)
			{
				continue;
			}
			const FString Parent = FLightgunDetector::ResolveUsbCompositeParent(Candidate.Path);
			if (Parent.IsEmpty())
			{
				continue;
			}
			for (int32 Player = 0; Player < NumPlayers; ++Player)
			{
				const FPlayerBinding& B = Bindings[Player];
				if (B.bActive && !B.bDesktopMouse && !B.UsbCompositeParentId.IsEmpty() && B.UsbCompositeParentId == Parent)
				{
					Claim(Candidate, Player, TEXT("usb parent"));
					break;
				}
			}
		}
		// VID/PID fallback, stable order: first unclaimed matching device goes to the
		// lower player index that still lacks one of this device type.
		for (const FCandidate& Candidate : Candidates)
		{
			if (Claimed.Contains(Candidate.Handle))
			{
				continue;
			}
			const int32 Vid = ParseVidFromPath(Candidate.UpperPath);
			const int32 Pid = ParsePidFromPath(Candidate.UpperPath);
			for (int32 Player = 0; Player < NumPlayers; ++Player)
			{
				const FPlayerBinding& B = Bindings[Player];
				if (!B.bActive || B.bDesktopMouse || B.Vid != Vid || B.Pid != Pid || Vid == 0)
				{
					continue;
				}
				bool bPlayerHasOne = false;
				const TMap<HANDLE, int32>& Map = Candidate.bKeyboard ? KeyboardToPlayer : MouseToPlayer;
				for (const TPair<HANDLE, int32>& Pair : Map)
				{
					if (Pair.Value == Player)
					{
						bPlayerHasOne = true;
						break;
					}
				}
				if (!bPlayerHasOne)
				{
					Claim(Candidate, Player, TEXT("vid/pid order"));
					break;
				}
			}
		}
	}

	virtual FString GetDebugSummary() const override
	{
		FString Summary;
		for (int32 Player = 0; Player < NumPlayers; ++Player)
		{
			const FPlayerBinding& B = Bindings[Player];
			if (!B.bActive)
			{
				continue;
			}
			int32 Mice = 0, Keyboards = 0;
			for (const TPair<HANDLE, int32>& Pair : MouseToPlayer) { Mice += Pair.Value == Player ? 1 : 0; }
			for (const TPair<HANDLE, int32>& Pair : KeyboardToPlayer) { Keyboards += Pair.Value == Player ? 1 : 0; }
			if (!Summary.IsEmpty())
			{
				Summary += TEXT("   ");
			}
			if (B.bDesktopMouse)
			{
				Summary += FString::Printf(TEXT("P%d: desktop mouse"), Player + 1);
			}
			else
			{
				Summary += FString::Printf(TEXT("P%d: %d mouse / %d kbd%s"), Player + 1, Mice, Keyboards,
					Runtime[Player].bAdoptedOrphan ? TEXT(" (adopted unmatched aim source)") : TEXT(""));
			}
		}
		return Summary;
	}

	// --- IWindowsMessageHandler ---
	virtual bool ProcessMessage(HWND Hwnd, uint32 Msg, WPARAM WParam, LPARAM LParam, int32& OutResult) override
	{
		if (Msg == WM_INPUT_DEVICE_CHANGE)
		{
			// Arrival/removal invalidates handles; rebuild before the next event routes.
			bMapDirty = true;
			return false;
		}
		// Re-activation is the exact moment we return from whatever clobbered the
		// per-process registration (editor captures toggling high-precision mouse
		// mode replace it and then RIDEV_REMOVE it). Re-assert immediately instead
		// of waiting for the ticker.
		if (bStarted && ((Msg == WM_ACTIVATE && LOWORD(WParam) != WA_INACTIVE) ||
			(Msg == WM_ACTIVATEAPP && WParam) || Msg == WM_SETFOCUS))
		{
			CheckAndReassertRegistration();
			return false;
		}
		if (Msg != WM_INPUT || !bStarted)
		{
			return false;
		}
		if (bMapDirty)
		{
			RebuildDeviceMap();
		}

		UINT Size = 0;
		::GetRawInputData(reinterpret_cast<HRAWINPUT>(LParam), RID_INPUT, nullptr, &Size, sizeof(RAWINPUTHEADER));
		if (Size == 0 || Size > 1024)
		{
			return false;
		}
		RAWINPUT* Data = static_cast<RAWINPUT*>(FMemory_Alloca(Size));
		if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(LParam), RID_INPUT, Data, &Size, sizeof(RAWINPUTHEADER)) != Size)
		{
			return false;
		}

		if (Data->header.dwType == RIM_TYPEMOUSE)
		{
			HandleMouse(Data->header.hDevice, Data->data.mouse);
		}
		else if (Data->header.dwType == RIM_TYPEKEYBOARD)
		{
			HandleKeyboard(Data->header.hDevice, Data->data.keyboard);
		}
		// Never consume: Slate's merged-cursor handling (panel buttons, 1P path) must
		// keep seeing the legacy messages this same input generates.
		return false;
	}

private:
	struct FPlayerRuntime
	{
		FVector2f Pos = FVector2f::ZeroVector;
		bool bHasAim = false;
		bool bMappedDeviceProducedAim = false;
		bool bAdoptedOrphan = false;
	};

	static HWND FindGameWindow()
	{
		if (GEngine && GEngine->GameViewport)
		{
			TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
			if (Window.IsValid() && Window->GetNativeWindow().IsValid())
			{
				return static_cast<HWND>(Window->GetNativeWindow()->GetOSWindowHandle());
			}
		}
		TSharedPtr<SWindow> Active = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (Active.IsValid() && Active->GetNativeWindow().IsValid())
		{
			return static_cast<HWND>(Active->GetNativeWindow()->GetOSWindowHandle());
		}
		return nullptr;
	}

	bool RegisterDevices()
	{
		// INPUTSINK: guns keep aiming while a panel or the border holds focus quirks;
		// DEVNOTIFY: WM_INPUT_DEVICE_CHANGE on unplug/replug. NO RIDEV_NOLEGACY - the
		// merged cursor must keep working for Slate buttons and the whole 1P path.
		RAWINPUTDEVICE Devices[2] = {};
		Devices[0].usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
		Devices[0].usUsage = 0x02;     // HID_USAGE_GENERIC_MOUSE
		Devices[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
		Devices[0].hwndTarget = TargetHwnd;
		Devices[1] = Devices[0];
		Devices[1].usUsage = 0x06;     // HID_USAGE_GENERIC_KEYBOARD
		if (!::RegisterRawInputDevices(Devices, 2, sizeof(RAWINPUTDEVICE)))
		{
			UE_LOG(LogLightgunLab, Warning, TEXT("RegisterRawInputDevices failed (%u)"), ::GetLastError());
			return false;
		}
		bDevicesRegistered = true;
		return true;
	}

	bool ReassertRegistration(float)
	{
		CheckAndReassertRegistration();
		return true;
	}

	void CheckAndReassertRegistration()
	{
		UINT Count = 0;
		::GetRegisteredRawInputDevices(nullptr, &Count, sizeof(RAWINPUTDEVICE));
		bool bMouseOk = false, bKeyboardOk = false;
		if (Count > 0)
		{
			TArray<RAWINPUTDEVICE> Registered;
			Registered.SetNumZeroed(Count);
			const UINT Filled = ::GetRegisteredRawInputDevices(Registered.GetData(), &Count, sizeof(RAWINPUTDEVICE));
			if (Filled != static_cast<UINT>(-1))
			{
				for (UINT Index = 0; Index < Filled; ++Index)
				{
					if (Registered[Index].usUsagePage != 0x01 || Registered[Index].hwndTarget != TargetHwnd)
					{
						continue;
					}
					bMouseOk |= Registered[Index].usUsage == 0x02 && (Registered[Index].dwFlags & RIDEV_INPUTSINK) != 0;
					bKeyboardOk |= Registered[Index].usUsage == 0x06 && (Registered[Index].dwFlags & RIDEV_INPUTSINK) != 0;
				}
			}
		}
		if (!bMouseOk || !bKeyboardOk)
		{
			// Log the loss once per episode - during an editor capture this check
			// re-fights every 250ms and would otherwise spam.
			if (bLastRegistrationOk)
			{
				UE_LOG(LogLightgunLab, Log, TEXT("Raw input registration was replaced elsewhere; re-asserting"));
			}
			bLastRegistrationOk = false;
			RegisterDevices();
		}
		else
		{
			bLastRegistrationOk = true;
		}
	}

	void HandleMouse(HANDLE Device, const RAWMOUSE& Mouse)
	{
		const bool bAbsolute = (Mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0;
		const int32 Player = ResolveMousePlayer(Device, bAbsolute);
		if (Player == INDEX_NONE)
		{
			return;
		}
		FPlayerRuntime& RT = Runtime[Player];

		// Movement first: button flags in the same packet fire from the fresh position.
		if (bAbsolute)
		{
			int32 Left = 0, Top = 0, Width = 0, Height = 0;
			if (Mouse.usFlags & MOUSE_VIRTUAL_DESKTOP)
			{
				Left = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
				Top = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
				Width = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
				Height = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
			}
			else
			{
				Width = ::GetSystemMetrics(SM_CXSCREEN);
				Height = ::GetSystemMetrics(SM_CYSCREEN);
			}
			if (Width > 0 && Height > 0)
			{
				RT.Pos.X = Left + Mouse.lLastX / 65535.0f * Width;
				RT.Pos.Y = Top + Mouse.lLastY / 65535.0f * Height;
				RT.bHasAim = true;
				if (MouseToPlayer.Contains(Device))
				{
					RT.bMappedDeviceProducedAim = true;
				}
				OnAim.Broadcast(Player, RT.Pos);
			}
		}
		else if (Mouse.lLastX != 0 || Mouse.lLastY != 0)
		{
			if (!RT.bHasAim)
			{
				RT.Pos = GetGameWindowCenter();
			}
			RT.Pos.X += Mouse.lLastX;
			RT.Pos.Y += Mouse.lLastY;
			ClampToGameWindow(RT.Pos);
			RT.bHasAim = true;
			OnAim.Broadcast(Player, RT.Pos);
		}

		const USHORT Buttons = Mouse.usButtonFlags;
		if (Buttons & RI_MOUSE_LEFT_BUTTON_DOWN)
		{
			OnTrigger.Broadcast(Player, RT.Pos);
		}
		if (Buttons & (RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_5_DOWN))
		{
			OnReload.Broadcast(Player, TEXT("gun button"));
		}
	}

	int32 ResolveMousePlayer(HANDLE Device, bool bAbsolute)
	{
		if (Device)
		{
			if (const int32* Found = MouseToPlayer.Find(Device))
			{
				return *Found;
			}
		}
		else if (bAbsolute && NullAbsolutePlayer != INDEX_NONE)
		{
			return NullAbsolutePlayer;
		}

		if (bAbsolute)
		{
			auto CanAdopt = [this](int32 Player)
			{
				const FPlayerBinding& B = Bindings[Player];
				return B.bActive && !B.bDesktopMouse &&
					!Runtime[Player].bMappedDeviceProducedAim && !Runtime[Player].bAdoptedOrphan;
			};
			auto Adopt = [this](int32 Player, HANDLE AdoptedDevice)
			{
				Runtime[Player].bAdoptedOrphan = true;
				if (AdoptedDevice)
				{
					MouseToPlayer.Add(AdoptedDevice, Player);
					UE_LOG(LogLightgunLab, Log, TEXT("Raw input: adopted virtual-driver mouse %p for P%d"), AdoptedDevice, Player + 1);
				}
				else
				{
					NullAbsolutePlayer = Player;
					UE_LOG(LogLightgunLab, Log, TEXT("Raw input: adopted injected (device-less) absolute aim for P%d"), Player + 1);
				}
				return Player;
			};

			if (Device)
			{
				// A REAL absolute device we couldn't match by path/parent/VIDPID.
				// Only a player whose gun aims through a virtual driver mouse
				// (GunCon 3) may adopt it - a spare gun on the desk must never
				// steer a player it wasn't assigned to.
				for (int32 Player = 0; Player < NumPlayers; ++Player)
				{
					if (Bindings[Player].bVirtualDriverAim && CanAdopt(Player))
					{
						return Adopt(Player, Device);
					}
				}
				return INDEX_NONE;
			}

			// DEVICE-LESS (SendInput-injected) aim: a vendor app moving the cursor
			// for its gun. Prefer non-virtual-driver players (a virtual-driver gun's
			// aim comes as a real device), then anyone still aimless.
			for (int32 Player = 0; Player < NumPlayers; ++Player)
			{
				if (!Bindings[Player].bVirtualDriverAim && CanAdopt(Player))
				{
					return Adopt(Player, nullptr);
				}
			}
			for (int32 Player = 0; Player < NumPlayers; ++Player)
			{
				if (CanAdopt(Player))
				{
					return Adopt(Player, nullptr);
				}
			}
			return INDEX_NONE;
		}

		// Relative + unmatched = the desk mouse; it drives the desktop-mouse player.
		for (int32 Player = 0; Player < NumPlayers; ++Player)
		{
			if (Bindings[Player].bActive && Bindings[Player].bDesktopMouse)
			{
				return Player;
			}
		}
		return INDEX_NONE;
	}

	void HandleKeyboard(HANDLE Device, const RAWKEYBOARD& Keyboard)
	{
		if (Keyboard.VKey == 0xFF)
		{
			return; // escape-sequence noise, not a key
		}
		TSet<uint16>& Pressed = PressedKeys.FindOrAdd(Device);
		if (Keyboard.Flags & RI_KEY_BREAK)
		{
			Pressed.Remove(Keyboard.VKey);
			return;
		}
		if (Pressed.Contains(Keyboard.VKey))
		{
			return; // auto-repeat
		}
		Pressed.Add(Keyboard.VKey);

		// Correlated keyboards (gun buttons presenting as keys) reload their player;
		// anything else - the desk keyboard, injected events - reloads P1.
		int32 Player = 0;
		if (Device)
		{
			if (const int32* Found = KeyboardToPlayer.Find(Device))
			{
				Player = *Found;
			}
		}
		if (Bindings[Player].bActive)
		{
			OnReload.Broadcast(Player, TEXT("key"));
		}
	}

	FVector2f GetGameWindowCenter() const
	{
		RECT Client = {};
		POINT Origin = { 0, 0 };
		if (TargetHwnd && ::GetClientRect(TargetHwnd, &Client) && ::ClientToScreen(TargetHwnd, &Origin))
		{
			return FVector2f(Origin.x + (Client.right - Client.left) * 0.5f, Origin.y + (Client.bottom - Client.top) * 0.5f);
		}
		return FVector2f::ZeroVector;
	}

	void ClampToGameWindow(FVector2f& Pos) const
	{
		RECT Client = {};
		POINT Origin = { 0, 0 };
		if (TargetHwnd && ::GetClientRect(TargetHwnd, &Client) && ::ClientToScreen(TargetHwnd, &Origin))
		{
			Pos.X = FMath::Clamp(Pos.X, static_cast<float>(Origin.x), static_cast<float>(Origin.x + Client.right - Client.left - 1));
			Pos.Y = FMath::Clamp(Pos.Y, static_cast<float>(Origin.y), static_cast<float>(Origin.y + Client.bottom - Client.top - 1));
		}
	}

	FPlayerBinding Bindings[NumPlayers];
	FPlayerRuntime Runtime[NumPlayers];
	TMap<HANDLE, int32> MouseToPlayer;
	TMap<HANDLE, int32> KeyboardToPlayer;
	TMap<HANDLE, TSet<uint16>> PressedKeys;
	int32 NullAbsolutePlayer = INDEX_NONE;
	HWND TargetHwnd = nullptr;
	bool bStarted = false;
	bool bHandlerAdded = false;
	bool bDevicesRegistered = false;
	bool bMapDirty = false;
	bool bLastRegistrationOk = true;
	FTSTicker::FDelegateHandle ReassertHandle;
};

TSharedPtr<FLightgunRawInputRouter> FLightgunRawInputRouter::Create()
{
	return MakeShared<FLightgunRawInputRouterWindows>();
}

#else // !PLATFORM_WINDOWS

TSharedPtr<FLightgunRawInputRouter> FLightgunRawInputRouter::Create()
{
	return nullptr;
}

#endif // PLATFORM_WINDOWS
