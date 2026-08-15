#include "SindenBorderWidget.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"

void ULightgunBorderWidget::SetPercents(float InWhitePercent, float InBlackPercent)
{
	WhitePercent = InWhitePercent;
	BlackPercent = InBlackPercent;
}

void ULightgunBorderWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

int32 ULightgunBorderWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	if (Size.X <= 0.f || Size.Y <= 0.f)
	{
		return MaxLayer;
	}

	// Thickness is a percentage of screen HEIGHT on all four edges (Sinden guidance).
	const float BlackPx = Size.Y * (BlackPercent / 100.f);
	const float WhitePx = Size.Y * (WhitePercent / 100.f);
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const int32 Layer = MaxLayer + 1;

	auto DrawRect = [&](float X, float Y, float W, float H, const FLinearColor& Color)
	{
		if (W <= 0.f || H <= 0.f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))),
			Brush, ESlateDrawEffect::None, Color);
	};

	const FLinearColor Black = FLinearColor::Black;
	const FLinearColor White = FLinearColor::White;

	if (BlackPx > 0.f)
	{
		DrawRect(0, 0, Size.X, BlackPx, Black);                                    // top
		DrawRect(0, Size.Y - BlackPx, Size.X, BlackPx, Black);                     // bottom
		DrawRect(0, BlackPx, BlackPx, Size.Y - 2.f * BlackPx, Black);              // left
		DrawRect(Size.X - BlackPx, BlackPx, BlackPx, Size.Y - 2.f * BlackPx, Black); // right
	}

	const float Inset = BlackPx;
	const FVector2f Inner = FVector2f(Size.X - 2.f * Inset, Size.Y - 2.f * Inset);
	if (Inner.X > 0.f && Inner.Y > 0.f && WhitePx > 0.f)
	{
		DrawRect(Inset, Inset, Inner.X, WhitePx, White);                             // top
		DrawRect(Inset, Inset + Inner.Y - WhitePx, Inner.X, WhitePx, White);         // bottom
		DrawRect(Inset, Inset + WhitePx, WhitePx, Inner.Y - 2.f * WhitePx, White);   // left
		DrawRect(Inset + Inner.X - WhitePx, Inset + WhitePx, WhitePx, Inner.Y - 2.f * WhitePx, White); // right
	}

	return Layer;
}
