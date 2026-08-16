// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "LightgunTypes.generated.h"

UENUM(BlueprintType)
enum class ELightgunModel : uint8
{
	None			UMETA(DisplayName = "None / Mouse only"),
	Gun4IR			UMETA(DisplayName = "GUN4IR"),
	OpenFIRE		UMETA(DisplayName = "OpenFIRE"),
	Blamcon			UMETA(DisplayName = "Blamcon"),
	RS3Reaper		UMETA(DisplayName = "Retro Shooter RS3 Reaper"),
	Sinden			UMETA(DisplayName = "Sinden"),
	/** Namco GunCon 3 via the community Windows driver: aim-only (no recoil or
	    rumble hardware), and the aim arrives on the driver's VIRTUAL mouse rather
	    than a device carrying Namco's IDs. */
	GunCon3			UMETA(DisplayName = "GunCon 3"),
	UnknownSerial	UMETA(DisplayName = "Unknown serial device")
};

UENUM(BlueprintType)
enum class ERecoilMode : uint8
{
	DirectSerial	UMETA(DisplayName = "Direct (game controls the gun)"),
	OutputsOnly		UMETA(DisplayName = "Outputs (MAMEHooker/QMamehook rig controls the gun)"),
	Disabled		UMETA(DisplayName = "Disabled")
};

/** User-configurable USB ID -> gun model mapping for hardware the built-in table doesn't know. */
USTRUCT(BlueprintType)
struct LIGHTGUNLAB_API FLightgunIdOverride
{
	GENERATED_BODY()

	/** USB vendor id, e.g. 0x2341. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightgun")
	int32 Vid = 0;

	/** USB product id, e.g. 0x8046. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightgun")
	int32 Pid = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightgun")
	ELightgunModel Model = ELightgunModel::Gun4IR;
};

USTRUCT(BlueprintType)
struct LIGHTGUNLAB_API FDetectedLightgun
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	ELightgunModel Model = ELightgunModel::None;

	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	FString DisplayName;

	/** e.g. "COM5". Empty for Sinden (controlled via its software's TCP server). */
	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	FString ComPort;

	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	int32 Vid = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	int32 Pid = 0;

	/** 1-based player slot guess from the device's PID where the vendor encodes it. */
	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	int32 PlayerHint = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	bool bRecoilCapable = false;

	/** Caveats surfaced to the UI, e.g. "USB ID match unverified" or "Sinden software not running". */
	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	FString DetectionNote;

	/** Raw Input device interface path of this gun's HID mouse, when detection saw it directly
	    (Sinden). Lets the 2P router match WM_INPUT hDevice exactly, even for identical PIDs. */
	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	FString RawInputMousePath;

	/** Device instance ID of the physical USB (composite) device, e.g. "USB\VID_2341&PID_8046\HIDPC".
	    For serial guns this ties the COM port to its sibling HID mouse interface so two identical
	    guns can still be told apart per-device. Empty when the walk failed. */
	UPROPERTY(BlueprintReadOnly, Category = "Lightgun")
	FString UsbCompositeParentId;
};

/** Number of local player slots the plugin supports (v0.4: two guns, one PC). */
inline constexpr int32 LightgunMaxPlayers = 2;

DECLARE_LOG_CATEGORY_EXTERN(LogLightgunLab, Log, All);
