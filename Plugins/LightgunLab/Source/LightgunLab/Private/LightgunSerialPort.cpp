// Copyright (c) 2026 del1verance. MIT License.

#include "LightgunSerialPort.h"
#include "LightgunTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include <windows.h>
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

FLightgunSerialPort::FLightgunSerialPort()
	: bStopRequested(false)
	, bWriteError(false)
{
	WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
}

FLightgunSerialPort::~FLightgunSerialPort()
{
	FlushAndClose(100);
	if (WakeEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
		WakeEvent = nullptr;
	}
}

bool FLightgunSerialPort::Open(const FString& PortName, int32 BaudRate, FString& OutError)
{
#if PLATFORM_WINDOWS
	if (Handle)
	{
		OutError = TEXT("Port already open");
		return false;
	}

	Port = PortName;
	const FString DevicePath = FString::Printf(TEXT("\\\\.\\%s"), *PortName);

	HANDLE H = CreateFileW(*DevicePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (H == INVALID_HANDLE_VALUE)
	{
		const DWORD Err = GetLastError();
		OutError = (Err == ERROR_ACCESS_DENIED)
			? FString::Printf(TEXT("%s is in use by another program (MAMEHooker/QMamehook/HOTR?)"), *PortName)
			: FString::Printf(TEXT("Failed to open %s (Windows error %u)"), *PortName, Err);
		return false;
	}

	DCB Dcb = {};
	Dcb.DCBlength = sizeof(DCB);
	GetCommState(H, &Dcb);
	Dcb.BaudRate = static_cast<DWORD>(BaudRate);
	Dcb.ByteSize = 8;
	Dcb.Parity = NOPARITY;
	Dcb.StopBits = ONESTOPBIT;
	// CDC guns commonly gate input on DTR; assert both control lines.
	Dcb.fDtrControl = DTR_CONTROL_ENABLE;
	Dcb.fRtsControl = RTS_CONTROL_ENABLE;
	Dcb.fOutxCtsFlow = 0;
	Dcb.fOutxDsrFlow = 0;
	if (!SetCommState(H, &Dcb))
	{
		CloseHandle(H);
		OutError = FString::Printf(TEXT("SetCommState failed on %s"), *PortName);
		return false;
	}

	COMMTIMEOUTS Timeouts = {};
	Timeouts.WriteTotalTimeoutConstant = 50;
	Timeouts.WriteTotalTimeoutMultiplier = 2;
	SetCommTimeouts(H, &Timeouts);

	Handle = H;
	bStopRequested = false;
	bWriteError = false;
	Thread = FRunnableThread::Create(this, *FString::Printf(TEXT("LightgunSerial_%s"), *PortName), 0, TPri_AboveNormal);
	if (!Thread)
	{
		CloseHandle_Internal();
		OutError = TEXT("Failed to start serial writer thread");
		return false;
	}

	UE_LOG(LogLightgunLab, Log, TEXT("Opened %s @ %d baud"), *PortName, BaudRate);
	return true;
#else
	OutError = TEXT("Serial ports are only supported on Windows");
	return false;
#endif
}

void FLightgunSerialPort::Enqueue(const FString& Command)
{
	if (!Handle || Command.IsEmpty())
	{
		return;
	}
	Commands.Enqueue(Command);
	PendingCount.Increment();
	if (WakeEvent)
	{
		WakeEvent->Trigger();
	}
}

void FLightgunSerialPort::FlushAndClose(int32 TimeoutMs)
{
	if (Thread)
	{
		const double Deadline = FPlatformTime::Seconds() + (TimeoutMs / 1000.0);
		while (PendingCount.GetValue() > 0 && FPlatformTime::Seconds() < Deadline)
		{
			FPlatformProcess::Sleep(0.005f);
		}
		Stop();
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	CloseHandle_Internal();
}

uint32 FLightgunSerialPort::Run()
{
#if PLATFORM_WINDOWS
	while (!bStopRequested)
	{
		FString Command;
		while (Commands.Dequeue(Command))
		{
			PendingCount.Decrement();
			const auto Utf8 = StringCast<ANSICHAR>(*Command);
			DWORD Written = 0;
			if (!WriteFile(static_cast<HANDLE>(Handle), Utf8.Get(), static_cast<DWORD>(Utf8.Length()), &Written, nullptr))
			{
				bWriteError = true;
				UE_LOG(LogLightgunLab, Warning, TEXT("Serial write failed on %s"), *Port);
			}
		}
		if (!bStopRequested && WakeEvent)
		{
			WakeEvent->Wait(50);
		}
	}
#endif
	return 0;
}

void FLightgunSerialPort::Stop()
{
	bStopRequested = true;
	if (WakeEvent)
	{
		WakeEvent->Trigger();
	}
}

void FLightgunSerialPort::CloseHandle_Internal()
{
#if PLATFORM_WINDOWS
	if (Handle)
	{
		CloseHandle(static_cast<HANDLE>(Handle));
		Handle = nullptr;
		UE_LOG(LogLightgunLab, Log, TEXT("Closed %s"), *Port);
	}
#endif
}
