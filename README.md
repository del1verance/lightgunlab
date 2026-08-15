# LightgunLab / ArcadeLightgun

Native lightgun support for Unreal Engine 5.8 on Windows: auto-detection, **ammo-gated recoil** (the gun kicks on live rounds and stays silent on an empty magazine, like a real arcade board), Sinden border rendering, and MAME-compatible output emission for existing cabinet rigs.

`Plugins/ArcadeLightgun` is the deliverable — drop it into any UE 5.8 project. The surrounding `LightgunLab` project is a minimal test bed.

## Supported guns

| Gun | Aim | Recoil control | Protocol source |
|---|---|---|---|
| **GUN4IR** | mouse cursor | COM 9600: `S6` → `F0.2.1` → `E` | official User Guide v1.2 |
| **Sinden** (software ≥ V2.08a) | mouse cursor + in-game border | TCP `localhost:13000`: `1K0` → `1A` → `1K1`, soft empty-chamber `1U4` | official `RecoilTcpServerReadme.txt` |
| **Retro Shooter RS3 Reaper** | mouse cursor | COM 115200: `ZS` → `Z1`–`Z5` → `ZX`, empty state `Z0` | official manual |
| **OpenFIRE** | mouse cursor | COM 9600: `S6` → `F0x2x1` → `E`, OLED ammo `FDA<n>` | firmware source |
| **Blamcon** | mouse cursor | COM 9600: `SM.6.1` → `FB.0.1` → `ES` | vendor serial-command docs |
| Anything MAMEHooker knows (AimTrak, custom cabs) | mouse cursor | **Outputs mode** — see below | MAME network/window-message output protocols |

Guns are auto-detected by USB VID/PID (Gun4IR `2341:8042/8043`, OpenFIRE `F143`, Blamcon `3673`, RS3 `0483:5740`, Sinden HID `16C0:0F0x` + software process check). A startup panel lists what was found, lets the player confirm/switch/test-fire, and remembers the choice. Manual override always available.

## The ammo gate

Your weapon code reports what happened; the plugin routes it:

```cpp
ULightgunSubsystem* LG = GetGameInstance()->GetSubsystem<ULightgunSubsystem>();

LG->FireRecoil();     // live round -> solenoid kick
LG->NotifyEmpty();    // dry fire   -> silence (RS3: Z0 reload state, Sinden: soft clunk)
LG->SetAmmo(Count);   // feeds P1_Ammo output + OpenFIRE's OLED counter
LG->NotifyDamaged();  // rumble pulse + P1_Damaged output
```

`BeginGameControl()` seizes the gun's feedback channel (suppressing its built-in trigger recoil); `EndGameControl()` hands it back — both automatic around session lifetime, both crash-guarded.

## Outputs mode (MAMEHooker ecosystem)

Instead of (or alongside) direct control, the game can impersonate MAME's output system:

- **TCP server on :8000** speaking MAME's exact network output protocol (`mame_start = <game>`, `P1_CtmRecoil = 1`, CR-terminated) — consumed by QMamehook, Hook of the Reaper, OutputHooker, and Sinden software ≥ 2.08a.
- **Window-message broadcast** (`MAMEOutput` window + `WM_COPYDATA` interop) for classic MAMEHooker 5.1.

Players with an existing rig point their `lightgunlab.ini` at the game and every device they've ever wired up just works.

## Requirements

- Unreal Engine **5.8** (built against 5.8.1), Windows 64-bit
- For Sinden: the Sinden software **V2.08a or newer** running with its Recoil Server started (`Lightgun.exe tcpserver`, or the checkbox on the recoil outputs tab)
- For serial guns: nothing — plug in and go (RS3 recoil additionally needs its 24 V PSU)
- Game should run **borderless fullscreen** (required for the Sinden border overlay; correct for cursor aim everywhere)

## Install

**Into your own project:** copy `Plugins/ArcadeLightgun` into `<YourProject>/Plugins/`, enable it, build. All UI panels are C++-built with zero content dependencies and can be subclassed in Blueprint for reskinning.

**This test bed:** clone into a UE 5.8 source-build root (or fix up `EngineAssociation`), then open `LightgunLab.uproject`.

## Status — v0.1

Compiles clean; protocols implemented verbatim from vendor documentation. **Not yet hardware-validated** — bench testing on real GUN4IR and Sinden hardware is in progress; RS3/OpenFIRE/Blamcon backends built to spec and awaiting community confirmation. Known open items: RS3 VID/PID confirmation, OpenFIRE `FDA` value formatting, live MAMEHooker 5.1 window-message validation.

No license chosen yet — all rights reserved until one is picked.

## Protocol credits

Sinden Lightgun software package (RecoilTcpServerReadme.txt, ExternalRecoilOutputs.txt) · GUN4IR official User Guide v1.2 by JB · Retro Shooter RS3 Reaper manual · [OpenFIRE firmware](https://github.com/TeamOpenFIRE/OpenFIRE-Firmware) · [Blamcon serial commands](https://blamcon.com/get-started-with-blamcon/serial-commands/) · [MAME](https://github.com/mamedev/mame) network/win32 output modules · MAME Interop SDK
