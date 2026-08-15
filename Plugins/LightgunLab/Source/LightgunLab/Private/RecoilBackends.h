// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "LightgunTypes.h"

class ULightgunSettings;

/**
 * One backend = one physical gun under game control.
 * The subsystem enforces the ammo gate; backends only ever hear about events
 * that should reach hardware.
 */
class IRecoilBackend
{
public:
	virtual ~IRecoilBackend() {}

	virtual bool Init(const FDetectedLightgun& Gun, const ULightgunSettings& Settings, FString& OutError) = 0;

	/** Seize feedback control (S6 / SM.6.1 / ZS / 1K0). The gun's own trigger recoil goes silent after this. */
	virtual void EnterGameControl() = 0;

	/** Hand control back (E / ES / ZX / 1K1). Always called on teardown. */
	virtual void ReleaseGameControl() = 0;

	/** One live round fired. */
	virtual void FireRecoil() = 0;

	/** Hammer down on an empty magazine. Default: nothing (silence = authentic for most guns). */
	virtual void NotifyEmpty() {}

	/** Live ammo count changed (OpenFIRE OLED, future Blamcon counter). */
	virtual void SetAmmo(int32 Count) {}

	virtual void SetLife(int32 Value) {}

	virtual void RumblePulse() {}

	/** Free-form gun effect, e.g. Sinden "T2200" shotgun rack. */
	virtual void PlayEffect(const FString& Effect) {}

	virtual bool IsHealthy() const = 0;
	virtual FString GetStatusText() const = 0;
};

/** Creates the right backend for a detected gun, or null for models without a control channel. */
TSharedPtr<IRecoilBackend> MakeRecoilBackend(const FDetectedLightgun& Gun);

/**
 * Pins the process-wide Sinden recoil-server TCP connection (bench law: exactly ONE
 * connection ever - churn or reconnects wedge the server until the software restarts).
 * All Sinden backends share it with per-player command prefixes; the subsystem holds
 * one of these for the whole session so gun reselection never closes the socket.
 */
TSharedPtr<class FSindenSharedConnection> PinSindenConnection();
