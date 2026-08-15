// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunCalibrationScreen.h"
#include "LightgunSubsystem.h"
#include "LightgunSettings.h"
#include "LightgunWeapon.h"
#include "LightgunRawInput.h"

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

FLinearColor ULightgunCalibrationScreen::GetPlayerColor(int32 PlayerIndex) const
{
	const ULightgunSettings* Settings = GetDefault<ULightgunSettings>();
	return PlayerIndex == 0 ? Settings->CrosshairColorP1 : Settings->CrosshairColorP2;
}

void ULightgunCalibrationScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	SetCursor(EMouseCursor::None); // the painted crosshair is the only pointer here

	ULightgunSubsystem* Lightgun = GetLightgun();
	bTwoPlayerMode = Lightgun && Lightgun->IsTwoPlayerMode();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	if (!bTwoPlayerMode)
	{
		// --- One player: the v0.3 layout, untouched ---
		Weapon = NewObject<ULightgunWeapon>(this);
		Weapon->Initialize(Lightgun);

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
	}
	else
	{
		// --- Two players: per-player HUD columns, raw-input-driven gameplay ---
		for (int32 Player = 0; Player < 2; ++Player)
		{
			PlayerWeapons[Player] = NewObject<ULightgunWeapon>(this);
			PlayerWeapons[Player]->Initialize(Lightgun, Player);
		}

		UVerticalBox* LeftBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UOverlaySlot* LeftSlot = Root->AddChildToOverlay(LeftBox);
		LeftSlot->SetHorizontalAlignment(HAlign_Left);
		LeftSlot->SetVerticalAlignment(VAlign_Top);
		LeftSlot->SetPadding(FMargin(48.f, 40.f, 0.f, 0.f));

		LeftBox->AddChildToVerticalBox(MakePanelText(WidgetTree, TEXT("AIM TEST RANGE - TWO PLAYERS"), 22, true));

		UTextBlock* Help = MakePanelText(WidgetTree,
			TEXT("6 rounds per mag. Each gun reloads itself: shoot offscreen or press its non-trigger buttons.\nThe desk keyboard reloads P1."),
			12, false, FLinearColor(0.75f, 0.77f, 0.8f));
		UVerticalBoxSlot* HelpSlot = LeftBox->AddChildToVerticalBox(Help);
		HelpSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

		UTextBlock* P1Title = MakePanelText(WidgetTree, TEXT("PLAYER 1"), 15, true, GetPlayerColor(0));
		UVerticalBoxSlot* P1TitleSlot = LeftBox->AddChildToVerticalBox(P1Title);
		P1TitleSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));

		PlayerStatusText[0] = MakePanelText(WidgetTree, TEXT("Loaded. 6/6."), 13, true, FLinearColor(0.55f, 0.85f, 0.6f));
		UVerticalBoxSlot* P1StatusSlot = LeftBox->AddChildToVerticalBox(PlayerStatusText[0]);
		P1StatusSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

		PlayerCounterText[0] = MakePanelText(WidgetTree, TEXT(""), 12, false, FLinearColor(0.7f, 0.72f, 0.75f));
		UVerticalBoxSlot* P1CounterSlot = LeftBox->AddChildToVerticalBox(PlayerCounterText[0]);
		P1CounterSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

		UVerticalBox* RightBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UOverlaySlot* RightSlot = Root->AddChildToOverlay(RightBox);
		RightSlot->SetHorizontalAlignment(HAlign_Right);
		RightSlot->SetVerticalAlignment(VAlign_Top);
		RightSlot->SetPadding(FMargin(0.f, 40.f, 48.f, 0.f));

		UTextBlock* P2Title = MakePanelText(WidgetTree, TEXT("PLAYER 2"), 15, true, GetPlayerColor(1));
		P2Title->SetJustification(ETextJustify::Right);
		RightBox->AddChildToVerticalBox(P2Title);

		PlayerStatusText[1] = MakePanelText(WidgetTree, TEXT("Loaded. 6/6."), 13, true, FLinearColor(0.55f, 0.85f, 0.6f));
		PlayerStatusText[1]->SetJustification(ETextJustify::Right);
		UVerticalBoxSlot* P2StatusSlot = RightBox->AddChildToVerticalBox(PlayerStatusText[1]);
		P2StatusSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

		PlayerCounterText[1] = MakePanelText(WidgetTree, TEXT(""), 12, false, FLinearColor(0.7f, 0.72f, 0.75f));
		PlayerCounterText[1]->SetJustification(ETextJustify::Right);
		UVerticalBoxSlot* P2CounterSlot = RightBox->AddChildToVerticalBox(PlayerCounterText[1]);
		P2CounterSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

		RefreshCounterTextForPlayer(0);
		RefreshCounterTextForPlayer(1);

		RouterInfoText = MakePanelText(WidgetTree, TEXT(""), 11, false, FLinearColor(0.55f, 0.57f, 0.62f));
		UOverlaySlot* InfoSlot = Root->AddChildToOverlay(RouterInfoText);
		InfoSlot->SetHorizontalAlignment(HAlign_Center);
		InfoSlot->SetVerticalAlignment(VAlign_Bottom);
		InfoSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
	}

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UOverlaySlot* ButtonSlot = Root->AddChildToOverlay(Buttons);
	ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	ButtonSlot->SetVerticalAlignment(VAlign_Bottom);
	ButtonSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 56.f));

	BackButton = MakePanelButton(WidgetTree, TEXT("  Back to gun select  "));
	BackButton->OnClicked.AddDynamic(this, &ULightgunCalibrationScreen::OnBackClicked);
	BackButton->SetCursor(EMouseCursor::None);
	Buttons->AddChildToHorizontalBox(BackButton);

	CrosshairToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CrosshairButtonLabel = MakePanelText(WidgetTree, TEXT("  Crosshair: ON  "), 12, true, FLinearColor(0.05f, 0.05f, 0.06f));
	CrosshairToggleButton->AddChild(CrosshairButtonLabel);
	CrosshairToggleButton->OnClicked.AddDynamic(this, &ULightgunCalibrationScreen::OnCrosshairToggleClicked);
	CrosshairToggleButton->SetCursor(EMouseCursor::None);
	UHorizontalBoxSlot* CrosshairSlot = Buttons->AddChildToHorizontalBox(CrosshairToggleButton);
	CrosshairSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

	if (bTwoPlayerMode)
	{
		SwapButton = MakePanelButton(WidgetTree, TEXT("  Swap P1 <-> P2  "));
		SwapButton->OnClicked.AddDynamic(this, &ULightgunCalibrationScreen::OnSwapClicked);
		SwapButton->SetCursor(EMouseCursor::None);
		UHorizontalBoxSlot* SwapSlot = Buttons->AddChildToHorizontalBox(SwapButton);
		SwapSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

		// In 2P the buttons are pressed by SHOOTING them (per-gun raw aim decides,
		// so the merged cursor can never misroute or double-press). Take them out
		// of Slate hit-testing entirely - the raw path is the only way in.
		BackButton->SetVisibility(ESlateVisibility::HitTestInvisible);
		CrosshairToggleButton->SetVisibility(ESlateVisibility::HitTestInvisible);
		SwapButton->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void ULightgunCalibrationScreen::NativeConstruct()
{
	Super::NativeConstruct();
	SetKeyboardFocus(); // any-key reload needs us focused

	if (bTwoPlayerMode)
	{
		ULightgunSubsystem* Lightgun = GetLightgun();
		TSharedPtr<FLightgunRawInputRouter> Router = Lightgun ? Lightgun->GetRawRouter() : nullptr;
		if (Router.IsValid() && !AimHandle.IsValid())
		{
			AimHandle = Router->OnAim.AddUObject(this, &ULightgunCalibrationScreen::HandleRawAim);
			TriggerHandle = Router->OnTrigger.AddUObject(this, &ULightgunCalibrationScreen::HandleRawTrigger);
			ReloadHandle = Router->OnReload.AddUObject(this, &ULightgunCalibrationScreen::HandleRawReload);
		}
		RefreshRouterInfo();
	}
}

void ULightgunCalibrationScreen::NativeDestruct()
{
	if (bTwoPlayerMode)
	{
		ULightgunSubsystem* Lightgun = GetLightgun();
		TSharedPtr<FLightgunRawInputRouter> Router = Lightgun ? Lightgun->GetRawRouter() : nullptr;
		if (Router.IsValid())
		{
			Router->OnAim.Remove(AimHandle);
			Router->OnTrigger.Remove(TriggerHandle);
			Router->OnReload.Remove(ReloadHandle);
		}
		AimHandle.Reset();
		TriggerHandle.Reset();
		ReloadHandle.Reset();
	}
	Super::NativeDestruct();
}

void ULightgunCalibrationScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bTwoPlayerMode)
	{
		if (FSlateApplication::IsInitialized())
		{
			CrosshairPos = FVector2f(MyGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos()));
		}
	}
	else
	{
		RouterInfoRefreshAccumulator += InDeltaTime;
		if (RouterInfoRefreshAccumulator > 1.f)
		{
			RouterInfoRefreshAccumulator = 0.f;
			RefreshRouterInfo();
		}
	}

	const double Now = FPlatformTime::Seconds();
	HitMarkers.RemoveAll([Now](const FHitMarker& M) { return Now - M.Time > 1.2; });
}

FReply ULightgunCalibrationScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	SetKeyboardFocus();

	if (bTwoPlayerMode)
	{
		// Gameplay input comes exclusively from the raw router in 2P; the merged
		// cursor would double-fire whichever player it last mirrored. Swallow it.
		// (Clicks on the Slate buttons never reach this handler.)
		return FReply::Handled();
	}

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

FReply ULightgunCalibrationScreen::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Rapid trigger work arrives as double-click events; a shot is a shot.
	// (In 2P this falls through to the same swallow as single clicks.)
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply ULightgunCalibrationScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bTwoPlayerMode)
	{
		// Raw keyboard routing owns reloads in 2P (per-device, desk keyboard -> P1).
		return FReply::Handled();
	}
	if (!InKeyEvent.IsRepeat())
	{
		DoReload(FString::Printf(TEXT("key: %s"), *InKeyEvent.GetKey().GetDisplayName().ToString()));
	}
	return FReply::Handled();
}

// --- Two-player raw event handlers ---

FVector2f ULightgunCalibrationScreen::DesktopToLocal(const FVector2f& DesktopPos) const
{
	return FVector2f(GetCachedGeometry().AbsoluteToLocal(FVector2D(DesktopPos.X, DesktopPos.Y)));
}

void ULightgunCalibrationScreen::HandleRawAim(int32 PlayerIndex, FVector2f DesktopPos)
{
	if (PlayerIndex >= 0 && PlayerIndex < 2)
	{
		PlayerCrosshairPos[PlayerIndex] = DesktopToLocal(DesktopPos);
		bPlayerHasAim[PlayerIndex] = true;
	}
}

void ULightgunCalibrationScreen::HandleRawTrigger(int32 PlayerIndex, FVector2f DesktopPos)
{
	if (PlayerIndex < 0 || PlayerIndex >= 2)
	{
		return;
	}
	const FVector2f Local = DesktopToLocal(DesktopPos);
	PlayerCrosshairPos[PlayerIndex] = Local;
	bPlayerHasAim[PlayerIndex] = true;

	// UI first: shooting a button presses it (and costs no ammo).
	if (TryShootButton(DesktopPos, PlayerIndex))
	{
		return;
	}

	// Corner detection runs on the per-device position, so one gun's offscreen
	// snap can never reload the other player. Before the first layout the cached
	// geometry is zero-sized and every point would look like a corner - fire instead.
	const FVector2f Size = FVector2f(GetCachedGeometry().GetLocalSize());
	const bool bGeometryReady = Size.X > OffscreenCornerPx * 4.f && Size.Y > OffscreenCornerPx * 4.f;
	const bool bCornerX = Local.X <= OffscreenCornerPx || Local.X >= Size.X - OffscreenCornerPx;
	const bool bCornerY = Local.Y <= OffscreenCornerPx || Local.Y >= Size.Y - OffscreenCornerPx;
	if (bGeometryReady && bCornerX && bCornerY)
	{
		DoReloadForPlayer(PlayerIndex, TEXT("offscreen shot"));
	}
	else
	{
		HandleTriggerPullForPlayer(PlayerIndex, Local);
	}
}

void ULightgunCalibrationScreen::HandleRawReload(int32 PlayerIndex, const FString& Reason)
{
	if (PlayerIndex >= 0 && PlayerIndex < 2)
	{
		DoReloadForPlayer(PlayerIndex, Reason);
	}
}

bool ULightgunCalibrationScreen::TryShootButton(const FVector2f& DesktopPos, int32 PlayerIndex)
{
	struct FShootable
	{
		UButton* Button;
		void (ULightgunCalibrationScreen::*Handler)();
	};
	const FShootable Shootables[] = {
		{ BackButton, &ULightgunCalibrationScreen::OnBackClicked },
		{ CrosshairToggleButton, &ULightgunCalibrationScreen::OnCrosshairToggleClicked },
		{ SwapButton, &ULightgunCalibrationScreen::OnSwapClicked },
	};
	const FVector2D Absolute(DesktopPos.X, DesktopPos.Y);
	for (const FShootable& Shootable : Shootables)
	{
		if (Shootable.Button && Shootable.Button->GetIsEnabled() &&
			Shootable.Button->GetCachedGeometry().IsUnderLocation(Absolute))
		{
			// A ring in the shooter's color = visible "who pressed it" feedback.
			FHitMarker Marker;
			Marker.Pos = DesktopToLocal(DesktopPos);
			Marker.Time = FPlatformTime::Seconds();
			Marker.PlayerIndex = PlayerIndex;
			HitMarkers.Add(Marker);
			(this->*(Shootable.Handler))();
			return true;
		}
	}
	return false;
}

void ULightgunCalibrationScreen::HandleTriggerPullForPlayer(int32 PlayerIndex, const FVector2f& LocalPos)
{
	ULightgunWeapon* PlayerWeapon = PlayerWeapons[PlayerIndex];
	if (!PlayerWeapon)
	{
		return;
	}

	if (PlayerWeapon->TryFire())
	{
		FHitMarker Marker;
		Marker.Pos = LocalPos;
		Marker.Time = FPlatformTime::Seconds();
		Marker.PlayerIndex = PlayerIndex;
		HitMarkers.Add(Marker);
		PlayerLastHitTime[PlayerIndex] = Marker.Time;
		++PlayerHitCount[PlayerIndex];
		if (PlayerStatusText[PlayerIndex])
		{
			PlayerStatusText[PlayerIndex]->SetText(FText::FromString(FString::Printf(TEXT("HIT - recoil fired. %d round%s left."),
				PlayerWeapon->GetAmmo(), PlayerWeapon->GetAmmo() == 1 ? TEXT("") : TEXT("s"))));
			PlayerStatusText[PlayerIndex]->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.85f, 0.6f)));
		}
	}
	else
	{
		++PlayerDryFireCount[PlayerIndex];
		if (PlayerStatusText[PlayerIndex])
		{
			PlayerStatusText[PlayerIndex]->SetText(FText::FromString(TEXT("CLICK - magazine empty, no recoil. Reload!")));
			PlayerStatusText[PlayerIndex]->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.45f, 0.4f)));
		}
	}
	RefreshCounterTextForPlayer(PlayerIndex);
}

void ULightgunCalibrationScreen::DoReloadForPlayer(int32 PlayerIndex, const FString& Reason)
{
	ULightgunWeapon* PlayerWeapon = PlayerWeapons[PlayerIndex];
	if (!PlayerWeapon)
	{
		return;
	}
	PlayerWeapon->Reload();
	++PlayerReloadCount[PlayerIndex];
	if (PlayerStatusText[PlayerIndex])
	{
		PlayerStatusText[PlayerIndex]->SetText(FText::FromString(FString::Printf(TEXT("RELOADED via %s - %d/%d."),
			*Reason, PlayerWeapon->GetAmmo(), PlayerWeapon->GetMagazineSize())));
		PlayerStatusText[PlayerIndex]->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.75f, 0.95f)));
	}
	RefreshCounterTextForPlayer(PlayerIndex);
}

void ULightgunCalibrationScreen::RefreshCounterTextForPlayer(int32 PlayerIndex)
{
	if (PlayerCounterText[PlayerIndex] && PlayerWeapons[PlayerIndex])
	{
		PlayerCounterText[PlayerIndex]->SetText(FText::FromString(FString::Printf(TEXT("Ammo %d/%d   Hits %d   Dry %d   Reloads %d"),
			PlayerWeapons[PlayerIndex]->GetAmmo(), PlayerWeapons[PlayerIndex]->GetMagazineSize(),
			PlayerHitCount[PlayerIndex], PlayerDryFireCount[PlayerIndex], PlayerReloadCount[PlayerIndex])));
	}
}

void ULightgunCalibrationScreen::RefreshRouterInfo()
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!RouterInfoText || !Lightgun)
	{
		return;
	}
	FString Info;
	for (int32 Player = 0; Player < 2; ++Player)
	{
		FString Device;
		if (Lightgun->HasActiveGunForPlayer(Player))
		{
			Device = Lightgun->GetActiveGunForPlayer(Player).DisplayName;
		}
		else if (Lightgun->IsPlayerDesktopMouse(Player))
		{
			Device = TEXT("Desktop mouse (aim only)");
		}
		else
		{
			Device = TEXT("no device");
		}
		Info += FString::Printf(TEXT("%sP%d: %s"), Player == 0 ? TEXT("") : TEXT("      "), Player + 1, *Device);
	}
	TSharedPtr<FLightgunRawInputRouter> Router = Lightgun->GetRawRouter();
	if (Router.IsValid())
	{
		const FString Routing = Router->GetDebugSummary();
		if (!Routing.IsEmpty())
		{
			Info += TEXT("\n") + Routing;
		}
	}
	else
	{
		Info += TEXT("\nRaw input router NOT running - 2P aim will not track");
	}
	RouterInfoText->SetText(FText::FromString(Info));
	RouterInfoText->SetJustification(ETextJustify::Center);
}

// --- One-player handlers (v0.3, untouched) ---

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

	auto DrawCrosshairAt = [&](const FVector2f& C, const FLinearColor& CrossColor)
	{
		constexpr float Arm = 16.f;
		constexpr float Hole = 5.f;
		DrawLine(C - FVector2f(Arm, 0.f), C - FVector2f(Hole, 0.f), CrossColor, 2.f);
		DrawLine(C + FVector2f(Hole, 0.f), C + FVector2f(Arm, 0.f), CrossColor, 2.f);
		DrawLine(C - FVector2f(0.f, Arm), C - FVector2f(0.f, Hole), CrossColor, 2.f);
		DrawLine(C + FVector2f(0.f, Hole), C + FVector2f(0.f, Arm), CrossColor, 2.f);
		DrawLine(C - FVector2f(1.f, 0.f), C + FVector2f(1.f, 0.f), CrossColor, 3.f);
	};

	auto DrawPipRow = [&](const ULightgunWeapon* PipWeapon, float FirstPipX, float PipY, const FLinearColor& LoadedColor)
	{
		constexpr float PipW = 16.f;
		constexpr float PipH = 26.f;
		constexpr float PipGap = 8.f;
		for (int32 Index = 0; Index < PipWeapon->GetMagazineSize(); ++Index)
		{
			const float PipX = FirstPipX + Index * (PipW + PipGap);
			const bool bLoaded = Index < PipWeapon->GetAmmo();
			const FLinearColor PipColor = bLoaded ? LoadedColor : FLinearColor(0.25f, 0.26f, 0.28f, 0.7f);
			FSlateDrawElement::MakeBox(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(FVector2f(PipW, PipH), FSlateLayoutTransform(FVector2f(PipX, PipY))),
				Brush, ESlateDrawEffect::None, PipColor);
		}
	};

	if (!bTwoPlayerMode)
	{
		// --- One player: the v0.3 painting, untouched ---
		if (Weapon)
		{
			DrawPipRow(Weapon, 48.f, Size.Y - 96.f, FLinearColor(1.f, 0.72f, 0.25f));

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
			DrawCrosshairAt(CrosshairPos, CrossColor);
		}

		return Layer;
	}

	// --- Two players ---
	constexpr float PipRowWidth = 16.f * 6 + 8.f * 5; // 6-pip row footprint
	for (int32 Player = 0; Player < 2; ++Player)
	{
		const ULightgunWeapon* PlayerWeapon = PlayerWeapons[Player];
		if (!PlayerWeapon)
		{
			continue;
		}
		const FLinearColor PlayerColor = GetPlayerColor(Player);
		const float FirstPipX = Player == 0 ? 48.f : Size.X - 48.f - PipRowWidth;
		DrawPipRow(PlayerWeapon, FirstPipX, Size.Y - 96.f, PlayerColor);

		if (PlayerWeapon->IsEmpty())
		{
			const float Pulse = 0.55f + 0.45f * FMath::Sin(static_cast<float>(Now) * 7.f);
			FSlateDrawElement::MakeText(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(FVector2f(400.f, 40.f), FSlateLayoutTransform(FVector2f(Player == 0 ? 48.f : Size.X - 48.f - 160.f, Size.Y - 140.f))),
				FString(TEXT("RELOAD!")), FCoreStyle::GetDefaultFontStyle("Bold", 24),
				ESlateDrawEffect::None, FLinearColor(PlayerColor.R, PlayerColor.G, PlayerColor.B, Pulse));
		}
	}

	// Hit rings tinted per player.
	for (const FHitMarker& Marker : HitMarkers)
	{
		const float Age = static_cast<float>(Now - Marker.Time);
		const float Alpha = FMath::Clamp(1.f - Age / 1.2f, 0.f, 1.f);
		const float Radius = 10.f + Age * 60.f;
		const FLinearColor Base = GetPlayerColor(Marker.PlayerIndex);
		const FLinearColor RingColor(Base.R, Base.G, Base.B, Alpha);
		constexpr int32 Segments = 20;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float Angle0 = 2.f * PI * Index / Segments;
			const float Angle1 = 2.f * PI * (Index + 1) / Segments;
			DrawLine(Marker.Pos + Radius * FVector2f(FMath::Cos(Angle0), FMath::Sin(Angle0)),
				Marker.Pos + Radius * FVector2f(FMath::Cos(Angle1), FMath::Sin(Angle1)), RingColor, 2.f);
		}
	}

	// Two crosshairs in the players' colors, flashing toward white on a hit.
	if (bCrosshairVisible)
	{
		for (int32 Player = 0; Player < 2; ++Player)
		{
			ULightgunSubsystem* Lightgun = GetLightgun();
			const bool bActive = Lightgun &&
				(Lightgun->HasActiveGunForPlayer(Player) || Lightgun->IsPlayerDesktopMouse(Player));
			if (!bActive)
			{
				continue;
			}
			// Park an untracked crosshair mid-screen on its player's side.
			const FVector2f C = bPlayerHasAim[Player]
				? PlayerCrosshairPos[Player]
				: FVector2f(Size.X * (Player == 0 ? 0.35f : 0.65f), Size.Y * 0.5f);
			const float Flash = FMath::Clamp(1.f - static_cast<float>(Now - PlayerLastHitTime[Player]) / 0.18f, 0.f, 1.f);
			const FLinearColor CrossColor = FMath::Lerp(GetPlayerColor(Player), FLinearColor::White, Flash * 0.8f);
			DrawCrosshairAt(C, CrossColor);
		}
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

void ULightgunCalibrationScreen::OnSwapClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->SwapPlayers();
		for (int32 Player = 0; Player < 2; ++Player)
		{
			if (PlayerStatusText[Player])
			{
				PlayerStatusText[Player]->SetText(FText::FromString(TEXT("Swapped P1 <-> P2.")));
				PlayerStatusText[Player]->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.75f, 0.95f)));
			}
		}
		RefreshRouterInfo();
	}
	SetKeyboardFocus();
}
