// Copyright (c) 2026 del1verance. MIT License.

#include "RecoilBackends.h"
#include "LightgunSerialPort.h"
#include "LightgunSettings.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Containers/Ticker.h"

namespace
{
	/** Command set for the serial gun families. Empty string = unsupported on that gun. */
	struct FSerialDialect
	{
		int32 Baud = 9600;
		FString Enter;
		FString Exit;
		FString Fire;
		FString Empty;
		FString Rumble;
		FString AmmoPrefix;   // live count appended as an integer
		FString Name;
	};

	FSerialDialect MakeDialect(ELightgunModel Model, const ULightgunSettings& Settings)
	{
		FSerialDialect D;
		switch (Model)
		{
		case ELightgunModel::Gun4IR:
			// Official User Guide v1.2 pp.20-21. Solenoid is pulse-only in firmware.
			D.Name = TEXT("GUN4IR");
			D.Baud = 9600;
			D.Enter = TEXT("S6");
			D.Exit = TEXT("E");
			D.Fire = TEXT("F0.2.1");
			D.Rumble = TEXT("F1.2.1");
			break;
		case ELightgunModel::OpenFIRE:
			// OpenFIRE-Firmware OpenFIREserial.cpp; MAMEHooker-convention 'x' separators.
			D.Name = TEXT("OpenFIRE");
			D.Baud = 9600;
			D.Enter = TEXT("S6");
			D.Exit = TEXT("E");
			D.Fire = TEXT("F0x2x1");
			D.Rumble = TEXT("F1x2x1");
			if (Settings.bOpenFireAmmoDisplay)
			{
				D.AmmoPrefix = TEXT("FDA");
			}
			break;
		case ELightgunModel::Blamcon:
			// Vendor spec: blamcon.com/get-started-with-blamcon/serial-commands
			D.Name = TEXT("Blamcon");
			D.Baud = 9600;
			D.Enter = TEXT("SM.6.1");
			D.Exit = TEXT("ES");
			D.Fire = TEXT("FB.0.1");
			D.Rumble = TEXT("FB.1.1");
			break;
		case ELightgunModel::RS3Reaper:
			// Retro Shooter manual "External control COM command list": 115200 8N1, 2-char commands.
			D.Name = TEXT("RS3 Reaper");
			D.Baud = 115200;
			D.Enter = TEXT("ZS");
			D.Exit = TEXT("ZX");
			D.Fire = Settings.RS3FireCommand.IsEmpty() ? TEXT("Z5") : Settings.RS3FireCommand;
			D.Empty = TEXT("Z0");
			D.Rumble = TEXT("ZZ");
			break;
		default:
			break;
		}
		return D;
	}

	/** GUN4IR / OpenFIRE / Blamcon / RS3 over a COM port. */
	class FSerialRecoilBackend : public IRecoilBackend
	{
	public:
		virtual bool Init(const FDetectedLightgun& Gun, const ULightgunSettings& Settings, FString& OutError) override
		{
			Dialect = MakeDialect(Gun.Model, Settings);
			if (Dialect.Enter.IsEmpty())
			{
				OutError = TEXT("No serial dialect for this gun model");
				return false;
			}
			Port = MakeUnique<FLightgunSerialPort>();
			return Port->Open(Gun.ComPort, Dialect.Baud, OutError);
		}

		virtual void EnterGameControl() override
		{
			bInControl = true;
			Port->Enqueue(Dialect.Enter);
		}

		virtual void ReleaseGameControl() override
		{
			if (bInControl && Port.IsValid())
			{
				Port->Enqueue(Dialect.Exit);
				bInControl = false;
			}
		}

		virtual void FireRecoil() override            { Port->Enqueue(Dialect.Fire); }
		virtual void NotifyEmpty() override           { if (!Dialect.Empty.IsEmpty()) { Port->Enqueue(Dialect.Empty); } }
		virtual void RumblePulse() override           { if (!Dialect.Rumble.IsEmpty()) { Port->Enqueue(Dialect.Rumble); } }
		virtual void PlayEffect(const FString& E) override { Port->Enqueue(E); }

		virtual void SetAmmo(int32 Count) override
		{
			if (!Dialect.AmmoPrefix.IsEmpty())
			{
				Port->Enqueue(Dialect.AmmoPrefix + FString::FromInt(FMath::Clamp(Count, 0, 99)));
			}
		}

		virtual bool IsHealthy() const override { return Port.IsValid() && Port->IsOpen() && !Port->HasError(); }

		virtual FString GetStatusText() const override
		{
			return FString::Printf(TEXT("%s on %s%s"), *Dialect.Name,
				Port.IsValid() ? *Port->GetPortName() : TEXT("?"),
				IsHealthy() ? TEXT("") : TEXT(" (write errors)"));
		}

		virtual ~FSerialRecoilBackend() override
		{
			ReleaseGameControl();
			if (Port.IsValid())
			{
				Port->FlushAndClose();
			}
		}

	private:
		TUniquePtr<FLightgunSerialPort> Port;
		FSerialDialect Dialect;
		bool bInControl = false;
	};

	/**
	 * Sinden: TCP to the vendor software's recoil server (RecoilTcpServerReadme.txt, V2.08b).
	 * One command per message, prefixed 1/2/B for player. Messages are paced ~15ms apart
	 * because the server treats each received chunk as a single command.
	 */
	class FSindenTcpBackend : public IRecoilBackend
	{
	public:
		virtual bool Init(const FDetectedLightgun& Gun, const ULightgunSettings& Settings, FString& OutError) override
		{
			Host = Settings.SindenHost;
			TcpPort = Settings.SindenTcpPort;
			Prefix = FString::FromInt(FMath::Clamp(Settings.PlayerSlot, 1, 2));
			Strength = FMath::Clamp(Settings.RecoilStrength, 0, 10);
			EmptyStrength = FMath::Clamp(Settings.SindenEmptyChamberStrength, 0, 10);
			MinGapSeconds = FMath::Clamp(Settings.SindenCommandGapMs, 15, 1000) / 1000.0;

			if (!Connect(OutError))
			{
				// Software may not be running yet; stay lazy and retry on first send.
				UE_LOG(LogLightgunLab, Warning, TEXT("Sinden recoil server not reachable yet: %s"), *OutError);
			}

			PumpHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FSindenTcpBackend::Pump), 0.005f);
			return true; // Presence of the HID gun is enough to construct; connection self-heals.
		}

		virtual void EnterGameControl() override
		{
			// Fresh software instances start with recoil DISABLED (bench-confirmed
			// 2026-08-15: without J1 every A command is silently ignored).
			Send(Prefix + TEXT("J1"));                                   // recoil master ON
			Send(Prefix + TEXT("K0"));                                   // our commands only, no trigger recoil
			Send(Prefix + TEXT("D"));                                    // single-shot mode
			Send(Prefix + TEXT("N") + FString::FromInt(Strength));       // strength
			bInControl = true;
		}

		virtual void ReleaseGameControl() override
		{
			if (bInControl)
			{
				Send(Prefix + TEXT("K1"));
				bInControl = false;
			}
		}

		virtual void FireRecoil() override  { Send(Prefix + TEXT("A")); }
		virtual void NotifyEmpty() override { Send(Prefix + TEXT("U") + FString::FromInt(EmptyStrength)); }
		virtual void RumblePulse() override { Send(Prefix + TEXT("U") + FString::FromInt(FMath::Min(EmptyStrength + 2, 10))); }
		virtual void PlayEffect(const FString& E) override { Send(Prefix + E); }

		virtual bool IsHealthy() const override { return Socket != nullptr; }

		virtual FString GetStatusText() const override
		{
			return FString::Printf(TEXT("Sinden via %s:%d%s"), *Host, TcpPort,
				Socket ? TEXT("") : TEXT(" (recoil server not connected - start Sinden software / 'Start Recoil Server')"));
		}

		virtual ~FSindenTcpBackend() override
		{
			ReleaseGameControl();
			// Push the release out before the socket dies.
			FString Unused;
			FlushQueueNow();
			if (PumpHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(PumpHandle);
			}
			CloseSocket();
		}

	private:
		bool Connect(FString& OutError)
		{
			CloseSocket();
			ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (!Subsystem)
			{
				OutError = TEXT("No socket subsystem");
				return false;
			}
			FIPv4Address Address;
			if (!FIPv4Address::Parse(Host, Address))
			{
				OutError = FString::Printf(TEXT("Bad Sinden host '%s'"), *Host);
				return false;
			}
			FSocket* NewSocket = Subsystem->CreateSocket(NAME_Stream, TEXT("SindenRecoil"), false);
			if (!NewSocket)
			{
				OutError = TEXT("Socket create failed");
				return false;
			}
			NewSocket->SetNoDelay(true);
			TSharedRef<FInternetAddr> Addr = Subsystem->CreateInternetAddr();
			Addr->SetIp(Address.Value);
			Addr->SetPort(TcpPort);
			if (!NewSocket->Connect(*Addr))
			{
				Subsystem->DestroySocket(NewSocket);
				OutError = FString::Printf(TEXT("Connect to %s:%d refused"), *Host, TcpPort);
				return false;
			}
			Socket = NewSocket;
			UE_LOG(LogLightgunLab, Log, TEXT("Connected to Sinden recoil server %s:%d"), *Host, TcpPort);
			return true;
		}

		void CloseSocket()
		{
			if (Socket)
			{
				Socket->Close();
				if (ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
				{
					Subsystem->DestroySocket(Socket);
				}
				Socket = nullptr;
			}
		}

		void Send(const FString& Command)
		{
			Outbox.Enqueue(Command);
		}

		bool Pump(float)
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - LastSendTime < MinGapSeconds)
			{
				return true;
			}
			FString Command;
			if (!Outbox.Dequeue(Command))
			{
				return true;
			}
			SendNow(Command, Now);
			return true;
		}

		void FlushQueueNow()
		{
			FString Command;
			while (Outbox.Dequeue(Command))
			{
				SendNow(Command, FPlatformTime::Seconds());
				FPlatformProcess::Sleep(static_cast<float>(MinGapSeconds));
			}
		}

		void SendNow(const FString& Command, double Now)
		{
			if (!Socket && Now - LastConnectAttempt > 3.0)
			{
				LastConnectAttempt = Now;
				FString Err;
				Connect(Err);
			}
			if (!Socket)
			{
				return;
			}
			const FString Line = Command + TEXT("\n");
			const auto Utf8 = StringCast<ANSICHAR>(*Line);
			int32 Sent = 0;
			if (!Socket->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent))
			{
				UE_LOG(LogLightgunLab, Warning, TEXT("Sinden recoil send failed; will reconnect"));
				CloseSocket();
			}
			LastSendTime = Now;
		}

		FSocket* Socket = nullptr;
		FString Host;
		int32 TcpPort = 13000;
		FString Prefix = TEXT("1");
		int32 Strength = 8;
		int32 EmptyStrength = 4;
		bool bInControl = false;
		double MinGapSeconds = 0.2;
		double LastSendTime = 0.0;
		double LastConnectAttempt = 0.0;
		TQueue<FString, EQueueMode::Mpsc> Outbox;
		FTSTicker::FDelegateHandle PumpHandle;
	};
}

TSharedPtr<IRecoilBackend> MakeRecoilBackend(const FDetectedLightgun& Gun)
{
	switch (Gun.Model)
	{
	case ELightgunModel::Gun4IR:
	case ELightgunModel::OpenFIRE:
	case ELightgunModel::Blamcon:
	case ELightgunModel::RS3Reaper:
		return MakeShared<FSerialRecoilBackend>();
	case ELightgunModel::Sinden:
		return MakeShared<FSindenTcpBackend>();
	default:
		return nullptr;
	}
}
