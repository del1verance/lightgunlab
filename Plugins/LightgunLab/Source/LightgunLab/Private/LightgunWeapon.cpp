// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunWeapon.h"
#include "LightgunSubsystem.h"

void ULightgunWeapon::Initialize(ULightgunSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	Ammo = MagazineSize;
	if (Subsystem)
	{
		Subsystem->SetAmmo(Ammo);
	}
	OnAmmoChanged.Broadcast(Ammo);
}

bool ULightgunWeapon::TryFire()
{
	if (Ammo <= 0)
	{
		if (Subsystem)
		{
			Subsystem->NotifyEmpty();
		}
		OnDryFire.Broadcast();
		return false;
	}

	--Ammo;
	if (Subsystem)
	{
		Subsystem->FireRecoil();
		Subsystem->SetAmmo(Ammo);
	}
	OnAmmoChanged.Broadcast(Ammo);
	return true;
}

void ULightgunWeapon::Reload()
{
	Ammo = MagazineSize;
	if (Subsystem)
	{
		Subsystem->SetAmmo(Ammo);
		// Sinden's vendor-suggested reload feel: a quick double pulse.
		if (Subsystem->GetActiveGun().Model == ELightgunModel::Sinden)
		{
			Subsystem->PlayGunEffect(TEXT("T2170"));
		}
	}
	OnAmmoChanged.Broadcast(Ammo);
	OnReloaded.Broadcast();
}
