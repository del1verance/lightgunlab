// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "LightgunStartupPanel.generated.h"

class UComboBoxString;
class UTextBlock;
class UButton;
class UVerticalBox;

/**
 * Boot-time gun picker. "One player" keeps the v0.3 flow untouched: one dropdown,
 * confirm/test/rescan. "Two players" swaps in two per-player dropdowns (any mix of
 * guns, or the desktop mouse as an aim-only pick) that can never hold the same
 * physical device. Built entirely in C++ so it works with zero content; subclass
 * in Blueprint to reskin.
 */
UCLASS(Blueprintable)
class LIGHTGUNLAB_API ULightgunStartupPanel : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnTestRecoilClicked();

	UFUNCTION()
	void OnTestVibrationClicked();

	UFUNCTION()
	void OnRescanClicked();

	UFUNCTION()
	void OnGunSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnOnePlayerClicked();

	UFUNCTION()
	void OnTwoPlayerClicked();

	UFUNCTION()
	void OnP1SelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnP2SelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnP1TestRecoilClicked();

	UFUNCTION()
	void OnP1TestVibrationClicked();

	UFUNCTION()
	void OnP2TestRecoilClicked();

	UFUNCTION()
	void OnP2TestVibrationClicked();

	UFUNCTION()
	void OnConfirmTwoPlayerClicked();

	UFUNCTION()
	void OnDetectedGunsHotChanged();

private:
	void Populate();
	void ApplyComboSelection();
	class ULightgunSubsystem* GetLightgun() const;

	// --- Two players ---
	void SetMode(bool bTwoPlayers);
	void PopulateTwoPlayer(bool bKeepCurrentPicks);
	void RebuildPlayerCombo(int32 PlayerIndex, int32 PreferredGunIndex, bool bPreferMouse);
	int32 GetPickedGunIndex(int32 PlayerIndex) const;   // INDEX_NONE = mouse/none
	bool IsMousePicked(int32 PlayerIndex) const;
	bool ApplyPickForPlayer(int32 PlayerIndex);
	void RefreshTwoPlayerStatus();
	/** Greys out Test recoil / Test vibration wherever the picked device lacks the hardware. */
	void UpdateTestButtonEnableStates();
	/** Shows the Sinden border as soon as one is DETECTED (a Sinden aims via its
	    software regardless of selection, so it needs the border to point at the picker). */
	void ShowBorderIfAnySindenDetected();
	/** Border tracks the pending picks: up while any pick is a Sinden, gone when none is. */
	void UpdateBorderForPicks();

	UPROPERTY(Transient) TObjectPtr<UComboBoxString> GunCombo;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient) TObjectPtr<UButton> OnePlayerButton;
	UPROPERTY(Transient) TObjectPtr<UButton> TwoPlayerButton;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> OnePlayerSection;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TwoPlayerSection;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> PlayerCombos[2];
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TwoPlayerNote;

	UPROPERTY(Transient) TObjectPtr<UButton> TestRecoilButton;
	UPROPERTY(Transient) TObjectPtr<UButton> TestVibrationButton;
	UPROPERTY(Transient) TObjectPtr<UButton> PlayerTestRecoilButtons[2];
	UPROPERTY(Transient) TObjectPtr<UButton> PlayerTestVibrationButtons[2];

	TArray<int32> ComboToGunIndex;
	int32 MouseOnlyComboIndex = INDEX_NONE;

	TArray<int32> PlayerComboToGunIndex[2];
	int32 PlayerMouseComboIndex[2] = { INDEX_NONE, INDEX_NONE };
	bool bSuppressComboEvents = false;
};
