// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunCalibrationScreen.h"
#include "LightgunSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "LightgunPanelHelpers.h"

namespace
{
	// GUN4IR-style guns report offscreen trigger pulls as a click snapped to a
	// screen corner (lower-left by default); treat any extreme-corner click as one.
	constexpr float OffscreenCornerPx = 12.f;
}

ULightgunSubsystem* ULightgunCalibrationScreen::GetLightgun() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<ULightgunSubsystem>() : nullptr;
}

void ULightgunCalibrationScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Visible);

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	UVerticalBox* TopBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UOverlaySlot* TopSlot = Root->AddChildToOverlay(TopBox);
	TopSlot->SetHorizontalAlignment(HAlign_Left);
	TopSlot->SetVerticalAlignment(VAlign_Top);
	TopSlot->SetPadding(FMargin(48.f, 40.f, 0.f, 0.f));

	TopBox->AddChildToVerticalBox(MakePanelText(WidgetTree, TEXT("AIM TEST RANGE"), 22, true));

	UTextBlock* Help = MakePanelText(WidgetTree,
		TEXT("Shoot anywhere inside the border: hit marker + recoil.\nAim off the screen and pull the trigger: nothing - that's the ammo gate doing its job.\n(Corner-snap clicks and right-clicks read as offscreen.)"),
		12, false, FLinearColor(0.75f, 0.77f, 0.8f));
	UVerticalBoxSlot* HelpSlot = TopBox->AddChildToVerticalBox(Help);
	HelpSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

	StatusText = MakePanelText(WidgetTree, TEXT("Ready."), 14, true, FLinearColor(0.55f, 0.85f, 0.6f));
	UVerticalBoxSlot* StatusSlot = TopBox->AddChildToVerticalBox(StatusText);
	StatusSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));

	CounterText = MakePanelText(WidgetTree, TEXT(""), 12, false, FLinearColor(0.7f, 0.72f, 0.75f));
	UVerticalBoxSlot* CounterSlot = TopBox->AddChildToVerticalBox(CounterText);
	CounterSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	RefreshCounterText();

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UOverlaySlot* ButtonSlot = Root->AddChildToOverlay(Buttons);
	ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	ButtonSlot->SetVerticalAlignment(VAlign_Bottom);
	ButtonSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 56.f));

	UButton* BackButton = MakePanelButton(WidgetTree, TEXT("  Back to gun select  "));
	BackButton->OnClicked.AddDynamic(this, &ULightgunCalibrationScreen::OnBackClicked);
	Buttons->AddChildToHorizontalBox(BackButton);

	UButton* CloseButton = MakePanelButton(WidgetTree, TEXT("  Close range  "));
	CloseButton->OnClicked.AddDynamic(this, &ULightgunCalibrationScreen::OnCloseClicked);
	UHorizontalBoxSlot* CloseSlot = Buttons->AddChildToHorizontalBox(CloseButton);
	CloseSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
}

void ULightgunCalibrationScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (FSlateApplication::IsInitialized())
	{
		CrosshairPos = FVector2f(MyGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos()));
	}

	const double Now = FPlatformTime::Seconds();
	HitMarkers.RemoveAll([Now](const FHitMarker& M) { return Now - M.Time > 1.2; });
}

FReply ULightgunCalibrationScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2f Local = FVector2f(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	const FVector2f Size = FVector2f(InGeometry.GetLocalSize());

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Sinden offscreen pulls arrive as right clicks (also GUN4IR's B button).
		RegisterOffscreen(TEXT("offscreen / reload input"));
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const bool bCornerX = Local.X <= OffscreenCornerPx || Local.X >= Size.X - OffscreenCornerPx;
		const bool bCornerY = Local.Y <= OffscreenCornerPx || Local.Y >= Size.Y - OffscreenCornerPx;
		if (bCornerX && bCornerY)
		{
			RegisterOffscreen(TEXT("corner-snap offscreen shot"));
		}
		else
		{
			RegisterHit(Local);
		}
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void ULightgunCalibrationScreen::RegisterHit(const FVector2f& LocalPos)
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->FireRecoil();
	}
	FHitMarker Marker;
	Marker.Pos = LocalPos;
	Marker.Time = FPlatformTime::Seconds();
	HitMarkers.Add(Marker);
	LastHitTime = Marker.Time;
	++HitCount;

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("HIT at %d, %d - recoil fired"),
			FMath::RoundToInt(LocalPos.X), FMath::RoundToInt(LocalPos.Y))));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.85f, 0.6f)));
	}
	RefreshCounterText();
}

void ULightgunCalibrationScreen::RegisterOffscreen(const FString& Reason)
{
	++OffscreenCount;
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("OFFSCREEN (%s) - no recoil, as intended"), *Reason)));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.65f, 0.4f)));
	}
	RefreshCounterText();
}

void ULightgunCalibrationScreen::RefreshCounterText()
{
	if (CounterText)
	{
		CounterText->SetText(FText::FromString(FString::Printf(TEXT("Hits: %d   Offscreen: %d"), HitCount, OffscreenCount)));
	}
}

int32 ULightgunCalibrationScreen::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 Layer = MaxLayer + 1;
	const double Now = FPlatformTime::Seconds();
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();

	auto DrawLine = [&](const FVector2f& A, const FVector2f& B, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2f> Points;
		Points.Add(A);
		Points.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, PaintGeometry, Points, ESlateDrawEffect::None, Color, true, Thickness);
	};

	// Fading expanding rings for recent hits.
	for (const FHitMarker& Marker : HitMarkers)
	{
		const float Age = static_cast<float>(Now - Marker.Time);
		const float Alpha = FMath::Clamp(1.f - Age / 1.2f, 0.f, 1.f);
		const float Radius = 10.f + Age * 60.f;
		const FLinearColor RingColor(1.f, 0.45f, 0.35f, Alpha);
		constexpr int32 Segments = 20;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float Angle0 = 2.f * PI * Index / Segments;
			const float Angle1 = 2.f * PI * (Index + 1) / Segments;
			DrawLine(Marker.Pos + Radius * FVector2f(FMath::Cos(Angle0), FMath::Sin(Angle0)),
				Marker.Pos + Radius * FVector2f(FMath::Cos(Angle1), FMath::Sin(Angle1)), RingColor, 2.f);
		}
	}

	// Crosshair: white normally, flashes warm right after a hit.
	const float Flash = FMath::Clamp(1.f - static_cast<float>(Now - LastHitTime) / 0.18f, 0.f, 1.f);
	const FLinearColor CrossColor = FMath::Lerp(FLinearColor::White, FLinearColor(1.f, 0.4f, 0.3f), Flash);
	const FVector2f C = CrosshairPos;
	constexpr float Arm = 16.f;
	constexpr float Hole = 5.f;
	DrawLine(C - FVector2f(Arm, 0.f), C - FVector2f(Hole, 0.f), CrossColor, 2.f);
	DrawLine(C + FVector2f(Hole, 0.f), C + FVector2f(Arm, 0.f), CrossColor, 2.f);
	DrawLine(C - FVector2f(0.f, Arm), C - FVector2f(0.f, Hole), CrossColor, 2.f);
	DrawLine(C + FVector2f(0.f, Hole), C + FVector2f(0.f, Arm), CrossColor, 2.f);
	DrawLine(C - FVector2f(1.f, 0.f), C + FVector2f(1.f, 0.f), CrossColor, 3.f);

	return Layer;
}

void ULightgunCalibrationScreen::OnBackClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->ShowStartupPanel();
	}
	RemoveFromParent();
}

void ULightgunCalibrationScreen::OnCloseClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = true;
	}
	RemoveFromParent();
}
