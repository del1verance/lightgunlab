#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"

/**
 * Write-only serial port with a dedicated writer thread so gun commands
 * never block the game thread. Commands are ASCII strings sent verbatim
 * (no terminator appended — the gun dialects define their own framing).
 */
class FLightgunSerialPort : public FRunnable
{
public:
	FLightgunSerialPort();
	virtual ~FLightgunSerialPort() override;

	/** PortName like "COM5". Returns false and fills OutError on failure. */
	bool Open(const FString& PortName, int32 BaudRate, FString& OutError);

	/** Queues a command for the writer thread. Safe from any thread. */
	void Enqueue(const FString& Command);

	/** Blocks up to TimeoutMs for the queue to drain (used to flush release commands on shutdown). */
	void FlushAndClose(int32 TimeoutMs = 300);

	bool IsOpen() const { return Handle != nullptr; }
	bool HasError() const { return bWriteError; }
	FString GetPortName() const { return Port; }

	// FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	void CloseHandle_Internal();

	FString Port;
	void* Handle = nullptr;
	FRunnableThread* Thread = nullptr;
	FEvent* WakeEvent = nullptr;
	TQueue<FString, EQueueMode::Mpsc> Commands;
	FThreadSafeBool bStopRequested;
	FThreadSafeBool bWriteError;
	FThreadSafeCounter PendingCount;
};
