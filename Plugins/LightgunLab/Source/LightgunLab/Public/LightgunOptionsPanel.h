// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "LightgunOptionsPanel.generated.h"

class UComboBoxString;
class UTextBlock;
class UCheckBox;
class USlider;

/**
 * In-game lightgun options: switch gun, recoil mode (direct / outputs rig / off),
 * strength, Sinden border controls, outputs emission toggles, test buttons.
 * C++-built with zero content; subclass in Blueprint to reskin.
 */
UCLASS(Blueprintable)
class LIGHTGUNLAB_API ULightgunOptionsPanel : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UFUNCTION() void OnGunSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnModeSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnStrengthChanged(float Value);
	UFUNCTION() void OnBorderToggled(bool bChecked);
	UFUNCTION() void OnBorderWhiteChanged(float Value);
	UFUNCTION() void OnBorderBlackChanged(float Value);
	UFUNCTION() void OnTcpOutputsToggled(bool bChecked);
	UFUNCTION() void OnWinMsgOutputsToggled(bool bChecked);
	UFUNCTION() void OnTestFireClicked();
	UFUNCTION() void OnTestEmptyClicked();
	UFUNCTION() void OnCloseClicked();

private:
	void PopulateGuns();
	void RefreshStatus();
	class ULightgunSubsystem* GetLightgun() const;

	UPROPERTY(Transient) TObjectPtr<UComboBoxString> GunCombo;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> ModeCombo;
	UPROPERTY(Transient) TObjectPtr<USlider> StrengthSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StrengthValueText;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> BorderCheck;
	UPROPERTY(Transient) TObjectPtr<USlider> BorderWhiteSlider;
	UPROPERTY(Transient) TObjectPtr<USlider> BorderBlackSlider;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> TcpOutputsCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> WinMsgOutputsCheck;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;

	TArray<int32> ComboToGunIndex;
	int32 MouseOnlyComboIndex = INDEX_NONE;
	bool bSuppressEvents = false;
};
