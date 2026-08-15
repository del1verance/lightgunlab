#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SindenBorderWidget.generated.h"

/**
 * Sinden tracking border: white frame (default 2% of screen height) inside a
 * black frame (default 3%), per Sinden's native-game guidance. Draws over
 * everything, ignores input. Requires the game to run in borderless fullscreen.
 */
UCLASS(Blueprintable)
class LIGHTGUNLAB_API ULightgunBorderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lightgun")
	void SetPercents(float InWhitePercent, float InBlackPercent);

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float WhitePercent = 2.0f;
	float BlackPercent = 3.0f;
};
