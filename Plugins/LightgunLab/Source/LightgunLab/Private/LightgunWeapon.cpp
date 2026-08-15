// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunWeapon.h"
#include "LightgunSubsystem.h"

void ULightgunWeapon::Initialize(ULightgunSubsystem* InSubsystem, int32 InPlayerIndex)
{
	Subsystem = InSubsystem;
	PlayerIndex = InPlayerIndex;
	Ammo = MagazineSize;
	if (Subsystem)
	{
		Subsystem->SetAmmoForPlayer(PlayerIndex, Ammo);
	}
	OnAmmoChanged.Broadcast(Ammo);
}

bool ULightgunWeapon::TryFire()
{
	if (Ammo <= 0)
	{
		if (Subsystem)
		{
			Subsystem->NotifyEmptyForPlayer(PlayerIndex);
		}
		OnDryFire.Broadcast();
		return false;
	}

	--Ammo;
	if (Subsystem)
	{
		Subsystem->FireRecoilForPlayer(PlayerIndex);
		Subsystem->SetAmmoForPlayer(PlayerIndex, Ammo);
	}
	OnAmmoChanged.Broadcast(Ammo);
	return true;
}

void ULightgunWeapon::Reload()
{
	Ammo = MagazineSize;
	if (Subsystem)
	{
		Subsystem->SetAmmoForPlayer(PlayerIndex, Ammo);
		// Sinden's vendor-suggested reload feel: a quick double pulse.
		if (Subsystem->GetActiveGunForPlayer(PlayerIndex).Model == ELightgunModel::Sinden)
		{
			Subsystem->PlayGunEffectForPlayer(PlayerIndex, TEXT("T2170"));
		}
	}
	OnAmmoChanged.Broadcast(Ammo);
	OnReloaded.Broadcast();
}
