#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LightgunTypes.h"
#include "LightgunSubsystem.generated.h"

class IRecoilBackend;
class FMameOutputServer;
class FMameWindowBroadcaster;
class ULightgunBorderWidget;
class ULightgunStartupPanel;
class ULightgunOptionsPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLightgunStatusChanged);

/**
 * Game-facing lightgun API. The weapon code only reports what happened
 * (live shot / dry fire / ammo count); this subsystem routes it to the active
 * gun backend and/or the MAME-compatible output emitters. The ammo gate lives
 * in the caller: FireRecoil() for live rounds, NotifyEmpty() for dry fire.
 */
UCLASS()
class LIGHTGUNLAB_API ULightgunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual ~ULightgunSubsystem() override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Detection / selection ---

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void ScanForLightguns();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	const TArray<FDetectedLightgun>& GetDetectedGuns() const { return DetectedGuns; }

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	bool SelectGunByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SelectMouseOnly();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	bool HasActiveGun() const { return ActiveGunIndex != INDEX_NONE; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	FDetectedLightgun GetActiveGun() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lightgun")
	FString GetStatusSummary() const;

	// --- Control lifecycle ---

	/** Seize the gun's feedback channel (call at match start; startup panel does it on confirm). */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void BeginGameControl();

	/** Return the gun to self-control (call at match end; automatic on shutdown). */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void EndGameControl();

	// --- Game events (the ammo gate is the caller's job) ---

	/** A live round fired: recoil now. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void FireRecoil();

	/** Trigger pulled on an empty magazine: no solenoid; RS3 gets Z0, Sinden gets the soft empty-chamber clunk. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void NotifyEmpty();

	/** Feed the live ammo count (drives P{n}_Ammo output + OpenFIRE OLED). */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetAmmo(int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetLife(int32 Value);

	/** Player took damage: rumble pulse + P{n}_Damaged output. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void NotifyDamaged();

	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void RumblePulse();

	/** Raw gun effect passthrough, e.g. Sinden "T2200" shotgun rack. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void PlayGunEffect(const FString& Effect);

	/** Enters control if needed and fires one recoil — wired to the Test Fire buttons. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void TestFire();

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

	UPROPERTY(BlueprintAssignable, Category = "Lightgun")
	FOnLightgunStatusChanged OnStatusChanged;

private:
	void OnPostLoadMap(class UWorld* World);
	void EmitOutput(const FString& ShortName, int32 Value);
	void EmitOutputPulse(const FString& ShortName);
	void StartOutputServersIfEnabled();
	void StopOutputServers();
	void TeardownBackend();
	void RestoreSavedSelection();
	FString PlayerPrefixed(const FString& ShortName) const;

	TArray<FDetectedLightgun> DetectedGuns;
	int32 ActiveGunIndex = INDEX_NONE;
	bool bInGameControl = false;
	bool bStartupPanelShown = false;
	FString LastError;

	TSharedPtr<IRecoilBackend> Backend;
	TSharedPtr<FMameOutputServer> TcpOutputs;
	TSharedPtr<FMameWindowBroadcaster> WindowOutputs;

	UPROPERTY(Transient)
	TObjectPtr<ULightgunBorderWidget> BorderWidget;

	UPROPERTY(Transient)
	TObjectPtr<ULightgunStartupPanel> StartupPanel;

	UPROPERTY(Transient)
	TObjectPtr<ULightgunOptionsPanel> OptionsPanel;

	FDelegateHandle PostLoadMapHandle;
};
