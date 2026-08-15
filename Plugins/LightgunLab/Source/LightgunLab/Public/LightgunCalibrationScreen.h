// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LightgunCalibrationScreen.generated.h"

class UTextBlock;
class ULightgunWeapon;

/**
 * Aim test range with a live 6-round weapon per player: a crosshair tracks each gun,
 * on-screen shots spend ammo and fire recoil, an empty magazine dry-fires
 * (no recoil - the ammo gate), and reload comes from an offscreen shot,
 * any non-trigger gun/mouse button, or a keyboard key. The range is the
 * home screen: the only ways out are back to gun select.
 *
 * One player: aim is the merged OS cursor and input arrives through Slate -
 * the v0.3 path, byte for byte. Two players: aim, triggers, and reloads come
 * exclusively from the Raw Input router (per-device, so the merged cursor
 * can't double-fire); Slate keeps only the panel buttons clickable.
 */
UCLASS(Blueprintable)
class LIGHTGUNLAB_API ULightgunCalibrationScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Which mode the widget tree was built for; a mode switch needs a fresh widget. */
	bool WasBuiltForTwoPlayer() const { return bTwoPlayerMode; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnCrosshairToggleClicked();

	UFUNCTION()
	void OnSwapClicked();

private:
	class ULightgunSubsystem* GetLightgun() const;
	void HandleTriggerPull(const FVector2f& LocalPos);
	void DoReload(const FString& Reason);
	void RefreshCounterText();

	// --- Two players ---
	void HandleRawAim(int32 PlayerIndex, FVector2f DesktopPos);
	void HandleRawTrigger(int32 PlayerIndex, FVector2f DesktopPos);
	void HandleRawReload(int32 PlayerIndex, const FString& Reason);
	void HandleTriggerPullForPlayer(int32 PlayerIndex, const FVector2f& LocalPos);
	void DoReloadForPlayer(int32 PlayerIndex, const FString& Reason);
	void RefreshCounterTextForPlayer(int32 PlayerIndex);
	void RefreshRouterInfo();
	FVector2f DesktopToLocal(const FVector2f& DesktopPos) const;
	FLinearColor GetPlayerColor(int32 PlayerIndex) const;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CounterText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CrosshairButtonLabel;
	UPROPERTY(Transient) TObjectPtr<ULightgunWeapon> Weapon;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayerStatusText[2];
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayerCounterText[2];
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RouterInfoText;
	UPROPERTY(Transient) TObjectPtr<ULightgunWeapon> PlayerWeapons[2];

	struct FHitMarker
	{
		FVector2f Pos = FVector2f::ZeroVector;
		double Time = 0.0;
		int32 PlayerIndex = 0;
	};

	TArray<FHitMarker> HitMarkers;
	FVector2f CrosshairPos = FVector2f::ZeroVector;
	double LastHitTime = -10.0;
	int32 HitCount = 0;
	int32 DryFireCount = 0;
	int32 ReloadCount = 0;
	bool bCrosshairVisible = true;

	bool bTwoPlayerMode = false;
	FVector2f PlayerCrosshairPos[2] = { FVector2f::ZeroVector, FVector2f::ZeroVector };
	bool bPlayerHasAim[2] = { false, false };
	double PlayerLastHitTime[2] = { -10.0, -10.0 };
	int32 PlayerHitCount[2] = { 0, 0 };
	int32 PlayerDryFireCount[2] = { 0, 0 };
	int32 PlayerReloadCount[2] = { 0, 0 };
	float RouterInfoRefreshAccumulator = 0.f;

	FDelegateHandle AimHandle;
	FDelegateHandle TriggerHandle;
	FDelegateHandle ReloadHandle;
};
