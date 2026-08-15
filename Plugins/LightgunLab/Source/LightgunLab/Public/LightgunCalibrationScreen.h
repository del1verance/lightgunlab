// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LightgunCalibrationScreen.generated.h"

class UTextBlock;
class ULightgunWeapon;

/**
 * Aim test range with a live 6-round weapon: a crosshair tracks the gun,
 * on-screen shots spend ammo and fire recoil, an empty magazine dry-fires
 * (no recoil - the ammo gate), and reload comes from an offscreen shot,
 * any non-trigger gun/mouse button, or any keyboard key. The range is the
 * home screen: the only ways out are back to gun select.
 */
UCLASS(Blueprintable)
class LIGHTGUNLAB_API ULightgunCalibrationScreen : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnCrosshairToggleClicked();

private:
	class ULightgunSubsystem* GetLightgun() const;
	void HandleTriggerPull(const FVector2f& LocalPos);
	void DoReload(const FString& Reason);
	void RefreshCounterText();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CounterText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CrosshairButtonLabel;
	UPROPERTY(Transient) TObjectPtr<ULightgunWeapon> Weapon;

	struct FHitMarker
	{
		FVector2f Pos = FVector2f::ZeroVector;
		double Time = 0.0;
	};

	TArray<FHitMarker> HitMarkers;
	FVector2f CrosshairPos = FVector2f::ZeroVector;
	double LastHitTime = -10.0;
	int32 HitCount = 0;
	int32 DryFireCount = 0;
	int32 ReloadCount = 0;
	bool bCrosshairVisible = true;
};
