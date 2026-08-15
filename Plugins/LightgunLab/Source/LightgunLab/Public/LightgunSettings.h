#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LightgunTypes.h"
#include "LightgunSettings.generated.h"

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Lightgun Lab"))
class LIGHTGUNLAB_API ULightgunSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "General")
	bool bAutoDetectOnStartup = true;

	/** Show the gun picker panel automatically after the first map loads. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "General")
	bool bShowStartupPanel = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "General")
	ERecoilMode RecoilMode = ERecoilMode::DirectSerial;

	/** 1 or 2. Selects Sinden message prefix and the P{n}_ prefix on emitted outputs. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "General", meta = (ClampMin = "1", ClampMax = "4"))
	int32 PlayerSlot = 1;

	/** 0-10. Applied where the gun supports strength (Sinden N command). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "General", meta = (ClampMin = "0", ClampMax = "10"))
	int32 RecoilStrength = 8;

	/** Persisted result of the user's pick in the startup panel / options menu. */
	UPROPERTY(config)
	ELightgunModel PreferredModel = ELightgunModel::None;

	UPROPERTY(config)
	FString PreferredComPort;

	// --- Sinden ---

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden")
	FString SindenHost = TEXT("127.0.0.1");

	/** Matches SindenTcpPort in Lightgun.exe.config (V2.08b default 13000). Software must run with the recoil server started. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden")
	int32 SindenTcpPort = 13000;

	/** Strength 0-10 for the soft empty-chamber clunk (Sinden U command) on dry fire. Sinden's docs suggest 4-5. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden", meta = (ClampMin = "0", ClampMax = "10"))
	int32 SindenEmptyChamberStrength = 4;

	/** Show the white tracking border automatically whenever a Sinden is the active gun. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden")
	bool bBorderAutoShow = true;

	/** Sinden native-game guidance: white border 2% of screen height, inside a 3% black frame. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden", meta = (ClampMin = "0.5", ClampMax = "8"))
	float BorderWhitePercent = 2.0f;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden", meta = (ClampMin = "0", ClampMax = "10"))
	float BorderBlackPercent = 3.0f;

	// --- Outputs emission (MAMEHooker ecosystem) ---

	/** Run a MAME-network-protocol TCP server so QMamehook / Hook of the Reaper / OutputHooker / Sinden software can consume our events. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Outputs")
	bool bEnableTcpOutputs = false;

	/** Broadcast MAME-style window messages (WM_COPYDATA interop) for classic MAMEHooker 5.1. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Outputs")
	bool bEnableWindowMessageOutputs = false;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Outputs")
	int32 OutputsTcpPort = 8000;

	/** Sent as "mame_start = <name>"; hooker apps use it to pick their <name>.ini. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Outputs")
	FString OutputsGameName = TEXT("lightgunlab");

	/** Output name for the recoil pulse; "CtmRecoil" matches DemulShooter-style inis, becomes P1_CtmRecoil. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Outputs")
	FString RecoilOutputName = TEXT("CtmRecoil");

	// --- Per-gun tuning ---

	/** RS3 fire command Z1..Z5 (progressive side-LED states; Z5 = all lit). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Per-Gun")
	FString RS3FireCommand = TEXT("Z5");

	/** Send the live ammo count to OpenFIRE's OLED via the FDA display command. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Per-Gun")
	bool bOpenFireAmmoDisplay = true;
};
