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
	// Reload feel = rumble motor only (guns that have one); the solenoid stays
	// quiet - recoil only ever means a live round.
	Ammo = MagazineSize;
	if (Subsystem)
	{
		Subsystem->SetAmmoForPlayer(PlayerIndex, Ammo);
		Subsystem->NotifyReloadedForPlayer(PlayerIndex);
	}
	OnAmmoChanged.Broadcast(Ammo);
	OnReloaded.Broadcast();
}
