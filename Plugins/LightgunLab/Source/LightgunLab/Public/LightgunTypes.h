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
	UnknownSerial	UMETA(DisplayName = "Unknown serial device")
};

UENUM(BlueprintType)
enum class ERecoilMode : uint8
{
	DirectSerial	UMETA(DisplayName = "Direct (game controls the gun)"),
	OutputsOnly		UMETA(DisplayName = "Outputs (MAMEHooker/QMamehook rig controls the gun)"),
	Disabled		UMETA(DisplayName = "Disabled")
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
};

DECLARE_LOG_CATEGORY_EXTERN(LogLightgunLab, Log, All);
