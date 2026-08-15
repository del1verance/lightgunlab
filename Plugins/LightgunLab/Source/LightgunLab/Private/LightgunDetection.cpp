// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunDetection.h"
#include "LightgunSettings.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <tlhelp32.h>
#include <cfgmgr32.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	bool ParseVidPid(const FString& HardwareId, int32& OutVid, int32& OutPid)
	{
		const FString Upper = HardwareId.ToUpper();
		const int32 VidIdx = Upper.Find(TEXT("VID_"));
		const int32 PidIdx = Upper.Find(TEXT("PID_"));
		if (VidIdx == INDEX_NONE || PidIdx == INDEX_NONE)
		{
			return false;
		}
		OutVid = FParse::HexNumber(*Upper.Mid(VidIdx + 4, 4));
		OutPid = FParse::HexNumber(*Upper.Mid(PidIdx + 4, 4));
		return true;
	}

	FString ReadPortName(HDEVINFO DevInfo, SP_DEVINFO_DATA& DevData)
	{
		HKEY Key = SetupDiOpenDevRegKey(DevInfo, &DevData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
		if (Key == INVALID_HANDLE_VALUE)
		{
			return FString();
		}
		WCHAR Buffer[64] = {};
		DWORD Size = sizeof(Buffer);
		DWORD Type = 0;
		FString Result;
		if (RegQueryValueExW(Key, L"PortName", nullptr, &Type, reinterpret_cast<LPBYTE>(Buffer), &Size) == ERROR_SUCCESS && Type == REG_SZ)
		{
			Result = FString(Buffer);
		}
		RegCloseKey(Key);
		return Result;
	}

	FString ReadStringProperty(HDEVINFO DevInfo, SP_DEVINFO_DATA& DevData, DWORD Property)
	{
		WCHAR Buffer[512] = {};
		DWORD Size = sizeof(Buffer);
		if (SetupDiGetDeviceRegistryPropertyW(DevInfo, &DevData, Property, nullptr, reinterpret_cast<PBYTE>(Buffer), Size, &Size))
		{
			// REG_MULTI_SZ properties: first string is enough for VID/PID matching.
			return FString(Buffer);
		}
		return FString();
	}

	/** True for a devnode ID naming the physical USB device rather than one of its interfaces. */
	bool IsPhysicalUsbDeviceId(const FString& UpperId)
	{
		return UpperId.StartsWith(TEXT("USB\\")) && UpperId.Contains(TEXT("VID_")) &&
			UpperId.Contains(TEXT("PID_")) && !UpperId.Contains(TEXT("&MI_"));
	}

	/** Walks parents from a devnode until the physical USB device instance ID is found. */
	FString ResolveCompositeParentFromDevNode(DEVINST DevNode)
	{
		DEVINST Current = DevNode;
		for (int32 Hop = 0; Hop < 4; ++Hop)
		{
			WCHAR IdBuffer[MAX_DEVICE_ID_LEN] = {};
			if (CM_Get_Device_IDW(Current, IdBuffer, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
			{
				return FString();
			}
			const FString Id = FString(IdBuffer).ToUpper();
			if (IsPhysicalUsbDeviceId(Id))
			{
				return Id;
			}
			DEVINST Parent = 0;
			if (CM_Get_Parent(&Parent, Current, 0) != CR_SUCCESS)
			{
				return FString();
			}
			Current = Parent;
		}
		return FString();
	}
}
#endif // PLATFORM_WINDOWS

FString FLightgunDetector::ResolveUsbCompositeParent(const FString& DeviceInterfacePath)
{
#if PLATFORM_WINDOWS
	// "\\?\HID#VID_2341&PID_8046&MI_01#7&2f5c8e2a&0&0000#{guid}" ->
	// instance ID "HID\VID_2341&PID_8046&MI_01\7&2f5c8e2a&0&0000", then walk to the USB parent.
	FString Work = DeviceInterfacePath;
	if (Work.StartsWith(TEXT("\\\\?\\")) || Work.StartsWith(TEXT("\\\\.\\")))
	{
		Work.RightChopInline(4);
	}
	const int32 GuidStart = Work.Find(TEXT("#{"));
	if (GuidStart != INDEX_NONE)
	{
		Work.LeftInline(GuidStart);
	}
	Work.ReplaceCharInline(TEXT('#'), TEXT('\\'));

	DEVINST DevNode = 0;
	// The device is present (we just got raw input / an enumeration hit for it), but be
	// lenient and accept phantoms too - the walk only reads IDs.
	if (CM_Locate_DevNodeW(&DevNode, const_cast<WCHAR*>(*Work), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS &&
		CM_Locate_DevNodeW(&DevNode, const_cast<WCHAR*>(*Work), CM_LOCATE_DEVNODE_PHANTOM) != CR_SUCCESS)
	{
		return FString();
	}
	return ResolveCompositeParentFromDevNode(DevNode);
#else
	return FString();
#endif
}

bool FLightgunDetector::IsSindenSoftwareRunning()
{
#if PLATFORM_WINDOWS
	HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (Snapshot == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	PROCESSENTRY32W Entry = {};
	Entry.dwSize = sizeof(Entry);
	bool bFound = false;
	if (Process32FirstW(Snapshot, &Entry))
	{
		do
		{
			if (FCString::Stricmp(Entry.szExeFile, TEXT("Lightgun.exe")) == 0)
			{
				bFound = true;
				break;
			}
		} while (Process32NextW(Snapshot, &Entry));
	}
	CloseHandle(Snapshot);
	return bFound;
#else
	return false;
#endif
}

void FLightgunDetector::Scan(TArray<FDetectedLightgun>& OutGuns)
{
	OutGuns.Reset();

#if PLATFORM_WINDOWS
	// --- Pass 1: COM-port class devices (GUN4IR / OpenFIRE / Blamcon / RS3) ---
	{
		HDEVINFO DevInfo = SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
		if (DevInfo != INVALID_HANDLE_VALUE)
		{
			SP_DEVINFO_DATA DevData = {};
			DevData.cbSize = sizeof(DevData);
			for (DWORD Index = 0; SetupDiEnumDeviceInfo(DevInfo, Index, &DevData); ++Index)
			{
				const FString HardwareId = ReadStringProperty(DevInfo, DevData, SPDRP_HARDWAREID);
				const FString PortName = ReadPortName(DevInfo, DevData);
				if (PortName.IsEmpty() || !PortName.StartsWith(TEXT("COM")))
				{
					continue;
				}

				int32 Vid = 0, Pid = 0;
				if (!ParseVidPid(HardwareId, Vid, Pid))
				{
					continue;
				}

				FDetectedLightgun Gun;
				Gun.ComPort = PortName;
				Gun.Vid = Vid;
				Gun.Pid = Pid;
				Gun.bRecoilCapable = true;
				Gun.UsbCompositeParentId = ResolveCompositeParentFromDevNode(DevData.DevInst);

				const FLightgunIdOverride* Override = GetDefault<ULightgunSettings>()->IdOverrides.FindByPredicate(
					[Vid, Pid](const FLightgunIdOverride& O) { return O.Vid == Vid && O.Pid == Pid; });

				if (Override)
				{
					Gun.Model = Override->Model;
					Gun.DisplayName = FString::Printf(TEXT("%s (%s, user mapping)"),
						*StaticEnum<ELightgunModel>()->GetDisplayNameTextByValue(static_cast<int64>(Override->Model)).ToString(), *PortName);
				}
				else if (Vid == 0x2341 && (Pid & 0xFFF0) == 0x8040)
				{
					// GUN4IR firmware uses the 0x804x block; 8042/8043 are the community-
					// documented P1/P2, but revisions differ (8046 seen on the bench).
					Gun.Model = ELightgunModel::Gun4IR;
					if (Pid >= 0x8042 && Pid <= 0x8045)
					{
						Gun.PlayerHint = (Pid - 0x8042) + 1;
						Gun.DisplayName = FString::Printf(TEXT("GUN4IR (P%d, %s)"), Gun.PlayerHint, *PortName);
					}
					else
					{
						Gun.DisplayName = FString::Printf(TEXT("GUN4IR (%s, PID %04X)"), *PortName, Pid);
					}
				}
				else if (Vid == 0xF143)
				{
					Gun.Model = ELightgunModel::OpenFIRE;
					Gun.DisplayName = FString::Printf(TEXT("OpenFIRE (%s)"), *PortName);
				}
				else if (Vid == 0x3673)
				{
					Gun.Model = ELightgunModel::Blamcon;
					Gun.DisplayName = FString::Printf(TEXT("Blamcon (%s)"), *PortName);
				}
				else if (Vid == 0x0483 && Pid == 0x5740)
				{
					Gun.Model = ELightgunModel::RS3Reaper;
					Gun.DisplayName = FString::Printf(TEXT("RS3 Reaper (%s)"), *PortName);
					Gun.DetectionNote = TEXT("USB ID match is community-sourced; confirm and pick manually if this is not an RS3.");
				}
				else
				{
					// Listed for the manual-override dropdown; never auto-selected, never written to.
					Gun.Model = ELightgunModel::UnknownSerial;
					Gun.bRecoilCapable = false;
					Gun.DisplayName = FString::Printf(TEXT("Unknown serial device (%s, VID %04X PID %04X)"), *PortName, Vid, Pid);
				}

				OutGuns.Add(MoveTemp(Gun));
			}
			SetupDiDestroyDeviceInfoList(DevInfo);
		}
	}

	// --- Pass 2: Sinden — HID mice VID 16C0 PID 0F01/0F02/0F38/0F39; recoil rides its software's TCP server ---
	// Enumerated through the Raw Input device list so each PHYSICAL gun (= one HID mouse
	// device) becomes its own entry, and its raw device path is captured for the 2P router.
	{
		struct FSindenMouse
		{
			FString Path;
			FString ParentId;
			int32 Pid = 0;
			int32 PlayerHint = 1;
		};
		TArray<FSindenMouse> SindenMice;

		UINT DeviceCount = 0;
		if (GetRawInputDeviceList(nullptr, &DeviceCount, sizeof(RAWINPUTDEVICELIST)) == 0 && DeviceCount > 0)
		{
			TArray<RAWINPUTDEVICELIST> Devices;
			Devices.SetNumZeroed(DeviceCount);
			const UINT Filled = GetRawInputDeviceList(Devices.GetData(), &DeviceCount, sizeof(RAWINPUTDEVICELIST));
			if (Filled != static_cast<UINT>(-1))
			{
				for (UINT Index = 0; Index < Filled; ++Index)
				{
					if (Devices[Index].dwType != RIM_TYPEMOUSE)
					{
						continue;
					}
					WCHAR NameBuffer[512] = {};
					UINT NameSize = UE_ARRAY_COUNT(NameBuffer);
					if (GetRawInputDeviceInfoW(Devices[Index].hDevice, RIDI_DEVICENAME, NameBuffer, &NameSize) == static_cast<UINT>(-1))
					{
						continue;
					}
					const FString Path = FString(NameBuffer);
					const FString Lower = Path.ToLower();
					if (!Lower.Contains(TEXT("vid_16c0")))
					{
						continue;
					}
					FSindenMouse Mouse;
					Mouse.Path = Path;
					if (Lower.Contains(TEXT("pid_0f01"))) { Mouse.Pid = 0x0F01; Mouse.PlayerHint = 1; }
					else if (Lower.Contains(TEXT("pid_0f02"))) { Mouse.Pid = 0x0F02; Mouse.PlayerHint = 2; }
					else if (Lower.Contains(TEXT("pid_0f38"))) { Mouse.Pid = 0x0F38; Mouse.PlayerHint = 1; }
					else if (Lower.Contains(TEXT("pid_0f39"))) { Mouse.Pid = 0x0F39; Mouse.PlayerHint = 2; }
					else { continue; }
					Mouse.ParentId = FLightgunDetector::ResolveUsbCompositeParent(Path);

					// One gun can theoretically surface several mouse-class interfaces;
					// collapse anything sharing a physical parent to a single entry.
					const bool bDuplicate = !Mouse.ParentId.IsEmpty() && SindenMice.ContainsByPredicate(
						[&Mouse](const FSindenMouse& Existing) { return Existing.ParentId == Mouse.ParentId; });
					if (!bDuplicate)
					{
						SindenMice.Add(MoveTemp(Mouse));
					}
				}
			}
		}

		// Stable order: the software's "Lightgun A" (P1-model PIDs) first, then by path.
		SindenMice.Sort([](const FSindenMouse& A, const FSindenMouse& B)
		{
			return A.PlayerHint != B.PlayerHint ? A.PlayerHint < B.PlayerHint : A.Path < B.Path;
		});
		// Two same-model guns: the software still assigns one A and one B - guess by
		// list order and let the range's Swap button fix a wrong guess.
		if (SindenMice.Num() >= 2 && SindenMice[0].PlayerHint == SindenMice[1].PlayerHint)
		{
			SindenMice[1].PlayerHint = SindenMice[0].PlayerHint == 1 ? 2 : 1;
		}

		const bool bSoftwareRunning = SindenMice.Num() > 0 ? IsSindenSoftwareRunning() : false;
		for (int32 Index = 0; Index < SindenMice.Num(); ++Index)
		{
			const FSindenMouse& Mouse = SindenMice[Index];
			FDetectedLightgun Gun;
			Gun.Model = ELightgunModel::Sinden;
			Gun.Vid = 0x16C0;
			Gun.Pid = Mouse.Pid;
			Gun.PlayerHint = Mouse.PlayerHint;
			Gun.bRecoilCapable = true;
			Gun.RawInputMousePath = Mouse.Path;
			Gun.UsbCompositeParentId = Mouse.ParentId;
			Gun.DisplayName = SindenMice.Num() == 1
				? TEXT("Sinden Lightgun")
				: FString::Printf(TEXT("Sinden Lightgun %c (PID %04X)"), Mouse.PlayerHint == 1 ? TEXT('A') : TEXT('B'), Mouse.Pid);
			if (!bSoftwareRunning)
			{
				Gun.DetectionNote = TEXT("Sinden software is NOT running - start it (aiming and recoil both need it).");
			}
			OutGuns.Add(MoveTemp(Gun));
		}
	}
#endif // PLATFORM_WINDOWS

	UE_LOG(LogLightgunLab, Log, TEXT("Lightgun scan found %d device(s)"), OutGuns.Num());
}
