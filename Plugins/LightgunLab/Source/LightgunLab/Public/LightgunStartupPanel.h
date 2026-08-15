// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "LightgunStartupPanel.generated.h"

class UComboBoxString;
class UTextBlock;
class UButton;

/**
 * Boot-time gun picker: lists detected lightguns, preselects the best match,
 * offers test fire / rescan / mouse-only. Built entirely in C++ so it works with
 * zero content; subclass in Blueprint to reskin.
 */
UCLASS(Blueprintable)
class LIGHTGUNLAB_API ULightgunStartupPanel : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnTestFireClicked();

	UFUNCTION()
	void OnRescanClicked();

	UFUNCTION()
	void OnGunSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

private:
	void Populate();
	void ApplyComboSelection();
	class ULightgunSubsystem* GetLightgun() const;

	UPROPERTY(Transient) TObjectPtr<UComboBoxString> GunCombo;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;

	TArray<int32> ComboToGunIndex;
	int32 MouseOnlyComboIndex = INDEX_NONE;
};
