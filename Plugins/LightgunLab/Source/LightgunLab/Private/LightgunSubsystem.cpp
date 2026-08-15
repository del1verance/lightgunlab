// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunSubsystem.h"
#include "LightgunSettings.h"
#include "LightgunDetection.h"
#include "RecoilBackends.h"
#include "MameOutputServer.h"
#include "MameWindowBroadcaster.h"
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

// Defined here so TUniquePtr members see complete backend/server types.
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
	EndGameControl();
	TeardownBackend();
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
	if (Settings->PreferredModel == ELightgunModel::None)
	{
		return;
	}
	for (int32 Index = 0; Index < DetectedGuns.Num(); ++Index)
	{
		const FDetectedLightgun& Gun = DetectedGuns[Index];
		if (Gun.Model == Settings->PreferredModel &&
			(Settings->PreferredComPort.IsEmpty() || Gun.ComPort == Settings->PreferredComPort))
		{
			SelectGunByIndex(Index);
			return;
		}
	}
}

bool ULightgunSubsystem::SelectGunByIndex(int32 Index)
{
	if (!DetectedGuns.IsValidIndex(Index))
	{
		return false;
	}
	// Reselecting the active gun must not churn the backend: tear-down/reconnect
	// cycles can wedge single-client recoil servers (Sinden, bench-confirmed).
	if (Index == ActiveGunIndex && Backend.IsValid() && Backend->IsHealthy())
	{
		return true;
	}
	const FDetectedLightgun& Gun = DetectedGuns[Index];
	if (Gun.Model == ELightgunModel::UnknownSerial || Gun.Model == ELightgunModel::None)
	{
		LastError = TEXT("That device has no supported recoil protocol; using it would mean aim-only.");
		ActiveGunIndex = Index;
		TeardownBackend();
		OnStatusChanged.Broadcast();
		return true;
	}

	TeardownBackend();
	ActiveGunIndex = Index;

	Backend = MakeRecoilBackend(Gun);
	if (Backend.IsValid())
	{
		FString Error;
		if (!Backend->Init(Gun, *GetDefault<ULightgunSettings>(), Error))
		{
			LastError = Error;
			UE_LOG(LogLightgunLab, Warning, TEXT("Backend init failed for %s: %s"), *Gun.DisplayName, *Error);
			Backend.Reset();
		}
		else
		{
			LastError.Reset();
		}
	}

	ULightgunSettings* Settings = GetMutableDefault<ULightgunSettings>();
	Settings->PreferredModel = Gun.Model;
	Settings->PreferredComPort = Gun.ComPort;
	Settings->SaveConfig();

	if (Gun.Model == ELightgunModel::Sinden && GetDefault<ULightgunSettings>()->bBorderAutoShow)
	{
		SetBorderVisible(true);
	}

	// Seize control now so the init handshake drains through the paced queue
	// long before the first shot - a fire queued right behind it can be lost.
	BeginGameControl();

	UE_LOG(LogLightgunLab, Log, TEXT("Selected lightgun: %s"), *Gun.DisplayName);
	OnStatusChanged.Broadcast();
	return true;
}

void ULightgunSubsystem::SelectMouseOnly()
{
	TeardownBackend();
	ActiveGunIndex = INDEX_NONE;
	LastError.Reset();

	ULightgunSettings* Settings = GetMutableDefault<ULightgunSettings>();
	Settings->PreferredModel = ELightgunModel::None;
	Settings->PreferredComPort.Reset();
	Settings->SaveConfig();

	SetBorderVisible(false);
	OnStatusChanged.Broadcast();
}

FDetectedLightgun ULightgunSubsystem::GetActiveGun() const
{
	return DetectedGuns.IsValidIndex(ActiveGunIndex) ? DetectedGuns[ActiveGunIndex] : FDetectedLightgun();
}

FString ULightgunSubsystem::GetStatusSummary() const
{
	if (!DetectedGuns.IsValidIndex(ActiveGunIndex))
	{
		return TEXT("No lightgun selected - playing with mouse.");
	}
	const FDetectedLightgun& Gun = DetectedGuns[ActiveGunIndex];
	FString Summary = Backend.IsValid() ? Backend->GetStatusText() : Gun.DisplayName + TEXT(" (aim only)");
	if (!Gun.DetectionNote.IsEmpty())
	{
		Summary += TEXT("\n") + Gun.DetectionNote;
	}
	if (!LastError.IsEmpty())
	{
		Summary += TEXT("\n") + LastError;
	}
	return Summary;
}

void ULightgunSubsystem::BeginGameControl()
{
	if (Backend.IsValid() && !bInGameControl && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Backend->EnterGameControl();
		bInGameControl = true;
	}
}

void ULightgunSubsystem::EndGameControl()
{
	if (Backend.IsValid() && bInGameControl)
	{
		Backend->ReleaseGameControl();
		bInGameControl = false;
	}
}

void ULightgunSubsystem::FireRecoil()
{
	if (Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		if (!bInGameControl)
		{
			BeginGameControl();
		}
		Backend->FireRecoil();
	}
	EmitOutputPulse(GetDefault<ULightgunSettings>()->RecoilOutputName);
}

void ULightgunSubsystem::NotifyEmpty()
{
	if (Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Backend->NotifyEmpty();
	}
	// Deliberately no recoil output emission: an empty gun stays silent on outputs rigs too.
}

void ULightgunSubsystem::SetAmmo(int32 Count)
{
	if (Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Backend->SetAmmo(Count);
	}
	EmitOutput(TEXT("Ammo"), Count);
}

void ULightgunSubsystem::SetLife(int32 Value)
{
	if (Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Backend->SetLife(Value);
	}
	EmitOutput(TEXT("Life"), Value);
}

void ULightgunSubsystem::NotifyDamaged()
{
	if (Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Backend->RumblePulse();
	}
	EmitOutputPulse(TEXT("Damaged"));
}

void ULightgunSubsystem::RumblePulse()
{
	if (Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Backend->RumblePulse();
	}
}

void ULightgunSubsystem::PlayGunEffect(const FString& Effect)
{
	if (Backend.IsValid() && GetRecoilMode() == ERecoilMode::DirectSerial)
	{
		Backend->PlayEffect(Effect);
	}
}

void ULightgunSubsystem::TestFire()
{
	BeginGameControl();
	FireRecoil();
}

void ULightgunSubsystem::SetRecoilMode(ERecoilMode Mode)
{
	ULightgunSettings* Settings = GetMutableDefault<ULightgunSettings>();
	if (Settings->RecoilMode != Mode)
	{
		if (Mode != ERecoilMode::DirectSerial)
		{
			EndGameControl();
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
	// Sinden picks strength up on the next EnterGameControl; push it live if we're in control.
	if (Backend.IsValid() && bInGameControl && GetActiveGun().Model == ELightgunModel::Sinden)
	{
		Backend->PlayEffect(FString::Printf(TEXT("N%d"), Settings->RecoilStrength));
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

void ULightgunSubsystem::EmitOutput(const FString& ShortName, int32 Value)
{
	const FString FullName = PlayerPrefixed(ShortName);
	if (TcpOutputs.IsValid())
	{
		TcpOutputs->SendOutput(FullName, Value);
	}
	if (WindowOutputs.IsValid())
	{
		WindowOutputs->SendOutput(FullName, Value);
	}
}

void ULightgunSubsystem::EmitOutputPulse(const FString& ShortName)
{
	if (!TcpOutputs.IsValid() && !WindowOutputs.IsValid())
	{
		return;
	}
	EmitOutput(ShortName, 1);
	const FString NameCopy = ShortName;
	TWeakObjectPtr<ULightgunSubsystem> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, NameCopy](float)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->EmitOutput(NameCopy, 0);
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

void ULightgunSubsystem::TeardownBackend()
{
	if (Backend.IsValid())
	{
		if (bInGameControl)
		{
			Backend->ReleaseGameControl();
			bInGameControl = false;
		}
		Backend.Reset();
	}
}

FString ULightgunSubsystem::PlayerPrefixed(const FString& ShortName) const
{
	return FString::Printf(TEXT("P%d_%s"), GetDefault<ULightgunSettings>()->PlayerSlot, *ShortName);
}
