// Copyright (c) 2026 del1verance. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

class FTcpListener;
class FSocket;
struct FIPv4Endpoint;

/**
 * MAME "-output network" compatible emitter: TCP server that greets each client
 * with "mame_start = <game>" and streams "name = value" lines (CR-terminated),
 * matching MAME's src/osd/modules/output/network.cpp wire format. QMamehook,
 * Hook of the Reaper, OutputHooker, and Sinden software (>=2.08a) consume this.
 */
class FMameOutputServer
{
public:
	explicit FMameOutputServer(const FString& InGameName);
	~FMameOutputServer();

	bool Start(int32 Port);
	void Stop();
	bool IsRunning() const { return Listener != nullptr; }
	int32 GetClientCount() const;

	void SendOutput(const FString& Name, int32 Value);

private:
	bool HandleConnection(FSocket* ClientSocket, const FIPv4Endpoint& Endpoint);
	void SendLineToAll(const FString& Line);
	static bool SendLine(FSocket* Target, const FString& Line);

	FString GameName;
	FTcpListener* Listener = nullptr;
	mutable FCriticalSection ClientsLock;
	TArray<FSocket*> Clients;
};
