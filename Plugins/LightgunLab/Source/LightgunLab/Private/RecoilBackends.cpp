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
			// Official User Guide v1.2 pp.20-21. F<chan>.<state>.<val>: state 0/1/2 =
			// off/on/pulses, val = strength (on) or pulse COUNT (pulse mode). Solenoid
			// is pulse-only in firmware; one solenoid pulse is a kick, but one rumble
			// pulse is an imperceptible blip (bench 2026-08-15) - buzz 3 pulses.
			D.Name = TEXT("GUN4IR");
			D.Baud = 9600;
			D.Enter = TEXT("S6");
			D.Exit = TEXT("E");
			D.Fire = TEXT("F0.2.1");
			D.Rumble = TEXT("F1.2.3");
			break;
		case ELightgunModel::OpenFIRE:
			// OpenFIRE-Firmware OpenFIREserial.cpp; MAMEHooker-convention 'x' separators.
			D.Name = TEXT("OpenFIRE");
			D.Baud = 9600;
			D.Enter = TEXT("S6");
			D.Exit = TEXT("E");
			D.Fire = TEXT("F0x2x1");
			D.Rumble = TEXT("F1x2x3"); // pulse mode, 3 pulses (1 = imperceptible; see GUN4IR note)
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
		// Reload feel = the rumble motor (GUN4IR/OpenFIRE/Blamcon F1..., RS3 ZZ),
		// never the solenoid. Guns without the motor fitted just ignore the command.
		virtual void NotifyReloaded() override        { if (!Dialect.Rumble.IsEmpty()) { Port->Enqueue(Dialect.Rumble); } }
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
	 * Sinden: thin per-player wrapper over the process-wide shared TCP connection.
	 * Its only job is the command prefix ("1"/"2" = the software's Lightgun A/B) and
	 * per-player strengths; pacing, bursts, and the socket live in the shared connection.
	 */
	class FSindenTcpBackend : public IRecoilBackend
	{
	public:
		virtual bool Init(const FDetectedLightgun& Gun, const ULightgunSettings& Settings, FString& OutError) override
		{
			Connection = PinSindenConnection();
			if (!Connection.IsValid())
			{
				OutError = TEXT("No socket subsystem");
				return false;
			}
			// 1P keeps the configured seat (validated path). In 2P the prefix follows the
			// HARDWARE's A/B identity (PID-derived PlayerHint), not our player slot: the
			// software addresses physical guns, and Swap on the range fixes a wrong guess.
			const int32 PrefixSlot = Settings.bTwoPlayerMode
				? FMath::Clamp(Gun.PlayerHint, 1, 2)
				: FMath::Clamp(Settings.PlayerSlot, 1, 2);
			Prefix = FString::FromInt(PrefixSlot);
			Strength = FMath::Clamp(Settings.RecoilStrength, 0, 10);
			EmptyStrength = FMath::Clamp(Settings.SindenEmptyChamberStrength, 0, 10);
			return true; // Presence of the HID gun is enough to construct; connection self-heals.
		}

		virtual void EnterGameControl() override
		{
			// Fresh software instances start with recoil DISABLED (bench-confirmed
			// 2026-08-15: without J1 every A command is silently ignored). No mode
			// command here: single-shot is the default, and a D mode-switch shortly
			// before a fire was observed to eat that fire on the bench.
			Send(TEXT("J1"));                                   // recoil master ON
			Send(TEXT("K0"));                                   // our commands only, no trigger recoil
			Send(TEXT("N") + FString::FromInt(Strength));       // strength
			bInControl = true;
		}

		virtual void ReleaseGameControl() override
		{
			if (bInControl)
			{
				Send(TEXT("K1"));
				bInControl = false;
			}
		}

		virtual void FireRecoil() override  { Send(TEXT("A")); }
		virtual void NotifyEmpty() override { Send(TEXT("U") + FString::FromInt(EmptyStrength)); }
		// No NotifyReloaded override: the Sinden's only actuator IS the solenoid,
		// and reload must stay silent (its "rumble" U command is a soft hammer hit).
		virtual void RumblePulse() override { Send(TEXT("U") + FString::FromInt(FMath::Min(EmptyStrength + 2, 10))); }
		virtual void PlayEffect(const FString& E) override { Send(E); }

		virtual bool IsHealthy() const override;
		virtual FString GetStatusText() const override;

		virtual ~FSindenTcpBackend() override
		{
			// The release lands in the shared queue; the connection (pinned by the
			// subsystem) keeps pumping it out after this wrapper dies.
			ReleaseGameControl();
		}

	private:
		void Send(const FString& Suffix);

		TSharedPtr<FSindenSharedConnection> Connection;
		FString Prefix = TEXT("1");
		int32 Strength = 8;
		int32 EmptyStrength = 4;
		bool bInControl = false;
	};
}

/**
 * The single Sinden recoil-server connection (RecoilTcpServerReadme.txt, V2.08b): one
 * socket, one paced pump, one queue for every player prefix. The server treats each
 * received chunk as one command and a second connection (or reconnect churn) wedges it
 * until the software restarts, so this object is process-wide and outlives backend churn.
 */
class FSindenSharedConnection
{
public:
	static TSharedPtr<FSindenSharedConnection> Acquire()
	{
		static TWeakPtr<FSindenSharedConnection> WeakInstance;
		TSharedPtr<FSindenSharedConnection> Instance = WeakInstance.Pin();
		if (!Instance.IsValid())
		{
			Instance = MakeShareable(new FSindenSharedConnection());
			if (!Instance->Start())
			{
				return nullptr;
			}
			WeakInstance = Instance;
		}
		return Instance;
	}

	void Send(const FString& Command)
	{
		Outbox.Enqueue(Command);
	}

	bool IsConnected() const { return Socket != nullptr; }

	FString GetEndpointText() const
	{
		return FString::Printf(TEXT("%s:%d"), *Host, TcpPort);
	}

	~FSindenSharedConnection()
	{
		// Push out whatever is queued (typically the K1 releases) before the socket dies.
		FlushQueueNow();
		if (PumpHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(PumpHandle);
		}
		CloseSocket();
	}

private:
	FSindenSharedConnection() = default;

	bool Start()
	{
		const ULightgunSettings* Settings = GetDefault<ULightgunSettings>();
		Host = Settings->SindenHost;
		TcpPort = Settings->SindenTcpPort;
		MinGapSeconds = FMath::Clamp(Settings->SindenCommandGapMs, 15, 1000) / 1000.0;

		FString Error;
		if (!Connect(Error))
		{
			// Software may not be running yet; stay lazy and retry on first send.
			UE_LOG(LogLightgunLab, Warning, TEXT("Sinden recoil server not reachable yet: %s"), *Error);
		}
		PumpHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FSindenSharedConnection::Pump), 0.005f);
		return true;
	}

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
		// Collapse a queued run of single fires FOR THE SAME PLAYER into one T burst
		// (count + inter-shot ms) so rapid trigger work isn't throttled to the TCP
		// pacing gap - the gun paces the burst internally. Runs never collapse across
		// prefixes, so interleaved 2P fire keeps its order.
		if (Command.Len() == 2 && Command[1] == TEXT('A') && (Command[0] == TEXT('1') || Command[0] == TEXT('2')))
		{
			int32 Count = 1;
			FString Next;
			while (Count < 9 && Outbox.Peek(Next) && Next == Command)
			{
				Outbox.Pop();
				++Count;
			}
			if (Count > 1)
			{
				Command = FString::Printf(TEXT("%cT%d120"), Command[0], Count);
			}
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
	double MinGapSeconds = 0.2;
	double LastSendTime = 0.0;
	double LastConnectAttempt = 0.0;
	TQueue<FString, EQueueMode::Mpsc> Outbox;
	FTSTicker::FDelegateHandle PumpHandle;
};

TSharedPtr<FSindenSharedConnection> PinSindenConnection()
{
	return FSindenSharedConnection::Acquire();
}

namespace
{
	void FSindenTcpBackend::Send(const FString& Suffix)
	{
		if (Connection.IsValid())
		{
			Connection->Send(Prefix + Suffix);
		}
	}

	bool FSindenTcpBackend::IsHealthy() const
	{
		return Connection.IsValid() && Connection->IsConnected();
	}

	FString FSindenTcpBackend::GetStatusText() const
	{
		return FString::Printf(TEXT("Sinden %s via %s%s"),
			Prefix == TEXT("1") ? TEXT("A") : TEXT("B"),
			Connection.IsValid() ? *Connection->GetEndpointText() : TEXT("?"),
			IsHealthy() ? TEXT("") : TEXT(" (recoil server not connected - start Sinden software / 'Start Recoil Server')"));
	}
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
