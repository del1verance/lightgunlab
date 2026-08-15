// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunSubsystem.h"
#include "LightgunSettings.h"
#include "LightgunDetection.h"
#include "RecoilBackends.h"
#include "MameOutputServer.h"
#include "MameWindowBroadcaster.h"
#include "LightgunRawInput.h"
#include "SindenBorderWidget.h"
#include "LightgunStartupPanel.h"
#include "LightgunOptionsPanel.h"
#include "LightgunCalibrationScreen.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "Misc/App.h"
#include "GameFramework/PlayerController.h"

namespace
{
	void EnableUiInteraction(UGameInstance* GameInstance)
	{
		if (APlayerController* PC = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr)
		{
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
		}
	}
}

// Defined here so TSharedPtr members see complete backend/server types.
ULightgunSubsystem::~ULightgunSubsystem() = default;

void ULightgunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const ULightgunSettings* Settings = GetDefault<ULightgunSettings>();
	if (Settings->bAutoDetectOnStartup)
	{
		ScanForLightguns();
		RestoreSavedSelection();
	}
	StartOutputServersIfEnabled();

	// PostLoadMapWithWorld never fires for PIE worlds (they're duplicated, not
	// loaded), so poll until a game world has begun play and has a viewport.
	if (Settings->bShowStartupPanel)
	{
		TWeakObjectPtr<ULightgunSubsystem> WeakThis(this);
		StartupPollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			ULightgunSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return false;
			}
			UGameInstance* GameInstance = Self->GetGameInstance();
			UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
			if (World && World->IsGameWorld() && World->HasBegunPlay() && GameInstance->GetGameViewportClient())
			{
				if (!Self->bStartupPanelShown)
				{
					Self->bStartupPanelShown = true;
					Self->ShowStartupPanel();
				}
				return false;
			}
			return true;
		}), 0.25f);
	}
}

void ULightgunSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(StartupPollHandle);
	if (RawRouter.IsValid())
	{
		RawRouter->Stop();
		RawRouter.Reset();
	}
	for (int32 Player = 0; Player < LightgunMaxPlayers; ++Player)
	{
		EndGameControlForPlayer(Player);
		TeardownBackend(Player);
	}
	// Last out: destroying the pinned connection flushes the queued K1 releases.
	SindenKeepalive.Reset();
	StopOutputServers();
	Super::Deinitialize();
}

void ULightgunSubsystem::ScanForLightguns()
{
	FLightgunDetector::Scan(DetectedGuns);
	OnStatusChanged.Broadcast();
}

void ULightgunSubsystem::RestoreSavedSelection()
{
	const ULightgunSettings* Settings = GetDefault<ULightgunSettings>();
	if (Settings->PreferredModel != ELightgunModel::None)
	{
		for (int32 Index = 0; Index < DetectedGuns.Num(); ++Index)
		{
			const FDetectedLightgun& Gun = DetectedGuns[Index];
			if (Gun.Model == Settings->PreferredModel &&
				(Settings->PreferredComPort.IsEmpty() || Gun.ComPort == Settings->PreferredComPort))
			{
				SelectGunForPlayer(0, Index);
				break;
			}
		}
	}

	if (Settings->bTwoPlayerMode)
	{
		if (Settings->bPreferredP2IsMouse || Settings->PreferredModelP2 == ELightgunModel::None)
		{
			SelectMouseForPlayer(1);
		}
		else
		{
			for (int32 Index = 0; Index < DetectedGuns.Num(); ++Index)
			{
				const FDetectedLightgun& Gun = DetectedGuns[Index];
				if (Index != Slots[0].GunIndex && Gun.Model == Settings->PreferredModelP2 &&
					(Settings->PreferredComPortP2.IsEmpty() || Gun.ComPort == Settings->PreferredComPortP2))
				{
					SelectGunForPlayer(1, Index);
					break;
				}
			}
		}
	}
}

bool ULightgunSubsystem::IsTwoPlayerMode() const
{
	return GetDefault<ULightgunSettings>()->bTwoPlayerMode;
}

void ULightgunSubsystem::SetTwoPlayerMode(bool bEnabled)
{
	ULightgunSettings* Settings = GetMutableDefault<ULightgunSettings>();
	if (Settings->bTwoPlayerMode == bEnabled)
	{
		return;
	}
	Settings->bTwoPlayerMode = bEnabled;
	Settings->SaveConfig();

	if (!bEnabled)
	{
		// Back to the validated 1P world: P2 releases its gun, the router goes away.
		if (RawRouter.IsValid())
		{
			RawRouter->Stop();
			RawRouter.Reset();
		}
		EndGameControlForPlayer(1);
		TeardownBackend(1);
		Slots[1] = FPlayerSlot();
	}
	OnStatusChanged.Broadcast();
}

bool ULightgunSubsystem::SelectGunByIndex(int32 Index)
{
	return SelectGunForPlayer(0, Index);
}

bool ULightgunSubsystem::SelectGunForPlayer(int32 PlayerIndex, int32 Index)
{
	if (!IsValidPlayer(PlayerIndex) || !DetectedGuns.IsValidIndex(Index))
	{
		return false;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];

	// Same physical device can't serve two players.
	if (IsTwoPlayerMode())
	{
		const int32 Other = 1 - PlayerIndex;
		if (Slots[Other].GunIndex == Index)
		{
			Slot.LastError = TEXT("That device is already assigned to the other player.");
			OnStatusChanged.Broadcast();
			return false;
		}
	}

	// Reselecting the active gun must not churn the backend: tear-down/reconnect
	// cycles can wedge single-client recoil servers (Sinden, bench-confirmed).
	if (Index == Slot.GunIndex && !Slot.bDesktopMouse && Slot.Backend.IsValid() && Slot.Backend->IsHealthy())
	{
		return true;
	}
	const FDetectedLightgun& Gun = DetectedGuns[Index];
	if (Gun.Model == ELightgunModel::UnknownSerial || Gun.Model == ELightgunModel::None)
	{
		Slot.LastError = TEXT("That device has no supported recoil protocol; using it would mean aim-only.");
		Slot.GunIndex = Index;
		Slot.bDesktopMouse = false;
		TeardownBackend(PlayerIndex);
		OnStatusChanged.Broadcast();
		return true;
	}

	TeardownBackend(PlayerIndex);
	Slot.GunIndex = Index;
	Slot.bDesktopMouse = false;

	Slot.Backend = MakeRecoilBackend(Gun);
	if (Slot.Backend.IsValid())
	{
		FString Error;
		if (!Slot.Backend->Init(Gun, *GetDefault<ULightgunSettings>(), Error))
		{
			Slot.LastError = Error;
			UE_LOG(LogLightgunLab, Warning, TEXT("Backend init failed for %s: %s"), *Gun.DisplayName, *Error);
			Slot.Backend.Reset();
		}
		else
		{
			Slot.LastError.Reset();
			if (Gun.Model == ELightgunModel::Sinden)
			{
				// Bench law: exactly one Sinden TCP connection, held for the session.
				SindenKeepalive = PinSindenConnection();
			}
		}
	}

	PersistSlotPrefs(PlayerIndex);

	if (IsTwoPlayerMode())
	{
		UpdateBorderForTwoPlayer();
		PushRouterBindings();
	}
	else if (Gun.Model == ELightgunModel::Sinden && GetDefault<ULightgunSettings>()->bBorderAutoShow)
	{
		SetBorderVisible(true);
	}

	// Seize control now so the init handshake drains through the paced queue
	// long before the first shot - a fire queued right behind it can be lost.
	BeginGameControlForPlayer(PlayerIndex);

	UE_LOG(LogLightgunLab, Log, TEXT("P%d selected lightgun: %s"), PlayerIndex + 1, *Gun.DisplayName);
	OnStatusChanged.Broadcast();
	return true;
}

void ULightgunSubsystem::SelectMouseOnly()
{
	SelectMouseForPlayer(0);
}

void ULightgunSubsystem::SelectMouseForPlayer(int32 PlayerIndex)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	TeardownBackend(PlayerIndex);
	Slot.GunIndex = INDEX_NONE;
	Slot.bDesktopMouse = true;
	Slot.LastError.Reset();

	PersistSlotPrefs(PlayerIndex);

	if (IsTwoPlayerMode())
	{
		UpdateBorderForTwoPlayer();
		PushRouterBindings();
	}
	else if (PlayerIndex == 0)
	{
		SetBorderVisible(false);
	}
	OnStatusChanged.Broadcast();
}

void ULightgunSubsystem::PersistSlotPrefs(int32 PlayerIndex)
{
	ULightgunSettings* Settings = GetMutableDefault<ULightgunSettings>();
	const FPlayerSlot& Slot = Slots[PlayerIndex];
	const bool bHasGun = DetectedGuns.IsValidIndex(Slot.GunIndex);
	const ELightgunModel Model = bHasGun ? DetectedGuns[Slot.GunIndex].Model : ELightgunModel::None;
	const FString ComPort = bHasGun ? DetectedGuns[Slot.GunIndex].ComPort : FString();

	if (PlayerIndex == 0)
	{
		Settings->PreferredModel = Model;
		Settings->PreferredComPort = ComPort;
	}
	else
	{
		Settings->PreferredModelP2 = Model;
		Settings->PreferredComPortP2 = ComPort;
		Settings->bPreferredP2IsMouse = Slot.bDesktopMouse;
	}
	Settings->SaveConfig();
}

bool ULightgunSubsystem::HasActiveGunForPlayer(int32 PlayerIndex) const
{
	return IsValidPlayer(PlayerIndex) && Slots[PlayerIndex].GunIndex != INDEX_NONE;
}

FDetectedLightgun ULightgunSubsystem::GetActiveGunForPlayer(int32 PlayerIndex) const
{
	if (IsValidPlayer(PlayerIndex) && DetectedGuns.IsValidIndex(Slots[PlayerIndex].GunIndex))
	{
		return DetectedGuns[Slots[PlayerIndex].GunIndex];
	}
	return FDetectedLightgun();
}

bool ULightgunSubsystem::IsPlayerDesktopMouse(int32 PlayerIndex) const
{
	return IsValidPlayer(PlayerIndex) && Slots[PlayerIndex].bDesktopMouse;
}

FString ULightgunSubsystem::GetStatusSummaryForPlayer(int32 PlayerIndex) const
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return FString();
	}
	const FPlayerSlot& Slot = Slots[PlayerIndex];
	if (!DetectedGuns.IsValidIndex(Slot.GunIndex))
	{
		return Slot.bDesktopMouse && IsTwoPlayerMode()
			? TEXT("Desktop mouse - aim only, no recoil hardware.")
			: TEXT("No lightgun selected - playing with mouse.");
	}
	const FDetectedLightgun& Gun = DetectedGuns[Slot.GunIndex];
	FString Summary = Slot.Backend.IsValid() ? Slot.Backend->GetStatusText() : Gun.DisplayName + TEXT(" (aim only)");
	if (!Gun.DetectionNote.IsEmpty())
	{
		Summary += TEXT("\n") + Gun.DetectionNote;
	}
	if (!Slot.LastError.IsEmpty())
	{
		Summary += TEXT("\n") + Slot.LastError;
	}
	return Summary;
}

void ULightgunSubsystem::SwapPlayers()
{
	if (!IsTwoPlayerMode())
	{
		return;
	}
	// Wholesale swap: guns, backends (they keep their hardware-tied prefixes and open
	// ports - nothing reconnects), and control state all move together.
	Swap(Slots[0], Slots[1]);
	PersistSlotPrefs(0);
	PersistSlotPrefs(1);
	PushRouterBindings();
	UE_LOG(LogLightgunLab, Log, TEXT("Swapped P1 and P2 assignments"));
	OnStatusChanged.Broadcast();
}

void ULightgunSubsystem::StartTwoPlayerSession()
{
	for (int32 Player = 0; Player < LightgunMaxPlayers; ++Player)
	{
		BeginGameControlForPlayer(Player);
	}
	UpdateBorderForTwoPlayer();

	if (!FApp::CanEverRender())
	{
		return; // headless: nothing to aim at, no window to hook
	}
	if (!RawRouter.IsValid())
	{
		RawRouter = FLightgunRawInputRouter::Create();
	}
	if (RawRouter.IsValid())
	{
		PushRouterBindings();
		if (!RawRouter->Start())
		{
			UE_LOG(LogLightgunLab, Warning, TEXT("Raw input router failed to start - 2P aim will not track"));
		}
		else
		{
			RawRouter->RebuildDeviceMap();
		}
	}
}

void ULightgunSubsystem::PushRouterBindings()
{
	if (!RawRouter.IsValid())
	{
		return;
	}
	for (int32 Player = 0; Player < LightgunMaxPlayers; ++Player)
	{
		const FPlayerSlot& Slot = Slots[Player];
		FLightgunRawInputRouter::FPlayerBinding Binding;
		Binding.bDesktopMouse = Slot.bDesktopMouse;
		Binding.bActive = Slot.bDesktopMouse || DetectedGuns.IsValidIndex(Slot.GunIndex);
		if (DetectedGuns.IsValidIndex(Slot.GunIndex))
		{
			const FDetectedLightgun& Gun = DetectedGuns[Slot.GunIndex];
			Binding.Vid = Gun.Vid;
			Binding.Pid = Gun.Pid;
			Binding.RawInputMousePath = Gun.RawInputMousePath;
			Binding.UsbCompositeParentId = Gun.UsbCompositeParentId;
		}
		RawRouter->SetPlayerBinding(Player, Binding);
	}
	RawRouter->RebuildDeviceMap();
}

void ULightgunSubsystem::UpdateBorderForTwoPlayer()
{
	// 2P rule: the border is global (the Sinden software tracks the whole screen),
	// so it shows when EITHER selected gun is a Sinden.
	bool bAnySinden = false;
	for (int32 Player = 0; Player < LightgunMaxPlayers; ++Player)
	{
		bAnySinden |= GetActiveGunForPlayer(Player).Model == ELightgunModel::Sinden;
	}
	if (bAnySinden && GetDefault<ULightgunSettings>()->bBorderAutoShow)
	{
		SetBorderVisible(true);
	}
	else if (!bAnySinden)
	{
		SetBorderVisible(false);
	}
}

void ULightgunSubsystem::BeginGameControlForPlayer(int32 PlayerIndex)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid() && !Slot.bInControl && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Slot.Backend->EnterGameControl();
		Slot.bInControl = true;
	}
}

void ULightgunSubsystem::EndGameControlForPlayer(int32 PlayerIndex)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid() && Slot.bInControl)
	{
		Slot.Backend->ReleaseGameControl();
		Slot.bInControl = false;
	}
}

void ULightgunSubsystem::FireRecoilForPlayer(int32 PlayerIndex)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		if (!Slot.bInControl)
		{
			BeginGameControlForPlayer(PlayerIndex);
		}
		Slot.Backend->FireRecoil();
	}
	EmitOutputPulse(PlayerIndex, GetDefault<ULightgunSettings>()->RecoilOutputName);
}

void ULightgunSubsystem::NotifyEmptyForPlayer(int32 PlayerIndex)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Slot.Backend->NotifyEmpty();
	}
	// Deliberately no recoil output emission: an empty gun stays silent on outputs rigs too.
}

void ULightgunSubsystem::SetAmmoForPlayer(int32 PlayerIndex, int32 Count)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Slot.Backend->SetAmmo(Count);
	}
	EmitOutput(PlayerIndex, TEXT("Ammo"), Count);
}

void ULightgunSubsystem::SetLifeForPlayer(int32 PlayerIndex, int32 Value)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Slot.Backend->SetLife(Value);
	}
	EmitOutput(PlayerIndex, TEXT("Life"), Value);
}

void ULightgunSubsystem::NotifyDamagedForPlayer(int32 PlayerIndex)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Slot.Backend->RumblePulse();
	}
	EmitOutputPulse(PlayerIndex, TEXT("Damaged"));
}

void ULightgunSubsystem::RumblePulseForPlayer(int32 PlayerIndex)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Slot.Backend->RumblePulse();
	}
}

void ULightgunSubsystem::PlayGunEffectForPlayer(int32 PlayerIndex, const FString& Effect)
{
	if (!IsValidPlayer(PlayerIndex))
	{
		return;
	}
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Slot.Backend->PlayEffect(Effect);
	}
}

void ULightgunSubsystem::TestFireForPlayer(int32 PlayerIndex)
{
	BeginGameControlForPlayer(PlayerIndex);
	FireRecoilForPlayer(PlayerIndex);
}

void ULightgunSubsystem::SetRecoilMode(ERecoilMode Mode)
{
	ULightgunSettings* Settings = GetMutableDefault<ULightgunSettings>();
	if (Settings->RecoilMode != Mode)
	{
		if (Mode != ERecoilMode::DirectSerial)
		{
			for (int32 Player = 0; Player < LightgunMaxPlayers; ++Player)
			{
				EndGameControlForPlayer(Player);
			}
		}
		Settings->RecoilMode = Mode;
		Settings->SaveConfig();
		OnStatusChanged.Broadcast();
	}
}

ERecoilMode ULightgunSubsystem::GetRecoilMode() const
{
	return GetDefault<ULightgunSettings>()->RecoilMode;
}

void ULightgunSubsystem::SetRecoilStrength(int32 Strength)
{
	ULightgunSettings* Settings = GetMutableDefault<ULightgunSettings>();
	Settings->RecoilStrength = FMath::Clamp(Strength, 0, 10);
	Settings->SaveConfig();
	// Sinden picks strength up on the next EnterGameControl; push it live to every
	// in-control Sinden slot.
	for (int32 Player = 0; Player < LightgunMaxPlayers; ++Player)
	{
		const FPlayerSlot& Slot = Slots[Player];
		if (Slot.Backend.IsValid() && Slot.bInControl && GetActiveGunForPlayer(Player).Model == ELightgunModel::Sinden)
		{
			Slot.Backend->PlayEffect(FString::Printf(TEXT("N%d"), Settings->RecoilStrength));
		}
	}
}

void ULightgunSubsystem::SetOutputsEnabled(bool bTcp, bool bWindowMessages)
{
	ULightgunSettings* Settings = GetMutableDefault<ULightgunSettings>();
	Settings->bEnableTcpOutputs = bTcp;
	Settings->bEnableWindowMessageOutputs = bWindowMessages;
	Settings->SaveConfig();
	StopOutputServers();
	StartOutputServersIfEnabled();
	OnStatusChanged.Broadcast();
}

void ULightgunSubsystem::SetBorderVisible(bool bVisible)
{
	if (bVisible && FApp::CanEverRender())
	{
		if (!BorderWidget)
		{
			BorderWidget = CreateWidget<ULightgunBorderWidget>(GetGameInstance(), ULightgunBorderWidget::StaticClass());
		}
		if (BorderWidget && !BorderWidget->IsInViewport())
		{
			const ULightgunSettings* Settings = GetDefault<ULightgunSettings>();
			BorderWidget->SetPercents(Settings->BorderWhitePercent, Settings->BorderBlackPercent);
			// Topmost: the Sinden tracking frame must never sit under a panel scrim.
			BorderWidget->AddToViewport(9900);
		}
	}
	else if (BorderWidget && BorderWidget->IsInViewport())
	{
		BorderWidget->RemoveFromParent();
	}
}

bool ULightgunSubsystem::IsBorderVisible() const
{
	return BorderWidget != nullptr && BorderWidget->IsInViewport();
}

void ULightgunSubsystem::SetBorderPercents(float WhitePercent, float BlackPercent)
{
	ULightgunSettings* Settings = GetMutableDefault<ULightgunSettings>();
	Settings->BorderWhitePercent = FMath::Clamp(WhitePercent, 0.5f, 8.0f);
	Settings->BorderBlackPercent = FMath::Clamp(BlackPercent, 0.0f, 10.0f);
	Settings->SaveConfig();
	if (BorderWidget)
	{
		BorderWidget->SetPercents(Settings->BorderWhitePercent, Settings->BorderBlackPercent);
	}
}

void ULightgunSubsystem::SaveSettings()
{
	GetMutableDefault<ULightgunSettings>()->SaveConfig();
}

void ULightgunSubsystem::ShowStartupPanel()
{
	if (!FApp::CanEverRender())
	{
		return; // headless (-nullrhi/commandlet): no viewport to panel over
	}
	if (!StartupPanel)
	{
		StartupPanel = CreateWidget<ULightgunStartupPanel>(GetGameInstance(), ULightgunStartupPanel::StaticClass());
	}
	if (StartupPanel && !StartupPanel->IsInViewport())
	{
		StartupPanel->AddToViewport(9500);
		EnableUiInteraction(GetGameInstance());
	}
}

void ULightgunSubsystem::ShowOptionsPanel()
{
	if (!FApp::CanEverRender())
	{
		return;
	}
	if (!OptionsPanel)
	{
		OptionsPanel = CreateWidget<ULightgunOptionsPanel>(GetGameInstance(), ULightgunOptionsPanel::StaticClass());
	}
	if (OptionsPanel && !OptionsPanel->IsInViewport())
	{
		OptionsPanel->AddToViewport(9500);
		EnableUiInteraction(GetGameInstance());
	}
}

void ULightgunSubsystem::ShowCalibrationScreen()
{
	if (!FApp::CanEverRender())
	{
		return;
	}
	// The widget tree is built for one mode; a 1P/2P switch needs a fresh build.
	if (CalibrationScreen && CalibrationScreen->WasBuiltForTwoPlayer() != IsTwoPlayerMode())
	{
		if (CalibrationScreen->IsInViewport())
		{
			CalibrationScreen->RemoveFromParent();
		}
		CalibrationScreen = nullptr;
	}
	if (!CalibrationScreen)
	{
		CalibrationScreen = CreateWidget<ULightgunCalibrationScreen>(GetGameInstance(), ULightgunCalibrationScreen::StaticClass());
	}
	if (CalibrationScreen && !CalibrationScreen->IsInViewport())
	{
		CalibrationScreen->AddToViewport(9000);
		if (APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false; // the range draws its own crosshair
		}
	}
}

void ULightgunSubsystem::EmitOutput(int32 PlayerIndex, const FString& ShortName, int32 Value)
{
	const FString FullName = PlayerPrefixed(PlayerIndex, ShortName);
	if (TcpOutputs.IsValid())
	{
		TcpOutputs->SendOutput(FullName, Value);
	}
	if (WindowOutputs.IsValid())
	{
		WindowOutputs->SendOutput(FullName, Value);
	}
}

void ULightgunSubsystem::EmitOutputPulse(int32 PlayerIndex, const FString& ShortName)
{
	if (!TcpOutputs.IsValid() && !WindowOutputs.IsValid())
	{
		return;
	}
	EmitOutput(PlayerIndex, ShortName, 1);
	const FString NameCopy = ShortName;
	TWeakObjectPtr<ULightgunSubsystem> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, NameCopy, PlayerIndex](float)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->EmitOutput(PlayerIndex, NameCopy, 0);
		}
		return false; // one-shot
	}), 0.05f);
}

void ULightgunSubsystem::StartOutputServersIfEnabled()
{
	const ULightgunSettings* Settings = GetDefault<ULightgunSettings>();
	if (Settings->bEnableTcpOutputs && !TcpOutputs.IsValid())
	{
		TcpOutputs = MakeShared<FMameOutputServer>(Settings->OutputsGameName);
		if (!TcpOutputs->Start(Settings->OutputsTcpPort))
		{
			TcpOutputs.Reset();
		}
	}
	if (Settings->bEnableWindowMessageOutputs && !WindowOutputs.IsValid())
	{
		WindowOutputs = MakeShared<FMameWindowBroadcaster>(Settings->OutputsGameName);
		if (!WindowOutputs->Start())
		{
			WindowOutputs.Reset();
		}
	}
}

void ULightgunSubsystem::StopOutputServers()
{
	TcpOutputs.Reset();
	WindowOutputs.Reset();
}

void ULightgunSubsystem::TeardownBackend(int32 PlayerIndex)
{
	FPlayerSlot& Slot = Slots[PlayerIndex];
	if (Slot.Backend.IsValid())
	{
		if (Slot.bInControl)
		{
			Slot.Backend->ReleaseGameControl();
			Slot.bInControl = false;
		}
		Slot.Backend.Reset();
	}
}

FString ULightgunSubsystem::PlayerPrefixed(int32 PlayerIndex, const FString& ShortName) const
{
	// 1P keeps the configurable seat number (cab setups that call the only seat P3).
	// In 2P both seats are local and literal: P1_ and P2_.
	const int32 Seat = IsTwoPlayerMode() ? PlayerIndex + 1 : GetDefault<ULightgunSettings>()->PlayerSlot;
	return FString::Printf(TEXT("P%d_%s"), Seat, *ShortName);
}
