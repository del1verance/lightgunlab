// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LightgunWeapon.generated.h"

class ULightgunSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponAmmoChanged, int32, Ammo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponEvent);

/**
 * The arcade weapon core: a magazine and the ammo gate. TryFire() spends a
 * round and commands recoil; on an empty magazine it dry-fires (RS3 Z0 /
 * Sinden soft clunk, silence elsewhere) and returns false. Owns no visuals -
 * a widget or actor drives it and listens to the delegates.
 */
UCLASS(BlueprintType, Blueprintable)
class LIGHTGUNLAB_API ULightgunWeapon : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lightgun|Weapon")
	void Initialize(ULightgunSubsystem* InSubsystem);

	/** Live round: spends ammo + fires recoil, returns true. Empty: dry-fire feedback, returns false. */
	UFUNCTION(BlueprintCallable, Category = "Lightgun|Weapon")
	bool TryFire();

	UFUNCTION(BlueprintCallable, Category = "Lightgun|Weapon")
	void Reload();

	UFUNCTION(BlueprintPure, Category = "Lightgun|Weapon")
	int32 GetAmmo() const { return Ammo; }

	UFUNCTION(BlueprintPure, Category = "Lightgun|Weapon")
	int32 GetMagazineSize() const { return MagazineSize; }

	UFUNCTION(BlueprintPure, Category = "Lightgun|Weapon")
	bool IsEmpty() const { return Ammo <= 0; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightgun|Weapon", meta = (ClampMin = "1", ClampMax = "99"))
	int32 MagazineSize = 6;

	UPROPERTY(BlueprintAssignable, Category = "Lightgun|Weapon")
	FOnWeaponAmmoChanged OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lightgun|Weapon")
	FOnWeaponEvent OnDryFire;

	UPROPERTY(BlueprintAssignable, Category = "Lightgun|Weapon")
	FOnWeaponEvent OnReloaded;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULightgunSubsystem> Subsystem;

	int32 Ammo = 0;
};
