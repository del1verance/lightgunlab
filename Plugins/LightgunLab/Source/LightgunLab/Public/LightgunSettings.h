// Copyright (c) 2026 del1verance. MIT License.

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

	/** Open the aim test range when the picker is confirmed. The plugin's own test
	    bed keeps this on (the range is its home screen and has no exit into a game);
	    host games set it false so confirm hands straight back to their flow -
	    StartRangeSession() still runs either way (game control + raw aim routing),
	    and the range stays reachable via ShowCalibrationScreen(). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "General")
	bool bShowRangeOnConfirm = true;

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

	// --- Two players, one PC ---

	/** Last mode chosen on the startup panel. In 2P the range runs both guns off Raw Input. */
	UPROPERTY(config)
	bool bTwoPlayerMode = false;

	/** Offer "Desktop mouse (aim only)" as a pickable player device in 2P mode. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Two Player")
	bool bAllowMouseAsGun = true;

	/** P1 crosshair (arcade convention: blue). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Two Player")
	FLinearColor CrosshairColorP1 = FLinearColor(0.25f, 0.55f, 1.f);

	/** P2 crosshair (arcade convention: red). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Two Player")
	FLinearColor CrosshairColorP2 = FLinearColor(1.f, 0.3f, 0.25f);

	/** Persisted P2 pick. Model None + bPreferredP2IsMouse means "Desktop mouse (aim only)". */
	UPROPERTY(config)
	ELightgunModel PreferredModelP2 = ELightgunModel::None;

	UPROPERTY(config)
	FString PreferredComPortP2;

	UPROPERTY(config)
	bool bPreferredP2IsMouse = true;

	// --- Sinden ---

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden")
	FString SindenHost = TEXT("127.0.0.1");

	/** Matches SindenTcpPort in Lightgun.exe.config (V2.08b default 13000). Software must run with the recoil server started. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden")
	int32 SindenTcpPort = 13000;

	/** Strength 0-10 for the soft empty-chamber clunk (Sinden U command) on dry fire. Sinden's docs suggest 4-5. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden", meta = (ClampMin = "0", ClampMax = "10"))
	int32 SindenEmptyChamberStrength = 4;

	/** Strength 1-10 for the Sinden's vibration stand-in (its lightest feelable solenoid tap) on
	    reload, damage rumble, and Test vibration. U1-U2 sit below the solenoid's physical actuation
	    threshold on the bench gun - 3 is the lightest default that actually registers. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden", meta = (ClampMin = "1", ClampMax = "10"))
	int32 SindenVibrationStrength = 3;

	/** Minimum ms between commands to the Sinden recoil server. Its reader treats each receive as one
	    command and chokes on merged messages; 200ms is bench-safe, lower at your own risk. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Sinden", meta = (ClampMin = "15", ClampMax = "1000"))
	int32 SindenCommandGapMs = 200;

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

	/** Extra USB VID/PID -> model mappings, checked before the built-in table.
	    For guns whose firmware ships identifiers we don't know yet. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Detection")
	TArray<FLightgunIdOverride> IdOverrides;
};
