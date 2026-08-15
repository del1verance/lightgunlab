// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunStartupPanel.h"
#include "LightgunSubsystem.h"
#include "LightgunSettings.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"
#include "Engine/GameInstance.h"
#include "LightgunPanelHelpers.h"

namespace
{
	const FLinearColor ModeSelectedColor(1.f, 0.72f, 0.25f);
	const FLinearColor ModeUnselectedColor(0.3f, 0.31f, 0.35f);
	const TCHAR* DesktopMouseOption = TEXT("Desktop mouse (aim only)");
}

ULightgunSubsystem* ULightgunStartupPanel::GetLightgun() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<ULightgunSubsystem>() : nullptr;
}

void ULightgunStartupPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Tree must exist before RebuildWidget/TakeWidget - NativeConstruct is too late.
	UBorder* Scrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Scrim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.65f));
	Scrim->SetHorizontalAlignment(HAlign_Center);
	Scrim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Scrim;

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrushColor(FLinearColor(0.035f, 0.04f, 0.05f, 0.98f));
	Card->SetPadding(FMargin(28.f, 22.f));
	Scrim->SetContent(Card);

	USizeBox* Sizer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Sizer->SetMinDesiredWidth(560.f);
	Card->SetContent(Sizer);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Sizer->SetContent(Column);

	auto AddRow = [&](UWidget* Widget, float TopPad) -> UVerticalBoxSlot*
	{
		UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Widget);
		RowSlot->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		return RowSlot;
	};

	// Title + mode toggle share the top row.
	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddRow(TitleRow, 0.f);
	UHorizontalBoxSlot* TitleSlot = TitleRow->AddChildToHorizontalBox(MakePanelText(WidgetTree, TEXT("LIGHTGUN SETUP"), 20, true));
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);

	OnePlayerButton = MakePanelButton(WidgetTree, TEXT("  One player  "));
	OnePlayerButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnOnePlayerClicked);
	UHorizontalBoxSlot* OnePSlot = TitleRow->AddChildToHorizontalBox(OnePlayerButton);
	OnePSlot->SetVerticalAlignment(VAlign_Center);

	TwoPlayerButton = MakePanelButton(WidgetTree, TEXT("  Two players  "));
	TwoPlayerButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnTwoPlayerClicked);
	UHorizontalBoxSlot* TwoPSlot = TitleRow->AddChildToHorizontalBox(TwoPlayerButton);
	TwoPSlot->SetVerticalAlignment(VAlign_Center);
	TwoPSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

	StatusText = MakePanelText(WidgetTree, TEXT("Scanning..."), 12, false);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.72f, 0.75f)));
	StatusText->SetAutoWrapText(true);
	AddRow(StatusText, 8.f);

	// --- One-player section: the untouched v0.3 flow ---
	OnePlayerSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddRow(OnePlayerSection, 0.f);

	auto AddOnePlayerRow = [&](UWidget* Widget, float TopPad) -> UVerticalBoxSlot*
	{
		UVerticalBoxSlot* RowSlot = OnePlayerSection->AddChildToVerticalBox(Widget);
		RowSlot->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		return RowSlot;
	};

	GunCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	GunCombo->OnSelectionChanged.AddDynamic(this, &ULightgunStartupPanel::OnGunSelectionChanged);
	AddOnePlayerRow(GunCombo, 14.f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddOnePlayerRow(Buttons, 16.f);

	UButton* ConfirmButton = MakePanelButton(WidgetTree, TEXT("  Use this gun  "));
	ConfirmButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnConfirmClicked);
	Buttons->AddChildToHorizontalBox(ConfirmButton);

	UButton* TestRecoilButton = MakePanelButton(WidgetTree, TEXT("  Test recoil  "));
	TestRecoilButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnTestRecoilClicked);
	UHorizontalBoxSlot* TestRecoilSlot = Buttons->AddChildToHorizontalBox(TestRecoilButton);
	TestRecoilSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

	UButton* TestVibrationButton = MakePanelButton(WidgetTree, TEXT("  Test vibration  "));
	TestVibrationButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnTestVibrationClicked);
	UHorizontalBoxSlot* TestVibrationSlot = Buttons->AddChildToHorizontalBox(TestVibrationButton);
	TestVibrationSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

	UButton* RescanButton = MakePanelButton(WidgetTree, TEXT("  Rescan  "));
	RescanButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnRescanClicked);
	UHorizontalBoxSlot* RescanSlot = Buttons->AddChildToHorizontalBox(RescanButton);
	RescanSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

	AddOnePlayerRow(MakePanelText(WidgetTree, TEXT("Recoil is game-controlled: it fires on live rounds and stays silent when the magazine is empty."), 11, false), 12.f);

	// --- Two-player section ---
	TwoPlayerSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddRow(TwoPlayerSection, 0.f);

	auto AddTwoPlayerRow = [&](UWidget* Widget, float TopPad) -> UVerticalBoxSlot*
	{
		UVerticalBoxSlot* RowSlot = TwoPlayerSection->AddChildToVerticalBox(Widget);
		RowSlot->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		return RowSlot;
	};

	UHorizontalBox* PlayersRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddTwoPlayerRow(PlayersRow, 14.f);

	const TCHAR* PlayerTitles[2] = { TEXT("PLAYER 1"), TEXT("PLAYER 2") };
	const FLinearColor PlayerColors[2] = { FLinearColor(0.35f, 0.6f, 1.f), FLinearColor(1.f, 0.4f, 0.35f) };
	for (int32 Player = 0; Player < 2; ++Player)
	{
		UVerticalBox* PlayerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBoxSlot* PlayerSlot = PlayersRow->AddChildToHorizontalBox(PlayerBox);
		PlayerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PlayerSlot->SetPadding(FMargin(Player == 0 ? 0.f : 8.f, 0.f, Player == 0 ? 8.f : 0.f, 0.f));

		PlayerBox->AddChildToVerticalBox(MakePanelText(WidgetTree, PlayerTitles[Player], 13, true, PlayerColors[Player]));

		PlayerCombos[Player] = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
		if (Player == 0)
		{
			PlayerCombos[Player]->OnSelectionChanged.AddDynamic(this, &ULightgunStartupPanel::OnP1SelectionChanged);
		}
		else
		{
			PlayerCombos[Player]->OnSelectionChanged.AddDynamic(this, &ULightgunStartupPanel::OnP2SelectionChanged);
		}
		UVerticalBoxSlot* ComboSlot = PlayerBox->AddChildToVerticalBox(PlayerCombos[Player]);
		ComboSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

		UHorizontalBox* TestRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBoxSlot* TestRowSlot = PlayerBox->AddChildToVerticalBox(TestRow);
		TestRowSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		TestRowSlot->SetHorizontalAlignment(HAlign_Left);

		UButton* PlayerTestRecoil = MakePanelButton(WidgetTree, TEXT("  Test recoil  "));
		UButton* PlayerTestVibration = MakePanelButton(WidgetTree, TEXT("  Test vibration  "));
		if (Player == 0)
		{
			PlayerTestRecoil->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnP1TestRecoilClicked);
			PlayerTestVibration->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnP1TestVibrationClicked);
		}
		else
		{
			PlayerTestRecoil->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnP2TestRecoilClicked);
			PlayerTestVibration->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnP2TestVibrationClicked);
		}
		TestRow->AddChildToHorizontalBox(PlayerTestRecoil);
		UHorizontalBoxSlot* VibrationSlot = TestRow->AddChildToHorizontalBox(PlayerTestVibration);
		VibrationSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}

	UHorizontalBox* TwoPlayerButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddTwoPlayerRow(TwoPlayerButtons, 16.f);

	UButton* ConfirmTwoButton = MakePanelButton(WidgetTree, TEXT("  Start two-player range  "));
	ConfirmTwoButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnConfirmTwoPlayerClicked);
	TwoPlayerButtons->AddChildToHorizontalBox(ConfirmTwoButton);

	UButton* RescanTwoButton = MakePanelButton(WidgetTree, TEXT("  Rescan  "));
	RescanTwoButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnRescanClicked);
	UHorizontalBoxSlot* RescanTwoSlot = TwoPlayerButtons->AddChildToHorizontalBox(RescanTwoButton);
	RescanTwoSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

	TwoPlayerNote = MakePanelText(WidgetTree,
		TEXT("The same device can't serve both players. Sinden recoil follows the software's Lightgun A/B assignment - if the wrong gun kicks, use Swap on the range."),
		11, false, FLinearColor(0.7f, 0.72f, 0.75f));
	TwoPlayerNote->SetAutoWrapText(true);
	AddTwoPlayerRow(TwoPlayerNote, 12.f);

	Populate();

	// Two (or more) real guns attached = a two-player rig: open in 2P with each
	// hint-labeled gun already in its seat. One gun keeps the last-used mode.
	int32 KnownGuns = 0;
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		KnownGuns = Lightgun->GetDetectedGuns().FilterByPredicate([](const FDetectedLightgun& G)
		{
			return G.Model != ELightgunModel::UnknownSerial && G.Model != ELightgunModel::None;
		}).Num();
	}
	SetMode(KnownGuns >= 2 || GetDefault<ULightgunSettings>()->bTwoPlayerMode);
}

void ULightgunStartupPanel::SetMode(bool bTwoPlayers)
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (Lightgun)
	{
		Lightgun->SetTwoPlayerMode(bTwoPlayers);
	}
	if (OnePlayerSection)
	{
		OnePlayerSection->SetVisibility(bTwoPlayers ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (TwoPlayerSection)
	{
		TwoPlayerSection->SetVisibility(bTwoPlayers ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (OnePlayerButton)
	{
		OnePlayerButton->SetBackgroundColor(bTwoPlayers ? ModeUnselectedColor : ModeSelectedColor);
	}
	if (TwoPlayerButton)
	{
		TwoPlayerButton->SetBackgroundColor(bTwoPlayers ? ModeSelectedColor : ModeUnselectedColor);
	}
	if (bTwoPlayers)
	{
		PopulateTwoPlayer(false);
		RefreshTwoPlayerStatus();
	}
	else if (Lightgun && StatusText)
	{
		Populate();
	}
}

void ULightgunStartupPanel::OnOnePlayerClicked()
{
	SetMode(false);
}

void ULightgunStartupPanel::OnTwoPlayerClicked()
{
	SetMode(true);
}

void ULightgunStartupPanel::Populate()
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun || !GunCombo)
	{
		return;
	}

	bSuppressComboEvents = true;
	GunCombo->ClearOptions();
	ComboToGunIndex.Reset();

	const TArray<FDetectedLightgun>& Guns = Lightgun->GetDetectedGuns();
	int32 DefaultComboIndex = INDEX_NONE;
	const ULightgunSettings* Settings = GetDefault<ULightgunSettings>();

	for (int32 GunIndex = 0; GunIndex < Guns.Num(); ++GunIndex)
	{
		GunCombo->AddOption(Guns[GunIndex].DisplayName);
		ComboToGunIndex.Add(GunIndex);

		const bool bKnownModel = Guns[GunIndex].Model != ELightgunModel::UnknownSerial;
		const bool bPreferred = Guns[GunIndex].Model == Settings->PreferredModel &&
			(Settings->PreferredComPort.IsEmpty() || Guns[GunIndex].ComPort == Settings->PreferredComPort);
		if (bPreferred || (DefaultComboIndex == INDEX_NONE && bKnownModel))
		{
			DefaultComboIndex = ComboToGunIndex.Num() - 1;
		}
	}

	GunCombo->AddOption(TEXT("Play with mouse only (no lightgun)"));
	MouseOnlyComboIndex = ComboToGunIndex.Num();

	GunCombo->SetSelectedIndex(DefaultComboIndex != INDEX_NONE ? DefaultComboIndex : MouseOnlyComboIndex);
	bSuppressComboEvents = false;

	const int32 RealGuns = Guns.FilterByPredicate([](const FDetectedLightgun& G) { return G.Model != ELightgunModel::UnknownSerial; }).Num();
	StatusText->SetText(FText::FromString(RealGuns > 0
		? FString::Printf(TEXT("Detected %d lightgun(s). Confirm your gun, or pick a different device."), RealGuns)
		: TEXT("No lightgun detected. Plug one in and rescan, or play with the mouse.")));
}

void ULightgunStartupPanel::PopulateTwoPlayer(bool bKeepCurrentPicks)
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun || !PlayerCombos[0] || !PlayerCombos[1])
	{
		return;
	}
	const TArray<FDetectedLightgun>& Guns = Lightgun->GetDetectedGuns();

	int32 WantGun[2] = { INDEX_NONE, INDEX_NONE };
	bool bWantMouse[2] = { false, false };

	if (bKeepCurrentPicks)
	{
		for (int32 Player = 0; Player < 2; ++Player)
		{
			WantGun[Player] = GetPickedGunIndex(Player);
			bWantMouse[Player] = IsMousePicked(Player);
		}
	}
	else
	{
		// Hardware seat identity drives the defaults: guns that label themselves
		// P1/P2 (GUN4IR 8042/8043-style PIDs, Sinden A/B models) land in their own
		// seat; unlabeled guns fill the remaining seats in detection order; the
		// desktop mouse covers whatever is left.
		TArray<int32> Candidates;
		for (int32 GunIndex = 0; GunIndex < Guns.Num(); ++GunIndex)
		{
			if (Guns[GunIndex].Model != ELightgunModel::UnknownSerial)
			{
				Candidates.Add(GunIndex);
			}
		}
		Candidates.StableSort([&Guns](int32 A, int32 B) { return Guns[A].PlayerHint < Guns[B].PlayerHint; });

		for (int32 Candidate : Candidates)
		{
			if (Guns[Candidate].PlayerHint >= 2)
			{
				WantGun[1] = Candidate; // a self-declared P2 gun claims that seat
				break;
			}
		}
		for (int32 Candidate : Candidates)
		{
			if (Candidate != WantGun[1])
			{
				WantGun[0] = Candidate;
				break;
			}
		}
		if (WantGun[1] == INDEX_NONE)
		{
			for (int32 Candidate : Candidates)
			{
				if (Candidate != WantGun[0])
				{
					WantGun[1] = Candidate;
					break;
				}
			}
		}
		bWantMouse[0] = WantGun[0] == INDEX_NONE;
		bWantMouse[1] = WantGun[1] == INDEX_NONE;
	}

	if (WantGun[0] != INDEX_NONE && WantGun[0] == WantGun[1])
	{
		WantGun[1] = INDEX_NONE;
		bWantMouse[1] = true;
	}

	bSuppressComboEvents = true;
	RebuildPlayerCombo(0, WantGun[0], bWantMouse[0]);
	RebuildPlayerCombo(1, WantGun[1], bWantMouse[1]);
	bSuppressComboEvents = false;
}

void ULightgunStartupPanel::RebuildPlayerCombo(int32 PlayerIndex, int32 PreferredGunIndex, bool bPreferMouse)
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	UComboBoxString* Combo = PlayerCombos[PlayerIndex];
	if (!Lightgun || !Combo)
	{
		return;
	}
	const TArray<FDetectedLightgun>& Guns = Lightgun->GetDetectedGuns();
	const int32 OtherPick = GetPickedGunIndex(1 - PlayerIndex);
	const bool bOtherMouse = IsMousePicked(1 - PlayerIndex);

	Combo->ClearOptions();
	PlayerComboToGunIndex[PlayerIndex].Reset();
	PlayerMouseComboIndex[PlayerIndex] = INDEX_NONE;

	int32 SelectIndex = INDEX_NONE;
	for (int32 GunIndex = 0; GunIndex < Guns.Num(); ++GunIndex)
	{
		if (GunIndex == OtherPick)
		{
			continue; // the same physical device cannot be selected in both
		}
		Combo->AddOption(Guns[GunIndex].DisplayName);
		PlayerComboToGunIndex[PlayerIndex].Add(GunIndex);
		if (GunIndex == PreferredGunIndex)
		{
			SelectIndex = PlayerComboToGunIndex[PlayerIndex].Num() - 1;
		}
	}
	if (GetDefault<ULightgunSettings>()->bAllowMouseAsGun && !bOtherMouse)
	{
		Combo->AddOption(DesktopMouseOption);
		PlayerMouseComboIndex[PlayerIndex] = PlayerComboToGunIndex[PlayerIndex].Num();
		if (bPreferMouse || SelectIndex == INDEX_NONE)
		{
			SelectIndex = PlayerMouseComboIndex[PlayerIndex];
		}
	}
	if (SelectIndex == INDEX_NONE && Combo->GetOptionCount() > 0)
	{
		SelectIndex = 0;
	}
	if (SelectIndex != INDEX_NONE)
	{
		Combo->SetSelectedIndex(SelectIndex);
	}
}

int32 ULightgunStartupPanel::GetPickedGunIndex(int32 PlayerIndex) const
{
	const UComboBoxString* Combo = PlayerCombos[PlayerIndex];
	const int32 ComboIndex = Combo ? Combo->GetSelectedIndex() : INDEX_NONE;
	return PlayerComboToGunIndex[PlayerIndex].IsValidIndex(ComboIndex) ? PlayerComboToGunIndex[PlayerIndex][ComboIndex] : INDEX_NONE;
}

bool ULightgunStartupPanel::IsMousePicked(int32 PlayerIndex) const
{
	const UComboBoxString* Combo = PlayerCombos[PlayerIndex];
	const int32 ComboIndex = Combo ? Combo->GetSelectedIndex() : INDEX_NONE;
	return ComboIndex != INDEX_NONE && ComboIndex == PlayerMouseComboIndex[PlayerIndex];
}

void ULightgunStartupPanel::OnP1SelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bSuppressComboEvents || SelectionType == ESelectInfo::Direct)
	{
		return;
	}
	// My pick changed: the other dropdown must stop offering it.
	bSuppressComboEvents = true;
	RebuildPlayerCombo(1, GetPickedGunIndex(1), IsMousePicked(1));
	bSuppressComboEvents = false;
	RefreshTwoPlayerStatus();
}

void ULightgunStartupPanel::OnP2SelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bSuppressComboEvents || SelectionType == ESelectInfo::Direct)
	{
		return;
	}
	bSuppressComboEvents = true;
	RebuildPlayerCombo(0, GetPickedGunIndex(0), IsMousePicked(0));
	bSuppressComboEvents = false;
	RefreshTwoPlayerStatus();
}

void ULightgunStartupPanel::RefreshTwoPlayerStatus()
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun || !StatusText)
	{
		return;
	}
	const TArray<FDetectedLightgun>& Guns = Lightgun->GetDetectedGuns();
	FString Lines;
	for (int32 Player = 0; Player < 2; ++Player)
	{
		const int32 GunIndex = GetPickedGunIndex(Player);
		FString Line = FString::Printf(TEXT("P%d: "), Player + 1);
		if (Guns.IsValidIndex(GunIndex))
		{
			Line += Guns[GunIndex].DisplayName;
			if (!Guns[GunIndex].DetectionNote.IsEmpty())
			{
				Line += TEXT(" - ") + Guns[GunIndex].DetectionNote;
			}
		}
		else if (IsMousePicked(Player))
		{
			Line += DesktopMouseOption;
		}
		else
		{
			Line += TEXT("nothing selected");
		}
		Lines += (Player == 0 ? TEXT("") : TEXT("\n")) + Line;
	}
	StatusText->SetText(FText::FromString(Lines));
}

bool ULightgunStartupPanel::ApplyPickForPlayer(int32 PlayerIndex)
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun)
	{
		return false;
	}
	const int32 GunIndex = GetPickedGunIndex(PlayerIndex);
	if (GunIndex == INDEX_NONE)
	{
		if (IsMousePicked(PlayerIndex))
		{
			Lightgun->SelectMouseForPlayer(PlayerIndex);
			return true;
		}
		return false;
	}
	// Free the gun first if the other slot still holds it from an earlier confirm
	// (its own new pick is guaranteed different by the dropdown filtering).
	const int32 Other = 1 - PlayerIndex;
	if (Lightgun->HasActiveGunForPlayer(Other) &&
		Lightgun->GetActiveGunForPlayer(Other).DisplayName == Lightgun->GetDetectedGuns()[GunIndex].DisplayName)
	{
		Lightgun->SelectMouseForPlayer(Other);
	}
	return Lightgun->SelectGunForPlayer(PlayerIndex, GunIndex);
}

void ULightgunStartupPanel::OnP1TestRecoilClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		if (ApplyPickForPlayer(0))
		{
			Lightgun->TestFireForPlayer(0);
		}
		RefreshTwoPlayerStatus();
	}
}

void ULightgunStartupPanel::OnP1TestVibrationClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		if (ApplyPickForPlayer(0))
		{
			Lightgun->TestVibrationForPlayer(0);
		}
		RefreshTwoPlayerStatus();
	}
}

void ULightgunStartupPanel::OnP2TestRecoilClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		if (ApplyPickForPlayer(1))
		{
			Lightgun->TestFireForPlayer(1);
		}
		RefreshTwoPlayerStatus();
	}
}

void ULightgunStartupPanel::OnP2TestVibrationClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		if (ApplyPickForPlayer(1))
		{
			Lightgun->TestVibrationForPlayer(1);
		}
		RefreshTwoPlayerStatus();
	}
}

void ULightgunStartupPanel::OnConfirmTwoPlayerClicked()
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun)
	{
		return;
	}
	ApplyPickForPlayer(0);
	ApplyPickForPlayer(1);
	Lightgun->StartRangeSession();
	RemoveFromParent();
	Lightgun->ShowCalibrationScreen();
}

void ULightgunStartupPanel::ApplyComboSelection()
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun || !GunCombo)
	{
		return;
	}
	const int32 ComboIndex = GunCombo->GetSelectedIndex();
	if (ComboIndex == MouseOnlyComboIndex || !ComboToGunIndex.IsValidIndex(ComboIndex))
	{
		Lightgun->SelectMouseOnly();
	}
	else
	{
		Lightgun->SelectGunByIndex(ComboToGunIndex[ComboIndex]);
	}
}

void ULightgunStartupPanel::OnConfirmClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		ApplyComboSelection();
		Lightgun->StartRangeSession(); // begins game control + raw routing for the selected gun
		RemoveFromParent();
		Lightgun->ShowCalibrationScreen();
	}
}

void ULightgunStartupPanel::OnTestRecoilClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		ApplyComboSelection();
		Lightgun->TestFire();
		StatusText->SetText(FText::FromString(Lightgun->GetStatusSummary()));
	}
}

void ULightgunStartupPanel::OnTestVibrationClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		ApplyComboSelection();
		Lightgun->TestVibrationForPlayer(0);
		StatusText->SetText(FText::FromString(Lightgun->GetStatusSummary()));
	}
}

void ULightgunStartupPanel::OnRescanClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->ScanForLightguns();
		Populate();
		if (Lightgun->IsTwoPlayerMode())
		{
			PopulateTwoPlayer(false);
			RefreshTwoPlayerStatus();
		}
	}
}

void ULightgunStartupPanel::OnGunSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bSuppressComboEvents)
	{
		return;
	}
	const int32 ComboIndex = GunCombo ? GunCombo->GetSelectedIndex() : INDEX_NONE;
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun || !StatusText)
	{
		return;
	}
	if (ComboToGunIndex.IsValidIndex(ComboIndex))
	{
		const FDetectedLightgun& Gun = Lightgun->GetDetectedGuns()[ComboToGunIndex[ComboIndex]];
		FString Info = Gun.DisplayName;
		if (!Gun.DetectionNote.IsEmpty())
		{
			Info += TEXT("\n") + Gun.DetectionNote;
		}
		if (Gun.Model == ELightgunModel::UnknownSerial)
		{
			Info += TEXT("\nNo recoil protocol known for this device - it would be aim-only.");
		}
		StatusText->SetText(FText::FromString(Info));
	}
	else
	{
		StatusText->SetText(FText::FromString(TEXT("Mouse aiming, no recoil hardware.")));
	}
}
