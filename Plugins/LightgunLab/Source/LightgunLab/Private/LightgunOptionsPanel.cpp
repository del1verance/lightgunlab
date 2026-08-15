// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunOptionsPanel.h"
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
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"
#include "Engine/GameInstance.h"
#include "LightgunPanelHelpers.h"

ULightgunSubsystem* ULightgunOptionsPanel::GetLightgun() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<ULightgunSubsystem>() : nullptr;
}

void ULightgunOptionsPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	const ULightgunSettings* Settings = GetDefault<ULightgunSettings>();

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

	auto AddRow = [&](UWidget* Widget, float TopPad)
	{
		UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Widget);
		RowSlot->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		return RowSlot;
	};
	auto AddSection = [&](const FString& Text)
	{
		AddRow(MakePanelText(WidgetTree, Text, 12, true, FLinearColor(0.85f, 0.4f, 0.35f)), 16.f);
	};

	AddRow(MakePanelText(WidgetTree, TEXT("LIGHTGUN OPTIONS"), 20, true), 0.f);

	StatusText = MakePanelText(WidgetTree, TEXT(""), 11, false, FLinearColor(0.7f, 0.72f, 0.75f));
	StatusText->SetAutoWrapText(true);
	AddRow(StatusText, 6.f);

	AddSection(TEXT("GUN"));
	GunCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	GunCombo->OnSelectionChanged.AddDynamic(this, &ULightgunOptionsPanel::OnGunSelectionChanged);
	AddRow(GunCombo, 6.f);

	AddSection(TEXT("RECOIL"));
	ModeCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	ModeCombo->AddOption(TEXT("Direct - this game controls the gun (recommended)"));
	ModeCombo->AddOption(TEXT("Outputs - my MAMEHooker/QMamehook/HOTR rig controls the gun"));
	ModeCombo->AddOption(TEXT("Disabled"));
	ModeCombo->SetSelectedIndex(static_cast<int32>(Settings->RecoilMode));
	ModeCombo->OnSelectionChanged.AddDynamic(this, &ULightgunOptionsPanel::OnModeSelectionChanged);
	AddRow(ModeCombo, 6.f);

	UHorizontalBox* StrengthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StrengthRow->AddChildToHorizontalBox(MakePanelText(WidgetTree, TEXT("Strength "), 12, false));
	StrengthSlider = MakePanelSlider(WidgetTree, 0.f, 10.f, static_cast<float>(Settings->RecoilStrength));
	StrengthSlider->SetStepSize(1.f);
	StrengthSlider->OnValueChanged.AddDynamic(this, &ULightgunOptionsPanel::OnStrengthChanged);
	UHorizontalBoxSlot* SliderSlot = StrengthRow->AddChildToHorizontalBox(StrengthSlider);
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SliderSlot->SetPadding(FMargin(8.f, 4.f, 8.f, 0.f));
	StrengthValueText = MakePanelText(WidgetTree, FString::FromInt(Settings->RecoilStrength), 12, true);
	StrengthRow->AddChildToHorizontalBox(StrengthValueText);
	AddRow(StrengthRow, 8.f);

	AddSection(TEXT("SINDEN BORDER"));
	UHorizontalBox* BorderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BorderCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	BorderCheck->OnCheckStateChanged.AddDynamic(this, &ULightgunOptionsPanel::OnBorderToggled);
	BorderRow->AddChildToHorizontalBox(BorderCheck);
	UHorizontalBoxSlot* BorderLabelSlot = BorderRow->AddChildToHorizontalBox(MakePanelText(WidgetTree, TEXT(" Show white tracking border"), 12, false));
	BorderLabelSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
	AddRow(BorderRow, 6.f);

	UHorizontalBox* WhiteRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WhiteRow->AddChildToHorizontalBox(MakePanelText(WidgetTree, TEXT("White % "), 11, false));
	BorderWhiteSlider = MakePanelSlider(WidgetTree, 0.5f, 8.f, Settings->BorderWhitePercent);
	BorderWhiteSlider->OnValueChanged.AddDynamic(this, &ULightgunOptionsPanel::OnBorderWhiteChanged);
	UHorizontalBoxSlot* WhiteSlot = WhiteRow->AddChildToHorizontalBox(BorderWhiteSlider);
	WhiteSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	WhiteSlot->SetPadding(FMargin(8.f, 4.f, 0.f, 0.f));
	AddRow(WhiteRow, 4.f);

	UHorizontalBox* BlackRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BlackRow->AddChildToHorizontalBox(MakePanelText(WidgetTree, TEXT("Black % "), 11, false));
	BorderBlackSlider = MakePanelSlider(WidgetTree, 0.f, 10.f, Settings->BorderBlackPercent);
	BorderBlackSlider->OnValueChanged.AddDynamic(this, &ULightgunOptionsPanel::OnBorderBlackChanged);
	UHorizontalBoxSlot* BlackSlot = BlackRow->AddChildToHorizontalBox(BorderBlackSlider);
	BlackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BlackSlot->SetPadding(FMargin(8.f, 4.f, 0.f, 0.f));
	AddRow(BlackRow, 4.f);

	AddSection(TEXT("OUTPUTS EMISSION (for cabinet rigs)"));
	UHorizontalBox* TcpRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	TcpOutputsCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	TcpOutputsCheck->OnCheckStateChanged.AddDynamic(this, &ULightgunOptionsPanel::OnTcpOutputsToggled);
	TcpRow->AddChildToHorizontalBox(TcpOutputsCheck);
	TcpRow->AddChildToHorizontalBox(MakePanelText(WidgetTree, TEXT(" MAME network outputs (TCP :8000) - QMamehook / HOTR / Sinden software"), 11, false));
	AddRow(TcpRow, 6.f);

	UHorizontalBox* WmRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WinMsgOutputsCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	WinMsgOutputsCheck->OnCheckStateChanged.AddDynamic(this, &ULightgunOptionsPanel::OnWinMsgOutputsToggled);
	WmRow->AddChildToHorizontalBox(WinMsgOutputsCheck);
	WmRow->AddChildToHorizontalBox(MakePanelText(WidgetTree, TEXT(" MAME window-message outputs - classic MAMEHooker 5.1"), 11, false));
	AddRow(WmRow, 4.f);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddRow(Buttons, 18.f);
	UButton* TestButton = MakePanelButton(WidgetTree, TEXT("  Test fire  "));
	TestButton->OnClicked.AddDynamic(this, &ULightgunOptionsPanel::OnTestFireClicked);
	Buttons->AddChildToHorizontalBox(TestButton);
	UButton* EmptyButton = MakePanelButton(WidgetTree, TEXT("  Test empty click  "));
	EmptyButton->OnClicked.AddDynamic(this, &ULightgunOptionsPanel::OnTestEmptyClicked);
	UHorizontalBoxSlot* EmptySlot = Buttons->AddChildToHorizontalBox(EmptyButton);
	EmptySlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	UButton* CloseButton = MakePanelButton(WidgetTree, TEXT("  Close  "));
	CloseButton->OnClicked.AddDynamic(this, &ULightgunOptionsPanel::OnCloseClicked);
	UHorizontalBoxSlot* CloseSlot = Buttons->AddChildToHorizontalBox(CloseButton);
	CloseSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

	PopulateGuns();
	RefreshStatus();
}

void ULightgunOptionsPanel::PopulateGuns()
{
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun || !GunCombo)
	{
		return;
	}
	bSuppressEvents = true;
	GunCombo->ClearOptions();
	ComboToGunIndex.Reset();

	const TArray<FDetectedLightgun>& Guns = Lightgun->GetDetectedGuns();
	const FDetectedLightgun Active = Lightgun->GetActiveGun();
	int32 SelectIndex = INDEX_NONE;

	for (int32 GunIndex = 0; GunIndex < Guns.Num(); ++GunIndex)
	{
		GunCombo->AddOption(Guns[GunIndex].DisplayName);
		ComboToGunIndex.Add(GunIndex);
		if (Lightgun->HasActiveGun() && Guns[GunIndex].DisplayName == Active.DisplayName)
		{
			SelectIndex = ComboToGunIndex.Num() - 1;
		}
	}
	GunCombo->AddOption(TEXT("Mouse only (no lightgun)"));
	MouseOnlyComboIndex = ComboToGunIndex.Num();
	GunCombo->SetSelectedIndex(SelectIndex != INDEX_NONE ? SelectIndex : MouseOnlyComboIndex);

	if (BorderCheck)
	{
		BorderCheck->SetIsChecked(Lightgun->IsBorderVisible());
	}
	const ULightgunSettings* Settings = GetDefault<ULightgunSettings>();
	if (TcpOutputsCheck)
	{
		TcpOutputsCheck->SetIsChecked(Settings->bEnableTcpOutputs);
	}
	if (WinMsgOutputsCheck)
	{
		WinMsgOutputsCheck->SetIsChecked(Settings->bEnableWindowMessageOutputs);
	}
	bSuppressEvents = false;
}

void ULightgunOptionsPanel::RefreshStatus()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(Lightgun->GetStatusSummary()));
		}
	}
}

void ULightgunOptionsPanel::OnGunSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bSuppressEvents || SelectionType == ESelectInfo::Direct)
	{
		return;
	}
	ULightgunSubsystem* Lightgun = GetLightgun();
	if (!Lightgun)
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
	RefreshStatus();
}

void ULightgunOptionsPanel::OnModeSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bSuppressEvents || SelectionType == ESelectInfo::Direct)
	{
		return;
	}
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->SetRecoilMode(static_cast<ERecoilMode>(FMath::Clamp(ModeCombo->GetSelectedIndex(), 0, 2)));
		RefreshStatus();
	}
}

void ULightgunOptionsPanel::OnStrengthChanged(float Value)
{
	if (bSuppressEvents)
	{
		return;
	}
	const int32 Strength = FMath::RoundToInt(Value);
	if (StrengthValueText)
	{
		StrengthValueText->SetText(FText::AsNumber(Strength));
	}
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->SetRecoilStrength(Strength);
	}
}

void ULightgunOptionsPanel::OnBorderToggled(bool bChecked)
{
	if (bSuppressEvents)
	{
		return;
	}
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->SetBorderVisible(bChecked);
	}
}

void ULightgunOptionsPanel::OnBorderWhiteChanged(float Value)
{
	if (!bSuppressEvents)
	{
		if (ULightgunSubsystem* Lightgun = GetLightgun())
		{
			Lightgun->SetBorderPercents(Value, GetDefault<ULightgunSettings>()->BorderBlackPercent);
		}
	}
}

void ULightgunOptionsPanel::OnBorderBlackChanged(float Value)
{
	if (!bSuppressEvents)
	{
		if (ULightgunSubsystem* Lightgun = GetLightgun())
		{
			Lightgun->SetBorderPercents(GetDefault<ULightgunSettings>()->BorderWhitePercent, Value);
		}
	}
}

void ULightgunOptionsPanel::OnTcpOutputsToggled(bool bChecked)
{
	if (!bSuppressEvents)
	{
		if (ULightgunSubsystem* Lightgun = GetLightgun())
		{
			Lightgun->SetOutputsEnabled(bChecked, GetDefault<ULightgunSettings>()->bEnableWindowMessageOutputs);
			RefreshStatus();
		}
	}
}

void ULightgunOptionsPanel::OnWinMsgOutputsToggled(bool bChecked)
{
	if (!bSuppressEvents)
	{
		if (ULightgunSubsystem* Lightgun = GetLightgun())
		{
			Lightgun->SetOutputsEnabled(GetDefault<ULightgunSettings>()->bEnableTcpOutputs, bChecked);
			RefreshStatus();
		}
	}
}

void ULightgunOptionsPanel::OnTestFireClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->TestFire();
		RefreshStatus();
	}
}

void ULightgunOptionsPanel::OnTestEmptyClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->NotifyEmpty();
		RefreshStatus();
	}
}

void ULightgunOptionsPanel::OnCloseClicked()
{
	if (ULightgunSubsystem* Lightgun = GetLightgun())
	{
		Lightgun->SaveSettings();
	}
	RemoveFromParent();
}
