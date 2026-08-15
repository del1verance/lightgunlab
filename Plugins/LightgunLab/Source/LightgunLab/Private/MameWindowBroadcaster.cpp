// Copyright (c) 2026 del1verance. MIT License.

#include "MameWindowBroadcaster.h"
#include "LightgunTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

namespace
{
	constexpr uintptr_t CopydataMessageIdString = 1;

	struct FCopydataIdString
	{
		uint32 Id;
		char String[1];
	};
}

struct FMameWindowBroadcaster::FImpl : public FRunnable
{
	FString GameName;
	HWND Hwnd = nullptr;
	FRunnableThread* Thread = nullptr;
	FEvent* ReadyEvent = nullptr;
	DWORD ThreadId = 0;

	UINT MsgStart = 0, MsgStop = 0, MsgUpdate = 0, MsgRegister = 0, MsgUnregister = 0, MsgGetIdString = 0;

	FCriticalSection Lock;
	TArray<HWND> Clients;
	TMap<FString, uint32> NameToId;
	TMap<uint32, FString> IdToName;
	uint32 NextId = 1;

	static FImpl* Instance;

	static LRESULT CALLBACK WndProc(HWND Wnd, UINT Msg, WPARAM WParam, LPARAM LParam)
	{
		FImpl* Self = Instance;
		if (Self)
		{
			if (Msg == Self->MsgRegister)
			{
				FScopeLock Guard(&Self->Lock);
				Self->Clients.AddUnique(reinterpret_cast<HWND>(WParam));
				return 0;
			}
			if (Msg == Self->MsgUnregister)
			{
				FScopeLock Guard(&Self->Lock);
				Self->Clients.Remove(reinterpret_cast<HWND>(WParam));
				return 0;
			}
			if (Msg == Self->MsgGetIdString)
			{
				Self->ReplyIdString(reinterpret_cast<HWND>(WParam), static_cast<uint32>(LParam));
				return 0;
			}
		}
		return DefWindowProcW(Wnd, Msg, WParam, LParam);
	}

	void ReplyIdString(HWND Client, uint32 Id)
	{
		FString Name;
		if (Id == 0)
		{
			Name = GameName; // id 0 is reserved for the machine name
		}
		else
		{
			FScopeLock Guard(&Lock);
			if (const FString* Found = IdToName.Find(Id))
			{
				Name = *Found;
			}
		}
		if (Name.IsEmpty())
		{
			return;
		}

		const auto Utf8 = StringCast<ANSICHAR>(*Name);
		const int32 PayloadSize = sizeof(uint32) + Utf8.Length() + 1;
		TArray<uint8> Buffer;
		Buffer.SetNumZeroed(PayloadSize);
		FCopydataIdString* Payload = reinterpret_cast<FCopydataIdString*>(Buffer.GetData());
		Payload->Id = Id;
		FMemory::Memcpy(Payload->String, Utf8.Get(), Utf8.Length());

		COPYDATASTRUCT Cds = {};
		Cds.dwData = CopydataMessageIdString;
		Cds.cbData = static_cast<DWORD>(PayloadSize);
		Cds.lpData = Buffer.GetData();
		SendMessageTimeoutW(Client, WM_COPYDATA, reinterpret_cast<WPARAM>(Hwnd), reinterpret_cast<LPARAM>(&Cds),
			SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, nullptr);
	}

	virtual uint32 Run() override
	{
		MsgStart = RegisterWindowMessageW(L"MAMEOutputStart");
		MsgStop = RegisterWindowMessageW(L"MAMEOutputStop");
		MsgUpdate = RegisterWindowMessageW(L"MAMEOutputUpdateState");
		MsgRegister = RegisterWindowMessageW(L"MAMEOutputRegister");
		MsgUnregister = RegisterWindowMessageW(L"MAMEOutputUnregister");
		MsgGetIdString = RegisterWindowMessageW(L"MAMEOutputGetIDString");

		WNDCLASSW WndClass = {};
		WndClass.lpfnWndProc = &FImpl::WndProc;
		WndClass.hInstance = GetModuleHandleW(nullptr);
		WndClass.lpszClassName = L"MAMEOutput";
		RegisterClassW(&WndClass);

		Hwnd = CreateWindowExW(0, L"MAMEOutput", *GameName, 0, 0, 0, 1, 1, HWND_MESSAGE, nullptr, WndClass.hInstance, nullptr);
		ThreadId = GetCurrentThreadId();

		if (Hwnd)
		{
			// Announce ourselves the way MAME does on machine start.
			PostMessageW(HWND_BROADCAST, MsgStart, reinterpret_cast<WPARAM>(Hwnd), 0);
		}
		if (ReadyEvent)
		{
			ReadyEvent->Trigger();
		}
		if (!Hwnd)
		{
			return 1;
		}

		MSG Msg;
		while (GetMessageW(&Msg, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&Msg);
			DispatchMessageW(&Msg);
		}

		PostMessageW(HWND_BROADCAST, MsgStop, reinterpret_cast<WPARAM>(Hwnd), 0);
		DestroyWindow(Hwnd);
		Hwnd = nullptr;
		return 0;
	}

	void Broadcast(const FString& Name, int32 Value)
	{
		uint32 Id = 0;
		{
			FScopeLock Guard(&Lock);
			if (const uint32* Found = NameToId.Find(Name))
			{
				Id = *Found;
			}
			else
			{
				Id = NextId++;
				NameToId.Add(Name, Id);
				IdToName.Add(Id, Name);
			}
		}
		FScopeLock Guard(&Lock);
		for (HWND Client : Clients)
		{
			PostMessageW(Client, MsgUpdate, static_cast<WPARAM>(Id), static_cast<LPARAM>(Value));
		}
	}

	void Shutdown()
	{
		if (ThreadId != 0)
		{
			PostThreadMessageW(ThreadId, WM_QUIT, 0, 0);
		}
		if (Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}
	}
};

FMameWindowBroadcaster::FImpl* FMameWindowBroadcaster::FImpl::Instance = nullptr;

#endif // PLATFORM_WINDOWS

FMameWindowBroadcaster::FMameWindowBroadcaster(const FString& InGameName)
	: GameName(InGameName)
{
}

FMameWindowBroadcaster::~FMameWindowBroadcaster()
{
	Stop();
}

bool FMameWindowBroadcaster::Start()
{
#if PLATFORM_WINDOWS
	if (Impl)
	{
		return true;
	}
	Impl = new FImpl();
	Impl->GameName = GameName;
	Impl->ReadyEvent = FPlatformProcess::GetSynchEventFromPool(true);
	FImpl::Instance = Impl;
	Impl->Thread = FRunnableThread::Create(Impl, TEXT("MameOutputWindow"));
	if (!Impl->Thread)
	{
		Stop();
		return false;
	}
	Impl->ReadyEvent->Wait(1000);
	UE_LOG(LogLightgunLab, Log, TEXT("MAME window-message outputs broadcasting as '%s'"), *GameName);
	return true;
#else
	return false;
#endif
}

void FMameWindowBroadcaster::Stop()
{
#if PLATFORM_WINDOWS
	if (Impl)
	{
		Impl->Shutdown();
		if (Impl->ReadyEvent)
		{
			FPlatformProcess::ReturnSynchEventToPool(Impl->ReadyEvent);
			Impl->ReadyEvent = nullptr;
		}
		FImpl::Instance = nullptr;
		delete Impl;
		Impl = nullptr;
	}
#endif
}

bool FMameWindowBroadcaster::IsRunning() const
{
#if PLATFORM_WINDOWS
	return Impl != nullptr && Impl->Hwnd != nullptr;
#else
	return false;
#endif
}

void FMameWindowBroadcaster::SendOutput(const FString& Name, int32 Value)
{
#if PLATFORM_WINDOWS
	if (Impl)
	{
		Impl->Broadcast(Name, Value);
	}
#endif
}
