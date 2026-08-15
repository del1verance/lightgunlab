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
#include "Styling/CoreStyle.h"
#include "Engine/GameInstance.h"
#include "LightgunPanelHelpers.h"

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

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Column);

	auto AddRow = [&](UWidget* Widget, float TopPad) -> UVerticalBoxSlot*
	{
		UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Widget);
		RowSlot->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		return RowSlot;
	};

	AddRow(MakePanelText(WidgetTree, TEXT("LIGHTGUN SETUP"), 20, true), 0.f);

	StatusText = MakePanelText(WidgetTree, TEXT("Scanning..."), 12, false);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.72f, 0.75f)));
	StatusText->SetAutoWrapText(true);
	AddRow(StatusText, 8.f);

	GunCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	GunCombo->OnSelectionChanged.AddDynamic(this, &ULightgunStartupPanel::OnGunSelectionChanged);
	AddRow(GunCombo, 14.f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddRow(Buttons, 16.f);

	UButton* ConfirmButton = MakePanelButton(WidgetTree, TEXT("  Use this gun  "));
	ConfirmButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnConfirmClicked);
	Buttons->AddChildToHorizontalBox(ConfirmButton);

	UButton* TestButton = MakePanelButton(WidgetTree, TEXT("  Test fire  "));
	TestButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnTestFireClicked);
	UHorizontalBoxSlot* TestSlot = Buttons->AddChildToHorizontalBox(TestButton);
	TestSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

	UButton* RescanButton = MakePanelButton(WidgetTree, TEXT("  Rescan  "));
	RescanButton->OnClicked.AddDynamic(this, &ULightgunStartupPanel::OnRescanClicked);
	UHorizontalBoxSlot* RescanSlot = Buttons->AddChildToHorizontalBox(RescanButton);
	RescanSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

	AddRow(MakePanelText(WidgetTree, TEXT("Recoil is game-controlled: it fires on live rounds and stays silent when the magazine is empty."), 11, false), 12.f);

	Populate();
}

void ULightgunStartupPanel::Populate()
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun || !GunCombo)
	{
		return;
	}

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

	const int32 RealGuns = Guns.FilterByPredicate([](const FDetectedLightgun& G) { return G.Model != ELightgunModel::UnknownSerial; }).Num();
	StatusText->SetText(FText::FromString(RealGuns > 0
		? FString::Printf(TEXT("Detected %d lightgun(s). Confirm your gun, or pick a different device."), RealGuns)
		: TEXT("No lightgun detected. Plug one in and rescan, or play with the mouse.")));
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
		Lightgun->BeginGameControl();
		RemoveFromParent();
	}
}

void ULightgunStartupPanel::OnTestFireClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		ApplyComboSelection();
		Lightgun->TestFire();
		StatusText->SetText(FText::FromString(Lightgun->GetStatusSummary()));
	}
}

void ULightgunStartupPanel::OnRescanClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->ScanForLightguns();
		Populate();
	}
}

void ULightgunStartupPanel::OnGunSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
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
