// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LightgunCalibrationScreen.generated.h"

class UTextBlock;

/**
 * Aim test range shown after gun selection: a crosshair tracks the gun,
 * on-screen shots register hits and fire recoil, off-screen shots
 * (corner-snap clicks from GUN4IR-style guns, right-clicks from
 * Sinden-style offscreen mapping) register silently with no recoil.
 */
UCLASS(Blueprintable)
class LIGHTGUNLAB_API ULightgunCalibrationScreen : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnCloseClicked();

private:
	class ULightgunSubsystem* GetLightgun() const;
	void RegisterHit(const FVector2f& LocalPos);
	void RegisterOffscreen(const FString& Reason);
	void RefreshCounterText();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CounterText;

	struct FHitMarker
	{
		FVector2f Pos = FVector2f::ZeroVector;
		double Time = 0.0;
	};

	TArray<FHitMarker> HitMarkers;
	FVector2f CrosshairPos = FVector2f::ZeroVector;
	double LastHitTime = -10.0;
	int32 HitCount = 0;
	int32 OffscreenCount = 0;
};
