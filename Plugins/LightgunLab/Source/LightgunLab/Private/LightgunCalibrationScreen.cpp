// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunCalibrationScreen.h"
#include "LightgunSubsystem.h"
#include "LightgunWeapon.h"

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
#include "Styling/CoreStyle.h"
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
	SetIsFocusable(true);

	Weapon = NewObject<ULightgunWeapon>(this);
	Weapon->Initialize(GetLightgun());

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	UVerticalBox* TopBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UOverlaySlot* TopSlot = Root->AddChildToOverlay(TopBox);
	TopSlot->SetHorizontalAlignment(HAlign_Left);
	TopSlot->SetVerticalAlignment(VAlign_Top);
	TopSlot->SetPadding(FMargin(48.f, 40.f, 0.f, 0.f));

	TopBox->AddChildToVerticalBox(MakePanelText(WidgetTree, TEXT("AIM TEST RANGE"), 22, true));

	UTextBlock* Help = MakePanelText(WidgetTree,
		TEXT("6 rounds in the mag. Shots inside the border hit and recoil; on empty the trigger just clicks.\nReload: shoot offscreen, press any other gun/mouse button, or any keyboard key."),
		12, false, FLinearColor(0.75f, 0.77f, 0.8f));
	UVerticalBoxSlot* HelpSlot = TopBox->AddChildToVerticalBox(Help);
	HelpSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

	StatusText = MakePanelText(WidgetTree, TEXT("Loaded. 6/6."), 14, true, FLinearColor(0.55f, 0.85f, 0.6f));
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

	UButton* CrosshairButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CrosshairButtonLabel = MakePanelText(WidgetTree, TEXT("  Crosshair: ON  "), 12, true, FLinearColor(0.05f, 0.05f, 0.06f));
	CrosshairButton->AddChild(CrosshairButtonLabel);
	CrosshairButton->OnClicked.AddDynamic(this, &ULightgunCalibrationScreen::OnCrosshairToggleClicked);
	UHorizontalBoxSlot* CrosshairSlot = Buttons->AddChildToHorizontalBox(CrosshairButton);
	CrosshairSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
}

void ULightgunCalibrationScreen::NativeConstruct()
{
	Super::NativeConstruct();
	SetKeyboardFocus(); // any-key reload needs us focused
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
	SetKeyboardFocus();

	const FVector2f Local = FVector2f(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	const FVector2f Size = FVector2f(InGeometry.GetLocalSize());

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const bool bCornerX = Local.X <= OffscreenCornerPx || Local.X >= Size.X - OffscreenCornerPx;
		const bool bCornerY = Local.Y <= OffscreenCornerPx || Local.Y >= Size.Y - OffscreenCornerPx;
		if (bCornerX && bCornerY)
		{
			DoReload(TEXT("offscreen shot"));
		}
		else
		{
			HandleTriggerPull(Local);
		}
		return FReply::Handled();
	}

	// Every non-trigger button - right (Sinden offscreen / GUN4IR B), middle,
	// thumb buttons - is a reload.
	DoReload(TEXT("gun button"));
	return FReply::Handled();
}

FReply ULightgunCalibrationScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat())
	{
		DoReload(FString::Printf(TEXT("key: %s"), *InKeyEvent.GetKey().GetDisplayName().ToString()));
	}
	return FReply::Handled();
}

void ULightgunCalibrationScreen::HandleTriggerPull(const FVector2f& LocalPos)
{
	if (!Weapon)
	{
		return;
	}

	if (Weapon->TryFire())
	{
		FHitMarker Marker;
		Marker.Pos = LocalPos;
		Marker.Time = FPlatformTime::Seconds();
		HitMarkers.Add(Marker);
		LastHitTime = Marker.Time;
		++HitCount;
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("HIT - recoil fired. %d round%s left."),
				Weapon->GetAmmo(), Weapon->GetAmmo() == 1 ? TEXT("") : TEXT("s"))));
			StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.85f, 0.6f)));
		}
	}
	else
	{
		++DryFireCount;
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("CLICK - magazine empty, no recoil. Reload!")));
			StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.45f, 0.4f)));
		}
	}
	RefreshCounterText();
}

void ULightgunCalibrationScreen::DoReload(const FString& Reason)
{
	if (!Weapon)
	{
		return;
	}
	Weapon->Reload();
	++ReloadCount;
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("RELOADED via %s - %d/%d."),
			*Reason, Weapon->GetAmmo(), Weapon->GetMagazineSize())));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.75f, 0.95f)));
	}
	RefreshCounterText();
}

void ULightgunCalibrationScreen::RefreshCounterText()
{
	if (CounterText && Weapon)
	{
		CounterText->SetText(FText::FromString(FString::Printf(TEXT("Ammo %d/%d   Hits %d   Dry fires %d   Reloads %d"),
			Weapon->GetAmmo(), Weapon->GetMagazineSize(), HitCount, DryFireCount, ReloadCount)));
	}
}

int32 ULightgunCalibrationScreen::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 Layer = MaxLayer + 1;
	const double Now = FPlatformTime::Seconds();
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const FPaintGeometry FullGeometry = AllottedGeometry.ToPaintGeometry();
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");

	auto DrawLine = [&](const FVector2f& A, const FVector2f& B, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2f> Points;
		Points.Add(A);
		Points.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, FullGeometry, Points, ESlateDrawEffect::None, Color, true, Thickness);
	};

	// Ammo pips, bottom-left.
	if (Weapon)
	{
		constexpr float PipW = 16.f;
		constexpr float PipH = 26.f;
		constexpr float PipGap = 8.f;
		const float PipY = Size.Y - 96.f;
		for (int32 Index = 0; Index < Weapon->GetMagazineSize(); ++Index)
		{
			const float PipX = 48.f + Index * (PipW + PipGap);
			const bool bLoaded = Index < Weapon->GetAmmo();
			const FLinearColor PipColor = bLoaded ? FLinearColor(1.f, 0.72f, 0.25f) : FLinearColor(0.25f, 0.26f, 0.28f, 0.7f);
			FSlateDrawElement::MakeBox(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(FVector2f(PipW, PipH), FSlateLayoutTransform(FVector2f(PipX, PipY))),
				Brush, ESlateDrawEffect::None, PipColor);
		}

		if (Weapon->IsEmpty())
		{
			const float Pulse = 0.55f + 0.45f * FMath::Sin(static_cast<float>(Now) * 7.f);
			FSlateDrawElement::MakeText(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(FVector2f(400.f, 40.f), FSlateLayoutTransform(FVector2f(48.f, Size.Y - 140.f))),
				FString(TEXT("RELOAD!")), FCoreStyle::GetDefaultFontStyle("Bold", 24),
				ESlateDrawEffect::None, FLinearColor(1.f, 0.35f, 0.3f, Pulse));
		}
	}

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

	// Crosshair: white normally, warm flash right after a hit. Toggleable.
	if (bCrosshairVisible)
	{
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
	}

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

void ULightgunCalibrationScreen::OnCrosshairToggleClicked()
{
	bCrosshairVisible = !bCrosshairVisible;
	if (CrosshairButtonLabel)
	{
		CrosshairButtonLabel->SetText(FText::FromString(bCrosshairVisible ? TEXT("  Crosshair: ON  ") : TEXT("  Crosshair: OFF  ")));
	}
	SetKeyboardFocus();
}
