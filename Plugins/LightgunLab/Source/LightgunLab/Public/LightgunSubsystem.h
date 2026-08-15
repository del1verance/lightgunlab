// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "LightgunTypes.h"
#include "LightgunSubsystem.generated.h"

class IRecoilBackend;
class FMameOutputServer;
class FMameWindowBroadcaster;
class FLightgunRawInputRouter;
class FSindenSharedConnection;
class ULightgunBorderWidget;
class ULightgunStartupPanel;
class ULightgunOptionsPanel;
class ULightgunCalibrationScreen;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLightgunStatusChanged);

/**
 * Game-facing lightgun API. The weapon code only reports what happened
 * (live shot / dry fire / ammo count); this subsystem routes it to the right
 * player's gun backend and/or the MAME-compatible output emitters. The ammo
 * gate lives in the caller: FireRecoil() for live rounds, NotifyEmpty() for
 * dry fire.
 *
 * v0.4: two player slots. The 1P API below is unchanged and routes to player 0;
 * the ForPlayer variants address a slot explicitly. In two-player mode the aim
 * test range takes input from the Raw Input router instead of the merged cursor.
 */
UCLASS()
class LIGHTGUNLAB_API ULightgunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual ~ULightgunSubsystem() override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Detection / selection (1P API = player 0) ---

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void ScanForLightguns();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	const TArray<FDetectedLightgun>& GetDetectedGuns() const { return DetectedGuns; }

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	bool SelectGunByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SelectMouseOnly();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	bool HasActiveGun() const { return HasActiveGunForPlayer(0); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	FDetectedLightgun GetActiveGun() const { return GetActiveGunForPlayer(0); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	FString GetStatusSummary() const { return GetStatusSummaryForPlayer(0); }

	// --- Two players, one PC ---

	/** Switches modes. Leaving 2P tears down P2's backend and stops the raw router. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void SetTwoPlayerMode(bool bEnabled);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun|TwoPlayer")
	bool IsTwoPlayerMode() const;

	/** PlayerIndex 0/1. Refuses the gun the other active slot already holds. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	bool SelectGunForPlayer(int32 PlayerIndex, int32 Index);

	/** Player 0: legacy "mouse only". Player 1: the "Desktop mouse (aim only)" pick. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void SelectMouseForPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun|TwoPlayer")
	bool HasActiveGunForPlayer(int32 PlayerIndex) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun|TwoPlayer")
	FDetectedLightgun GetActiveGunForPlayer(int32 PlayerIndex) const;

	/** True when the slot's pick is the aim-only desktop mouse. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun|TwoPlayer")
	bool IsPlayerDesktopMouse(int32 PlayerIndex) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun|TwoPlayer")
	FString GetStatusSummaryForPlayer(int32 PlayerIndex) const;

	/**
	 * Swaps the two player assignments wholesale (guns, backends, control state).
	 * The bench fix for "my crosshair kicks the other gun" when identical hardware
	 * or the Sinden software's A/B order was guessed wrong.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void SwapPlayers();

	/** Enters game control for every active slot and starts the raw router (2P confirm). */
	void StartTwoPlayerSession();

	/** The raw router, alive during a 2P session. Null in 1P / before confirm / off-Windows. */
	TSharedPtr<FLightgunRawInputRouter> GetRawRouter() const { return RawRouter; }

	// --- Control lifecycle ---

	/** Seize the gun's feedback channel (call at match start; startup panel does it on confirm). */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void BeginGameControl() { BeginGameControlForPlayer(0); }

	/** Return the gun to self-control (call at match end; automatic on shutdown). */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void EndGameControl() { EndGameControlForPlayer(0); }

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void BeginGameControlForPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void EndGameControlForPlayer(int32 PlayerIndex);

	// --- Game events (the ammo gate is the caller's job) ---

	/** A live round fired: recoil now. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void FireRecoil() { FireRecoilForPlayer(0); }

	/** Trigger pulled on an empty magazine: no solenoid; RS3 gets Z0, Sinden gets the soft empty-chamber clunk. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void NotifyEmpty() { NotifyEmptyForPlayer(0); }

	/** Feed the live ammo count (drives P{n}_Ammo output + OpenFIRE OLED). */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetAmmo(int32 Count) { SetAmmoForPlayer(0, Count); }

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetLife(int32 Value) { SetLifeForPlayer(0, Value); }

	/** Player took damage: rumble pulse + P{n}_Damaged output. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void NotifyDamaged() { NotifyDamagedForPlayer(0); }

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void RumblePulse() { RumblePulseForPlayer(0); }

	/** Raw gun effect passthrough, e.g. Sinden "T2200" shotgun rack. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void PlayGunEffect(const FString& Effect) { PlayGunEffectForPlayer(0, Effect); }

	/** Enters control if needed and fires one recoil — wired to the Test Fire buttons. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void TestFire() { TestFireForPlayer(0); }

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void FireRecoilForPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void NotifyEmptyForPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void SetAmmoForPlayer(int32 PlayerIndex, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void SetLifeForPlayer(int32 PlayerIndex, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void NotifyDamagedForPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void RumblePulseForPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void PlayGunEffectForPlayer(int32 PlayerIndex, const FString& Effect);

	UFUNCTION(BlueprintCallable, Category = "Lightgun|TwoPlayer")
	void TestFireForPlayer(int32 PlayerIndex);

	// --- Options ---

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetRecoilMode(ERecoilMode Mode);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	ERecoilMode GetRecoilMode() const;

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetRecoilStrength(int32 Strength);

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetOutputsEnabled(bool bTcp, bool bWindowMessages);

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetBorderVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	bool IsBorderVisible() const;

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetBorderPercents(float WhitePercent, float BlackPercent);

	/** Persists current settings (gun choice, mode, strengths, border) to the user's config. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SaveSettings();

	// --- UI ---

	UFUNCTION(BlueprintCallable, Category = "Lightgun|UI")
	void ShowStartupPanel();

	UFUNCTION(BlueprintCallable, Category = "Lightgun|UI")
	void ShowOptionsPanel();

	/** Aim test range: crosshair tracking, hits fire recoil, offscreen shots stay silent. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun|UI")
	void ShowCalibrationScreen();

	UPROPERTY(BlueprintAssignable, Category = "Lightgun")
	FOnLightgunStatusChanged OnStatusChanged;

private:
	struct FPlayerSlot
	{
		int32 GunIndex = INDEX_NONE;
		/** The explicit aim-only desktop mouse pick (2P). */
		bool bDesktopMouse = false;
		TSharedPtr<IRecoilBackend> Backend;
		bool bInControl = false;
		FString LastError;
	};

	void EmitOutput(int32 PlayerIndex, const FString& ShortName, int32 Value);
	void EmitOutputPulse(int32 PlayerIndex, const FString& ShortName);
	void StartOutputServersIfEnabled();
	void StopOutputServers();
	void TeardownBackend(int32 PlayerIndex);
	void RestoreSavedSelection();
	void PersistSlotPrefs(int32 PlayerIndex);
	void UpdateBorderForTwoPlayer();
	void PushRouterBindings();
	FString PlayerPrefixed(int32 PlayerIndex, const FString& ShortName) const;
	bool IsValidPlayer(int32 PlayerIndex) const { return PlayerIndex >= 0 && PlayerIndex < LightgunMaxPlayers; }

	TArray<FDetectedLightgun> DetectedGuns;
	FPlayerSlot Slots[LightgunMaxPlayers];
	bool bStartupPanelShown = false;

	TSharedPtr<FMameOutputServer> TcpOutputs;
	TSharedPtr<FMameWindowBroadcaster> WindowOutputs;
	TSharedPtr<FLightgunRawInputRouter> RawRouter;

	/** Pins the single process-wide Sinden TCP connection for the whole session so
	    backend churn (reselects, swaps, mode switches) never closes the socket. */
	TSharedPtr<FSindenSharedConnection> SindenKeepalive;

	UPROPERTY(Transient)
	TObjectPtr<ULightgunBorderWidget> BorderWidget;

	UPROPERTY(Transient)
	TObjectPtr<ULightgunStartupPanel> StartupPanel;

	UPROPERTY(Transient)
	TObjectPtr<ULightgunOptionsPanel> OptionsPanel;

	UPROPERTY(Transient)
	TObjectPtr<ULightgunCalibrationScreen> CalibrationScreen;

	FTSTicker::FDelegateHandle StartupPollHandle;
};
