// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunDetection.h"
#include "LightgunSettings.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <tlhelp32.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	// {4D1E55B2-F16F-11CF-88CB-001111000030} — GUID_DEVINTERFACE_HID
	static const GUID HidInterfaceGuid = { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };

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
}
#endif // PLATFORM_WINDOWS

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

	// --- Pass 2: Sinden — HID mouse VID 16C0 PID 0F01/0F38/0F39; recoil rides its software's TCP server ---
	{
		bool bSindenHidPresent = false;
		int32 SindenPid = 0;

		HDEVINFO DevInfo = SetupDiGetClassDevsW(&HidInterfaceGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
		if (DevInfo != INVALID_HANDLE_VALUE)
		{
			SP_DEVICE_INTERFACE_DATA InterfaceData = {};
			InterfaceData.cbSize = sizeof(InterfaceData);
			for (DWORD Index = 0; SetupDiEnumDeviceInterfaces(DevInfo, nullptr, &HidInterfaceGuid, Index, &InterfaceData); ++Index)
			{
				DWORD RequiredSize = 0;
				SetupDiGetDeviceInterfaceDetailW(DevInfo, &InterfaceData, nullptr, 0, &RequiredSize, nullptr);
				if (RequiredSize == 0 || RequiredSize > 1024)
				{
					continue;
				}
				TArray<uint8> Buffer;
				Buffer.SetNumZeroed(RequiredSize);
				PSP_DEVICE_INTERFACE_DETAIL_DATA_W Detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(Buffer.GetData());
				Detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
				if (SetupDiGetDeviceInterfaceDetailW(DevInfo, &InterfaceData, Detail, RequiredSize, nullptr, nullptr))
				{
					const FString Path = FString(Detail->DevicePath).ToLower();
					if (Path.Contains(TEXT("vid_16c0")) &&
						(Path.Contains(TEXT("pid_0f01")) || Path.Contains(TEXT("pid_0f38")) || Path.Contains(TEXT("pid_0f39"))))
					{
						bSindenHidPresent = true;
						SindenPid = Path.Contains(TEXT("pid_0f39")) ? 0x0F39 : (Path.Contains(TEXT("pid_0f38")) ? 0x0F38 : 0x0F01);
						break;
					}
				}
			}
			SetupDiDestroyDeviceInfoList(DevInfo);
		}

		if (bSindenHidPresent)
		{
			FDetectedLightgun Gun;
			Gun.Model = ELightgunModel::Sinden;
			Gun.Vid = 0x16C0;
			Gun.Pid = SindenPid;
			Gun.bRecoilCapable = true;
			Gun.DisplayName = TEXT("Sinden Lightgun");
			if (!IsSindenSoftwareRunning())
			{
				Gun.DetectionNote = TEXT("Sinden software is NOT running - start it (aiming and recoil both need it).");
			}
			OutGuns.Add(MoveTemp(Gun));
		}
	}
#endif // PLATFORM_WINDOWS

	UE_LOG(LogLightgunLab, Log, TEXT("Lightgun scan found %d device(s)"), OutGuns.Num());
}
