// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"

/** Latest aim point for a player, in desktop pixels (same space as FSlateApplication::GetCursorPos). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLightgunRawAim, int32 /*PlayerIndex*/, FVector2f /*DesktopPx*/);
/** Trigger (left button) pressed on the player's device; position is the aim at press time. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLightgunRawTrigger, int32 /*PlayerIndex*/, FVector2f /*DesktopPx*/);
/** Any non-trigger button or a correlated/desk keyboard key: a reload request for that player. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLightgunRawReload, int32 /*PlayerIndex*/, const FString& /*Reason*/);

/**
 * Windows Raw Input router - the piece that turns "the OS merges all mice into one
 * cursor" into two independent aim points. Registers for raw mouse AND keyboard input
 * (RIDEV_INPUTSINK on the game window) and correlates every WM_INPUT event's source
 * device to a player slot:
 *
 *   1. exact raw device path (Sinden entries carry theirs from detection),
 *   2. shared physical USB parent (a COM gun's HID mouse sibling - survives identical PIDs),
 *   3. VID/PID match (unique guns),
 *   4. VID/PID match in stable path order (identical-PID pair fallback - Swap fixes a wrong guess).
 *
 * Unmatched RELATIVE mice drive the "Desktop mouse (aim only)" player, if one is
 * configured. Unmatched ABSOLUTE devices (a software-injected aim stream, e.g. a
 * vendor app moving the cursor for its gun) are adopted by the first gun player whose
 * own device hasn't produced aim yet - logged loudly so the bench can see the routing.
 * Keyboards correlate the same way; an uncorrelated (desk) keyboard reloads P1.
 * Key auto-repeats are filtered; raw input has no double-click synthesis, so rapid
 * trigger work arrives as clean per-press events (the v0.3 lost-shot pitfall can't recur).
 *
 * Everything runs on the game thread (the Windows message pump + core tickers).
 * Windows-only: Create() returns null elsewhere.
 */
class FLightgunRawInputRouter
{
public:
	struct FPlayerBinding
	{
		/** Slot participates in routing (has a gun or is the desktop-mouse player). */
		bool bActive = false;
		/** Aim-only desktop mouse pick: fed by unmatched relative mice, never by gun matching. */
		bool bDesktopMouse = false;
		int32 Vid = 0;
		int32 Pid = 0;
		FString RawInputMousePath;
		FString UsbCompositeParentId;
	};

	/** Null on non-Windows platforms or when Slate isn't up (headless). */
	static TSharedPtr<FLightgunRawInputRouter> Create();

	virtual ~FLightgunRawInputRouter() {}

	/** Registers devices + the window message hook. Safe to call again after Stop(). */
	virtual bool Start() = 0;
	virtual void Stop() = 0;

	virtual void SetPlayerBinding(int32 PlayerIndex, const FPlayerBinding& Binding) = 0;

	/** Re-correlates hDevice -> player. Call after bindings change or on rescan. */
	virtual void RebuildDeviceMap() = 0;

	/** One line per player about device correlation, for on-screen bench diagnostics. */
	virtual FString GetDebugSummary() const = 0;

	FOnLightgunRawAim OnAim;
	FOnLightgunRawTrigger OnTrigger;
	FOnLightgunRawReload OnReload;
};
