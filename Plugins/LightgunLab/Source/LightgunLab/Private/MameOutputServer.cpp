#include "MameOutputServer.h"
#include "LightgunTypes.h"

#include "Common/TcpListener.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

FMameOutputServer::FMameOutputServer(const FString& InGameName)
	: GameName(InGameName)
{
}

FMameOutputServer::~FMameOutputServer()
{
	Stop();
}

bool FMameOutputServer::Start(int32 Port)
{
	if (Listener)
	{
		return true;
	}
	const FIPv4Endpoint Endpoint(FIPv4Address(127, 0, 0, 1), static_cast<uint16>(Port));
	Listener = new FTcpListener(Endpoint);
	Listener->OnConnectionAccepted().BindRaw(this, &FMameOutputServer::HandleConnection);
	if (!Listener->IsActive())
	{
		UE_LOG(LogLightgunLab, Warning, TEXT("Outputs server failed to bind 127.0.0.1:%d (port in use?)"), Port);
		Stop();
		return false;
	}
	UE_LOG(LogLightgunLab, Log, TEXT("Outputs server listening on 127.0.0.1:%d as '%s'"), Port, *GameName);
	return true;
}

void FMameOutputServer::Stop()
{
	if (Listener)
	{
		SendLineToAll(TEXT("mame_stop = 1"));
	}

	{
		FScopeLock Lock(&ClientsLock);
		ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		for (FSocket* Client : Clients)
		{
			Client->Close();
			if (Subsystem)
			{
				Subsystem->DestroySocket(Client);
			}
		}
		Clients.Reset();
	}

	if (Listener)
	{
		delete Listener; // stops + joins the accept thread
		Listener = nullptr;
	}
}

int32 FMameOutputServer::GetClientCount() const
{
	FScopeLock Lock(&ClientsLock);
	return Clients.Num();
}

bool FMameOutputServer::HandleConnection(FSocket* ClientSocket, const FIPv4Endpoint& Endpoint)
{
	ClientSocket->SetNoDelay(true);
	ClientSocket->SetNonBlocking(true);
	// MAME greets each new client with the running machine's name.
	SendLine(ClientSocket, FString::Printf(TEXT("mame_start = %s"), *GameName));
	{
		FScopeLock Lock(&ClientsLock);
		Clients.Add(ClientSocket);
	}
	UE_LOG(LogLightgunLab, Log, TEXT("Outputs client connected from %s"), *Endpoint.ToString());
	return true;
}

void FMameOutputServer::SendOutput(const FString& Name, int32 Value)
{
	if (!Listener)
	{
		return;
	}
	SendLineToAll(FString::Printf(TEXT("%s = %d"), *Name, Value));
}

void FMameOutputServer::SendLineToAll(const FString& Line)
{
	FScopeLock Lock(&ClientsLock);
	ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	for (int32 Index = Clients.Num() - 1; Index >= 0; --Index)
	{
		if (!SendLine(Clients[Index], Line))
		{
			Clients[Index]->Close();
			if (Subsystem)
			{
				Subsystem->DestroySocket(Clients[Index]);
			}
			Clients.RemoveAt(Index);
		}
	}
}

bool FMameOutputServer::SendLine(FSocket* Target, const FString& Line)
{
	// MAME's network output module terminates lines with '\r'.
	const FString Framed = Line + TEXT("\r");
	const auto Utf8 = StringCast<ANSICHAR>(*Framed);
	int32 Sent = 0;
	const bool bOk = Target->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent);
	return bOk && Sent == Utf8.Length();
}
